#pragma once                   // prevents this header from being included more than once per translation unit

#include <SDL2/SDL.h>          // SDL2 core: window, renderer, events, keyboard, rects
#include <SDL2/SDL_ttf.h>      // SDL_ttf: TrueType font loading and text rendering
#include <vector>              // std::vector for entity lists
#include <string>              // std::string for text rendering helper
#include <random>              // std::mt19937 and distributions for RNG

// ─── CONSTANTS ───────────────────────────────────────────────────────────────

constexpr int   CANVAS_W          = 480;     // game window width in pixels
constexpr int   CANVAS_H          = 640;     // game window height in pixels
constexpr float MAX_DT            = 50.0f;   // delta-time cap in ms (prevents physics explosion on tab switch)

constexpr int   PADDLE_W          = 80;      // default paddle width in pixels
constexpr int   PADDLE_H          = 12;      // paddle height in pixels
constexpr int   PADDLE_Y          = 600;     // vertical position of the paddle from the top
constexpr float PADDLE_SPEED      = 6.0f;    // pixels the paddle moves per frame when a key is held

constexpr int   BALL_RADIUS       = 7;       // ball radius in pixels
constexpr float BALL_BASE_SPEED   = 4.5f;    // ball speed at level 1 in pixels per frame
constexpr float BALL_MAX_SPEED    = 9.0f;    // maximum ball speed regardless of level
constexpr float BALL_SPEED_INC    = 0.25f;   // speed added per level

constexpr int   BLOCK_COLS        = 10;      // number of block columns
constexpr int   BLOCK_ROWS_BASE   = 5;       // block rows at level 1
constexpr int   BLOCK_ROWS_MAX    = 8;       // maximum block rows at high levels
constexpr int   BLOCK_W           = 40;      // block width in pixels
constexpr int   BLOCK_H           = 18;      // block height in pixels
constexpr int   BLOCK_PAD         = 4;       // gap between blocks in pixels
constexpr int   BLOCK_OFFSET_TOP  = 60;      // distance from top of canvas to first block row
constexpr int   BLOCK_OFFSET_LEFT = 20;      // distance from left of canvas to first block column
constexpr float BLOCK_SKIP_CHANCE = 0.15f;   // probability a grid cell is left empty for visual variety

constexpr float POWERUP_CHANCE    = 0.18f;   // probability a destroyed block drops a powerup
constexpr float POWERUP_SPEED     = 2.0f;    // pixels per frame the powerup falls
constexpr float WIDE_DURATION_MS  = 8000.f;  // milliseconds the wide-paddle powerup lasts
constexpr float SLOW_DURATION_MS  = 6000.f;  // milliseconds the slow-ball powerup lasts
constexpr float LASER_DURATION_MS = 10000.f; // milliseconds the laser powerup lasts
constexpr float LASER_COOLDOWN_MS = 400.f;   // milliseconds between laser shots
constexpr float LASER_SPEED       = 12.0f;   // pixels per frame lasers travel upward
constexpr float WIDE_FACTOR       = 1.8f;    // paddle width multiplier for wide powerup
constexpr float SLOW_FACTOR       = 0.7f;    // ball speed multiplier for slow powerup

constexpr int   LIVES_START       = 3;       // lives the player starts with
constexpr float AUTO_ADVANCE_MS   = 2500.f;  // ms before life-lost/level-complete screens auto-advance
constexpr float SHAKE_DURATION_MS = 400.f;   // ms the screen shakes after losing a ball
constexpr int   SHAKE_MAGNITUDE   = 3;       // max pixel offset during screen shake

constexpr const char* FONT_PATH   = "C:/Windows/Fonts/courbd.ttf"; // system font (change on Linux/macOS)
constexpr int   FONT_SIZE_LARGE   = 28;      // large font size for title and game-over text
constexpr int   FONT_SIZE_MED     = 18;      // medium font size for state messages
constexpr int   FONT_SIZE_SMALL   = 11;      // small font size for HUD
constexpr int   FONT_SIZE_TINY    = 9;       // tiny font size for powerup labels and hints

constexpr const char* HS_FILE     = "breakout_hs.txt"; // plain-text file storing the high score

// colors (SDL_Color = {R, G, B, A})
constexpr SDL_Color COL_BG      = {10,  10,  26,  255}; // very dark blue background
constexpr SDL_Color COL_WHITE   = {255, 255, 255, 255}; // white
constexpr SDL_Color COL_HUD     = {224, 224, 224, 255}; // light gray HUD text
constexpr SDL_Color COL_LIFE    = {244, 143, 177, 255}; // pink for life hearts
constexpr SDL_Color COL_RED     = {239, 83,  80,  255}; // red for game-over text
constexpr SDL_Color COL_GREEN   = {129, 199, 132, 255}; // green for level-complete text
constexpr SDL_Color COL_GOLD    = {255, 183, 77,  255}; // gold for high score text
constexpr SDL_Color COL_HP2     = {239, 154, 154, 255}; // red-pink for two-hit blocks
constexpr SDL_Color COL_INDESTR = {120, 144, 156, 255}; // gray for indestructible blocks

