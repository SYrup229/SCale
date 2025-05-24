#include "Color_Sensor.h"

/* ---------- PUBLIC API ---------- */
bool initSpectralSensor(DFRobot_AS7341 &sensor,
                        uint8_t again,
                        uint8_t atime,
                        uint16_t astep,
                        uint8_t wtime)
{
  if (sensor.begin(DFRobot_AS7341::eSpm) != 0)
    return false;

  sensor.setAGAIN(again);
  sensor.setAtime(atime);
  sensor.setAstep(astep);
  sensor.setWtime(wtime);
  return true;
}

bool captureSpectrum(DFRobot_AS7341 &sensor, spectralColor &out, colorSensorState &state) {

  /* First SMUX group: F1-F4 + Clear + NIR */
  if (state == INACTIVE) {

    sensor.startMeasure(DFRobot_AS7341::eF1F4ClearNIR);
    state = ACTIVE1;
    return false;

  } else if (state == ACTIVE1) {

    if (sensor.measureComplete()) {
        auto g1 = sensor.readSpectralDataOne();
        out.f[0] = g1.ADF1;  out.f[1] = g1.ADF2;  out.f[2] = g1.ADF3;  out.f[3] = g1.ADF4;
        out.f[8] = g1.ADCLEAR;   // Clear (no-filter)
        out.f[9] = g1.ADNIR;     // Near-IR

        sensor.startMeasure(DFRobot_AS7341::eF5F8ClearNIR);
        state = ACTIVE2;
    }

    return false;
    
  } else if (state == ACTIVE2) {

    if (sensor.measureComplete()) {
        auto g2 = sensor.readSpectralDataTwo();
        out.f[4] = g2.ADF5;  out.f[5] = g2.ADF6;  out.f[6] = g2.ADF7;  out.f[7] = g2.ADF8;

        /* Average the duplicate Clear/NIR measurements for stability */
        out.f[8] = uint16_t((uint32_t(out.f[8]) + g2.ADCLEAR) / 2);
        out.f[9] = uint16_t((uint32_t(out.f[9]) + g2.ADNIR)   / 2);

        state = INACTIVE;  // Reset state for next capture
        return true;       // Spectrum capture complete
    }
  } else {

    Serial.println("Invalid state in captureSpectrum()");

    // Invalid state, reset to INACTIVE
    state = INACTIVE;
  }

  return false;  // No valid capture  
}
