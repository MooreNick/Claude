#include "Block.h"   // includes the Block class declaration

Block::Block(int x, int y, int hp, SDL_Color color) // constructs a block at a given position with hit points and color
    : x(x)                                          // stores left edge
    , y(y)                                          // stores top edge
    , width(BLOCK_W)                                // sets width from the global constant
    , height(BLOCK_H)                               // sets height from the global constant
    , hp(hp)                                        // stores current hit points (-1 = indestructible)
    , maxHp(hp)                                     // stores original hit points for the damage visual
    , color(color)                                  // stores the fill color
    , active(true)                                  // block starts as active (not yet destroyed)
    , scoreValue(hp == -1 ? 0 : hp * 10)            // indestructible blocks give no score; others give 10 pts per hp
{}

SDL_Rect Block::rect() const {          // returns the axis-aligned bounding rectangle for this block
    return SDL_Rect{x, y, width, height}; // constructs SDL_Rect directly from stored position and size
}

void Block::hit() {                     // called when a ball or laser makes contact with this block
    if (hp == -1) return;              // indestructible blocks cannot be damaged; bail out immediately
    hp--;                              // reduces hit points by one
    if (hp <= 0) {                     // if all hit points have been consumed
        active = false;                // marks block for removal from the game list
    }
}

void Block::draw(SDL_Renderer* renderer) const { // renders the block to the screen
    if (!active) return;                         // skip drawing if the block has already been destroyed

    // Compute brightness: full (1.0) when at full health, dimmer as hp decreases
    float brightness = (maxHp == -1) ? 1.0f                              // indestructible blocks always render at full brightness
                                     : static_cast<float>(hp) / maxHp;   // normal blocks dim proportionally to damage

    // Scale each color channel by brightness to create a damage visual effect
    Uint8 r = static_cast<Uint8>(color.r * brightness); // scaled red channel
    Uint8 g = static_cast<Uint8>(color.g * brightness); // scaled green channel
    Uint8 b = static_cast<Uint8>(color.b * brightness); // scaled blue channel

    SDL_Rect rc = rect();                                // gets the bounding rectangle for this block

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); // disables blending for the solid fill
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);           // sets the block fill color
    SDL_RenderFillRect(renderer, &rc);                        // fills the block rectangle with the color

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);        // dark gray for the block border
    SDL_RenderDrawRect(renderer, &rc);                        // draws the block outline for visual separation
}
