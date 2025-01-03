#include "dataRW.h"

bool spiffs_init = false;
File file;
config_t current_config = default_config;

void print_files(){
  Serial.println("Files found on SPIFFS:");
  File root = SPIFFS.open("/");

  File file = root.openNextFile();

  while(file){

      Serial.print("FILE: ");
      Serial.println(file.name());
      Serial.printf("SIZE: %d\n", file.size());
      file = root.openNextFile();
  }
}

bool dataRW_init() {
  if (!spiffs_init) {
    if (!SPIFFS.begin(true)) {
      Serial.println("SPIFFS Mount Failed");
      return false;
    }
    print_files();
    spiffs_init = true;
  }
  if (!SPIFFS.exists(DATA_FILE)) {
    file = SPIFFS.open(DATA_FILE, "w+");
    if (!file) {
      Serial.println("Failed to create file for writing");
      return false;
    }
  }
  else{
    file = SPIFFS.open(DATA_FILE, "r+");

    if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }
  }
  return true;
}

void dataRW_reset_mem() {
  file.close();
  if (SPIFFS.exists(DATA_FILE)) {
    SPIFFS.remove(DATA_FILE);
  }
  dataRW_init();
}

void dataRW_write_data(record_t data) {
  // check if new date is greater than last date
  if (file.size() > 0) {
    file.seek(0, SeekEnd);
    record_t last_data;
    file.seek(file.position() - sizeof(record_t));
    file.read((uint8_t*)&last_data, sizeof(record_t));
    if (data.time < last_data.time) {
      Serial.println("New date is less than last date");
      return;
    }
  }

  // append data to file
  file.seek(0, SeekEnd);
  int written = file.write((uint8_t*)&data, sizeof(record_t));
}

uint32_t dataRW_read_first_date() {
  record_t data;
  if (file.size() == 0) {
    return 0;
  }
  file.seek(0, SeekSet);
  file.read((uint8_t*)&data, sizeof(record_t));
  return data.time;
}

uint32_t dataRW_read_last_date() {
  record_t data;
  if (file.size() == 0) {
    return 0;
  }
  file.seek(0, SeekEnd);
  file.seek(file.position() - sizeof(record_t));
  file.read((uint8_t*)&data, sizeof(record_t));
  return data.time;
}

int dataRW_read_data(record_t *data, uint32_t from, uint32_t to, uint32_t limit) {
  uint32_t num_records = dataRW_read_num_records();
  if (num_records == 0) {
    return 0;
  }
  if (from > to) {
    return -1;
  }
  if (from > dataRW_read_last_date()) {
    return -2;
  }
  if (to < dataRW_read_first_date()) {
    return -3;
  }
  if (limit > num_records) {
    limit = num_records;
  }
  file.flush();
  file.seek(0, SeekSet);
  uint32_t num_read = 0;
  record_t temp;
  while (file.available() >= sizeof(record_t)) {
    unsigned int readX = file.readBytes((char*)&temp, sizeof(record_t));
    if (readX != sizeof(record_t)) {
      break;
    }
    if (temp.time >= to) {
      break;
    }
    if (temp.time >= from) {
      data[num_read] = temp;
      num_read++;
      if (num_read >= limit) {
        break;
      }
    }
  }
  return num_read;
}

int dataRW_read_num_records() {
  return file.size() / sizeof(record_t);
}

void dataRW_flush() {
  file.flush();
}

void print_config(config_t *config) {
  Serial.println("Current config:");
  Serial.printf("Weight calibration value: %f\n", config->weight_calibration_value);
  Serial.printf("Weight tare offset: %ld\n", config->weight_tare_offset);
}

bool dataRW_read_config(config_t *config) {
  File cfile = SPIFFS.open(CONFIG_FILE, "r");
  if (!cfile) {
    Serial.println("Failed to open config file for reading");
    *config = default_config;
    return false;
  }
  cfile.read((uint8_t*)config, sizeof(config_t));
  memcpy(&current_config, config, sizeof(config_t));
  cfile.close();
  print_config(config);
  return true;
}

void dataRW_write_config(config_t *config) {
  File cfile = SPIFFS.open(CONFIG_FILE, "w");
  if (!cfile) {
    Serial.println("Failed to open config file for writing");
    return;
  }
  cfile.write((uint8_t*)config, sizeof(config_t));
  cfile.close();
}