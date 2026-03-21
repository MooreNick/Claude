#include "Powerup.h"   // includes the Powerup class declaration

Powerup::Powerup(float x, float y, PowerupType type) // constructs a powerup at the given position
    : x(x)                                           // stores left edge position
    , y(y)                                           // stores top edge position
    , width(34)                                      // pill width in pixels
    , height(16)                                     // pill height in pixels
    , vy(POWERUP_SPEED)                              // falling speed from the global constant
    , type(type)                                     // stores which powerup type this is
    , active(true)                                   // starts as active (not yet collected or off-screen)
{}

SDL_Rect Powerup::rect() const {                     // returns the bounding rectangle for collision checks
    return SDL_Rect{                                 // constructs an SDL_Rect from current position and size
        static_cast<int>(x),                        // left edge cast to int for SDL
        static_cast<int>(y),                        // top edge cast to int for SDL
        width,                                      // pill width
        height                                      // pill height
    };
}

void Powerup::update() {                             // moves the powerup downward each frame
    y += vy;                                         // advances vertical position by the falling speed
    if (y > static_cast<float>(CANVAS_H) + 20.f) {  // checks if the pill has fallen below the visible screen
        active = false;                              // deactivates the powerup so the Game removes it
    }
}

// Returns the fill color and short label for each powerup type
static SDL_Color typeColor(PowerupType t) {                      // helper returning the pill's background color
    switch (t) {
        case PowerupType::Wide:      return {165, 214, 167, 255}; // green for wide paddle
        case PowerupType::Slow:      return {128, 222, 234, 255}; // cyan for slow ball
        case PowerupType::Multiball: return {255, 241, 118, 255}; // yellow for multi-ball
        case PowerupType::Life:      return {244, 143, 177, 255}; // pink for extra life
        case PowerupType::Laser:     return {255, 138, 101, 255}; // orange for laser
        default:                     return {255, 255, 255, 255}; // white fallback
    }
}

static const char* typeLabel(PowerupType t) {        // helper returning the short text label for the pill
    switch (t) {
        case PowerupType::Wide:      return "WID";   // three-character label for wide powerup
        case PowerupType::Slow:      return "SLW";   // three-character label for slow powerup
        case PowerupType::Multiball: return " x3";   // multi-ball label
        case PowerupType::Life:      return "+1";    // extra life label
        case PowerupType::Laser:     return "LZR";   // laser label
        default:                     return "?";     // fallback label
    }
}

void Powerup::draw(SDL_Renderer* renderer, TTF_Font* font) const { // renders the powerup pill to the screen
    SDL_Rect rc = rect();                                          // gets the current bounding rectangle

    SDL_Color fc = typeColor(type);                                // gets the fill color for this powerup type
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);      // disables blending for the solid fill
    SDL_SetRenderDrawColor(renderer, fc.r, fc.g, fc.b, 255);       // sets the pill fill color
    SDL_RenderFillRect(renderer, &rc);                             // fills the pill rectangle

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);               // black border color
    SDL_RenderDrawRect(renderer, &rc);                             // draws the pill outline

    if (font) {                                                    // only renders text if a valid font pointer was provided
        SDL_Color black = {0, 0, 0, 255};                          // black text for contrast against the colored pill
        SDL_Surface* surf = TTF_RenderText_Blended(font, typeLabel(type), black); // renders label to a surface
        if (surf) {                                                // proceeds only if surface creation succeeded
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf); // converts surface to a GPU texture
            if (tex) {                                             // proceeds only if texture creation succeeded
                int tw, th;                                        // will hold the texture's pixel dimensions
                SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th); // queries texture width and height
                SDL_Rect dst = {                                   // defines where to draw the label on screen
                    rc.x + (rc.w - tw) / 2,                       // centers label horizontally within the pill
                    rc.y + (rc.h - th) / 2,                       // centers label vertically within the pill
                    tw,                                            // label texture width
                    th                                             // label texture height
                };
                SDL_RenderCopy(renderer, tex, nullptr, &dst);      // draws the label texture onto the screen
                SDL_DestroyTexture(tex);                           // frees GPU texture memory
            }
            SDL_FreeSurface(surf);                                 // frees CPU surface memory
        }
    }
}
