#pragma once               // prevents this header from being included more than once per translation unit

#include <SDL2/SDL.h>      // includes SDL2 core for window, renderer, event, and keyboard types
#include <SDL2/SDL_ttf.h>  // includes SDL_ttf for font loading and text rendering
#include <vector>          // includes std::vector for entity lists
#include <random>          // includes std::mt19937 for random number generation

#include "Constants.h"     // includes GameState, PowerupType, and all numeric constants
#include "Ball.h"          // includes the Ball entity class
#include "Paddle.h"        // includes the Paddle entity class
#include "Block.h"         // includes the Block entity class
#include "Powerup.h"       // includes the Powerup entity class
#include "Laser.h"         // includes the Laser entity class
#include "Particle.h"      // includes the Particle class for block-destruction effects

// Game is the central class that owns all SDL resources, entity lists, and game state.
// It implements a six-state state machine and runs the main game loop via run().
class Game {
public:
    // Initializes SDL2, SDL_ttf, creates the window/renderer, loads fonts,
    // reads the saved high score, and sets the initial state to Title.
    Game();

    // Shuts down SDL_ttf, SDL2, and closes the window on destruction.
    ~Game();

    // Starts the main game loop; returns when the player closes the window.
    void run();

private:
    // ── SDL resources ────────────────────────────────────────────────────────
    SDL_Window*   window;     // the OS window that holds the game canvas
    SDL_Renderer* renderer;   // the hardware-accelerated 2D renderer attached to the window
    TTF_Font*     fontLarge;  // large pixel font (28pt) for title and game-over text
    TTF_Font*     fontMed;    // medium font (18pt) for level/state transition messages
    TTF_Font*     fontSmall;  // small font (11pt) for HUD score/lives/level display
    TTF_Font*     fontTiny;   // tiny font (9pt) for powerup labels and hints

    // ── Persistent state ─────────────────────────────────────────────────────
    int           highScore;  // best score across all sessions, loaded from and saved to HS_FILE

    // ── Per-session game state ────────────────────────────────────────────────
    GameState     state;      // which screen the game is currently displaying
    int           level;      // current level number (increases on level complete)
    int           lives;      // player's remaining lives
    int           score;      // player's accumulated score for this session

    // ── Entity lists ─────────────────────────────────────────────────────────
    std::vector<Ball>     balls;     // all active balls (supports multi-ball powerup)
    std::vector<Block>    blocks;    // all blocks in the current level
    std::vector<Powerup>  powerups;  // all falling powerup pills
    std::vector<Laser>    lasers;    // all active laser projectiles
    std::vector<Particle> particles; // all active block-destruction particles
    Paddle                paddle;    // the single player paddle

    // ── Timers (all in milliseconds) ─────────────────────────────────────────
    float stateTimer;     // general timer used for auto-advancing life-lost and level-complete screens
    float shakeTimer;     // how many milliseconds of screen shake remain after a ball is lost
    float blinkTimer;     // cumulative timer used to drive blinking text on UI screens
    float titleAnimTimer; // cumulative timer driving the animated block grid on the title screen
    bool  slowActive;     // true while the slow-ball powerup effect is running
    float slowTimer;      // milliseconds remaining for the slow-ball effect

    // ── Random number generator ───────────────────────────────────────────────
    std::mt19937 rng;              // Mersenne-Twister RNG seeded at construction
    std::uniform_real_distribution<float> dist01; // uniform distribution over [0, 1) for general rolls

    // ── Private helpers: game management ──────────────────────────────────────
    void resetGameVars();           // resets all per-session variables to their starting values
    void startNewGame();            // resets vars, builds entities, and transitions to Playing
    void createBallOnPaddle();      // adds a new ball attached to the paddle at the correct starting position
    void resetAfterLifeLost();      // clears powerup state and creates a fresh ball, then resumes Playing
    void loadNextLevel();           // increments level, regenerates blocks, and resumes Playing

    // ── Private helpers: level generation ─────────────────────────────────────
    std::vector<Block> generateLevel(int lvl); // builds and returns a randomized block grid for the given level
    PowerupType        randomPowerupType();     // selects a powerup type using weighted probability

    // ── Private helpers: collision ─────────────────────────────────────────────
    void ballPaddleCollision(Ball& ball);                        // resolves ball bouncing off the paddle with angle control
    bool ballBlockCollision(Ball& ball, Block& block);           // AABB test + resolve; returns true if collision occurred
    bool laserBlockCollision(Laser& laser, Block& block);        // AABB test + resolve; returns true if collision occurred
    void collectPowerup(Powerup& pu);                            // applies a powerup effect and deactivates the pill

    // ── Private helpers: update ────────────────────────────────────────────────
    void update(float dt);         // dispatches to the correct per-state update function
    void updatePlaying(float dt);  // full gameplay update: movement, collisions, entity cleanup, win/loss checks
    void updateLifeLost(float dt); // waits for player input or 2.5 s timeout before resuming play
    void updateLevelComplete(float dt); // waits for player input or 2.5 s timeout before loading next level

    // ── Private helpers: render ────────────────────────────────────────────────
    void render();                                    // dispatches to the correct per-state render function
    void renderGame();                                // draws all live entities and the HUD; shared by several states
    void renderTitle();                               // animated title screen with decorative block grid
    void renderOverlay(Uint8 alpha);                  // draws a full-screen semi-transparent black rectangle
    void drawHUD();                                   // renders score, level number, and life-heart icons at the top
    void drawText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color, bool centered = false) const;
                                                      // renders a string using SDL_ttf and blits it at (x,y); centered=true aligns horizontally
    void drawBlink(const std::string& text, int y, TTF_Font* font, SDL_Color color, float periodMs); // renders text only during the "on" half of a blink cycle
    bool blinkOn(float periodMs) const;               // returns true when the blink timer is in the visible phase

    // ── Private helpers: persistence ───────────────────────────────────────────
    int  loadHighScore() const;        // reads the saved high score from HS_FILE; returns 0 on failure
    void saveHighScore(int score) const; // writes the given score to HS_FILE

    // ── Private helpers: events ────────────────────────────────────────────────
    void handleEvent(const SDL_Event& event); // processes a single SDL_Event (keyboard, quit, etc.)
};
