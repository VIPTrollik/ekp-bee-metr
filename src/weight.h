#ifndef WEIGHT_H
#define WEIGHT_H

#include <HX711_ADC.h>
#include "dataRW.h"

const int HX711_dout = 22; // mcu > HX711 dout pin
const int HX711_sck = 21;  // mcu > HX711 sck pin

bool load_cell_init();

void load_cell_loop();

float load_cell_read();

void load_cell_tare();

void load_cell_calibrate(float calibration_factor);

#endif // WEIGHT_H