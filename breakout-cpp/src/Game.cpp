#include "Game.h"       // includes the Game class declaration and all entity headers

#include <SDL2/SDL.h>   // SDL2 core: window, renderer, events, keyboard
#include <SDL2/SDL_ttf.h> // SDL_ttf: font loading and text surface rendering

#include <algorithm>    // std::remove_if, std::any_of, std::min
#include <cmath>        // std::sin for title animation
#include <fstream>      // std::ifstream / std::ofstream for high score file I/O
#include <string>       // std::string and std::to_string
#include <stdexcept>    // std::runtime_error for fatal initialization errors
#include <cstdlib>      // std::rand, std::srand (used only for SHAKE_MAGNITUDE random offset)
#include <ctime>        // std::time for seeding std::srand

// ─── Constructor / Destructor ─────────────────────────────────────────────────

Game::Game()                                // constructs the entire game: SDL, window, renderer, fonts, initial state
    : window(nullptr)                       // window handle starts null; set below
    , renderer(nullptr)                     // renderer handle starts null; set below
    , fontLarge(nullptr)                    // large font starts null; set below
    , fontMed(nullptr)                      // medium font starts null; set below
    , fontSmall(nullptr)                    // small font starts null; set below
    , fontTiny(nullptr)                     // tiny font starts null; set below
    , highScore(0)                          // high score starts at 0; overwritten by loadHighScore()
    , state(GameState::Title)               // game begins on the title screen
    , level(1)                              // level counter starts at 1
    , lives(LIVES_START)                    // lives counter starts at the configured value
    , score(0)                              // score starts at zero
    , paddle()                              // constructs the paddle using its default constructor
    , stateTimer(0.f)                       // general state timer starts at zero
    , shakeTimer(0.f)                       // no screen shake at startup
    , blinkTimer(0.f)                       // blink timer starts at zero
    , titleAnimTimer(0.f)                   // title animation timer starts at zero
    , slowActive(false)                     // slow powerup not active at startup
    , slowTimer(0.f)                        // slow timer starts at zero
    , rng(std::random_device{}())           // seeds Mersenne-Twister from hardware entropy
    , dist01(0.f, 1.f)                      // uniform [0,1) distribution for probability rolls
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {    // initializes SDL2 video subsystem
        throw std::runtime_error(SDL_GetError()); // throws if SDL cannot initialize (e.g. no display)
    }

    if (TTF_Init() != 0) {                  // initializes SDL_ttf font library
        throw std::runtime_error(TTF_GetError()); // throws if TTF cannot initialize
    }

    window = SDL_CreateWindow(             // creates the OS window
        "BREAKOUT",                        // window title bar text
        SDL_WINDOWPOS_CENTERED,            // centers the window horizontally on the display
        SDL_WINDOWPOS_CENTERED,            // centers the window vertically on the display
        CANVAS_W,                          // window width in pixels
        CANVAS_H,                          // window height in pixels
        0                                  // no special flags (not fullscreen, not resizable)
    );
    if (!window) throw std::runtime_error(SDL_GetError()); // throws if window creation failed

    renderer = SDL_CreateRenderer(         // creates the hardware-accelerated 2D renderer
        window,                            // attaches renderer to the window created above
        -1,                               // -1 selects the first available rendering driver
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC // uses GPU acceleration and vsync to cap at monitor refresh rate
    );
    if (!renderer) throw std::runtime_error(SDL_GetError()); // throws if renderer creation failed

    // Load fonts from the system font path defined in Constants.h
    fontLarge = TTF_OpenFont(FONT_PATH, FONT_SIZE_LARGE); // opens font at large size for titles
    fontMed   = TTF_OpenFont(FONT_PATH, FONT_SIZE_MED);   // opens font at medium size for state messages
    fontSmall = TTF_OpenFont(FONT_PATH, FONT_SIZE_SMALL); // opens font at small size for HUD
    fontTiny  = TTF_OpenFont(FONT_PATH, FONT_SIZE_TINY);  // opens font at tiny size for powerup labels
    // If any font fails to load, the game will still run but text will not render.
    // To fix, update FONT_PATH in Constants.h to a valid .ttf file on your system.

    highScore = loadHighScore();            // reads the best score from the save file on disk

    std::srand(static_cast<unsigned>(std::time(nullptr))); // seeds std::rand for the screen shake random offset
}

Game::~Game() {                            // destructor: releases all SDL resources in reverse construction order
    if (fontTiny)  TTF_CloseFont(fontTiny);  // frees tiny font handle
    if (fontSmall) TTF_CloseFont(fontSmall); // frees small font handle
    if (fontMed)   TTF_CloseFont(fontMed);   // frees medium font handle
    if (fontLarge) TTF_CloseFont(fontLarge); // frees large font handle
    if (renderer)  SDL_DestroyRenderer(renderer); // frees the renderer and all GPU textures associated with it
    if (window)    SDL_DestroyWindow(window);     // closes the OS window
    TTF_Quit();    // shuts down SDL_ttf
    SDL_Quit();    // shuts down all SDL2 subsystems
}

// ─── Main Loop ────────────────────────────────────────────────────────────────

