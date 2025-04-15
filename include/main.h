#pragma once

// ----- Includes -----

#include <Arduino.h>


// ----- Defines -----

#define BUTTON_PIN      ( 1U )
#define DS18B20_PIN     ( 2U )
#define SD_CS           ( 10U )
#define calibrationFile ( "/calibration.csv" )

#ifndef waitForHighSignal
#    define waitForHighSignal( pin )                                                               \
        do                                                                                         \
        {                                                                                          \
            while( digitalRead( pin ) == LOW )                                                     \
            {                                                                                      \
                delay( 1U );                                                                       \
            }                                                                                      \
            delay( 10U );                                                                          \
        } while( false )
#endif

void init();
void waitForButtonPress( const String& message );
