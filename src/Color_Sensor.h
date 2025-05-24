#pragma once
#include <Arduino.h>
#include <DFRobot_AS7341.h>

/*  A single entry in your colour-reference table.
    Provide one row per colour you want to classify.          */
struct spectralColor {
  const char *name;            // "Banana-Yellow", "Strawberry-Red", …
  uint16_t f[10];              // F1-F8, Clear, NIR (in that order)
};

enum colorSensorState {
    INACTIVE,
    ACTIVE1,
    ACTIVE2
};

/*  Initialise the AS7341 in spectral-pulse mode (eSpm).
    Returns true on success, false if the chip is missing.    */
bool initSpectralSensor(DFRobot_AS7341 &sensor,
                        uint8_t  again  = 4,      // 0-10  (×0.5 … ×512) gain
                        uint8_t  atime  = 100,    // integration time register - time per step
                        uint16_t astep  = 999,    // number of steps - steps per read 
                        uint8_t  wtime  = 0);     // wait time between reads

/*  Grab one full 10-channel spectrum.  
    On success ‘out[0]…out[9]’ is filled as:
    F1-F8 = visible bands, [8]=Clear, [9]=NIR.               */
bool captureSpectrum(DFRobot_AS7341 &sensor, spectralColor &out, colorSensorState &state);