void Game::run() {                         // starts and runs the game loop until the window is closed
    bool running = true;                   // loop control flag; set to false by SDL_QUIT event
    Uint32 lastTick = SDL_GetTicks();      // records the timestamp at the start of the first frame

    while (running) {                      // loops until the player closes the window
        Uint32 now = SDL_GetTicks();       // gets the current timestamp in milliseconds
        float  dt  = static_cast<float>(now - lastTick); // computes elapsed time since last frame
        if (dt > MAX_DT) dt = MAX_DT;     // caps delta time to prevent physics explosion if the window was minimized
        lastTick = now;                    // saves current time for next iteration

        SDL_Event event;                   // declares an event object to receive SDL events
        while (SDL_PollEvent(&event)) {    // drains the SDL event queue (may contain multiple events per frame)
            if (event.type == SDL_QUIT) {  // user clicked the window close button or pressed Alt+F4
                running = false;           // signals the outer loop to exit after this iteration
            }
            handleEvent(event);            // dispatches the event to the game's input handler
        }

        update(dt);                        // advances game logic by dt milliseconds
        render();                          // draws the current frame to the back buffer
        SDL_RenderPresent(renderer);       // swaps back buffer to screen, making the frame visible
    }
}

// ─── Game Management ──────────────────────────────────────────────────────────

void Game::resetGameVars() {               // resets all per-session variables to starting values
    level      = 1;                        // resets level counter
    lives      = LIVES_START;             // restores full lives
    score      = 0;                        // clears score
    balls.clear();                         // removes all ball objects
    blocks.clear();                        // removes all block objects
    powerups.clear();                      // removes all powerup objects
    lasers.clear();                        // removes all laser objects
    particles.clear();                     // removes all particle objects
    slowActive = false;                    // clears slow powerup state
    slowTimer  = 0.f;                      // clears slow timer
    stateTimer = 0.f;                      // clears general state timer
    shakeTimer = 0.f;                      // clears shake timer
}

void Game::startNewGame() {                // prepares all entities and transitions to Playing
    resetGameVars();                       // resets all session variables
    paddle = Paddle();                     // creates a fresh paddle at the center
    blocks = generateLevel(level);        // generates the block grid for level 1
    createBallOnPaddle();                 // places a ball on the paddle ready for launch
    state  = GameState::Playing;          // switches to the gameplay state
}

void Game::createBallOnPaddle() {                          // creates a new ball glued to the paddle's top surface
    float spd = std::min(BALL_BASE_SPEED + (level - 1) * BALL_SPEED_INC, BALL_MAX_SPEED); // scales speed with level
    Ball b(paddle.centerX(), static_cast<float>(paddle.y - BALL_RADIUS), 0.f, -spd, spd); // positions ball centered above paddle
    b.onPaddle = true;                                     // marks ball as attached (not launched yet)
    balls.push_back(b);                                    // adds ball to the active list
}

void Game::resetAfterLifeLost() {                          // sets up a new ball and resumes play after a life is lost
    balls.clear();                                         // removes any remaining balls (including multi-balls)
    powerups.clear();                                      // clears all falling powerups
    lasers.clear();                                        // clears all laser projectiles
    paddle.width      = paddle.baseWidth;                  // resets paddle to normal width
    paddle.wideTimer  = 0.f;                               // clears wide timer
    paddle.laserActive = false;                            // disables laser mode
    paddle.laserTimer  = 0.f;                              // clears laser timer
    slowActive = false;                                    // clears slow effect
    slowTimer  = 0.f;                                      // clears slow timer
    createBallOnPaddle();                                  // places a fresh ball on the paddle
    state      = GameState::Playing;                       // transitions back to gameplay
    stateTimer = 0.f;                                      // resets state timer
}

void Game::loadNextLevel() {                               // advances the game to the next level
    level++;                                               // increments the level counter
    balls.clear();                                         // clears all balls
    powerups.clear();                                      // clears all powerups
    lasers.clear();                                        // clears all lasers
    particles.clear();                                     // clears lingering particles
    paddle.width       = paddle.baseWidth;                 // resets paddle width
    paddle.wideTimer   = 0.f;                              // clears wide timer
    paddle.laserActive = false;                            // disables laser
    paddle.laserTimer  = 0.f;                              // clears laser timer
    slowActive = false;                                    // clears slow effect
    slowTimer  = 0.f;                                      // clears slow timer
    blocks = generateLevel(level);                         // generates a fresh block grid for the new level
    createBallOnPaddle();                                  // places a new ball on the paddle
    state      = GameState::Playing;                       // transitions to gameplay
    stateTimer = 0.f;                                      // resets state timer
}

// ─── Level Generation ─────────────────────────────────────────────────────────

