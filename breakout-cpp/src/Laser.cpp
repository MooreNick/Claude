#include "Laser.h"  // includes the Laser class declaration

Laser::Laser(float x, float y)  // constructs a laser beam at the given starting position
    : x(x)                      // stores horizontal center
    , y(y)                      // stores initial top edge position
    , width(3)                  // beam width: 3 pixels (thin line)
    , height(14)                // beam height: 14 pixels (short rectangle)
    , vy(-LASER_SPEED)          // negative because upward movement decreases the y coordinate
    , active(true)              // laser starts as active
{}

SDL_Rect Laser::rect() const {                            // returns the bounding rect for collision checks
    return SDL_Rect{                                      // constructs SDL_Rect from position and size
        static_cast<int>(x) - width / 2,                 // centers rect on x by subtracting half the width
        static_cast<int>(y),                             // top edge of beam
        width,                                           // beam width
        height                                           // beam height
    };
}

void Laser::update() {                                    // advances the laser upward each frame
    y += vy;                                             // moves y upward (vy is negative)
    if (y + static_cast<float>(height) < 0.f) {          // checks if the entire beam has exited the top of the screen
        active = false;                                  // deactivates so the Game removes it from the list
    }
}

void Laser::draw(SDL_Renderer* renderer) const {          // renders the laser beam as a bright red rectangle
    SDL_Rect rc = rect();                                 // gets the current bounding rectangle
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); // disables blending for a solid fill
    SDL_SetRenderDrawColor(renderer, 255, 68, 68, 255);   // sets bright red draw color
    SDL_RenderFillRect(renderer, &rc);                    // fills the laser rectangle
}
