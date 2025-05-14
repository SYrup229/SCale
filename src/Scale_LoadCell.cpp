// set_scale seteaza manual calibrarea
// calibrate_scale are nevoie de o valoare dupa care sa se calibreze

#include "Scale_LoadCell.h"

HX711 scale;

void scale_setup(const float calibration_factor) {
  
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.tare();
  scale.set_scale(calibration_factor);
}

float scale_getWeight() {

  float weight = scale.get_units(10);
  return weight;
}