std::vector<Block> Game::generateLevel(int lvl) {          // builds and returns a random block grid for the given level
    std::vector<Block> result;                             // list to collect created blocks
    int rows = std::min(BLOCK_ROWS_BASE + lvl / 2, BLOCK_ROWS_MAX); // increases row count every 2 levels up to the maximum
    for (int r = 0; r < rows; r++) {                       // iterates each block row
        for (int c = 0; c < BLOCK_COLS; c++) {             // iterates each block column
            if (dist01(rng) < BLOCK_SKIP_CHANCE) continue; // randomly skips ~15% of cells for visual variety
            int x = BLOCK_OFFSET_LEFT + c * (BLOCK_W + BLOCK_PAD); // calculates block left edge from column index
            int y = BLOCK_OFFSET_TOP  + r * (BLOCK_H + BLOCK_PAD); // calculates block top edge from row index
            float roll = dist01(rng);                      // rolls a random number to determine block type
            int       hp;                                  // hit points for this block
            SDL_Color color;                               // color for this block
            if (lvl >= 6 && roll < 0.10f) {               // 10% chance of indestructible block from level 6 onward
                hp    = -1;                                // indestructible
                color = COL_INDESTR;                       // gray color
            } else if (lvl >= 3 && roll < 0.30f) {        // 30% chance of two-hit block from level 3 onward
                hp    = 2;                                 // requires two hits
                color = COL_HP2;                           // red-pink color
            } else {                                       // default: normal single-hit block
                hp    = 1;                                 // destroyed in one hit
                int ci = static_cast<int>(dist01(rng) * NORMAL_COLOR_COUNT); // picks a random index from the palette
                color = NORMAL_COLORS[ci % NORMAL_COLOR_COUNT];              // selects the color (modulo guards bounds)
            }
            result.emplace_back(x, y, hp, color);         // constructs and adds the block in-place
        }
    }
    return result;                                         // returns the completed block list
}

PowerupType Game::randomPowerupType() {                    // selects a powerup type using weighted probability
    // Define weights: higher weight = more frequent drop
    constexpr float weights[] = {30.f, 25.f, 20.f, 15.f, 10.f}; // Wide, Slow, Multiball, Laser, Life
    constexpr int   N         = 5;                         // number of powerup types
    float total = 0.f;                                     // will hold the sum of all weights
    for (int i = 0; i < N; i++) total += weights[i];      // sums all weights into total

    float roll = dist01(rng) * total;                      // picks a random value within the weight range
    for (int i = 0; i < N; i++) {                         // iterates through each weight entry
        roll -= weights[i];                                // subtracts each weight from the roll
        if (roll <= 0.f) {                                 // when roll reaches zero, this entry is selected
            return static_cast<PowerupType>(i);            // returns the corresponding PowerupType enum value
        }
    }
    return PowerupType::Wide;                              // fallback (should never be reached)
}

// ─── Collision ────────────────────────────────────────────────────────────────

void Game::ballPaddleCollision(Ball& ball) {               // resolves a ball bouncing off the paddle
    if (ball.y + ball.radius < paddle.y) return;           // ball is entirely above the paddle; no collision
    if (ball.y - ball.radius > paddle.y + paddle.height) return; // ball is entirely below the paddle; no collision
    if (ball.x + ball.radius < paddle.x) return;           // ball is entirely left of the paddle; no collision
    if (ball.x - ball.radius > paddle.x + paddle.width)  return; // ball is entirely right of the paddle; no collision
    if (ball.vy < 0.f) return;                             // ball is already moving upward; skip (prevents sticking on re-entry)

    ball.y = static_cast<float>(paddle.y - ball.radius);   // snaps ball above the paddle surface to prevent sinking

    float hitPos = (ball.x - paddle.centerX()) / (paddle.width / 2.f); // normalizes hit position: -1 (left edge) to +1 (right edge)
    if (hitPos < -0.95f) hitPos = -0.95f;                  // clamps to prevent nearly-horizontal angles on far left
    if (hitPos >  0.95f) hitPos =  0.95f;                  // clamps to prevent nearly-horizontal angles on far right

    float angle = hitPos * (3.14159f / 3.f);               // maps hit position to a launch angle: ±60 degrees from vertical
    ball.vx = ball.speed * std::sin(angle);                 // sets horizontal velocity component from the angle
    ball.vy = -ball.speed * std::cos(angle);                // sets vertical velocity upward from the angle
}

bool Game::ballBlockCollision(Ball& ball, Block& block) {  // AABB test and resolve for a ball hitting a block; returns true if hit
    if (!block.active) return false;                       // skip already-destroyed blocks

    float bL = ball.x - ball.radius; // ball left edge
    float bR = ball.x + ball.radius; // ball right edge
    float bT = ball.y - ball.radius; // ball top edge
    float bB = ball.y + ball.radius; // ball bottom edge

    if (bR < block.x)               return false; // ball is entirely left of block; no overlap
    if (bL > block.x + block.width) return false; // ball is entirely right of block; no overlap
    if (bB < block.y)               return false; // ball is entirely above block; no overlap
    if (bT > block.y + block.height)return false; // ball is entirely below block; no overlap

    // Compute penetration depth on each side to determine which face the ball hit
    float overlapL = bR - block.x;                          // depth of penetration from the block's left face
    float overlapR = (block.x + block.width)  - bL;        // depth of penetration from the block's right face
    float overlapT = bB - block.y;                          // depth of penetration from the block's top face
    float overlapB = (block.y + block.height) - bT;        // depth of penetration from the block's bottom face

    float minX = std::min(overlapL, overlapR);              // smallest horizontal penetration (indicates side hit)
    float minY = std::min(overlapT, overlapB);              // smallest vertical penetration (indicates top/bottom hit)

    if (minX < minY) {                                      // horizontal face was hit (left or right side of block)
        ball.vx *= -1.f;                                    // reverses horizontal velocity
    } else {                                                // vertical face was hit (top or bottom of block)
        ball.vy *= -1.f;                                    // reverses vertical velocity
    }

    block.hit();                                            // reduces block HP and deactivates if zero

    if (!block.active) {                                    // if this hit destroyed the block
        score += block.scoreValue;                          // adds the block's point value to the score
        for (int i = 0; i < 8; i++) {                      // spawns 8 particles at the block's center
            particles.emplace_back(                         // constructs particle in-place in the list
                static_cast<float>(block.x + block.width / 2),  // particle origin x (block center)
                static_cast<float>(block.y + block.height / 2), // particle origin y (block center)
                block.color                                      // particle color matches block
            );
        }
        if (dist01(rng) < POWERUP_CHANCE) {                 // rolls for powerup drop
            float px = static_cast<float>(block.x + block.width / 2 - 17); // powerup x centered on block
            float py = static_cast<float>(block.y);         // powerup y at top of block
            powerups.emplace_back(px, py, randomPowerupType()); // spawns powerup in-place
        }
    }

    return true;                                            // signals that a collision occurred
}

