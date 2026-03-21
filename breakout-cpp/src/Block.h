#pragma once           // prevents this header from being included more than once per translation unit

#include <SDL2/SDL.h>  // includes SDL2 for SDL_Renderer, SDL_Rect, and SDL_Color
#include "Constants.h" // includes game-wide constants (BLOCK_W, BLOCK_H, etc.)

// Block represents a single tile in the brick grid.
// hp == -1 means the block is indestructible and never deactivates.
// hp == 1 is a standard single-hit block; hp == 2 requires two hits to destroy.
class Block {
public:
    int       x;          // left edge of the block in pixels
    int       y;          // top edge of the block in pixels
    int       width;      // block width in pixels (always BLOCK_W)
    int       height;     // block height in pixels (always BLOCK_H)
    int       hp;         // current hit points (-1 = indestructible, 0 = destroyed, >0 = alive)
    int       maxHp;      // original hit points stored for the damage-dimming visual effect
    SDL_Color color;      // fill color of the block as an SDL_Color RGBA struct
    bool      active;     // false once hp reaches 0; marks the block for removal from the list
    int       scoreValue; // points awarded to the player when this block is destroyed

    // Constructs a block at the given grid pixel position with the given hit points and color.
    Block(int x, int y, int hp, SDL_Color color);

    // Returns an SDL_Rect for the block's current bounding box (used for collision and rendering).
    SDL_Rect rect() const;

    // Reduces hp by 1 (unless indestructible) and sets active = false when hp reaches 0.
    void hit();

    // Renders the block as a filled rectangle, dimmed proportionally to current damage.
    void draw(SDL_Renderer* renderer) const;
};
