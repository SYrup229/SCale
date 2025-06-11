#include "Color_Sensor.h"
#include <algorithm>
#include <Arduino.h> 


const spectralTable colorTable[] = {
  { "Red",        {  2000,  4000,  6000,  8000, 12000, 25000, 65535, 50000, 0 } },
  { "Dark Red",   {  1000,  2000,  3000,  4000,  8000, 20000, 65535, 45000, 0 } },
  { "Orange",     {  2000,  4000,  8000, 12000, 30000, 65535, 48000, 10000, 0 } },
  { "Brown",      {  3000,  5000,  8000, 10000, 25000, 45000, 30000,  8000, 0 } },
  { "Yellow",     {  3000,  6000, 12000, 25000, 55000, 65535, 20000,  5000, 0 } },
  { "Lime",       {  4000,  8000, 16000, 40000, 65535, 35000, 10000,  3000, 0 } },
  { "Green",      {  3000,  5000, 10000, 25000, 65535, 30000,  5000,  2000, 0 } },
  { "Dark Green", {  2000,  4000,  7000, 20000, 50000, 25000,  4000,  1500, 0 } },
  { "Teal",       {  4000, 10000, 30000, 55000, 35000, 20000,  5000,  2000, 0 } },
  { "Cyan",       {  5000, 15000, 45000, 60000, 30000, 12000,  5000,  2000, 0 } },
  { "Blue",       { 10000, 65535, 40000,  8000,  4000,  2500,  1000,   800, 0 } },
  { "Blueberry",  {  8000, 60000, 45000, 10000,  5000,  3000,  2000,  1500, 0 } },
  { "Purple",     {  6000, 40000, 30000, 12000,  5000, 20000, 60000, 45000, 0 } },
  { "Magenta",    {  4000, 30000, 20000,  8000,  5000, 15000, 65535, 50000, 0 } },
  { "Pink",       {  3000, 12000,  8000,  4000,  8000, 20000, 50000, 30000, 0 } },
  { "Grape",      {  5000, 35000, 25000, 10000,  4000, 15000, 60000, 52000, 0 } },
  { "Olive",      {  2500,  4500,  7000, 22000, 48000, 30000, 10000,  2000, 0 } },
  { "Tan",        {  4000,  7000, 10000, 18000, 28000, 38000, 30000,  8000, 0 } },
  { "Turquoise",  {  6000, 18000, 50000, 60000, 35000, 15000,  4000,  1500, 0 } },
};

const size_t colorCount = sizeof(colorTable) / sizeof(colorTable[0]);

String formatTopColors(const spectralColors& s)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "%s (%u%%), %s (%u%%), %s (%u%%)",
             s.name[0], s.percent[0],
             s.name[1], s.percent[1],
             s.name[2], s.percent[2]);
    return String(buf);
}


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
  sensor.enableSpectralMeasure(true);
  return true;
}

/*  Capture a full spectrum from the AS7341 sensor.
    Returns true when the capture is complete, false otherwise. 
    The state machine tracks the capture process.              */
bool captureSpectrum(DFRobot_AS7341 &sensor, spectralColors &out, colorSensorState &state) {

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
void normalizeSpectrum(spectralColors &spectrum) {
  
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
void colorMap(spectralColors &spectrum) {

  uint8_t bestMatchIndex[3] = {0, 0, 0}; // Store indices of best matches
  uint32_t bestMatchDelta[3] = {UINT32_MAX, UINT32_MAX, UINT32_MAX}; // Store delta values for best matches

  for (size_t i = 0; i < colorCount; i++) {

    uint32_t deltaTotal = 0;

    for (uint8_t f = 0; f < 8; f++) {
      int32_t d = int32_t(spectrum.f[f]) - int32_t(colorTable[i].f[f]);
      deltaTotal += uint32_t(d < 0 ? -d : d);
    }

    if (deltaTotal < bestMatchDelta[2]) {
      bestMatchDelta[2] = deltaTotal;
      bestMatchIndex[2] = i;

      if (bestMatchDelta[2] < bestMatchDelta[1]) {
        // Shift down the best matches
        std::swap(bestMatchDelta[2], bestMatchDelta[1]);
        std::swap(bestMatchIndex[2], bestMatchIndex[1]);

        if (bestMatchDelta[1] < bestMatchDelta[0]) {
        // Shift down the first match
        std::swap(bestMatchDelta[1], bestMatchDelta[0]);
        std::swap(bestMatchIndex[1], bestMatchIndex[0]);
        }  
      }
    }
  }

  spectrum.name[0] = colorTable[bestMatchIndex[0]].name;
  spectrum.name[1] = colorTable[bestMatchIndex[1]].name;
  spectrum.name[2] = colorTable[bestMatchIndex[2]].name;

  float weights[3] = {0.0f, 0.0f, 0.0f};
  weights[0] = 1.0f / (bestMatchDelta[0] + 1.0f);
  weights[1] = 1.0f / (bestMatchDelta[1] + 1.0f);
  weights[2] = 1.0f / (bestMatchDelta[2] + 1.0f);

  float totalWeight = weights[0] + weights[1] + weights[2];

  float percentages[3] = {0.0f, 0.0f, 0.0f};
  percentages[0] = (weights[0] / totalWeight) * 100.0f;
  percentages[1] = (weights[1] / totalWeight) * 100.0f;
  percentages[2] = (weights[2] / totalWeight) * 100.0f;

  spectrum.percent[0] = uint8_t(percentages[0] + 0.5f);
  spectrum.percent[1] = uint8_t(percentages[1] + 0.5f);
  spectrum.percent[2] = uint8_t(percentages[2] + 0.5f);
}