bool Game::laserBlockCollision(Laser& laser, Block& block) { // AABB test for laser hitting a block; returns true if hit
    if (!block.active || !laser.active) return false;        // skip if either entity is already inactive

    SDL_Rect lr = laser.rect();                              // gets the laser bounding rect
    SDL_Rect br = block.rect();                              // gets the block bounding rect
    if (!SDL_HasIntersection(&lr, &br)) return false;        // SDL's built-in AABB test; returns false if no overlap

    laser.active = false;                                    // destroys the laser on contact
    block.hit();                                             // damages the block

    if (!block.active) {                                     // if this hit destroyed the block
        score += block.scoreValue;                           // awards points
        for (int i = 0; i < 6; i++) {                       // spawns 6 particles
            particles.emplace_back(                          // constructs particle in-place
                static_cast<float>(block.x + block.width / 2),
                static_cast<float>(block.y + block.height / 2),
                block.color
            );
        }
        if (dist01(rng) < POWERUP_CHANCE) {                  // rolls for powerup drop
            float px = static_cast<float>(block.x + block.width / 2 - 17);
            float py = static_cast<float>(block.y);
            powerups.emplace_back(px, py, randomPowerupType());
        }
    }

    return true;                                             // signals collision occurred
}

void Game::collectPowerup(Powerup& pu) {                     // applies the powerup's effect and deactivates it
    pu.active = false;                                       // marks powerup as consumed

    switch (pu.type) {                                       // dispatches effect based on powerup type
        case PowerupType::Wide:                              // wide paddle powerup
            paddle.activateWide();                           // expands the paddle
            break;

        case PowerupType::Slow:                              // slow ball powerup
            if (!slowActive) {                               // only slows balls if not already slowed
                for (auto& b : balls) {                      // iterates all active balls
                    b.speed *= SLOW_FACTOR;                  // reduces speed scalar
                    b.normalizeSpeed();                      // reapplies scalar to velocity vector
                }
            }
            slowActive = true;                               // marks slow effect as active
            slowTimer  = SLOW_DURATION_MS;                   // resets slow duration timer
            break;

        case PowerupType::Multiball: {                       // multi-ball powerup: spawns two extra balls
            Ball* src = nullptr;                             // will point to the first in-flight ball
            for (auto& b : balls) {                          // searches for an active flying ball to clone
                if (!b.onPaddle && b.active) { src = &b; break; } // stops at the first match
            }
            if (src) {                                       // if a flying ball was found
                Ball b1(src->x, src->y, src->vx + 2.f, src->vy, src->speed); // new ball deflected slightly right
                b1.onPaddle = false;                         // immediately in flight
                Ball b2(src->x, src->y, src->vx - 2.f, src->vy, src->speed); // new ball deflected slightly left
                b2.onPaddle = false;                         // immediately in flight
                b1.normalizeSpeed();                         // corrects speed after offset
                b2.normalizeSpeed();                         // corrects speed after offset
                balls.push_back(b1);                         // adds first extra ball
                balls.push_back(b2);                         // adds second extra ball
            }
            break;
        }

        case PowerupType::Life:                              // extra life powerup
            lives++;                                         // grants one additional life
            break;

        case PowerupType::Laser:                             // laser powerup
            paddle.activateLaser();                          // enables the paddle's laser mode
            break;
    }
}

// ─── Update ───────────────────────────────────────────────────────────────────

void Game::update(float dt) {                                // dispatches to the correct per-state update function
    blinkTimer     += dt;                                    // always advances blink timer regardless of state
    titleAnimTimer += dt;                                    // always advances title animation timer

    switch (state) {                                         // dispatches based on current game state
        case GameState::Title:         /* nothing extra */   break; // title screen has no per-frame logic beyond timers
        case GameState::Playing:       updatePlaying(dt);    break; // runs the full gameplay update
        case GameState::LifeLost:      updateLifeLost(dt);   break; // waits for input or timeout
        case GameState::LevelComplete: updateLevelComplete(dt); break; // waits for input or timeout
        case GameState::GameOver:      /* nothing */         break; // game over screen waits for keypress only
        case GameState::Paused:        /* nothing */         break; // paused state waits for ESC only
    }
}

