#pragma once           // prevents this header from being included more than once per translation unit

#include <SDL2/SDL.h>  // includes SDL2 for SDL_Renderer, SDL_Color, and drawing functions
#include "Constants.h" // includes game constants

// Particle represents a single small square that bursts out of a destroyed block.
// Particles fly in a random direction, slow due to gravity, and fade out over time.
class Particle {
public:
    float     x;       // current horizontal position of the particle center
    float     y;       // current vertical position of the particle center
    float     vx;      // horizontal velocity in pixels per frame
    float     vy;      // vertical velocity in pixels per frame (biased upward initially)
    float     size;    // side length of the square in pixels (random between 2 and 6)
    SDL_Color color;   // particle color matching the destroyed block
    int       alpha;   // current opacity 0–255; decreases as the particle ages
    float     life;    // milliseconds of lifetime remaining
    float     maxLife; // original lifetime stored for proportional alpha calculation
    bool      active;  // false when lifetime expires; marks the particle for removal

    // Constructs a particle at the given position with random velocity, size, and lifetime.
    Particle(float x, float y, SDL_Color color);

    // Advances the particle's position, applies gravity, and fades its alpha.
    // dt is the elapsed time in milliseconds since the last frame.
    void update(float dt);

    // Renders the particle as a small filled square at its current opacity.
    void draw(SDL_Renderer* renderer) const;
};