// color palette for normal single-hit blocks
constexpr SDL_Color NORMAL_COLORS[5] = {
    {79,  195, 247, 255}, // light blue
    {129, 199, 132, 255}, // green
    {255, 183, 77,  255}, // amber
    {206, 147, 216, 255}, // purple
    {77,  208, 225, 255}, // cyan
};
constexpr int NORMAL_COLOR_COUNT = 5; // number of entries in the NORMAL_COLORS array

// ─── ENUMS ────────────────────────────────────────────────────────────────────

enum class GameState {   // every screen the game can be in
    Title,               // animated title / main menu
    Playing,             // active gameplay
    LifeLost,            // brief pause after ball falls below the paddle
    LevelComplete,       // all destructible blocks cleared
    GameOver,            // no lives remaining
    Paused,              // player pressed ESC
};

enum class PowerupType { // the five collectible powerup varieties
    Wide,                // expands paddle width for a duration
    Slow,                // reduces ball speed for a duration
    Multiball,           // spawns two extra balls
    Life,                // grants one additional life
    Laser,               // enables the paddle to fire laser beams
};

// ─── ENTITY CLASSES ───────────────────────────────────────────────────────────

// Ball: a bouncing ball; multiple can exist simultaneously (multi-ball powerup)
class Ball {
public:
    float x, y;       // center position in pixels
    float vx, vy;     // velocity components in pixels per frame
    float speed;      // scalar speed for renormalization after deflections
    int   radius;     // collision and drawing radius
    bool  onPaddle;   // true while glued to the paddle before launch
    bool  active;     // false once the ball falls off the bottom edge

    Ball(float x, float y, float vx, float vy, float speed); // constructs ball at given position/velocity
    void update();            // moves ball by velocity each frame; skips if onPaddle
    void normalizeSpeed();    // restores velocity magnitude to the speed scalar
    void draw(SDL_Renderer*) const; // renders white circle with soft glow ring
};

// Paddle: the player-controlled bar at the bottom
class Paddle {
public:
    float baseWidth, width; // original and current width (wide powerup changes width)
    int   height;           // fixed height in pixels
    float x;                // left edge position (float for smooth movement)
    int   y;                // fixed vertical position
    float speed;            // pixels per frame when a movement key is held
    float wideTimer;        // ms remaining for wide powerup
    bool  laserActive;      // true while laser powerup is active
    float laserTimer;       // ms remaining for laser powerup
    float laserCooldownTimer; // ms until next laser shot is allowed

    Paddle();                              // centers paddle with default dimensions
    float    centerX() const;             // returns horizontal center of paddle
    SDL_Rect rect()    const;             // returns bounding rect for collision and rendering
    void update(float dt, const Uint8* keys); // moves paddle, ticks powerup timers
    void activateWide();                  // expands width to WIDE_FACTOR and starts timer
    void activateLaser();                 // enables laser firing and starts timer
    void draw(SDL_Renderer*) const;       // renders gradient rectangle (orange when laser active)
};

// Block: a single tile in the brick grid
class Block {
public:
    int       x, y, width, height; // position and dimensions in pixels
    int       hp, maxHp;           // current and original hit points (-1 = indestructible)
    SDL_Color color;               // fill color
    bool      active;              // false once hp reaches 0
    int       scoreValue;          // points awarded when destroyed

    Block(int x, int y, int hp, SDL_Color color); // constructs block at grid position
    SDL_Rect rect() const;    // returns bounding rect
    void hit();               // reduces hp by 1; deactivates when hp reaches 0
    void draw(SDL_Renderer*) const; // renders block dimmed proportionally to damage
};

// Powerup: a collectible pill that falls from a destroyed block
class Powerup {
public:
    float       x, y;    // top-left position in pixels (float for smooth falling)
    int         width, height; // pill dimensions in pixels
    float       vy;      // falling speed in pixels per frame
    PowerupType type;    // which effect this pill grants
    bool        active;  // false once collected or fallen off screen

    Powerup(float x, float y, PowerupType type); // constructs pill at given position
    SDL_Rect rect() const;                        // returns bounding rect for collision
    void update();                                // moves downward; deactivates if off screen
    void draw(SDL_Renderer*, TTF_Font*) const;    // renders colored pill with short label
};

// Laser: an upward-moving projectile fired from the paddle
class Laser {
public:
    float x, y;     // horizontal center and top edge in pixels
    int   width, height; // beam dimensions in pixels
    float vy;       // upward velocity (negative value)
    bool  active;   // false once it hits a block or leaves the screen

    Laser(float x, float y);           // constructs laser moving upward from given position
    SDL_Rect rect() const;             // returns bounding rect centered on x
    void update();                     // moves upward; deactivates when off screen
    void draw(SDL_Renderer*) const;    // renders bright red rectangle
};

