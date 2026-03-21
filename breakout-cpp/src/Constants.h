#pragma once             // prevents this header from being included more than once per translation unit

#include <SDL2/SDL.h>    // includes SDL2 core for SDL_Color definition

// ── Window / Canvas ───────────────────────────────────────────────────────────
constexpr int   CANVAS_W          = 480;    // game window width in pixels
constexpr int   CANVAS_H          = 640;    // game window height in pixels
constexpr int   TARGET_FPS        = 60;     // desired frames per second for the game loop
constexpr float MAX_DT            = 50.0f;  // maximum delta-time cap in milliseconds (prevents physics explosion on tab switch)

// ── Paddle ────────────────────────────────────────────────────────────────────
constexpr int   PADDLE_W          = 80;     // default paddle width in pixels
constexpr int   PADDLE_H          = 12;     // paddle height in pixels
constexpr int   PADDLE_Y          = 600;    // vertical position of the paddle from the top of the window
constexpr float PADDLE_SPEED      = 6.0f;   // pixels the paddle moves per frame when a movement key is held

// ── Ball ──────────────────────────────────────────────────────────────────────
constexpr int   BALL_RADIUS       = 7;      // radius of the ball in pixels
constexpr float BALL_BASE_SPEED   = 4.5f;   // ball speed in pixels per frame at level 1
constexpr float BALL_MAX_SPEED    = 9.0f;   // maximum ball speed regardless of level
constexpr float BALL_SPEED_INC    = 0.25f;  // how much ball speed increases per level

// ── Block Grid ────────────────────────────────────────────────────────────────
constexpr int   BLOCK_COLS        = 10;     // number of block columns in the grid
constexpr int   BLOCK_ROWS_BASE   = 5;      // number of block rows at level 1
constexpr int   BLOCK_ROWS_MAX    = 8;      // maximum number of block rows at high levels
constexpr int   BLOCK_W           = 40;     // width of each block in pixels
constexpr int   BLOCK_H           = 18;     // height of each block in pixels
constexpr int   BLOCK_PAD         = 4;      // gap between adjacent blocks in pixels
constexpr int   BLOCK_OFFSET_TOP  = 60;     // distance from window top to first block row
constexpr int   BLOCK_OFFSET_LEFT = 20;     // distance from window left to first block column
constexpr float BLOCK_SKIP_CHANCE = 0.15f;  // probability that a grid cell is left empty for visual variety

// ── Powerups ──────────────────────────────────────────────────────────────────
constexpr float POWERUP_CHANCE    = 0.18f;  // probability a destroyed block drops a powerup
constexpr float POWERUP_SPEED     = 2.0f;   // pixels per frame the powerup falls downward
constexpr float WIDE_DURATION_MS  = 8000.f; // milliseconds the wide-paddle powerup lasts
constexpr float SLOW_DURATION_MS  = 6000.f; // milliseconds the slow-ball powerup lasts
constexpr float LASER_DURATION_MS = 10000.f;// milliseconds the laser powerup lasts
constexpr float LASER_COOLDOWN_MS = 400.f;  // milliseconds between laser shots
constexpr float LASER_SPEED       = 12.0f;  // pixels per frame lasers travel upward
constexpr float WIDE_FACTOR       = 1.8f;   // paddle width multiplier when wide powerup is active
constexpr float SLOW_FACTOR       = 0.7f;   // ball speed multiplier when slow powerup is active

// ── Lives ─────────────────────────────────────────────────────────────────────
constexpr int   LIVES_START       = 3;      // number of lives the player starts with

// ── State machine transition timers ──────────────────────────────────────────
constexpr float AUTO_ADVANCE_MS   = 2500.f; // milliseconds before life-lost/level-complete screens auto-advance

// ── Screen shake ─────────────────────────────────────────────────────────────
constexpr float SHAKE_DURATION_MS = 400.f;  // milliseconds the screen shakes after losing a ball
constexpr int   SHAKE_MAGNITUDE   = 3;      // maximum pixel offset applied during screen shake

// ── Font path (Windows system font; change this path on Linux/macOS) ──────────
constexpr const char* FONT_PATH   = "C:/Windows/Fonts/courbd.ttf"; // Courier New Bold, always present on Windows
constexpr int   FONT_SIZE_LARGE   = 28;     // font size in points for title and game over text
constexpr int   FONT_SIZE_MED     = 18;     // font size for level/state transition messages
constexpr int   FONT_SIZE_SMALL   = 11;     // font size for HUD (score, lives, level)
constexpr int   FONT_SIZE_TINY    = 9;      // font size for powerup labels and hints

// ── High score file path ──────────────────────────────────────────────────────
constexpr const char* HS_FILE     = "breakout_hs.txt"; // plain-text file storing the high score as a single integer

// ── Colors (SDL_Color = {R, G, B, A}) ────────────────────────────────────────
constexpr SDL_Color COL_BG           = {10,  10,  26,  255}; // very dark blue background
constexpr SDL_Color COL_WHITE        = {255, 255, 255, 255}; // white for ball and general text
constexpr SDL_Color COL_HUD          = {224, 224, 224, 255}; // light gray for HUD text
constexpr SDL_Color COL_LIFE         = {244, 143, 177, 255}; // pink for life heart icons
constexpr SDL_Color COL_RED          = {239, 83,  80,  255}; // red for game-over and ball-lost messages
constexpr SDL_Color COL_GREEN        = {129, 199, 132, 255}; // green for level-complete messages
constexpr SDL_Color COL_GOLD         = {255, 183, 77,  255}; // gold for high score text
constexpr SDL_Color COL_HP2          = {239, 154, 154, 255}; // red-pink for two-hit blocks
constexpr SDL_Color COL_INDESTR      = {120, 144, 156, 255}; // gray for indestructible blocks

// normal single-hit block colors (cycled by grid position)
constexpr SDL_Color NORMAL_COLORS[5] = {
    {79,  195, 247, 255}, // light blue
    {129, 199, 132, 255}, // green
    {255, 183, 77,  255}, // amber
    {206, 147, 216, 255}, // purple
    {77,  208, 225, 255}, // cyan
};
constexpr int NORMAL_COLOR_COUNT = 5; // number of entries in the NORMAL_COLORS array

// ── Game state identifiers ────────────────────────────────────────────────────
enum class GameState {  // strongly-typed enum listing every possible screen the game can be in
    Title,              // animated title / main menu
    Playing,            // active gameplay
    LifeLost,           // brief pause after ball falls below the paddle
    LevelComplete,      // all destructible blocks cleared; advancing to next level
    GameOver,           // no lives remaining
    Paused,             // player pressed ESC
};

// ── Powerup type identifiers ──────────────────────────────────────────────────
enum class PowerupType { // strongly-typed enum for the five powerup varieties
    Wide,               // expands paddle width for a duration
    Slow,               // reduces ball speed for a duration
    Multiball,          // spawns two extra balls
    Life,               // grants one additional life
    Laser,              // enables the paddle to fire laser beams
};