void Game::updatePlaying(float dt) {                                     // full gameplay update for one frame
    stateTimer += dt;                                                    // advances general state timer

    const Uint8* keys = SDL_GetKeyboardState(nullptr);                   // gets the current keyboard state map

    // Fire lasers when Space is held and laser powerup is active and not on cooldown
    if (keys[SDL_SCANCODE_SPACE] && paddle.laserActive && paddle.laserCooldownTimer <= 0.f) {
        float laserX1 = paddle.x + paddle.width * 0.25f;                // left laser fires from 25% of paddle width
        float laserX2 = paddle.x + paddle.width * 0.75f;                // right laser fires from 75% of paddle width
        lasers.emplace_back(laserX1, static_cast<float>(paddle.y) - 2.f); // spawns left laser above paddle
        lasers.emplace_back(laserX2, static_cast<float>(paddle.y) - 2.f); // spawns right laser above paddle
        paddle.laserCooldownTimer = LASER_COOLDOWN_MS;                   // starts cooldown before next shot is allowed
    }

    paddle.update(dt, keys);                                             // moves paddle and ticks powerup timers

    // Snap attached balls to the paddle center
    for (auto& b : balls) {                                              // iterates all balls
        if (b.onPaddle) {                                                // only adjusts balls still glued to the paddle
            b.x = paddle.centerX();                                      // centers ball on paddle horizontally
            b.y = static_cast<float>(paddle.y - b.radius);              // positions ball just above paddle surface
        }
    }

    // Move all entities
    for (auto& b : balls)     b.update();    // moves each ball by its velocity
    for (auto& p : powerups)  p.update();    // falls each powerup downward
    for (auto& l : lasers)    l.update();    // moves each laser upward
    for (auto& p : particles) p.update(dt); // advances each particle's physics and fade

    // Wall and ceiling collisions for each ball
    for (auto& ball : balls) {                                                         // checks each ball
        if (ball.onPaddle) continue;                                                   // skip attached balls
        if (ball.x - ball.radius < 0.f) {                                             // hit left wall
            ball.x  = static_cast<float>(ball.radius);                                // pushes ball inside boundary
            ball.vx = std::abs(ball.vx);                                              // ensures ball moves rightward
        }
        if (ball.x + ball.radius > static_cast<float>(CANVAS_W)) {                   // hit right wall
            ball.x  = static_cast<float>(CANVAS_W - ball.radius);                    // pushes ball inside boundary
            ball.vx = -std::abs(ball.vx);                                             // ensures ball moves leftward
        }
        if (ball.y - ball.radius < 0.f) {                                             // hit ceiling
            ball.y  = static_cast<float>(ball.radius);                                // pushes ball below ceiling
            ball.vy = std::abs(ball.vy);                                              // ensures ball moves downward
        }
        if (ball.y - ball.radius > static_cast<float>(CANVAS_H)) {                   // ball fell below screen
            ball.active = false;                                                       // marks ball as lost
        }
    }

    // Ball vs paddle
    for (auto& ball : balls) {                                           // checks each ball against the paddle
        if (!ball.onPaddle) ballPaddleCollision(ball);                   // only tests in-flight balls
    }

    // Ball vs blocks
    for (auto& ball : balls) {                                           // checks each ball against all blocks
        if (ball.onPaddle) continue;                                     // skip attached balls
        for (auto& block : blocks) {                                     // checks each block
            if (ballBlockCollision(ball, block)) break;                  // stops after first block hit per ball per frame
        }
    }

    // Laser vs blocks
    for (auto& laser : lasers) {                                         // checks each laser against all blocks
        for (auto& block : blocks) {                                     // checks each block
            if (laserBlockCollision(laser, block)) break;                // stops after first block hit per laser
        }
    }

    // Powerup collection by paddle
    SDL_Rect pr = paddle.rect();                                         // gets paddle bounding rect once
    for (auto& pu : powerups) {                                          // checks each powerup
        if (pu.active) {                                                 // only checks active powerups
            SDL_Rect pRect = pu.rect();                                  // gets powerup bounding rect
            if (SDL_HasIntersection(&pRect, &pr)) {                      // tests for overlap with paddle
                collectPowerup(pu);                                      // applies effect and deactivates
            }
        }
    }

    // Tick slow-ball timer
    if (slowActive) {                                                    // if slow powerup is currently running
        slowTimer -= dt;                                                 // counts down slow duration
        if (slowTimer <= 0.f) {                                          // if slow duration just expired
            slowActive = false;                                          // clears slow state
            float normalSpd = std::min(BALL_BASE_SPEED + (level - 1) * BALL_SPEED_INC, BALL_MAX_SPEED); // recalculates normal speed for this level
            for (auto& b : balls) {                                      // restores all balls to normal speed
                b.speed = normalSpd;                                     // sets speed scalar back to normal
                b.normalizeSpeed();                                      // reapplies scalar to velocity direction
            }
        }
    }

    // Remove inactive entities from all lists using the erase-remove idiom
    balls.erase(    std::remove_if(balls.begin(),     balls.end(),     [](const Ball&     b){ return !b.active;  }), balls.end());
    powerups.erase( std::remove_if(powerups.begin(),  powerups.end(),  [](const Powerup&  p){ return !p.active;  }), powerups.end());
    lasers.erase(   std::remove_if(lasers.begin(),    lasers.end(),    [](const Laser&    l){ return !l.active;  }), lasers.end());
    particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p){ return !p.active;  }), particles.end());
    blocks.erase(   std::remove_if(blocks.begin(),    blocks.end(),    [](const Block&    b){ return !b.active && b.hp != -1; }), blocks.end());
    // Note: blocks with hp == -1 (indestructible) are never active==false, but the lambda double-checks

    // Check if all balls have been lost
    if (balls.empty()) {                                                 // no balls remain in play
        lives--;                                                         // deducts a life
        if (lives <= 0) {                                                // all lives exhausted
            if (score > highScore) {                                     // new high score achieved
                highScore = score;                                       // updates high score in memory
                saveHighScore(highScore);                                // writes updated high score to disk
            }
            state = GameState::GameOver;                                 // transitions to game over screen
        } else {                                                         // player has lives remaining
            shakeTimer = SHAKE_DURATION_MS;                              // triggers screen shake
            state = GameState::LifeLost;                                 // transitions to life-lost pause
        }
        stateTimer = 0.f;                                                // resets timer for the new state
        return;                                                          // exits update early (no need to check block count)
    }

    // Check if all destructible blocks are cleared
    bool anyDestructible = std::any_of(blocks.begin(), blocks.end(),    // searches for at least one block with hp != -1
        [](const Block& b){ return b.hp != -1; });
    if (!anyDestructible) {                                              // no destructible blocks remain
        if (score > highScore) {                                         // check for new high score on level complete too
            highScore = score;
            saveHighScore(highScore);
        }
        state      = GameState::LevelComplete;                           // transitions to level complete screen
        stateTimer = 0.f;                                                // resets timer
    }
}

