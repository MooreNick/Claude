#pragma once                 // prevents this header from being included more than once per translation unit

#include <SDL2/SDL.h>        // includes SDL2 core for SDL_Renderer and rendering functions
#include "Constants.h"       // includes game-wide constants (BALL_RADIUS, etc.)

// Ball represents a single ball bouncing around the arena.
// Multiple Ball objects can exist simultaneously when the multi-ball powerup is active.
class Ball {
public:
    float x;         // horizontal center position of the ball in pixels (float for sub-pixel precision)
    float y;         // vertical center position of the ball in pixels
    float vx;        // horizontal velocity component in pixels per frame
    float vy;        // vertical velocity component in pixels per frame
    float speed;     // scalar speed used to renormalize velocity after deflections, preserving constant magnitude
    int   radius;    // collision and drawing radius in pixels
    bool  onPaddle;  // true while ball is glued to the paddle before the player launches it with Space
    bool  active;    // false means this ball has fallen off the bottom and should be removed from the list

    // Constructs a ball at the given position with the given velocity and speed scalar.
    Ball(float x, float y, float vx, float vy, float speed);

    // Moves the ball by (vx, vy) each frame; skips movement if onPaddle is true.
    void update();

    // Restores the velocity vector's magnitude to the stored speed scalar.
    // Called after every deflection to prevent drift from floating-point rounding.
    void normalizeSpeed();

    // Draws the ball as a filled white circle with a soft blue glow ring.
    void draw(SDL_Renderer* renderer) const;
};