// Particle: a small square that flies out of a destroyed block and fades away
class Particle {
public:
    float     x, y;      // current position in pixels
    float     vx, vy;    // velocity (vy biased upward initially, then gravity applied)
    float     size;      // side length in pixels (random 2–6)
    SDL_Color color;     // inherits the destroyed block's color
    int       alpha;     // current opacity 0–255; decreases as lifetime runs out
    float     life, maxLife; // remaining and original lifetime in milliseconds
    bool      active;    // false when lifetime expires

    Particle(float x, float y, SDL_Color color); // random velocity, size, and lifetime
    void update(float dt);          // advances physics (gravity), decrements lifetime, fades alpha
    void draw(SDL_Renderer*) const; // renders small colored square at current opacity
};

// ─── GAME CLASS ───────────────────────────────────────────────────────────────

// Game owns all SDL resources, entity lists, and game state.
// It implements a six-state state machine and drives the main loop via run().
class Game {
public:
    Game();   // initializes SDL2/SDL_ttf, creates window and renderer, loads fonts and high score
    ~Game();  // releases all SDL resources and shuts down subsystems
    void run(); // starts the game loop; returns when the window is closed

private:
    // SDL resources
    SDL_Window*   window;    // the OS window
    SDL_Renderer* renderer;  // hardware-accelerated 2D renderer
    TTF_Font*     fontLarge; // 28pt font for title and game-over text
    TTF_Font*     fontMed;   // 18pt font for level/state messages
    TTF_Font*     fontSmall; // 11pt font for HUD
    TTF_Font*     fontTiny;  // 9pt font for powerup labels and hints

    // Persistent state
    int highScore; // best score across sessions, saved to and loaded from HS_FILE

    // Per-session state
    GameState state;  // current screen
    int level, lives, score; // gameplay counters

    // Entity lists
    std::vector<Ball>     balls;     // all active balls
    std::vector<Block>    blocks;    // all blocks in current level
    std::vector<Powerup>  powerups;  // all falling powerup pills
    std::vector<Laser>    lasers;    // all active laser projectiles
    std::vector<Particle> particles; // all active block-destruction particles
    Paddle                paddle;    // the player paddle

    // Timers (all in milliseconds)
    float stateTimer;      // drives auto-advance for life-lost and level-complete screens
    float shakeTimer;      // remaining duration of screen-shake effect
    float blinkTimer;      // cumulative timer for blinking text
    float titleAnimTimer;  // cumulative timer for title screen block animation
    bool  slowActive;      // true while slow-ball powerup effect is running
    float slowTimer;       // ms remaining for slow-ball effect

    // Random number generation
    std::mt19937 rng;               // Mersenne-Twister seeded at construction
    std::uniform_real_distribution<float> dist01; // uniform [0,1) distribution

    // Game management
    void resetGameVars();        // resets all per-session variables to starting values
    void startNewGame();         // resets vars, builds entities, transitions to Playing
    void createBallOnPaddle();   // adds a new ball glued to the paddle (speed scales with level)
    void resetAfterLifeLost();   // clears powerup state, creates fresh ball, resumes Playing
    void loadNextLevel();        // increments level, regenerates blocks, resumes Playing

    // Level generation
    std::vector<Block> generateLevel(int lvl); // builds randomized block grid for given level
    PowerupType        randomPowerupType();     // weighted random powerup type selection

    // Collision
    void ballPaddleCollision(Ball& ball);              // angle-based bounce off paddle
    bool ballBlockCollision(Ball& ball, Block& block); // AABB + resolve; returns true if hit
    bool laserBlockCollision(Laser& laser, Block& block); // AABB + resolve; returns true if hit
    void collectPowerup(Powerup& pu);                  // applies effect and deactivates pill

    // Update (per state)
    void update(float dt);              // dispatches to correct per-state update function
    void updatePlaying(float dt);       // full gameplay: movement, collisions, cleanup, win/loss
    void updateLifeLost(float dt);      // waits for input or 2.5s timeout
    void updateLevelComplete(float dt); // waits for input or 2.5s timeout

    // Render (per state)
    void render();             // dispatches to correct per-state render function
    void renderGame();         // draws all live entities and HUD with optional screen shake
    void renderTitle();        // animated title screen with wave-effect block grid
    void renderOverlay(Uint8 alpha); // full-screen semi-transparent black rectangle

    // Render helpers
    void drawHUD(); // renders score (left), level (center), hearts (right), best score
    void drawText(const std::string& text, int x, int y, TTF_Font*, SDL_Color, bool centered = false) const;
    void drawBlink(const std::string& text, int y, TTF_Font*, SDL_Color, float periodMs); // text visible only during blink-on phase
    bool blinkOn(float periodMs) const; // true during the visible half of each blink period

    // Persistence
    int  loadHighScore() const;         // reads saved high score from HS_FILE; returns 0 on failure
    void saveHighScore(int score) const; // writes score to HS_FILE

    // Events
    void handleEvent(const SDL_Event& event); // processes keyboard input and quit events
};