void Game::updateLifeLost(float dt) {                        // handles the brief pause after losing a ball
    stateTimer += dt;                                        // ticks the auto-advance timer
    if (stateTimer > AUTO_ADVANCE_MS) {                      // if 2.5 seconds have elapsed without player pressing Space
        resetAfterLifeLost();                                // resumes play automatically
    }
}

void Game::updateLevelComplete(float dt) {                   // handles the level-complete pause screen
    stateTimer += dt;                                        // ticks the auto-advance timer
    if (stateTimer > AUTO_ADVANCE_MS) {                      // if 2.5 seconds have elapsed without player pressing Space
        loadNextLevel();                                     // advances to the next level automatically
    }
}

// ─── Render ───────────────────────────────────────────────────────────────────

void Game::render() {                                        // dispatches to the correct per-state render function
    switch (state) {
        case GameState::Title:                               // title screen
            renderTitle();                                   // draws animated title
            break;
        case GameState::Playing:                             // active gameplay
            renderGame();                                    // draws all entities and HUD
            break;
        case GameState::LifeLost:                            // life lost pause
            renderGame();                                    // draws frozen game state underneath
            renderOverlay(153);                              // ~60% opacity dark overlay
            drawText("BALL LOST!", CANVAS_W / 2, CANVAS_H / 2 - 30, fontMed, COL_RED, true);
            drawText(std::to_string(lives) + (lives == 1 ? " LIFE REMAINING" : " LIVES REMAINING"),
                     CANVAS_W / 2, CANVAS_H / 2 + 10, fontSmall, COL_HUD, true);
            drawBlink("PRESS SPACE", CANVAS_H / 2 + 55, fontSmall, COL_WHITE, 500.f);
            break;
        case GameState::LevelComplete:                       // level complete screen
            renderGame();
            renderOverlay(165);                              // ~65% opacity overlay
            drawText("LEVEL " + std::to_string(level), CANVAS_W / 2, CANVAS_H / 2 - 40, fontMed, COL_GREEN, true);
            drawText("COMPLETE!",                     CANVAS_W / 2, CANVAS_H / 2 - 10, fontMed, COL_GREEN, true);
            drawText("SCORE: " + std::to_string(score), CANVAS_W / 2, CANVAS_H / 2 + 30, fontSmall, COL_WHITE, true);
            drawBlink("PRESS SPACE", CANVAS_H / 2 + 65, fontSmall, COL_WHITE, 500.f);
            break;
        case GameState::GameOver:                            // game over screen
            renderGame();
            renderOverlay(191);                              // ~75% opacity overlay
            drawText("GAME OVER",                  CANVAS_W / 2, CANVAS_H / 2 - 30, fontLarge, COL_RED, true);
            drawText("SCORE: " + std::to_string(score), CANVAS_W / 2, CANVAS_H / 2 + 20, fontSmall, COL_HUD, true);
            if (score > 0 && score >= highScore)             // shows congratulation only on new high score
                drawText("NEW BEST!", CANVAS_W / 2, CANVAS_H / 2 + 45, fontSmall, COL_GOLD, true);
            drawBlink("PRESS SPACE", CANVAS_H / 2 + 80, fontSmall, COL_WHITE, 500.f);
            break;
        case GameState::Paused:                              // paused screen
            renderGame();
            renderOverlay(127);                              // 50% opacity overlay
            drawText("PAUSED",        CANVAS_W / 2, CANVAS_H / 2 - 10, fontLarge, COL_WHITE, true);
            drawText("ESC TO RESUME", CANVAS_W / 2, CANVAS_H / 2 + 35, fontSmall, COL_HUD,   true);
            break;
    }
}

