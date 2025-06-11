#pragma once
#include <Arduino.h>
#include <DFRobot_AS7341.h>

/*  A single entry in your colour-reference table.
    Provide one row per colour you want to classify.          */
struct spectralColors {
  const char* name[3];
  uint8_t percent[3];
  uint16_t f[9];    // F1-F8, Clear
};

struct spectralTable {
  const char* name;
  uint16_t f[9];
};

/*  Eight-element vectors (F1‒F8) – NO Clear or NIR here.
    Values are guesstimates built from published channel bands
    F1 405-425 nm   F2 435-455 nm   F3 470-490 nm   F4 505-525 nm
    F5 545-565 nm   F6 580-600 nm   F7 620-640 nm   F8 670-690 nm */

extern const spectralTable colorTable[];
extern const size_t colorCount;


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

/*  Grab one full 9-channel spectrum. (-NIR)  
    On success ‘out[0]…out[9]’ is filled as:
    F1-F8 = visible bands, [8]=Clear                          */
bool captureSpectrum(DFRobot_AS7341 &sensor, spectralColors &out, colorSensorState &state);

/*  Normalize the spectrum against the Clear channel.
    This is done by dividing each F1-F8 value by Clear.       */
void normalizeSpectrum(spectralColors &spectrum);

/*  Map the normalized spectrum to a color name.
    The best match is stored in ‘spectrum.name’.              */
void colorMap(spectralColors &spectrum);

String formatTopColors(const spectralColors& s);