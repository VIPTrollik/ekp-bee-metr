#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <math.h>
#include <ESP32Time.h>

#include <Time.h>

#include "dataRW.h"
#include "weight.h"

// Global Variables
#define DEVICE_NAME "WAHA"

bool deviceConnected = false;

ESP32Time rtc;
bool rtc_time_set = false;

uint32_t date_from = 0;
uint32_t date_to = UINT32_MAX;

int offset = 0;

bool notifyInProgress = false;
bool dataRequested = false;

typedef struct
{
  float time;
  float weight;
} record_msg_t;

record_t record_buff[100];
record_msg_t record_msg_buff[100];


uint32_t last_time_sent = 0;
bool ledState = false;
uint32_t blink_last = 0;
uint32_t last_weight_time = 0;
uint32_t last_recording_time = 0;

// BLE Service and Characteristic UUIDs

#define GRAPH_VIEW_SERVICE_UUID "d052cc98-92ab-420a-b19c-780f74650aec"
#define CHAR_UUID_DATE_FROM "d052cc98-92ab-420a-b19c-780f74650ae1"         // read, integer as timestamp
#define CHAR_UUID_DATE_TO "d052cc98-92ab-420a-b19c-780f74650ae2"           // read, integer as timestamp
#define CHAR_UUID_NUMBER_OF_RECORDS "d052cc98-92ab-420a-b19c-780f74650ae6" // read, integer
#define CHAR_UUID_READ_DATA "d052cc98-92ab-420a-b19c-780f74650ae5"         // notify, must subscribe to notify, writing date range will trigger notify
#define CHAR_UUID_DELETE_DATA "d052cc98-92ab-420a-b19c-780f74650ae7"       // write, delete data from date range
#define CHAR_UUID_READ_TRIGGER "d052cc98-92ab-420a-b19c-780f74650ae8"      // write, writing will trigger read data

#define WEIGHT_SERVICE_UUID "d052cc98-92ab-420a-b19c-780f74650adc"
#define CHAR_UUID_WEIGHT_READ "d052cc98-92ab-420a-b19c-780f74650ad1"        // read, float, notify
#define CHAR_UUID_WEIGHT_CALIBRATION "d052cc98-92ab-420a-b19c-780f74650ad2" // write, float
#define CHAR_UUID_WEIGHT_TARE_ZERO "d052cc98-92ab-420a-b19c-780f74650ad3"   // write, tare zero

#define GENERAL_SERVICE_UUID "d052cc98-92ab-420a-b19c-780f74650abc"
#define CHAR_UUID_TIME_SET "d052cc98-92ab-420a-b19c-780f74650ab1" // write, integer as timestamp

// Server Callbacks
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    Serial.println("Device connected");
    deviceConnected = true;
    BLEDevice::startAdvertising();
  }

  void onDisconnect(BLEServer *pServer)
  {
    Serial.printf("Device disconnected");
    deviceConnected = false;
    BLEDevice::startAdvertising();
  }
};

class ReadWeightCallbacks : public BLECharacteristicCallbacks
{
  void onRead(BLECharacteristic *pCharacteristic)
  {
    float weight = load_cell_read();
    pCharacteristic->setValue((uint8_t *)&weight, sizeof(float));
  }
};

class CalibrateWeightCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    if (pCharacteristic->getValue().length() != 4)
    {
      return;
    }
    uint8_t *data = pCharacteristic->getData();
    printf("Calibration data: %d %d %d %d\n", data[0], data[1], data[2], data[3]);
    // uint8_t datab[4] = {data[3], data[2], data[1], data[0]};
    float_t known_weight = 3.69f;
    memcpy(&known_weight, data, sizeof(float));
    printf("Calibration data: %f\n", known_weight);
    load_cell_calibrate(known_weight);
  }
};

class TareZeroCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    load_cell_tare();
  }
};

class SetDateCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    if (pCharacteristic->getValue().length() != 4)
    {
      return;
    }
    uint8_t *data = pCharacteristic->getData();

    unsigned long epoch_time = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];

    rtc.setTime(epoch_time);

    Serial.printf("Epoch time set to: %lu\n", epoch_time);
    Serial.printf("Time set to: %s\n", rtc.getTime().c_str());
    Serial.printf("Date set to: %s\n", rtc.getDate().c_str());

    rtc_time_set = true;

    if (dataRW_read_num_records() == 0)
    {
      dataRW_reset_mem();

      uint32_t ttm = rtc.getLocalEpoch();

      for (int i = 0; i < 1000; i++)
      {
        record_t record;
        record.time = ttm + i * 600;
        record.weight = sin(i / 10.0f) * 10.0f + sin(i / 3.0f) * 2.0f + 50.0f;
        dataRW_write_data(record);
        Serial.printf("Record %d: %lu, %.2f\n", i, record.time, record.weight);
      }
      dataRW_flush();
    }
  }
};

class DeleteDataCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    Serial.println("Deleting all data");
    dataRW_reset_mem();
    Serial.println("All data deleted");
    last_recording_time = 0;
  }
};

