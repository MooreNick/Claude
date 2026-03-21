#pragma once               // prevents this header from being included more than once per translation unit

#include <SDL2/SDL.h>      // includes SDL2 for SDL_Renderer, SDL_Rect, and keyboard state
#include "Constants.h"     // includes game-wide constants (PADDLE_W, PADDLE_Y, etc.)

// Paddle represents the player-controlled bar at the bottom of the screen.
// It tracks powerup timers for the wide and laser effects and exposes
// helper methods to activate each powerup.
class Paddle {
public:
    float baseWidth;           // original paddle width; used to restore size when wide powerup expires
    float width;               // current paddle width in pixels (changes when wide powerup is active)
    int   height;              // paddle height in pixels (fixed)
    float x;                   // left edge of the paddle in pixels (float for smooth movement)
    int   y;                   // vertical position of the paddle (fixed, from PADDLE_Y constant)
    float speed;               // pixels the paddle moves per frame when a movement key is held
    float wideTimer;           // milliseconds remaining for the wide-paddle powerup
    bool  laserActive;         // true while the laser powerup is active
    float laserTimer;          // milliseconds remaining for the laser powerup duration
    float laserCooldownTimer;  // milliseconds until the player can fire the next laser shot

    // Constructs the paddle centered horizontally with default dimensions.
    Paddle();

    // Returns the horizontal center of the paddle (useful for ball angle calculations).
    float centerX() const;

    // Returns an SDL_Rect for the paddle's current bounding box (used for rendering and collision).
    SDL_Rect rect() const;

    // Moves the paddle based on held keyboard keys and ticks all powerup countdown timers.
    // dt is the elapsed time in milliseconds since the last frame.
    void update(float dt, const Uint8* keys);

    // Applies the wide-paddle powerup: expands width and starts the duration timer.
    void activateWide();

    // Applies the laser powerup: enables laser firing and starts the duration timer.
    void activateLaser();

    // Renders the paddle as a gradient-filled rectangle; turns orange when laser is active.
    void draw(SDL_Renderer* renderer) const;
};