void Game::renderGame() {                                    // draws all live entities and the HUD
    // Compute screen-shake offset
    int ox = 0, oy = 0;                                      // default: no shake offset
    if (shakeTimer > 0.f) {                                  // if shake is still active
        shakeTimer -= 16.f;                                  // decrements shake timer by ~1 frame at 60 fps
        ox = (std::rand() % (SHAKE_MAGNITUDE * 2 + 1)) - SHAKE_MAGNITUDE; // random horizontal offset
        oy = (std::rand() % (SHAKE_MAGNITUDE * 2 + 1)) - SHAKE_MAGNITUDE; // random vertical offset
        SDL_RenderSetViewport(renderer, nullptr);             // ensures viewport is full screen before translating
    }

    // Translate the renderer origin for shake (achieved by drawing everything offset)
    // SDL2 does not have a global translate, so we apply ox/oy inline during each draw call;
    // instead, we use SDL_RenderSetClipRect / logical coordinates trick: draw everything shifted.
    // For simplicity, entities absorb the shake by adjusting their draw positions via a lambda.
    // Here we apply shake via a temporary viewport shift:
    if (ox != 0 || oy != 0) {                                // only bother if shake offset is non-zero
        SDL_Rect vp = {ox, oy, CANVAS_W, CANVAS_H};          // viewport shifted by shake amount
        SDL_RenderSetViewport(renderer, &vp);                 // offsets the rendering viewport
    }

    SDL_SetRenderDrawColor(renderer, COL_BG.r, COL_BG.g, COL_BG.b, 255); // sets background color
    SDL_RenderClear(renderer);                               // fills the entire canvas with the background color

    for (auto& p : particles) p.draw(renderer);              // draws particles behind all other entities
    for (auto& b : blocks)    b.draw(renderer);              // draws all block tiles
    for (auto& p : powerups)  p.draw(renderer, fontTiny);   // draws falling powerup pills with labels
    for (auto& l : lasers)    l.draw(renderer);              // draws laser beams
    paddle.draw(renderer);                                   // draws the player paddle
    for (auto& b : balls)     b.draw(renderer);              // draws all balls

    if (ox != 0 || oy != 0) {                                // restores the viewport after shake drawing
        SDL_RenderSetViewport(renderer, nullptr);             // resets to the full-screen viewport
    }

    drawHUD();                                               // draws HUD on top of everything (no shake applied to HUD)
}

void Game::renderTitle() {                                   // draws the animated title screen
    SDL_SetRenderDrawColor(renderer, COL_BG.r, COL_BG.g, COL_BG.b, 255); // background color
    SDL_RenderClear(renderer);                               // clears the canvas

    float t = titleAnimTimer / 1000.f;                       // converts timer from ms to seconds for sine wave input

    for (int r = 0; r < 5; r++) {                            // draws 5 rows of decorative animated blocks
        for (int c = 0; c < BLOCK_COLS; c++) {               // draws 10 columns
            int bx = BLOCK_OFFSET_LEFT + c * (BLOCK_W + BLOCK_PAD);     // block x position
            int by = BLOCK_OFFSET_TOP + 20 + r * (BLOCK_H + BLOCK_PAD); // block base y position
            float wave = std::sin(t + static_cast<float>(c) * 0.5f) * 6.f; // sine wave displacement per column
            int   drawY = by + static_cast<int>(wave);       // applies wave to y position
            float pulse = 0.4f + 0.3f * std::sin(t * 2.f + r + c);     // pulsing opacity value (0.1–0.7)
            Uint8 alpha = static_cast<Uint8>(255.f * pulse);             // converts float opacity to 0–255 byte

            SDL_Color col = NORMAL_COLORS[(r * BLOCK_COLS + c) % NORMAL_COLOR_COUNT]; // cycles palette by position
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);   // enables blending for pulsing alpha
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, alpha); // sets color with pulsing opacity
            SDL_Rect rc = {bx, drawY, BLOCK_W, BLOCK_H};                 // block rect at animated position
            SDL_RenderFillRect(renderer, &rc);                           // fills the decorative block
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);            // resets blend mode

    drawText("BREAKOUT", CANVAS_W / 2, static_cast<int>(CANVAS_H * 0.55f), fontLarge, {79, 195, 247, 255}, true); // game title in cyan
    drawBlink("PRESS SPACE TO START",      static_cast<int>(CANVAS_H * 0.70f), fontSmall, COL_WHITE, 600.f);       // blinking start prompt
    drawText("ARROW KEYS / A-D TO MOVE",  CANVAS_W / 2, static_cast<int>(CANVAS_H * 0.78f), fontTiny, {120,120,120,255}, true); // movement hint
    drawText("SPACE TO LAUNCH BALL",      CANVAS_W / 2, static_cast<int>(CANVAS_H * 0.83f), fontTiny, {120,120,120,255}, true); // launch hint
    drawText("ESC TO PAUSE",              CANVAS_W / 2, static_cast<int>(CANVAS_H * 0.88f), fontTiny, {120,120,120,255}, true); // pause hint
    if (highScore > 0)                                                   // shows high score only if one exists
        drawText("BEST: " + std::to_string(highScore), CANVAS_W / 2, static_cast<int>(CANVAS_H * 0.93f), fontSmall, COL_GOLD, true);
}

void Game::renderOverlay(Uint8 alpha) {                      // draws a semi-transparent black rectangle over the entire canvas
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // enables alpha blending
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);         // black with the specified opacity
    SDL_RenderFillRect(renderer, nullptr);                    // nullptr fills the entire rendering target
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); // restores normal blend mode
}

