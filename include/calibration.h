#pragma once
#include "main.h"

#include <SD.h>
#include "ds18b20.h"


void calibration();

void updateDeviceCount();
void initializeSDCard();
void handleExistingCalibrationFile();
void openNewCalibrationFile();
void writeSensorAddresses();
void recordTemperatureData( float targetTemperature );
