//Inclusão das bibliotecas necessárias para conexão WiFi, HTTP, JSON e sensor DHT11
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT11.h"

//Configurações primárias
const char* ssid = "";    //Nome do WiFi
const char* password = "";    //Senha do WiFi
String token = "";  // Token do Tago.io

//Definição dos pinos dos LEDs, fotoresistor, relé e sensor DHT11
#define led1 16
#define led2 17
#define led3 18
#define led4 19
#define ft 32
#define rele 22

#define DHT_PIN 21
DHT11 dht11(DHT_PIN);

//Variáveis para controle das condições ideais por fase de vida
int faseVida = 0;
float temperaturaIdealMin = 0.0;
float umidadeIdealMin = 0.0;
float temperaturaIdealMax = 0.0;
float umidadeIdealMax = 0.0;
int temperaturaAtual = 0;
int umidadeAtual = 0;

//Variáveis de status
String ledstatus = "desconhecido";
String ventstatus = "desconhecido";
String silostatus = "desconhecido";

//Configuração inicial dos pinos, comunicação serial e conexão WiFi
void setup() {
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  pinMode(led4, OUTPUT);
  pinMode(ft, INPUT);
  pinMode(rele, OUTPUT);
  pinMode(15,OUTPUT);
  pinMode(23,INPUT);
  Serial.begin(9600);

  digitalWrite(rele, HIGH); // Desliga a ventoinha inicialmente
  verificaWifi();
}

//Função principal que executa repetidamente as leituras, ajustes e envio de dados
void loop() {
  lerFaseVida();
  ajustarCondicoes();
  atualizarTemperaturaEUmidade();
  controleDaVentoinha();

  int echo;
  int lum;

  lerDist(&echo);
  relatardist(echo); 
  lerFt(&lum);
  ajustarlum(lum);

  enviaDados(); 

  delay(5000); // Evita sobrecarga de dados no TagoIO
}

//Lê a distância com o sensor ultrassônico e armazena o valor em centímetros
void lerDist(int* echo1){
  *echo1 = 0.01723 * lerDistancia(15, 23);
  Serial.print("Distância: ");
  Serial.println(*echo1);
}

//Lê o valor do fotoresistor e imprime no monitor serial
void lerFt(int* lum1){
  *lum1 = analogRead(ft);
  Serial.print("luminosidade");
  Serial.println(*lum1);
}

long lerDistancia(int triggerPin, int echoPin){
  //pinMode(triggerPin, OUTPUT);
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);
  //pinMode(echoPin, INPUT);
  return pulseIn(echoPin, HIGH);
}

//Função para relatar se o silo está cheio ou vazio 
void relatardist(int echo){
  if (echo <= 4){
    Serial.println("Silo cheio");
    Serial.print("Distância: ");
    Serial.println(echo);
    silostatus = "silo abastecido";
  } else{
    Serial.println("Silo vazio");
    Serial.print("Distância: ");
    Serial.println(echo);
    silostatus = "silo vazio";
  }
}

//Função para verificar a conexão do esp com o WiFi
void verificaWifi() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    Serial.println("Conectando ao Wi-Fi...");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nWi-Fi conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
  }
}

//Função que promove a devolução de dados do tago para a fase de vida
void lerFaseVida() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.tago.io/data?variable=fase_vida&qty=1";
    http.begin(url);
    http.addHeader("Device-Token", token);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error && doc.containsKey("result") && doc["result"].size() > 0) {
        faseVida = doc["result"][0]["value"].as<int>();
        Serial.print("Fase recebida: ");
        Serial.println(faseVida);
      } else {
        Serial.println("Erro na resposta JSON.");
      }
    } else {
      Serial.print("Erro GET fase_vida: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    verificaWifi();
  }
}

//Função para ler a temperatura e a umidade do ambiente
void atualizarTemperaturaEUmidade() {
  int temp, umid;
  int resultado = dht11.readTemperatureHumidity(temp, umid);

  if (resultado == 0) {
    temperaturaAtual = temp;
    umidadeAtual = umid;
  } else {
    Serial.print("Erro no sensor DHT11: ");
    Serial.println(DHT11::getErrorString(resultado));
    temperaturaAtual = -1;
    umidadeAtual = -1;
  }
}