void Game::drawHUD() {                                       // renders the score, level, and lives at the top of the screen
    drawText("SCORE " + [](int s){                           // formats score as a zero-padded 6-digit string
        std::string r = std::to_string(s);                   // converts integer score to string
        while (r.size() < 6) r = "0" + r;                   // prepends zeros until string is 6 characters
        return r;                                            // returns zero-padded score string
    }(score), 8, 6, fontSmall, COL_HUD, false);              // draws score left-aligned in top-left corner

    drawText("LVL " + std::to_string(level), CANVAS_W / 2, 6, fontSmall, COL_HUD, true); // draws level number centered at top

    for (int i = 0; i < lives; i++) {                        // draws one heart icon per remaining life
        drawText("\u2665",                                    // Unicode heart character ♥
            CANVAS_W - 14 - i * 22, 6,                       // positions hearts from right edge inward
            fontSmall, COL_LIFE, false);                      // pink color for life hearts
    }

    if (highScore > 0) {                                     // shows best score only if one has been set
        drawText("BEST " + std::to_string(highScore),
            CANVAS_W / 2, 22, fontTiny, {100, 100, 100, 255}, true); // dim gray, small font, centered below level
    }
}

void Game::drawText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color, bool centered) const {
    if (!font || text.empty()) return;                        // guard: skip if font is null or text is empty
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), color); // renders UTF-8 text to a CPU surface with alpha
    if (!surf) return;                                        // guard: skip if surface creation failed
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf); // uploads the surface to a GPU texture
    if (tex) {                                               // proceeds only if texture creation succeeded
        int tw, th;                                          // will hold the texture pixel dimensions
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);   // queries the texture's width and height
        int drawX = centered ? x - tw / 2 : x;              // if centered, adjusts x so the text is horizontally centered
        SDL_Rect dst = {drawX, y, tw, th};                   // defines the destination rectangle on screen
        SDL_RenderCopy(renderer, tex, nullptr, &dst);        // draws the texture at the destination rectangle
        SDL_DestroyTexture(tex);                             // frees GPU texture memory after drawing
    }
    SDL_FreeSurface(surf);                                   // frees CPU surface memory after texture creation
}

void Game::drawBlink(const std::string& text, int y, TTF_Font* font, SDL_Color color, float periodMs) {
    if (blinkOn(periodMs))                                   // checks whether the blink timer is in the "visible" phase
        drawText(text, CANVAS_W / 2, y, font, color, true); // draws centered text when blink is on
}

bool Game::blinkOn(float periodMs) const {                   // returns true during the first half of each blink period
    int phase = static_cast<int>(blinkTimer / periodMs);     // determines which period count we're in
    return (phase % 2) == 0;                                 // even periods = visible, odd periods = invisible
}

// ─── Persistence ──────────────────────────────────────────────────────────────

int Game::loadHighScore() const {                            // reads the saved high score from disk
    std::ifstream f(HS_FILE);                                // opens the high score file for reading
    if (!f.is_open()) return 0;                              // returns 0 if file doesn't exist yet
    int hs = 0;                                              // will hold the parsed score value
    f >> hs;                                                 // reads the integer from the file
    return hs;                                               // returns the loaded high score
}

void Game::saveHighScore(int s) const {                      // writes the given score to disk
    std::ofstream f(HS_FILE);                                // opens (or creates) the high score file for writing
    if (f.is_open()) f << s;                                 // writes the score as a plain integer if file opened successfully
}

// ─── Event Handling ───────────────────────────────────────────────────────────

void Game::handleEvent(const SDL_Event& event) {             // processes a single SDL input event
    if (event.type != SDL_KEYDOWN) return;                   // only handles key-press events (not key-release or mouse)

    SDL_Keycode key = event.key.keysym.sym;                  // extracts the virtual key code from the event

    if (key == SDLK_SPACE) {                                 // Space bar was pressed
        switch (state) {
            case GameState::Title:                           // on the title screen
                startNewGame();                              // starts a fresh game
                break;
            case GameState::Playing:                         // during gameplay
                if (!paddle.laserActive) {                   // if laser is not active, Space launches the ball
                    for (auto& b : balls) {                  // iterates all balls
                        if (b.onPaddle) {                    // only launches balls still glued to the paddle
                            b.onPaddle = false;              // detaches ball from paddle
                            b.vx = 0.f;                      // no initial horizontal velocity
                            b.vy = -b.speed;                 // launches ball straight upward at full speed
                        }
                    }
                }
                // If laser is active, Space fires — that is handled in updatePlaying each frame
                break;
            case GameState::LifeLost:                        // on the life-lost pause screen
                resetAfterLifeLost();                        // resumes immediately without waiting for the auto-timer
                break;
            case GameState::LevelComplete:                   // on the level-complete screen
                loadNextLevel();                             // advances to next level immediately
                break;
            case GameState::GameOver:                        // on the game over screen
                state      = GameState::Title;               // returns to the title screen
                blinkTimer = 0.f;                            // resets blink timer so the title prompt starts visible
                break;
            default: break;                                  // other states ignore Space
        }
    }

    if (key == SDLK_ESCAPE) {                                // Escape key was pressed
        if (state == GameState::Playing) {                   // if currently playing
            state = GameState::Paused;                       // pauses the game
        } else if (state == GameState::Paused) {             // if currently paused
            state      = GameState::Playing;                 // resumes gameplay
            stateTimer = 0.f;                                // resets state timer to avoid timer-based misfires
        }
    }
}