class ReadDateFromCallbacks : public BLECharacteristicCallbacks
{
  void onRead(BLECharacteristic *pCharacteristic)
  {
    uint32_t firstRecordEpoch = dataRW_read_first_date();
    Serial.printf("First record epoch: %lu\n", firstRecordEpoch);
    pCharacteristic->setValue((uint8_t *)&firstRecordEpoch, sizeof(uint32_t));
  }
};

class ReadDateToCallbacks : public BLECharacteristicCallbacks
{
  void onRead(BLECharacteristic *pCharacteristic)
  {
    uint32_t lastRecordEpoch = dataRW_read_last_date();
    Serial.printf("Last record epoch: %lu\n", lastRecordEpoch);
    pCharacteristic->setValue((uint8_t *)&lastRecordEpoch, sizeof(uint32_t));
  }
};

class ReadNumRecordsCallbacks : public BLECharacteristicCallbacks
{
  void onRead(BLECharacteristic *pCharacteristic)
  {
    uint32_t num_records = dataRW_read_num_records();
    Serial.printf("number of records ret: %d\n", num_records);

    pCharacteristic->setValue((uint8_t *)&num_records, sizeof(int));
  }
};

class ReadDataCallbacks : public BLECharacteristicCallbacks
{
  void onStatus(BLECharacteristic *pCharacteristic, Status s, uint32_t code)
  {
    Serial.printf("Status: %d\n", s);
    if (s == 1)
    {
      Serial.println("Notify good");
      notifyInProgress = false;
    }
    else if (s == BLECharacteristicCallbacks::Status::ERROR_NOTIFY_DISABLED)
    {
      Serial.println("Notify disabled");
      notifyInProgress = false;
      dataRequested = false;
    }
  }
  void onNotify(BLECharacteristic *pCharacteristic)
  {
    Serial.println("On notify");
  }
};

class TriggerReadDataCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    if (pCharacteristic->getValue().length() != 8)
    {
      return;
    }
    uint8_t *data = pCharacteristic->getData();

    date_from = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
    date_to = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];

    dataRequested = true; // Set the flag
    Serial.printf("Data requested from %lu to %lu\n", date_from, date_to);
  }
};

BLEServer *pServer;

BLEService *pServiceGeneral;
BLECharacteristic *timeSetCharacteristic;
BLECharacteristic *weightReadCharacteristic;

BLEService *pServiceGraphView;
BLECharacteristic *dateFromCharacteristic;
BLECharacteristic *dateToCharacteristic;
BLECharacteristic *numRecordsCharacteristic;
BLECharacteristic *readDataCharacteristic;
BLECharacteristic *readDataTriggerCharacteristic;
BLECharacteristic *deleteDataCharacteristic;

