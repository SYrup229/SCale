#pragma once
#include <Arduino.h>
#include <DFRobot_AS7341.h>

/*  A single entry in your colour-reference table.
    Provide one row per colour you want to classify.          */
struct spectralColor {
  const char *name;            // "Banana-Yellow", "Strawberry-Red", …
  uint16_t f[9];               // F1-F8, Clear, ignoring NIR
};

/*  Eight-element vectors (F1‒F8) – NO Clear or NIR here.
    Values are guesstimates built from published channel bands
    F1 405-425 nm   F2 435-455 nm   F3 470-490 nm   F4 505-525 nm
    F5 545-565 nm   F6 580-600 nm   F7 620-640 nm   F8 670-690 nm */

extern const spectralColor colorTable[] = {
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

extern const size_t colorCount = sizeof(colorTable) / sizeof(colorTable[0]);


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

/*  Normalize the spectrum against the Clear channel.
    This is done by dividing each F1-F8 value by Clear.       */
void normalizeSpectrum(spectralColor &spectrum);

/*  Map the normalized spectrum to a color name.
    The best match is stored in ‘spectrum.name’.              */
void colorMap(spectralColor &spectrum);