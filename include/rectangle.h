#pragma once
#include "point.h"


class Rectangle
{
public:
    Rectangle( const Point& _topLeft, uint32_t _width, uint32_t _height )
        : topLeft( _topLeft )
        , width( _width )
        , height( _height )
    {
    }
    Rectangle( uint32_t _x, uint32_t _y, uint32_t _width, uint32_t _height )
        : topLeft( _x, _y )
        , width( _width )
        , height( _height )
    {
    }

    bool contains( const Point& point ) const
    {
        return bool(
            ( topLeft.x <= point.x ) && ( point.x <= topLeft.x + width ) &&
            ( topLeft.y <= point.y ) && ( point.y <= topLeft.y + height )
        );
    }

    Point    topLeft;
    uint32_t width;
    uint32_t height;
};
