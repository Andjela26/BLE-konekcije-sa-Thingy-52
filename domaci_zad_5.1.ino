#include "BLEDevice.h"
const int BTN_UP_PIN = 5;    //pin tastera za inkrement
const int BTN_DOWN_PIN = 6;  //pin tastera za dekrement


// UUIDs za thingy servise i karakteristike
BLEUUID thingyConfigServiceUUID("EF680100-9B35-4933-9B10-52FFA9740042");
BLEUUID environmentServiceUUID("EF680200-9B35-4933-9B10-52FFA9740042");
BLEUUID soundServiceUUID("EF680500-9B35-4933-9B10-52FFA9740042");
BLEUUID humidityCharUUID("EF680203-9B35-4933-9B10-52FFA9740042");
BLEUUID temperatureCharUUID("EF680201-9B35-4933-9B10-52FFA9740042");
BLEUUID pressureCharUUID("EF680202-9B35-4933-9B10-52FFA9740042");

BLEUUID configCharUUID("EF680501-9B35-4933-9B10-52FFA9740042");
BLEUUID speakerCharUUID("EF680502-9B35-4933-9B10-52FFA9740042");

BLEAdvertisedDevice* thingyDevice; // Pointer  Thingy
BLEScan* pBLEScan; // Pointer  BLE scanner
BLERemoteCharacteristic* pRemoteHumidityChar;         //pointeri na karakteristike
BLERemoteCharacteristic* pRemoteTemperatureChar;
BLERemoteCharacteristic* pRemotePressureChar;

BLERemoteCharacteristic* pRemoteConfigChar;
BLERemoteCharacteristic* pRemoteSpeakerChar;


bool connected = false;                 //indikacija aktivne konekcije
bool onConnected = false;               //indikacija upravo otvorene konekcije
bool deviceFound = false;               //indikacija da je pronadjen BLE Server 2
int scanTime = 5;                       //vreme skeniranja (u sekundama)


//callback funkcija BLE skenera
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(thingyConfigServiceUUID)) {
      pBLEScan->stop();
      thingyDevice = new BLEAdvertisedDevice(advertisedDevice);
      deviceFound = true;
    }
  }
};
//callback funkcije BLE klijenta
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {}
  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("Disconnected");
  }
};

void humidityNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    uint8_t humidity = pData[0];
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
}
void temperatureNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    int16_t tempIntPart = pData[0];
    uint8_t tempFracPart = pData[1];
    float temperature = tempIntPart + (tempFracPart / 100.0);
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");
}


void pressureNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    uint32_t pressIntPart = pData[0] | (pData[1] << 8) | (pData[2] << 16) | (pData[3] << 24);
    uint8_t pressFracPart = pData[4];
    float pressure = pressIntPart + (pressFracPart / 100.0);
    Serial.print("Pressure: ");
    Serial.print(pressure);
    Serial.println(" Pa");
}

/*
//callback funkcija notifikacije karakteristike 2
void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                    uint8_t* pData, size_t length, bool isNotify) {
  remoteBtnCntr = (pData[1] << 8) + pData[0];
  Serial.print("Button counter: ");
  Serial.println(remoteBtnCntr);
}*/

