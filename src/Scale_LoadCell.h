#ifndef SCALE_LOADCELL_H
#define SCALE_LOADCELL_H

#include "HX711.h"

#define LOADCELL_DOUT_PIN 32  // HX711 data pin
#define LOADCELL_SCK_PIN  33  // HX711 clock pin

// Initialize & tare the HX711, optionally override default calibration factor
void scale_setup(const float calibration_factor = -7050.0);

// Read the current weight in grams
float scale_getWeight();

#endif