//Função com as configurações ideais para cada fase de vida + 2 fases para teste em sala de aula
void ajustarCondicoes() {
  switch (faseVida) {
    case 1:   //2 a 3 semana
      temperaturaIdealMin = 28.3; temperaturaIdealMax = 29.4;
      umidadeIdealMin = 50.0; umidadeIdealMax = 60.0;
      break;
    case 2:   //3 a 4 semana
      temperaturaIdealMin = 25.6; temperaturaIdealMax = 26.7;
      umidadeIdealMin = 50.0; umidadeIdealMax = 60.0;
      break;
    case 3:   //5 a 6 semana 
      temperaturaIdealMin = 23.9; temperaturaIdealMax = 24.5;
      umidadeIdealMin = 50.0; umidadeIdealMax = 60.0;
      break;
    case 4:   //6 em diante
      temperaturaIdealMin = 21.1; temperaturaIdealMax = 23.8;
      umidadeIdealMin = 50.0; umidadeIdealMax = 60.0;
      break;
    case 5: // Teste ligado
      temperaturaIdealMin = 0.0; temperaturaIdealMax = 10.0;
      umidadeIdealMin = 0.0; umidadeIdealMax = 100.0;
      break;
    case 6: // Teste desligado
      temperaturaIdealMin = 35.0; temperaturaIdealMax = 35.0;
      umidadeIdealMin = 0.0; umidadeIdealMax = 100.0;
      break;
    default:   //para caso o proprietário nao selecione nenhuma
      temperaturaIdealMin = 23.0; temperaturaIdealMax = 28.0;
      umidadeIdealMin = 50.0; umidadeIdealMax = 60.0;
      break;
  }

  //Apenas para saber se está lendo corretamente
  Serial.print("Cond. ideal => Temp: ");
  Serial.print(temperaturaIdealMin);
  Serial.print(" a ");
  Serial.print(temperaturaIdealMax);
  Serial.print(" °C | Umid: ");
  Serial.print(umidadeIdealMin);
  Serial.print(" a ");
  Serial.println(umidadeIdealMax);
}

//Controla a ventoinha de acordo com os parametros definidos pela fase de vida
void controleDaVentoinha() {
  if (temperaturaAtual == -1 || umidadeAtual == -1) {
    digitalWrite(rele, HIGH);
    ventstatus = "erro";
    Serial.println("Ventoinha DESLIGADA - Erro na leitura");
    return;
  }

  if (temperaturaAtual > temperaturaIdealMax || umidadeAtual > umidadeIdealMax) {
    digitalWrite(rele, LOW); // LIGA
    ventstatus = "ligado";
    Serial.println("Ventoinha Ligada");
  } else {
    digitalWrite(rele, HIGH); // DESLIGA
    ventstatus = "desligado";
    Serial.println("Ventoinha Desligada");
  }
}

//Liga ou desliga os LEDs com base no valor de luminosidade
void ajustarlum(int lum) {
  if (lum > 650) {
    ligarleds();
  } else {
    desligarleds();
  }
}

//Liga ou desliga todos os LEDs e atualiza o status
void ligarleds() {
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);
  digitalWrite(led4, HIGH);
  ledstatus = "ligada";
}

void desligarleds() {
  digitalWrite(led1, LOW);
  digitalWrite(led2, LOW);
  digitalWrite(led3, LOW);
  digitalWrite(led4, LOW);
  ledstatus = "desligada";
}

//Envia os dados de temperatura, umidade, LEDs, ventoinha e silo para o TagoIO
void enviaDados() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;   //é um objeto que permite fazer requisições HTTP por GET POST, etc.
    http.begin("https://api.tago.io/data");   //inicia a conexão
    http.addHeader("Content-Type", "application/json");  
    http.addHeader("Device-Token", token); //adiciona um cabeçalho com o token que é necessário para a autenticação

    StaticJsonDocument<512> doc;
    JsonArray data = doc.to<JsonArray>();  //array que contém os dados que serão enviados

    //"Pacotes" do json com as variáveis e dados
    JsonObject ldr = data.createNestedObject();
    ldr["variable"] = "led_status";
    ldr["value"] = ledstatus;

    JsonObject statusV = data.createNestedObject();
    statusV["variable"] = "status_ventoinha";
    statusV["value"] = ventstatus;

    JsonObject temp = data.createNestedObject();
    temp["variable"] = "temperatura";
    temp["value"] = temperaturaAtual;
    temp["unit"] = "°C";

    JsonObject hum = data.createNestedObject();
    hum["variable"] = "umidade";
    hum["value"] = umidadeAtual;
    hum["unit"] = "%";

    JsonObject silo = data.createNestedObject();
    silo["variable"] = "conteudo_silo";
    silo["value"] = silostatus;

    String json;
    serializeJson(doc, json);

    int httpResponseCode = http.POST(json);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Resposta do TagoIO:");
      Serial.println(response);
    } else {
      Serial.print("Erro no envio: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}