//povezivanje sa BLE serverom
// Connect to Thingy:52 server
void connectToServer() {
    connected = false;
    Serial.print("Connecting to: ");
    Serial.println(thingyDevice->getAddress().toString().c_str());
    BLEClient* pClient = BLEDevice::createClient();                                        //kreiranje instance ble klijenta
    pClient->setClientCallbacks(new MyClientCallback());                                    // povezivanje sa callback funk
    if (!pClient->connect(thingyDevice)) {                                                  //povezivanje sa upravo oktrivenim serverom
        Serial.println("Failed to connect to the device");
        return;
    }
    pClient->setMTU(517); // maax duzina paketa

    // Get the Thingy Configuration Service
    BLERemoteService* pRemoteService = pClient->getService(thingyConfigServiceUUID);                  //refernca na servis pre uuid-u servisa
    if (pRemoteService == nullptr) {                                                                 //ako je pRemoteService=null znaci da taj servis ne postoji
        Serial.print("Thingy Servis nije pronadjen UUID: ");
        Serial.println(thingyConfigServiceUUID.toString().c_str());
        pClient->disconnect();                                                                      //ble konekcija se raskida
        return;
    }
    Serial.println("Thingy Service pronadjen");

    // Get the Environment Service
    BLERemoteService* pEnvironmentService = pClient->getService(environmentServiceUUID);
    if (pEnvironmentService == nullptr) {
        Serial.print("Environment Servis nije pronadjen  UUID: ");
        Serial.println(environmentServiceUUID.toString().c_str());
        pClient->disconnect();
        return;
    }
    Serial.println("Environment Servis pronadjen");

    // Get the Sound Service
    BLERemoteService* pSoundService = pClient->getService(soundServiceUUID);
    if (pSoundService == nullptr) {
        Serial.print("Sound  Servis nije pronadjen  UUID: ");
        Serial.println(soundServiceUUID.toString().c_str());
        pClient->disconnect();
        return;
    }
    Serial.println("Sound Servis pronadjen");

    // Get characteristics
    pRemoteTemperatureChar = pEnvironmentService->getCharacteristic(temperatureCharUUID);
    pRemotePressureChar = pEnvironmentService->getCharacteristic(pressureCharUUID);
    pRemoteHumidityChar = pEnvironmentService->getCharacteristic(humidityCharUUID);             //pribavljanje karakteristike
    pRemoteConfigChar = pSoundService->getCharacteristic(configCharUUID);
    pRemoteSpeakerChar = pSoundService->getCharacteristic(speakerCharUUID);

    if (pRemoteHumidityChar == nullptr) {
        Serial.print("Vlaznost vazduha  UUID: ");
        Serial.println(humidityCharUUID.toString().c_str());                         //Ako je pRomete=null znaci da karakteristika ne postoji
    }
    if (pRemoteTemperatureChar == nullptr) {
        Serial.print("Temperatura nije pronadjen UUID: ");
        Serial.println(temperatureCharUUID.toString().c_str());
    }
    if (pRemotePressureChar == nullptr) {
        Serial.print("Pritisak nije pronadjen UUID: ");
        Serial.println(pressureCharUUID.toString().c_str());
    }
    
    if (pRemoteConfigChar == nullptr) {
        Serial.print("Config nije pronadjen UUID: ");
        Serial.println(configCharUUID.toString().c_str());
    }
    if (pRemoteSpeakerChar == nullptr) {
        Serial.print("Speaker nije pronadjen UUID: ");
        Serial.println(speakerCharUUID.toString().c_str());
    }

    if (pRemoteTemperatureChar == nullptr || pRemotePressureChar == nullptr || pRemoteHumidityChar == nullptr || pRemoteConfigChar == nullptr || pRemoteSpeakerChar == nullptr) {
        pClient->disconnect();                                                        //ako bilo koji od ovih pointer je null raskida se konekcija
        return;
    }

    // Enable notifications
    if (pRemoteHumidityChar->canNotify())
        pRemoteHumidityChar->registerForNotify(humidityNotifyCallback);
        
    if (pRemoteTemperatureChar->canNotify())
        pRemoteTemperatureChar->registerForNotify(temperatureNotifyCallback);
    if (pRemotePressureChar->canNotify())
        pRemotePressureChar->registerForNotify(pressureNotifyCallback);
   
    // Generate a tone to confirm the connection
    if (pRemoteSpeakerChar->canWriteNoResponse()) {
        uint8_t soundArray[5] = {0xF4, 0x01, 0xE8, 0x03, 0x50}; // Frequency: 500 Hz, Duration: 1000 ms, Volume: 80%
        pRemoteSpeakerChar->writeValue(soundArray, 5, true);
    }

    connected = true;                             //konekcija je uspesno otvorena i upravo otvorena (ako je sve do sad sve bilo kako treba tj, nije doslo do raskida konecije pClient->disconnect(); )
    onConnected = true;
}
void setup() {
    Serial.begin(115200);
    BLEDevice::init("ESP32 BLE Client");

    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
}

void loop() {
    if (!connected) {
        deviceFound = false;
        while (!deviceFound) {
            delay(1000);
            Serial.println("Scanning...");
            pBLEScan->start(scanTime, false);
            pBLEScan->clearResults();
        }
        connectToServer();
    } else {
        if (onConnected) {
            onConnected = false;
        }
    }
    delay(1000);
}