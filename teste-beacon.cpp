#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEBeacon.h>
#include <Arduino.h>

BLEScan *scannerBluetooth;
String idDoMeuBeacon = "ID DO SEU BEACON";
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
        String idDispositivo = oBeacon.getProximityUUID().toString();

        if (idDispositivo == idDoMeuBeacon)
        {
            scannerBluetooth->stop();

            int potenciaSinal = dispositivoBluetooth.getRSSI();
            float distancia = calcularDistancia(potenciaSinal);
            Serial.printf("Beacon encontrado a %.1f metros!\n", distancia);
        }
    }
};

void setup()
{
    BLEDevice::init("");
    scannerBluetooth = BLEDevice::getScan();
    scannerBluetooth->setAdvertisedDeviceCallbacks(new MeuRastreador());
    scannerBluetooth->setActiveScan(true); // captura mais informações
    scannerBluetooth->setInterval(100);
    scannerBluetooth->setWindow(99);
};
void loop()
{
    scannerBluetooth->start(1, true); // escaneie durante 1s
    scannerBluetooth->clearResults();
}