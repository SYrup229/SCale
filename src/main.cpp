#include "main.h"

void init()
{
    pinMode( BUTTON_PIN, INPUT_PULLDOWN );
    Serial.begin( 115200U );
}

void waitForButtonPress( const String& message )
{
    Serial.println( message );
    Serial.println( "Press the button to continue" );
    waitForHighSignal( BUTTON_PIN );
}
