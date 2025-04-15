#pragma once
#include <cmath>
#include <cstdint>


class Point
{
public:
    Point( uint32_t _x, uint32_t _y )
        : x( _x )
        , y( _y )
    {
    }
    uint32_t x;
    uint32_t y;

    constexpr bool operator==( const Point& p ) const { return x == p.x && y == p.y; }
    constexpr bool operator!=( const Point& p ) const { return x != p.x || y != p.y; }

    constexpr double distanceTo( const Point& p ) const
    {
        const uint32_t dx = static_cast<uint32_t>( abs( x - p.x ) );
        const uint32_t dy = static_cast<uint32_t>( abs( y - p.y ) );
        return sqrt( dx * dx + dy * dy );
    }
};
