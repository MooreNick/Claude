#pragma once           // prevents this header from being included more than once per translation unit

#include <SDL2/SDL.h>  // includes SDL2 for SDL_Renderer and SDL_Rect
#include "Constants.h" // includes game constants (LASER_SPEED, CANVAS_H, etc.)

// Laser represents a single projectile fired upward from the paddle
// when the laser powerup is active.  Two lasers are fired per shot.
class Laser {
public:
    float x;      // horizontal center of the laser beam in pixels
    float y;      // top edge of the laser beam in pixels (moves upward each frame)
    int   width;  // width of the laser beam in pixels
    int   height; // height of the laser beam in pixels
    float vy;     // vertical velocity (negative = moving upward)
    bool  active; // false once the laser hits a block or leaves the top of the screen

    // Constructs a laser at the given position, moving upward at LASER_SPEED.
    Laser(float x, float y);

    // Returns an SDL_Rect centered on x for collision detection against blocks.
    SDL_Rect rect() const;

    // Moves the laser upward each frame and deactivates it when it leaves the screen.
    void update();

    // Renders the laser as a bright red rectangle.
    void draw(SDL_Renderer* renderer) const;
};