void initBLE()
{
  Serial.println("Starting BLE Server...");

  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setMTU(512);

  // Create BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // General Service
  pServiceGeneral = pServer->createService(GENERAL_SERVICE_UUID);

  timeSetCharacteristic = pServiceGeneral->createCharacteristic(
      CHAR_UUID_TIME_SET,
      BLECharacteristic::PROPERTY_WRITE);
  timeSetCharacteristic->setCallbacks(new SetDateCallbacks());

  pServiceGeneral->start();

  // Graph View Service
  pServiceGraphView = pServer->createService(GRAPH_VIEW_SERVICE_UUID);

  dateFromCharacteristic = pServiceGraphView->createCharacteristic(
      CHAR_UUID_DATE_FROM,
      BLECharacteristic::PROPERTY_READ);
  dateFromCharacteristic->setCallbacks(new ReadDateFromCallbacks());

  dateToCharacteristic = pServiceGraphView->createCharacteristic(
      CHAR_UUID_DATE_TO,
      BLECharacteristic::PROPERTY_READ);
  dateToCharacteristic->setCallbacks(new ReadDateToCallbacks());

  numRecordsCharacteristic = pServiceGraphView->createCharacteristic(
      CHAR_UUID_NUMBER_OF_RECORDS,
      BLECharacteristic::PROPERTY_READ);
  numRecordsCharacteristic->setCallbacks(new ReadNumRecordsCallbacks());

  readDataCharacteristic = pServiceGraphView->createCharacteristic(
      CHAR_UUID_READ_DATA, BLECharacteristic::PROPERTY_NOTIFY);
  readDataCharacteristic->addDescriptor(new BLE2902());
  readDataCharacteristic->setCallbacks(new ReadDataCallbacks());

  readDataTriggerCharacteristic = pServiceGraphView->createCharacteristic(
      CHAR_UUID_READ_TRIGGER,
      BLECharacteristic::PROPERTY_WRITE);
  readDataTriggerCharacteristic->setCallbacks(new TriggerReadDataCallbacks());

  deleteDataCharacteristic = pServiceGraphView->createCharacteristic(
      CHAR_UUID_DELETE_DATA,
      BLECharacteristic::PROPERTY_WRITE);
  deleteDataCharacteristic->setCallbacks(new DeleteDataCallbacks());

  pServiceGraphView->start();

  // Weight Service
  BLEService *pServiceWeight = pServer->createService(WEIGHT_SERVICE_UUID);

  weightReadCharacteristic = pServiceWeight->createCharacteristic(
      CHAR_UUID_WEIGHT_READ,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLE2902 *ble2902desc = new BLE2902();
  weightReadCharacteristic->addDescriptor(ble2902desc);
  weightReadCharacteristic->setCallbacks(new ReadWeightCallbacks());

  BLECharacteristic *weightCalibrationCharacteristic = pServiceWeight->createCharacteristic(
      CHAR_UUID_WEIGHT_CALIBRATION,
      BLECharacteristic::PROPERTY_WRITE);
  weightCalibrationCharacteristic->setCallbacks(new CalibrateWeightCallbacks());

  BLECharacteristic *weightTareZeroCharacteristic = pServiceWeight->createCharacteristic(
      CHAR_UUID_WEIGHT_TARE_ZERO,
      BLECharacteristic::PROPERTY_WRITE);
  weightTareZeroCharacteristic->setCallbacks(new TareZeroCallbacks());

  pServiceWeight->start();

  // Start Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(GENERAL_SERVICE_UUID);
  pAdvertising->addServiceUUID(GRAPH_VIEW_SERVICE_UUID);
  pAdvertising->addServiceUUID(WEIGHT_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  pAdvertising->setMaxPreferred(0x24);

  BLEDevice::startAdvertising();

  Serial.println("BLE Server is running...");
}

void setup()
{
  Serial.begin(115200);

  // init dataRW
  if (!dataRW_init())
  {
    Serial.println("Failed to initialize dataRW");
    while (1)
      ;
  }
  Serial.println("dataRW initialized");
  Serial.printf("First record epoch: %lu\n", dataRW_read_first_date());
  Serial.printf("Last record epoch: %lu\n", dataRW_read_last_date());
  Serial.printf("Number of records: %d\n", dataRW_read_num_records());

  if (dataRW_read_num_records() >= 1)
  {
    unsigned long cca_time = dataRW_read_last_date();
    rtc.setTime(cca_time);
  }

  // init weight
  Serial.println("Initializing load cell");
  if (!load_cell_init())
  {
    Serial.println("Failed to initialize load cell");
    while (1)
      ;
  }
  Serial.println("Load cell initialized");

  // init BLE
  initBLE();

  pinMode(LED_BUILTIN, OUTPUT);
}

void ble_send_loop()
{
  if (dataRequested && !notifyInProgress)
  {
    Serial.println("Sending data...");
    int msg_max_size = BLEDevice::getMTU() - 3;
    Serial.printf("from: %lu, to: %lu, max_count: %d\n", (last_time_sent < date_from) ? date_from : last_time_sent, date_to, msg_max_size / sizeof(record_t));
    int num_records = dataRW_read_data(record_buff, (last_time_sent < date_from) ? date_from : last_time_sent, date_to, msg_max_size / sizeof(record_t));
    Serial.printf("Records read: %d\n", num_records);

    if (num_records > 0)
    {
      uint32_t last_record_time = record_buff[num_records - 1].time;
      if (last_record_time != last_time_sent)
      {
        last_time_sent = record_buff[num_records - 1].time + 1;
        for (int i = 0; i < num_records; i++)
        {
          record_msg_buff[i].time = (float)((double)(record_buff[i].time - date_from) / (date_to - date_from));
          record_msg_buff[i].weight = record_buff[i].weight;
        }
        readDataCharacteristic->setValue((uint8_t *)record_msg_buff, num_records * sizeof(record_msg_t));
        notifyInProgress = true;
        readDataCharacteristic->notify();
      }
      else
      {
        Serial.println("No new data");
        dataRequested = false;
      }
    }
    if (last_time_sent >= date_to || num_records <= 0 || !dataRequested)
    {
      Serial.println("All data sent");
      dataRequested = false;
      last_time_sent = 0;
    }
  }
}

void notify_weight(){
    float weight = load_cell_read();
    Serial.printf("Weight: %.2f\n", weight);
    weightReadCharacteristic->setValue((uint8_t *)&weight, sizeof(float));
    weightReadCharacteristic->notify();
}

void loop()
{
  unsigned long mils = millis();
  // Blink LED every second
  // Notify status value every second if connected
  if (deviceConnected && mils > blink_last + 1000)
  {
    blink_last = mils;
    digitalWrite(LED_BUILTIN, ledState);
    ledState = !ledState;
  }
  else if (ledState)
  {
    digitalWrite(LED_BUILTIN, LOW);
    ledState = false;
  }

  ble_send_loop();
  load_cell_loop();

  if (mils > last_weight_time + 500 && deviceConnected)
  {
    last_weight_time = mils;
    notify_weight();
  }

  if (mils > last_recording_time + 5000){
    last_recording_time = mils;
    record_t record;
    record.time = rtc.getLocalEpoch();
    record.weight = load_cell_read();
    dataRW_write_data(record);
    dataRW_flush();
  }
}
