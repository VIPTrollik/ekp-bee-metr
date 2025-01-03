#ifndef DATARW_H
#define DATARW_H

#include <Arduino.h>
#include <SPIFFS.h>

#define DATA_FILE "/data.bin"
#define CONFIG_FILE "/config.bin"

typedef struct {
  uint32_t time;
  float weight;
} record_t;

typedef struct {
  float weight_calibration_value;
  long weight_tare_offset;
} config_t;

static config_t default_config = {
  .weight_calibration_value = 1.0,
  .weight_tare_offset = 0
};

extern config_t current_config;

bool dataRW_init();

void dataRW_reset_mem();

void dataRW_write_data(record_t data);

uint32_t dataRW_read_first_date();

uint32_t dataRW_read_last_date();

int dataRW_read_data(record_t *data, uint32_t from, uint32_t to, uint32_t limit);

int dataRW_read_num_records();

void dataRW_flush();

bool dataRW_read_config(config_t *config);

void dataRW_write_config(config_t *config);


#endif // DATARW_H