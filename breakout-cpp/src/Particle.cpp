#include "Particle.h"  // includes the Particle class declaration
#include <random>      // includes std::mt19937 and distribution types for random particle properties
#include <algorithm>   // includes std::max for clamping alpha to zero

// Module-level random number generator; seeded once at startup
static std::mt19937                          rng{std::random_device{}()}; // Mersenne-Twister seeded from hardware entropy
static std::uniform_real_distribution<float> distVel(-2.5f, 2.5f);       // random velocity range: ±2.5 pixels/frame
static std::uniform_real_distribution<float> distSize(2.0f, 6.0f);       // random particle size: 2–6 pixels
static std::uniform_real_distribution<float> distLife(300.0f, 500.0f);   // random lifetime: 300–500 milliseconds

Particle::Particle(float x, float y, SDL_Color color) // constructs a particle at the given position with the block's color
    : x(x)                                            // sets initial x position (block center)
    , y(y)                                            // sets initial y position (block center)
    , vx(distVel(rng))                                // picks a random horizontal velocity
    , vy(distVel(rng) - 2.0f)                         // picks a random velocity biased upward (subtract 2 for upward kick)
    , size(distSize(rng))                             // picks a random particle size
    , color(color)                                    // copies the block's color so particles match
    , alpha(255)                                      // starts at full opacity
    , life(distLife(rng))                             // picks a random lifetime
    , maxLife(life)                                   // records the original lifetime for alpha proportion calculation
    , active(true)                                    // particle starts as alive
{}

void Particle::update(float dt) {                        // advances particle physics each frame
    x  += vx;                                           // moves horizontally by velocity
    y  += vy;                                           // moves vertically by velocity
    vy += 0.1f;                                         // applies gravity: gradually increases downward velocity
    life -= dt;                                         // counts down the remaining lifetime
    alpha = std::max(0, static_cast<int>(255.f * life / maxLife)); // fades out proportionally to remaining lifetime
    if (life <= 0.f) {                                  // if lifetime has completely expired
        active = false;                                 // marks particle for removal from the list
    }
}

void Particle::draw(SDL_Renderer* renderer) const {      // renders the particle as a small colored square
    if (alpha <= 0) return;                              // skip invisible particles to save draw calls
    int s = static_cast<int>(size);                     // converts float size to int for the SDL_Rect
    SDL_Rect rc = {                                      // builds a square rect centered on the particle's position
        static_cast<int>(x) - s / 2,                   // left edge: center minus half the size
        static_cast<int>(y) - s / 2,                   // top edge: center minus half the size
        s,                                              // rect width
        s                                               // rect height
    };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);           // enables alpha blending for fade effect
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, static_cast<Uint8>(alpha)); // sets color with current opacity
    SDL_RenderFillRect(renderer, &rc);                                   // fills the particle square
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);            // restores blend mode to avoid affecting later draws
}
