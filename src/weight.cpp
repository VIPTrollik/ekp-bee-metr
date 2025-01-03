#include "weight.h"

HX711_ADC LoadCell(HX711_dout, HX711_sck);
unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
boolean _tare = true;                 // set this to false if you don't want tare to be performed in the next step


bool load_cell_init(){
    LoadCell.begin();
    // LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
    LoadCell.start(stabilizingtime, false);
    if (LoadCell.getSignalTimeoutFlag())
    {
        Serial.println("Failed to init weight, check MCU>HX711 wiring and pin designations");
        return false;
    }
    else
    {
        config_t config;
        dataRW_read_config(&config);
        LoadCell.setCalFactor(config.weight_calibration_value);
        LoadCell.setTareOffset(config.weight_tare_offset);
        Serial.println("Weight startup is complete");
    }
    //while (!LoadCell.update())
    return true;
}

void load_cell_loop(){
    if (LoadCell.dataWaitingAsync()){
        LoadCell.updateAsync();
    }
    if (LoadCell.getTareStatus())
    {
        Serial.println("Tare complete");
        Serial.printf("Tare offsets: %d\n", LoadCell.getTareOffset());
        current_config.weight_tare_offset = LoadCell.getTareOffset();
        dataRW_write_config(&current_config);
    }
}

void load_cell_tare(){
    Serial.println("Tare started");
    LoadCell.tareNoDelay(); // async, continues in load_cell_loop
}

float load_cell_read(){
    return LoadCell.getData();
}

void load_cell_calibrate(float known_mass){
    Serial.printf("Calibrating for Known mass: %f\n", known_mass);
    Serial.printf("refreshed weight: %f\n", LoadCell.getData());
    float newCalibrationValue = LoadCell.getNewCalibration(known_mass);
    Serial.printf("New calibration value: %f\n", newCalibrationValue);
    default_config.weight_calibration_value = newCalibrationValue;
    dataRW_write_config(&default_config);
}

