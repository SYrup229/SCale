#ifndef SCALE_LOADCELL_H
#define SCALE_LOADCELL_H

#include "HX711.h"

#define LOADCELL_DOUT_PIN 17  // HX711 data pin
#define LOADCELL_SCK_PIN  18  // HX711 clock pin

// Initialize & tare the HX711, optionally override default calibration factor
void scale_setup(const float calibration_factor = 391);

// Read the current weight in grams
float scale_getWeight();

#endif
