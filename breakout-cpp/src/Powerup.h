#pragma once                  // prevents this header from being included more than once per translation unit

#include <SDL2/SDL.h>         // includes SDL2 for SDL_Renderer, SDL_Rect, and drawing functions
#include <SDL2/SDL_ttf.h>     // includes SDL_ttf for TTF_Font used to render the powerup label
#include "Constants.h"        // includes game constants and the PowerupType enum

// Powerup represents a collectible pill that falls from a destroyed block.
// When the player catches it with the paddle, its effect is applied by the Game class.
class Powerup {
public:
    float       x;       // left edge of the powerup pill in pixels (float for smooth falling)
    float       y;       // top edge of the powerup pill in pixels
    int         width;   // width of the pill in pixels
    int         height;  // height of the pill in pixels
    float       vy;      // downward falling speed in pixels per frame
    PowerupType type;    // which powerup this pill grants (Wide, Slow, Multiball, Life, Laser)
    bool        active;  // false once the powerup is collected by the paddle or falls off the bottom

    // Constructs a powerup at the given position with the given type.
    Powerup(float x, float y, PowerupType type);

    // Returns an SDL_Rect for collision detection against the paddle.
    SDL_Rect rect() const;

    // Moves the powerup downward each frame and deactivates it if it leaves the screen.
    void update();

    // Renders the powerup as a colored rounded pill with a short text label.
    void draw(SDL_Renderer* renderer, TTF_Font* font) const;
};
