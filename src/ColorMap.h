#pragma once
#include <Arduino.h>
#include <map>
#include <array>
#include <cstring>  // for strcmp

// Spectrum = 10-channel AS7341 mock reading
using Spectrum = std::array<uint16_t, 10>;

// Comparator for const char* keys
struct cmp_str {
    bool operator()(const char* a, const char* b) const {
        return strcmp(a, b) < 0;
    }
};

// Simulated color spectral database
static const std::map<const char*, Spectrum, cmp_str> colorSpectra = {
    { "red",     {180, 200, 210, 250, 240, 180, 100, 60, 40, 20} },
    { "blue",    {60,  80, 100, 150, 200, 240, 250, 255, 240, 230} },
    { "yellow",  {200, 240, 255, 250, 220, 160, 110, 60, 30, 10} },
    { "green",   {140, 180, 210, 255, 240, 220, 180, 140, 100, 60} },
    { "orange",  {210, 230, 255, 240, 200, 160, 110, 70, 40, 20} },
    { "purple",  {100, 120, 150, 180, 200, 230, 240, 200, 150, 110} },
    { "pink",    {190, 220, 240, 230, 200, 160, 120, 80, 60, 40} },
    { "brown",   {100, 120, 140, 150, 130, 110, 90, 60, 30, 20} },
    { "white",   {255, 255, 255, 255, 255, 255, 255, 255, 255, 255} },
    { "black",   {5,   5,   5,   5,   5,   5,   5,   5,   5,   5} },
    { "gray",    {100, 110, 120, 130, 140, 150, 140, 130, 120, 110} },
    { "cyan",    {80,  100, 150, 200, 240, 250, 200, 140, 90, 60} },
    { "magenta", {180, 200, 230, 240, 220, 180, 130, 90, 60, 30} },
    { "beige",   {220, 230, 240, 240, 230, 210, 180, 150, 120, 100} },
    { "lime",    {180, 220, 255, 240, 210, 160, 120, 80, 50, 20} },
    { "teal",    {60,  90, 120, 180, 200, 190, 160, 130, 100, 70} },
    { "maroon",  {80,  100, 120, 140, 160, 140, 100, 70, 40, 20} },
    { "navy",    {50,  70, 90,  130, 160, 200, 230, 240, 245, 240} },
    { "gold",    {230, 240, 255, 250, 240, 210, 170, 130, 90, 60} },
    { "silver",  {200, 210, 220, 230, 230, 220, 210, 200, 190, 180} }
};
