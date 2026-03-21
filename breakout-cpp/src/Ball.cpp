#include "Ball.h"       // includes the Ball class declaration
#include <cmath>        // includes std::hypot for computing vector magnitude
#include <algorithm>    // includes std::min / std::max (not directly used here but common companion)

// Constructs a Ball with the given position, velocity, and speed scalar.
Ball::Ball(float x, float y, float vx, float vy, float speed)
    : x(x)            // initializes horizontal center position
    , y(y)            // initializes vertical center position
    , vx(vx)          // initializes horizontal velocity component
    , vy(vy)          // initializes vertical velocity component
    , speed(speed)    // initializes the speed scalar used for renormalization
    , radius(BALL_RADIUS)  // sets radius from the global constant
    , onPaddle(true)  // new balls always start attached to the paddle
    , active(true)    // ball starts as active (not yet lost)
{}

void Ball::update() {         // advances the ball's position by its velocity
    if (onPaddle) return;     // skip movement while the ball is still glued to the paddle
    x += vx;                  // advance horizontal position by horizontal velocity
    y += vy;                  // advance vertical position by vertical velocity
}

void Ball::normalizeSpeed() {                       // restores velocity magnitude to the stored speed scalar
    float mag = std::hypot(vx, vy);                // computes the current speed as the length of the velocity vector
    if (mag == 0.0f) return;                       // guard: do nothing if velocity is zero (avoids division by zero)
    vx = (vx / mag) * speed;                      // scales horizontal component so the total magnitude equals speed
    vy = (vy / mag) * speed;                      // scales vertical component so the total magnitude equals speed
}

void Ball::draw(SDL_Renderer* renderer) const {     // renders the ball as a white circle with a glow ring
    // Draw glow ring: a slightly larger semi-transparent circle behind the ball
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // enables alpha blending for the glow
    SDL_SetRenderDrawColor(renderer, 160, 196, 255, 50);       // pale blue at low opacity for the glow color
    int gr = radius + 4;                                       // glow ring is 4 pixels larger than the ball radius
    for (int dy = -gr; dy <= gr; dy++) {                       // iterates each pixel row within the glow circle's bounding box
        for (int dx = -gr; dx <= gr; dx++) {                   // iterates each pixel column in the same bounding box
            if (dx * dx + dy * dy <= gr * gr) {                // checks if this pixel is inside the glow circle
                SDL_RenderDrawPoint(renderer,                   // draws a single pixel of the glow
                    static_cast<int>(x) + dx,                  // translates relative pixel to absolute screen x
                    static_cast<int>(y) + dy);                  // translates relative pixel to absolute screen y
            }
        }
    }

    // Draw the solid white ball
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);  // disables alpha blending for the solid ball
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);      // sets draw color to fully opaque white
    for (int dy = -radius; dy <= radius; dy++) {               // iterates each pixel row within the ball's bounding box
        for (int dx = -radius; dx <= radius; dx++) {           // iterates each pixel column in the same bounding box
            if (dx * dx + dy * dy <= radius * radius) {        // checks if this pixel is inside the ball circle
                SDL_RenderDrawPoint(renderer,                   // draws a single pixel of the ball
                    static_cast<int>(x) + dx,                  // absolute screen x coordinate
                    static_cast<int>(y) + dy);                  // absolute screen y coordinate
            }
        }
    }
}
