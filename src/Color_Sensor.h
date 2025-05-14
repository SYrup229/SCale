#ifndef AS7341_COLOR_SENSOR_H
#define AS7341_COLOR_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

/* * Typical use:
 *   AS7341ColorSensor sensor;
 *   sensor.begin();          // initialise
 *   sensor.readChannels();   // grab fresh data
 *   String c = sensor.getColor();
 */

class AS7341ColorSensor {
public:
    bool    begin(TwoWire &bus = Wire, uint8_t addr = 0x39);
    void    readChannels();
    String  getColor() const;
    uint16_t getRawChannel(uint8_t index) const;   // 0-7:F1-F8, 8:CLEAR, 9:NIR

private:
    TwoWire  *_bus  = nullptr;
    uint8_t   _addr = 0x39;
    uint16_t  _channels[10] = {0};

    String classify() const;
};

#endif
