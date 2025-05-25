#include "Color_Sensor.h"

extern const spectralColor colorTable[];  // Color reference table from ColorMap.h
extern const size_t colorCount;           // Number of colors in the reference table

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

/*  Capture a full spectrum from the AS7341 sensor.
    Returns true when the capture is complete, false otherwise. 
    The state machine tracks the capture process.              */
bool captureSpectrum(DFRobot_AS7341 &sensor, spectralColor &out, colorSensorState &state) {

  /* First SMUX group: F1-F4 + Clear*/
  if (state == INACTIVE) {

    sensor.startMeasure(DFRobot_AS7341::eF1F4ClearNIR);
    state = ACTIVE1;
    return false;

  } else if (state == ACTIVE1) {

    if (sensor.measureComplete()) {
        auto g1 = sensor.readSpectralDataOne();
        out.f[0] = g1.ADF1;  out.f[1] = g1.ADF2;  out.f[2] = g1.ADF3;  out.f[3] = g1.ADF4;
        out.f[8] = g1.ADCLEAR;   // Clear (no-filter)

        sensor.startMeasure(DFRobot_AS7341::eF5F8ClearNIR);
        state = ACTIVE2;
    }

    return false;
    
  } else if (state == ACTIVE2) {

    if (sensor.measureComplete()) {
        auto g2 = sensor.readSpectralDataTwo();
        out.f[4] = g2.ADF5;  out.f[5] = g2.ADF6;  out.f[6] = g2.ADF7;  out.f[7] = g2.ADF8;

        // Average the duplicate Clear for stability
        out.f[8] = uint16_t((uint32_t(out.f[8]) + g2.ADCLEAR) / 2);

        // Data processing
        normalizeSpectrum(out);
        colorMap(out);

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

/*  Normalize the spectrum against the Clear channel.
    This ensures that all F1-F8 values are relative to the Clear channel. 
    If Clear is zero, normalization is skipped to avoid division by zero.   */
void normalizeSpectrum(spectralColor &spectrum) {
  
  if (spectrum.f[8] == 0) {
    Serial.println("Error: Clear channel is zero, cannot normalize spectrum.");
    return;  // Avoid division by zero
  }

  uint32_t temp;

  for (uint8_t i = 0; i < 8; i++) {

    temp = uint32_t(spectrum.f[i]) * 65535u;
    spectrum.f[i] = uint16_t(temp / spectrum.f[8]);  // Normalize F1-F8 against Clear
  }
}

// Map the normalized spectrum to the closest color in the color reference table.
void colorMap(spectralColor &spectrum) {

  uint32_t deltaTotal = 0;
  uint32_t minDelta = UINT32_MAX;
  uint8_t bestMatchIndex = 0;

  for (int i = 0; i < colorCount; i++) {

    for (int i = 0; i < 8; i++) deltaTotal += abs(int(spectrum.f[i]) - int(colorTable[i].f[i]));

    if (deltaTotal < minDelta) {
      minDelta = deltaTotal;
      bestMatchIndex = i;
    }

    deltaTotal = 0;  // Reset for next color
  }

  spectrum.name = colorTable[bestMatchIndex].name;
}