#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEBeacon.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "certificados.h"
#include <MQTT.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// variaveis
WiFiClientSecure conexaoSegura;
BLEScan *scannerBluetooth;
float distancia;
WiFiClient conexao;
MQTTClient mqtt(1000);

// Constantes
int NUMERO_BEACONS = 2;
int NUMERO_ARDUINO_INT = 1;
const char *NUMERO_ARDUINO_STR = "1";
String beacons_ids_string[] = {"51:00:23:11:04:6d", "51:00:23:11:04:38"};
int indice_beacon_atual = 0;

int encontrar_beacon_requisitado(String beacon)
{
  distancia = 0;
  for (int i = 0; i < NUMERO_BEACONS; i++)
  {
    if (beacon == beacons_ids_string[i])
    {
      return i;
    }
  }
  return indice_beacon_atual;
}

void reconectarWiFi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.begin("LabIoT", "4n1m4l5@))!!");
    Serial.print("Conectando ao WiFi...");
    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print(".");
      delay(1000);
    }
    Serial.print("conectado!\nEndereço IP: ");
    Serial.println(WiFi.localIP());
  }
}

void reconectarMQTT()
{
  String num_ard = String(NUMERO_ARDUINO_STR);
  if (!mqtt.connected())
  {
    Serial.print("Conectando MQTT...");
    while (!mqtt.connected())
    {

      mqtt.connect(NUMERO_ARDUINO_STR, "aula", "zowmad-tavQez");// anonimizacao
      Serial.print(".");
      delay(1000);
    }
    Serial.println(" conectado!");

    mqtt.subscribe("A1/esp32/"+num_ard+"/+", 2);
  }
}

void recebeuMensagem(String topico, String conteudo)
{
  //Serial.println(topico + ": " + conteudo);
  String numero_ard = String(NUMERO_ARDUINO_STR);

  if (topico.startsWith("A1/esp32/" + numero_ard))
  {
    //Serial.println("Recebi mensagem. Enviando para o MQTT.");
    String beacon_requisitado = topico.substring(11);
    Serial.println("Beacon requisitado: " + beacon_requisitado);
    
    // se recebi mensagem, tenho que identificar o indice do beacon_atual (passado como parametro apos o numero ard)
    indice_beacon_atual = encontrar_beacon_requisitado(beacon_requisitado);
    Serial.println("Indice atual " + String(indice_beacon_atual));

    // agora que ele encontrou o beacon, entao podemos pedir para medir a distancia
    scannerBluetooth->start(1, true); // escaneie durante 1s
    scannerBluetooth->clearResults();

    JsonDocument dados;
    dados["distancia"] = distancia;
    dados["beacon_id"] = beacons_ids_string[indice_beacon_atual];

    String informacoes;

    serializeJson(dados, informacoes);

    mqtt.publish("A1/esp32/" + numero_ard + "/distancia", informacoes, false, 2);
  }
}

float calcularDistancia(int potenciaSinal)
{
  if (potenciaSinal == 0)
  {
    return -1.0; // Não foi possível estimar a distância
  }
  int potenciaReferencia = -59; // dBm medido com distância de 1 metro
  float razao = potenciaSinal * 1.0 / potenciaReferencia;
  float distancia;
  if (razao < 1.0)
  {
    distancia = pow(razao, 10); // cálculo para distâncias < 1m
  }
  else
  {
    distancia = (0.89976) * pow(razao, 7.7095) + 0.111;
  }
  return (float)(int)(distancia * 10 + 0.5) / 10.0; // arredonda para 1 casa
}

class MeuRastreador : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice dispositivoBluetooth)
  {
    String dadosBeacon = dispositivoBluetooth.getManufacturerData();
    if (dadosBeacon.length() != 25)
    {
      return; // não é Beacon
    }
    BLEBeacon oBeacon = BLEBeacon();
    oBeacon.setData(dadosBeacon);
    String idDispositivo = dispositivoBluetooth.getAddress().toString();

    if (idDispositivo == beacons_ids_string[indice_beacon_atual])
    {
      scannerBluetooth->stop();

      int potenciaSinal = dispositivoBluetooth.getRSSI();
      distancia = calcularDistancia(potenciaSinal);
      Serial.printf("Beacon encontrado a %.1f metros!\n", distancia);
    }
  }
};

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("Projeto Final - Semana 2");

  reconectarWiFi();
  conexaoSegura.setCACert(certificado1);
  mqtt.begin("mqtt.janks.dev.br", 1883, conexao); // anonimizacao
  mqtt.onMessage(recebeuMensagem);
  mqtt.setKeepAlive(10);
  mqtt.setWill("tópico da desconexão", "conteúdo");

  reconectarMQTT();

  BLEDevice::init("");
  scannerBluetooth = BLEDevice::getScan();
  scannerBluetooth->setAdvertisedDeviceCallbacks(new MeuRastreador());
  scannerBluetooth->setActiveScan(true); // captura mais informações
  scannerBluetooth->setInterval(100);
  scannerBluetooth->setWindow(99);
}

void loop()
{
  // scannerBluetooth->start(1, true); // escaneie durante 1s
  // scannerBluetooth->clearResults();

  reconectarWiFi();
  reconectarMQTT();
  mqtt.loop();
}