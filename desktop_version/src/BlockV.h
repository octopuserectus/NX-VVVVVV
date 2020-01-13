#pragma once

#if defined(__SWITCH__) || defined(__WIIU__)
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif
#include <string>

class blockclass
{
public:
    blockclass();

    void clear();

    void rectset(const int xi, const int yi, const int wi, const int hi);
public:
    //Fundamentals
    bool active;
    SDL_Rect rect;
    int type;
    int trigger;
    int xp, yp, wp, hp;
    std::string script, prompt;
    int r, g, b;

    //These would come from the sprite in the flash
    float x;
    float y;

};
