#include "ds18b20.h"


DS18B20::DS18B20( uint8_t pin )
    : oneWire( pin )
    , sensors( &oneWire )
{
    sensors.begin();
    sensors.setResolution( 9U );
}

float DS18B20::getTemperature( uint8_t index )
{
    sensors.requestTemperatures();
    return sensors.getTempCByIndex( index );
}

uint64_t DS18B20::getAddres( uint8_t index )
{
    uint64_t retval;
    if( sensors.getAddress( ( uint8_t* )&retval, index ) == false )
    {
        retval = 0;
    }
    return retval;
};
