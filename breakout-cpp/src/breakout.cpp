#include "breakout.h"   // includes all class declarations, constants, enums, and SDL headers

#include <algorithm>    // std::remove_if, std::any_of, std::min, std::max
#include <cmath>        // std::hypot, std::sin for ball normalization and title animation
#include <cstdlib>      // std::rand for screen-shake random offset
#include <ctime>        // std::time for seeding std::srand
#include <fstream>      // std::ifstream / std::ofstream for high score file I/O
#include <stdexcept>    // std::runtime_error for fatal SDL initialization errors

// ─── MODULE-LEVEL RNG (used by Particle) ──────────────────────────────────────

static std::mt19937                          s_rng{std::random_device{}()}; // Mersenne-Twister seeded from hardware entropy
static std::uniform_real_distribution<float> s_distVel(-2.5f, 2.5f);       // random velocity component: ±2.5 px/frame
static std::uniform_real_distribution<float> s_distSize(2.0f, 6.0f);       // random particle size: 2–6 pixels
static std::uniform_real_distribution<float> s_distLife(300.0f, 500.0f);   // random particle lifetime: 300–500 ms

// ─── BALL ─────────────────────────────────────────────────────────────────────

Ball::Ball(float x, float y, float vx, float vy, float speed) // constructs ball with position, velocity, and speed scalar
    : x(x), y(y), vx(vx), vy(vy), speed(speed)               // initializes all position and velocity members
    , radius(BALL_RADIUS)                                      // sets radius from global constant
    , onPaddle(true)                                           // new balls always start attached to the paddle
    , active(true)                                             // ball is alive at construction
{}

void Ball::update() {           // moves ball by its velocity each frame
    if (onPaddle) return;       // skip movement while ball is glued to the paddle
    x += vx;                    // advance horizontal position
    y += vy;                    // advance vertical position
}

void Ball::normalizeSpeed() {                    // restores velocity magnitude to the stored speed scalar
    float mag = std::hypot(vx, vy);             // computes current speed as vector length
    if (mag == 0.0f) return;                    // guard: avoid division by zero
    vx = (vx / mag) * speed;                   // scales horizontal component to target speed
    vy = (vy / mag) * speed;                   // scales vertical component to target speed
}

void Ball::draw(SDL_Renderer* r) const {                         // renders ball as white circle with blue glow
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);          // enables alpha blending for the glow effect
    SDL_SetRenderDrawColor(r, 160, 196, 255, 50);                // pale blue at low opacity for the glow
    int gr = radius + 4;                                         // glow ring is 4 pixels larger than the ball
    for (int dy = -gr; dy <= gr; dy++)                           // iterates each row within the glow bounding box
        for (int dx = -gr; dx <= gr; dx++)                       // iterates each column within the glow bounding box
            if (dx*dx + dy*dy <= gr*gr)                          // checks if pixel is inside the glow circle
                SDL_RenderDrawPoint(r, (int)x + dx, (int)y + dy); // draws one glow pixel

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);           // disables blending for the solid ball
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);               // solid white fill
    for (int dy = -radius; dy <= radius; dy++)                   // iterates each row within the ball bounding box
        for (int dx = -radius; dx <= radius; dx++)               // iterates each column within the ball bounding box
            if (dx*dx + dy*dy <= radius*radius)                  // checks if pixel is inside the ball circle
                SDL_RenderDrawPoint(r, (int)x + dx, (int)y + dy); // draws one ball pixel
}

// ─── PADDLE ───────────────────────────────────────────────────────────────────

Paddle::Paddle()                                            // centers paddle with default dimensions
    : baseWidth((float)PADDLE_W), width((float)PADDLE_W)   // stores original and current width
    , height(PADDLE_H)                                      // fixed height from constant
    , x((float)((CANVAS_W - PADDLE_W) / 2))                // starts centered horizontally
    , y(PADDLE_Y)                                           // fixed vertical position from constant
    , speed(PADDLE_SPEED)                                   // movement speed from constant
    , wideTimer(0.f), laserActive(false)                    // no powerups active at start
    , laserTimer(0.f), laserCooldownTimer(0.f)              // all timers start at zero
{}

float Paddle::centerX() const { return x + width / 2.f; }  // horizontal midpoint of the paddle

SDL_Rect Paddle::rect() const {                             // returns current bounding rectangle
    return {(int)x, y, (int)width, height};                 // constructs SDL_Rect from current position and size
}

void Paddle::update(float dt, const Uint8* keys) {                    // moves paddle and ticks powerup timers each frame
    if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) x -= speed; // moves left when left arrow or A is held
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) x += speed; // moves right when right arrow or D is held
    x = std::max(0.f, std::min((float)(CANVAS_W - (int)width), x));   // clamps paddle within canvas bounds

    if (wideTimer > 0.f) {          // if wide powerup is counting down
        wideTimer -= dt;            // ticks down the wide timer
        if (wideTimer <= 0.f) width = baseWidth; // restores normal width when timer expires
    }
    if (laserCooldownTimer > 0.f) laserCooldownTimer -= dt; // ticks down laser cooldown
    if (laserTimer > 0.f) {         // if laser powerup is active
        laserTimer -= dt;           // ticks down laser duration
        if (laserTimer <= 0.f) laserActive = false; // disables laser when timer expires
    }
}

void Paddle::activateWide() {                    // applies the wide-paddle powerup
    width = baseWidth * WIDE_FACTOR;             // expands paddle to 180% of normal width
    wideTimer = WIDE_DURATION_MS;               // starts the countdown timer
}

void Paddle::activateLaser() {                   // applies the laser powerup
    laserActive = true;                          // enables laser firing
    laserTimer  = LASER_DURATION_MS;             // starts the countdown timer
}

void Paddle::draw(SDL_Renderer* r) const {                              // renders paddle as gradient rectangle
    SDL_Rect rc = rect();                                               // gets current bounding rectangle
    SDL_Color top    = laserActive ? SDL_Color{255,138,101,255} : SDL_Color{220,220,220,255}; // orange or gray top
    SDL_Color bottom = laserActive ? SDL_Color{191, 54, 12,255} : SDL_Color{158,158,158,255}; // darker shade for bottom
    for (int i = 0; i < rc.h; i++) {                                   // draws each horizontal row with interpolated color
        float t = (float)i / rc.h;                                     // interpolation factor: 0=top, 1=bottom
        Uint8 cr = (Uint8)(top.r + (bottom.r - top.r) * t);            // interpolated red channel
        Uint8 cg = (Uint8)(top.g + (bottom.g - top.g) * t);            // interpolated green channel
        Uint8 cb = (Uint8)(top.b + (bottom.b - top.b) * t);            // interpolated blue channel
        SDL_SetRenderDrawColor(r, cr, cg, cb, 255);                     // sets color for this row
        SDL_RenderDrawLine(r, rc.x, rc.y+i, rc.x+rc.w-1, rc.y+i);     // draws horizontal line across paddle
    }
    if (laserActive) {                                                  // draws red border when laser is active
        SDL_SetRenderDrawColor(r, 255, 100, 100, 255);                  // bright red border color
        SDL_RenderDrawRect(r, &rc);                                     // draws the outline
    }
}

// ─── BLOCK ────────────────────────────────────────────────────────────────────

Block::Block(int x, int y, int hp, SDL_Color color)       // constructs block at grid position
    : x(x), y(y), width(BLOCK_W), height(BLOCK_H)         // stores position and sets fixed dimensions from constants
    , hp(hp), maxHp(hp), color(color)                      // stores hit points and color
    , active(true)                                         // block starts alive
    , scoreValue(hp == -1 ? 0 : hp * 10)                  // indestructible blocks give no score; others give 10 pts per hp
{}

SDL_Rect Block::rect() const { return {x, y, width, height}; } // returns current bounding rectangle

void Block::hit() {                         // called when a ball or laser contacts this block
    if (hp == -1) return;                  // indestructible blocks cannot be damaged
    if (--hp <= 0) active = false;         // decrements HP; deactivates block when it reaches zero
}

void Block::draw(SDL_Renderer* r) const {                         // renders block with damage-dimming effect
    if (!active) return;                                          // skip destroyed blocks
    float brt = (maxHp == -1) ? 1.f : (float)hp / maxHp;        // brightness: 1.0 at full health, less when damaged
    SDL_Rect rc = rect();                                         // gets bounding rectangle
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);            // no blending for solid fill
    SDL_SetRenderDrawColor(r, (Uint8)(color.r*brt), (Uint8)(color.g*brt), (Uint8)(color.b*brt), 255); // dimmed color
    SDL_RenderFillRect(r, &rc);                                   // fills the block rectangle
    SDL_SetRenderDrawColor(r, 40, 40, 40, 255);                   // dark gray border
    SDL_RenderDrawRect(r, &rc);                                   // draws border outline
}

// ─── POWERUP ──────────────────────────────────────────────────────────────────

static SDL_Color s_puColor(PowerupType t) {                        // maps powerup type to pill fill color
    switch (t) {
        case PowerupType::Wide:      return {165,214,167,255};     // green for wide
        case PowerupType::Slow:      return {128,222,234,255};     // cyan for slow
        case PowerupType::Multiball: return {255,241,118,255};     // yellow for multi-ball
        case PowerupType::Life:      return {244,143,177,255};     // pink for extra life
        case PowerupType::Laser:     return {255,138,101,255};     // orange for laser
        default:                     return {255,255,255,255};     // white fallback
    }
}

static const char* s_puLabel(PowerupType t) {                      // maps powerup type to short text label
    switch (t) {
        case PowerupType::Wide:      return "WID";                 // three-char label for wide
        case PowerupType::Slow:      return "SLW";                 // three-char label for slow
        case PowerupType::Multiball: return " x3";                 // multi-ball label
        case PowerupType::Life:      return "+1";                  // extra life label
        case PowerupType::Laser:     return "LZR";                 // laser label
        default:                     return "?";                   // fallback
    }
}

Powerup::Powerup(float x, float y, PowerupType type)    // constructs powerup at given position
    : x(x), y(y), width(34), height(16)                 // sets position and pill dimensions
    , vy(POWERUP_SPEED), type(type), active(true)        // sets falling speed, type, and active flag
{}

SDL_Rect Powerup::rect() const { return {(int)x,(int)y,width,height}; } // returns bounding rectangle

void Powerup::update() {                                            // moves powerup downward each frame
    y += vy;                                                        // advances y by falling speed
    if (y > (float)CANVAS_H + 20.f) active = false;                // deactivates if it falls below the screen
}

void Powerup::draw(SDL_Renderer* r, TTF_Font* font) const {         // renders colored pill with label
    SDL_Rect rc = rect();                                           // gets bounding rectangle
    SDL_Color fc = s_puColor(type);                                 // gets the pill's fill color
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);              // no blending for solid fill
    SDL_SetRenderDrawColor(r, fc.r, fc.g, fc.b, 255);              // sets fill color
    SDL_RenderFillRect(r, &rc);                                     // fills the pill rectangle
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);                        // black border
    SDL_RenderDrawRect(r, &rc);                                     // draws pill outline
    if (font) {                                                     // renders label only if font is valid
        SDL_Color black = {0,0,0,255};                              // black text for contrast
        SDL_Surface* surf = TTF_RenderText_Blended(font, s_puLabel(type), black); // renders label to surface
        if (surf) {                                                 // proceeds if surface creation succeeded
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf); // uploads surface to GPU texture
            if (tex) {                                              // proceeds if texture creation succeeded
                int tw, th;                                         // texture dimensions
                SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);  // queries texture pixel size
                SDL_Rect dst = {rc.x+(rc.w-tw)/2, rc.y+(rc.h-th)/2, tw, th}; // centers label in pill
                SDL_RenderCopy(r, tex, nullptr, &dst);              // draws label texture
                SDL_DestroyTexture(tex);                            // frees GPU texture
            }
            SDL_FreeSurface(surf);                                  // frees CPU surface
        }
    }
}

// ─── LASER ────────────────────────────────────────────────────────────────────

Laser::Laser(float x, float y)                // constructs laser beam at given position
    : x(x), y(y), width(3), height(14)        // 3x14 pixel beam dimensions
    , vy(-LASER_SPEED), active(true)           // upward velocity; starts active
{}

SDL_Rect Laser::rect() const {                // returns bounding rect centered on x
    return {(int)x - width/2, (int)y, width, height}; // centered horizontally on x
}

void Laser::update() {                         // moves laser upward each frame
    y += vy;                                  // advances y upward (vy is negative)
    if (y + height < 0.f) active = false;     // deactivates when entire beam exits the top
}

void Laser::draw(SDL_Renderer* r) const {     // renders laser as bright red rectangle
    SDL_Rect rc = rect();                     // gets current bounding rect
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE); // no blending for solid fill
    SDL_SetRenderDrawColor(r, 255, 68, 68, 255);       // bright red color
    SDL_RenderFillRect(r, &rc);               // fills the laser beam
}

// ─── PARTICLE ─────────────────────────────────────────────────────────────────

Particle::Particle(float x, float y, SDL_Color color) // constructs particle at block center with random properties
    : x(x), y(y)                                       // initial position
    , vx(s_distVel(s_rng))                             // random horizontal velocity
    , vy(s_distVel(s_rng) - 2.f)                       // random velocity biased upward
    , size(s_distSize(s_rng))                          // random size 2–6 pixels
    , color(color)                                     // inherits block color
    , alpha(255)                                       // starts fully opaque
    , life(s_distLife(s_rng)), maxLife(life)           // random lifetime 300–500 ms; stored for fade calculation
    , active(true)                                     // particle starts alive
{}

void Particle::update(float dt) {                        // advances particle physics and fade each frame
    x += vx; y += vy;                                   // moves by velocity
    vy += 0.1f;                                         // applies gravity (increases downward speed)
    life -= dt;                                         // counts down lifetime
    alpha = std::max(0, (int)(255.f * life / maxLife)); // fades proportionally to remaining lifetime
    if (life <= 0.f) active = false;                    // marks particle for removal when lifetime expires
}

void Particle::draw(SDL_Renderer* r) const {                        // renders small colored square at current opacity
    if (alpha <= 0) return;                                          // skip invisible particles
    int s = (int)size;                                               // integer size for SDL_Rect
    SDL_Rect rc = {(int)x - s/2, (int)y - s/2, s, s};              // square rect centered on particle position
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);             // enables alpha blending for fade effect
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, (Uint8)alpha); // sets color with current opacity
    SDL_RenderFillRect(r, &rc);                                      // fills the particle square
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);              // restores blend mode
}

// ─── GAME — CONSTRUCTION / DESTRUCTION ────────────────────────────────────────

Game::Game()                                         // initializes SDL, creates window/renderer, loads fonts and high score
    : window(nullptr), renderer(nullptr)             // SDL handles start null; assigned below
    , fontLarge(nullptr), fontMed(nullptr)           // fonts start null; assigned below
    , fontSmall(nullptr), fontTiny(nullptr)          // remaining fonts start null
    , highScore(0)                                   // high score starts at zero; overwritten by loadHighScore()
    , state(GameState::Title)                        // game begins on the title screen
    , level(1), lives(LIVES_START), score(0)         // gameplay counters start at initial values
    , paddle()                                       // constructs paddle at default position
    , stateTimer(0.f), shakeTimer(0.f)               // timers start at zero
    , blinkTimer(0.f), titleAnimTimer(0.f)           // animation timers start at zero
    , slowActive(false), slowTimer(0.f)              // slow powerup not active at startup
    , rng(std::random_device{}())                    // seeds Mersenne-Twister from hardware entropy
    , dist01(0.f, 1.f)                               // uniform [0,1) distribution for probability rolls
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)               // initializes SDL2 video subsystem
        throw std::runtime_error(SDL_GetError());    // throws if SDL cannot initialize
    if (TTF_Init() != 0)                             // initializes SDL_ttf font library
        throw std::runtime_error(TTF_GetError());    // throws if TTF cannot initialize

    window = SDL_CreateWindow("BREAKOUT",            // creates the OS window with title "BREAKOUT"
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, // centers window on the display
        CANVAS_W, CANVAS_H, 0);                      // sets window size; no special flags
    if (!window) throw std::runtime_error(SDL_GetError()); // throws if window creation failed

    renderer = SDL_CreateRenderer(window, -1,        // creates hardware-accelerated renderer for the window
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); // GPU acceleration + vsync to cap at monitor refresh
    if (!renderer) throw std::runtime_error(SDL_GetError()); // throws if renderer creation failed

    fontLarge = TTF_OpenFont(FONT_PATH, FONT_SIZE_LARGE); // loads font at large size for titles
    fontMed   = TTF_OpenFont(FONT_PATH, FONT_SIZE_MED);   // loads font at medium size for state messages
    fontSmall = TTF_OpenFont(FONT_PATH, FONT_SIZE_SMALL); // loads font at small size for HUD
    fontTiny  = TTF_OpenFont(FONT_PATH, FONT_SIZE_TINY);  // loads font at tiny size for powerup labels
    // If FONT_PATH is not found, text won't render — update FONT_PATH in breakout.h for your OS

    highScore = loadHighScore();                          // reads best score from disk
    std::srand((unsigned)std::time(nullptr));             // seeds std::rand for screen-shake random offset
}

Game::~Game() {                                      // releases all SDL resources in reverse construction order
    if (fontTiny)  TTF_CloseFont(fontTiny);          // frees tiny font
    if (fontSmall) TTF_CloseFont(fontSmall);         // frees small font
    if (fontMed)   TTF_CloseFont(fontMed);           // frees medium font
    if (fontLarge) TTF_CloseFont(fontLarge);         // frees large font
    if (renderer)  SDL_DestroyRenderer(renderer);    // destroys renderer and GPU resources
    if (window)    SDL_DestroyWindow(window);        // closes the OS window
    TTF_Quit();                                      // shuts down SDL_ttf
    SDL_Quit();                                      // shuts down all SDL2 subsystems
}

// ─── GAME — MAIN LOOP ─────────────────────────────────────────────────────────

void Game::run() {                               // starts and runs the game loop until window is closed
    bool running = true;                         // loop control; set false by SDL_QUIT
    Uint32 lastTick = SDL_GetTicks();            // timestamp of the first frame

    while (running) {                            // main loop: poll → update → render → present
        Uint32 now = SDL_GetTicks();             // current timestamp in milliseconds
        float  dt  = (float)(now - lastTick);   // elapsed time since last frame in ms
        if (dt > MAX_DT) dt = MAX_DT;           // caps dt to prevent physics explosion on tab switch
        lastTick = now;                          // saves timestamp for next iteration

        SDL_Event ev;                            // event object for this iteration
        while (SDL_PollEvent(&ev)) {             // drains the SDL event queue
            if (ev.type == SDL_QUIT) running = false; // exits on window close or Alt+F4
            handleEvent(ev);                     // dispatches other events to the input handler
        }

        update(dt);                              // advances game logic by dt milliseconds
        render();                                // draws the current frame to the back buffer
        SDL_RenderPresent(renderer);             // swaps back buffer to screen
    }
}

// ─── GAME — GAME MANAGEMENT ───────────────────────────────────────────────────

void Game::resetGameVars() {                     // resets all per-session variables to starting values
    level = 1; lives = LIVES_START; score = 0;  // resets gameplay counters
    balls.clear(); blocks.clear();               // clears all entity lists
    powerups.clear(); lasers.clear(); particles.clear();
    slowActive = false; slowTimer = 0.f;         // clears slow powerup state
    stateTimer = 0.f; shakeTimer = 0.f;          // clears all timers
}

void Game::startNewGame() {                      // prepares all entities and transitions to Playing
    resetGameVars();                             // resets all session variables
    paddle = Paddle();                           // creates a fresh centered paddle
    blocks = generateLevel(level);              // generates block layout for level 1
    createBallOnPaddle();                       // places ball on paddle ready for launch
    state = GameState::Playing;                 // transitions to gameplay state
}

void Game::createBallOnPaddle() {               // adds a new ball glued to the paddle
    float spd = std::min(BALL_BASE_SPEED + (level-1)*BALL_SPEED_INC, BALL_MAX_SPEED); // speed scales with level
    Ball b(paddle.centerX(), (float)(paddle.y - BALL_RADIUS), 0.f, -spd, spd); // ball above paddle center
    b.onPaddle = true;                          // marks ball as attached to paddle
    balls.push_back(b);                         // adds to active list
}

void Game::resetAfterLifeLost() {               // prepares for play to resume after losing a life
    balls.clear(); powerups.clear(); lasers.clear(); // removes all balls, powerups, and lasers
    paddle.width = paddle.baseWidth;            // resets paddle to normal width
    paddle.wideTimer = 0.f; paddle.laserActive = false; paddle.laserTimer = 0.f; // clears powerup state
    slowActive = false; slowTimer = 0.f;        // clears slow effect
    createBallOnPaddle();                       // places fresh ball on the paddle
    state = GameState::Playing; stateTimer = 0.f; // transitions back to gameplay
}

void Game::loadNextLevel() {                    // advances game to the next level
    level++;                                    // increments level counter
    balls.clear(); powerups.clear(); lasers.clear(); particles.clear(); // clears all entities
    paddle.width = paddle.baseWidth;            // resets paddle width
    paddle.wideTimer = 0.f; paddle.laserActive = false; paddle.laserTimer = 0.f; // clears powerup state
    slowActive = false; slowTimer = 0.f;        // clears slow effect
    blocks = generateLevel(level);             // generates new block layout
    createBallOnPaddle();                      // places fresh ball on paddle
    state = GameState::Playing; stateTimer = 0.f; // transitions to gameplay
}

// ─── GAME — LEVEL GENERATION ──────────────────────────────────────────────────

std::vector<Block> Game::generateLevel(int lvl) {   // builds randomized block grid for the given level
    std::vector<Block> result;                       // will hold all created blocks
    int rows = std::min(BLOCK_ROWS_BASE + lvl/2, BLOCK_ROWS_MAX); // more rows every 2 levels, capped at max
    for (int r = 0; r < rows; r++) {                // iterates each block row
        for (int c = 0; c < BLOCK_COLS; c++) {      // iterates each block column
            if (dist01(rng) < BLOCK_SKIP_CHANCE) continue; // ~15% of cells are left empty
            int x = BLOCK_OFFSET_LEFT + c * (BLOCK_W + BLOCK_PAD); // block left edge
            int y = BLOCK_OFFSET_TOP  + r * (BLOCK_H + BLOCK_PAD); // block top edge
            float roll = dist01(rng);               // random roll determines block type
            int hp; SDL_Color color;
            if (lvl >= 6 && roll < 0.10f) {         // 10% indestructible from level 6
                hp = -1; color = COL_INDESTR;        // gray indestructible block
            } else if (lvl >= 3 && roll < 0.30f) {  // 30% two-hit blocks from level 3
                hp = 2;  color = COL_HP2;            // red-pink tough block
            } else {                                 // default: normal single-hit block
                hp = 1;
                int ci = (int)(dist01(rng) * NORMAL_COLOR_COUNT); // random color index
                color = NORMAL_COLORS[ci % NORMAL_COLOR_COUNT];   // color from palette
            }
            result.emplace_back(x, y, hp, color);   // adds block to result list
        }
    }
    return result;                                   // returns completed block list
}

PowerupType Game::randomPowerupType() {              // selects a powerup type using weighted probability
    constexpr float w[] = {30.f,25.f,20.f,15.f,10.f}; // Wide, Slow, Multiball, Laser, Life weights
    float total = 0.f;
    for (float wi : w) total += wi;                 // sums all weights
    float roll = dist01(rng) * total;               // random value within total range
    for (int i = 0; i < 5; i++) {                   // iterates weights
        roll -= w[i];                               // subtracts each weight
        if (roll <= 0.f) return (PowerupType)i;    // returns type when roll hits zero
    }
    return PowerupType::Wide;                        // fallback (never reached)
}

// ─── GAME — COLLISION ─────────────────────────────────────────────────────────

void Game::ballPaddleCollision(Ball& ball) {                         // resolves ball bouncing off the paddle
    if (ball.y + ball.radius < paddle.y)              return;        // ball is above paddle; no collision
    if (ball.y - ball.radius > paddle.y + paddle.height) return;    // ball is below paddle; no collision
    if (ball.x + ball.radius < paddle.x)              return;       // ball is left of paddle; no collision
    if (ball.x - ball.radius > paddle.x + paddle.width)  return;   // ball is right of paddle; no collision
    if (ball.vy < 0.f) return;                                       // ball moving upward already; skip

    ball.y = (float)(paddle.y - ball.radius);                        // snaps ball above paddle surface
    float hit = (ball.x - paddle.centerX()) / (paddle.width / 2.f); // hit position: -1 (left edge) to +1 (right edge)
    hit = std::max(-0.95f, std::min(0.95f, hit));                    // clamps to avoid nearly-horizontal angles
    float angle = hit * (3.14159f / 3.f);                            // maps to ±60 degree launch angle
    ball.vx = ball.speed * std::sin(angle);                          // horizontal component from angle
    ball.vy = -ball.speed * std::cos(angle);                         // vertical component upward
}

bool Game::ballBlockCollision(Ball& ball, Block& block) {            // AABB test and resolve; returns true if hit
    if (!block.active) return false;                                 // skip destroyed blocks
    float bL = ball.x - ball.radius, bR = ball.x + ball.radius;    // ball left and right edges
    float bT = ball.y - ball.radius, bB = ball.y + ball.radius;    // ball top and bottom edges
    if (bR < block.x || bL > block.x+block.width)  return false;   // no horizontal overlap
    if (bB < block.y || bT > block.y+block.height) return false;   // no vertical overlap

    float oL = bR - block.x,           oR = (block.x+block.width)  - bL; // penetration from left/right face
    float oT = bB - block.y,           oB = (block.y+block.height) - bT; // penetration from top/bottom face
    if (std::min(oL,oR) < std::min(oT,oB)) ball.vx *= -1.f;        // horizontal face hit: flip vx
    else                                    ball.vy *= -1.f;        // vertical face hit: flip vy

    block.hit();                                                     // damages the block
    if (!block.active) {                                             // if block was just destroyed
        score += block.scoreValue;                                   // awards points
        for (int i = 0; i < 8; i++)                                  // spawns 8 particles
            particles.emplace_back((float)(block.x+block.width/2), (float)(block.y+block.height/2), block.color);
        if (dist01(rng) < POWERUP_CHANCE)                            // 18% chance to drop a powerup
            powerups.emplace_back((float)(block.x+block.width/2-17), (float)block.y, randomPowerupType());
    }
    return true;                                                     // signals collision occurred
}

bool Game::laserBlockCollision(Laser& laser, Block& block) {         // AABB test for laser vs block
    if (!block.active || !laser.active) return false;                // skip inactive entities
    SDL_Rect lr = laser.rect(), br = block.rect();                   // get bounding rects
    if (!SDL_HasIntersection(&lr, &br)) return false;                // no overlap; no collision
    laser.active = false;                                            // destroys laser on contact
    block.hit();                                                     // damages block
    if (!block.active) {                                             // if block was just destroyed
        score += block.scoreValue;                                   // awards points
        for (int i = 0; i < 6; i++)                                  // spawns 6 particles
            particles.emplace_back((float)(block.x+block.width/2), (float)(block.y+block.height/2), block.color);
        if (dist01(rng) < POWERUP_CHANCE)                            // 18% chance powerup drop
            powerups.emplace_back((float)(block.x+block.width/2-17), (float)block.y, randomPowerupType());
    }
    return true;                                                     // signals collision occurred
}

void Game::collectPowerup(Powerup& pu) {                             // applies powerup effect and deactivates pill
    pu.active = false;                                               // marks pill as consumed
    switch (pu.type) {
        case PowerupType::Wide:      paddle.activateWide();   break; // expands paddle
        case PowerupType::Slow:                                       // slows all balls
            if (!slowActive) for (auto& b : balls) { b.speed *= SLOW_FACTOR; b.normalizeSpeed(); }
            slowActive = true; slowTimer = SLOW_DURATION_MS;  break; // starts slow timer
        case PowerupType::Multiball: {                                // spawns two extra balls from first flying ball
            Ball* src = nullptr;
            for (auto& b : balls) if (!b.onPaddle && b.active) { src = &b; break; }
            if (src) {
                Ball b1(src->x,src->y,src->vx+2.f,src->vy,src->speed); b1.onPaddle=false; b1.normalizeSpeed();
                Ball b2(src->x,src->y,src->vx-2.f,src->vy,src->speed); b2.onPaddle=false; b2.normalizeSpeed();
                balls.push_back(b1); balls.push_back(b2);             // adds both extra balls
            }
            break;
        }
        case PowerupType::Life:      lives++;                  break; // grants one extra life
        case PowerupType::Laser:     paddle.activateLaser();  break; // enables laser firing
    }
}

// ─── GAME — UPDATE ────────────────────────────────────────────────────────────

void Game::update(float dt) {                            // dispatches to per-state update function each frame
    blinkTimer += dt; titleAnimTimer += dt;              // always advance UI animation timers
    switch (state) {
        case GameState::Playing:       updatePlaying(dt);       break; // full gameplay update
        case GameState::LifeLost:      updateLifeLost(dt);      break; // pause screen countdown
        case GameState::LevelComplete: updateLevelComplete(dt); break; // pause screen countdown
        default: break;                                                 // other states need no per-frame logic
    }
}

void Game::updatePlaying(float dt) {                                       // full gameplay update for one frame
    stateTimer += dt;                                                      // advances state timer
    const Uint8* keys = SDL_GetKeyboardState(nullptr);                     // gets current keyboard state

    // Fire lasers: Space held + laser active + not on cooldown
    if (keys[SDL_SCANCODE_SPACE] && paddle.laserActive && paddle.laserCooldownTimer <= 0.f) {
        lasers.emplace_back(paddle.x + paddle.width*0.25f, (float)paddle.y - 2.f); // left laser
        lasers.emplace_back(paddle.x + paddle.width*0.75f, (float)paddle.y - 2.f); // right laser
        paddle.laserCooldownTimer = LASER_COOLDOWN_MS;                     // starts cooldown
    }

    paddle.update(dt, keys);                                               // moves paddle and ticks powerup timers

    for (auto& b : balls)     // snaps attached balls to paddle center
        if (b.onPaddle) { b.x = paddle.centerX(); b.y = (float)(paddle.y - b.radius); }

    for (auto& b : balls)     b.update();   // moves all balls
    for (auto& p : powerups)  p.update();   // falls all powerups
    for (auto& l : lasers)    l.update();   // moves all lasers upward
    for (auto& p : particles) p.update(dt); // advances all particle physics

    // Wall and ceiling collisions
    for (auto& ball : balls) {
        if (ball.onPaddle) continue;                                       // skip attached balls
        if (ball.x - ball.radius < 0.f)         { ball.x = (float)ball.radius;           ball.vx =  std::abs(ball.vx); } // left wall
        if (ball.x + ball.radius > (float)CANVAS_W) { ball.x = (float)(CANVAS_W-ball.radius); ball.vx = -std::abs(ball.vx); } // right wall
        if (ball.y - ball.radius < 0.f)         { ball.y = (float)ball.radius;           ball.vy =  std::abs(ball.vy); } // ceiling
        if (ball.y - ball.radius > (float)CANVAS_H) ball.active = false;  // fell below screen → lost
    }

    for (auto& ball : balls) if (!ball.onPaddle) ballPaddleCollision(ball); // ball vs paddle

    for (auto& ball : balls) {                                             // ball vs blocks (one block per ball per frame)
        if (ball.onPaddle) continue;
        for (auto& block : blocks) if (ballBlockCollision(ball, block)) break;
    }

    for (auto& laser : lasers)                                             // laser vs blocks (one block per laser)
        for (auto& block : blocks) if (laserBlockCollision(laser, block)) break;

    SDL_Rect pr = paddle.rect();                                           // paddle rect for powerup overlap test
    for (auto& pu : powerups)                                              // powerup collection by paddle
        if (pu.active) { SDL_Rect pr2 = pu.rect(); if (SDL_HasIntersection(&pr2,&pr)) collectPowerup(pu); }

    if (slowActive) {                                                      // tick slow-ball timer
        slowTimer -= dt;
        if (slowTimer <= 0.f) {                                            // slow expired: restore normal speed
            slowActive = false;
            float ns = std::min(BALL_BASE_SPEED+(level-1)*BALL_SPEED_INC, BALL_MAX_SPEED); // normal speed for this level
            for (auto& b : balls) { b.speed = ns; b.normalizeSpeed(); }   // restores all balls
        }
    }

    // Remove inactive entities (erase-remove idiom)
    auto inactive = [](const auto& e){ return !e.active; };               // lambda: true if entity is inactive
    balls.erase(    std::remove_if(balls.begin(),     balls.end(),     inactive), balls.end());
    powerups.erase( std::remove_if(powerups.begin(),  powerups.end(),  inactive), powerups.end());
    lasers.erase(   std::remove_if(lasers.begin(),    lasers.end(),    inactive), lasers.end());
    particles.erase(std::remove_if(particles.begin(), particles.end(), inactive), particles.end());
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(),          // remove destroyed non-indestructible blocks
        [](const Block& b){ return !b.active && b.hp != -1; }), blocks.end());

    if (balls.empty()) {                                                   // all balls lost
        lives--;                                                           // deducts a life
        if (lives <= 0) {                                                  // no lives remain
            if (score > highScore) { highScore = score; saveHighScore(highScore); } // saves new high score
            state = GameState::GameOver;                                   // transitions to game over
        } else {
            shakeTimer = SHAKE_DURATION_MS;                                // triggers screen shake
            state = GameState::LifeLost;                                   // transitions to life-lost pause
        }
        stateTimer = 0.f; return;                                          // resets timer and exits update
    }

    bool anyDestructible = std::any_of(blocks.begin(), blocks.end(),      // checks for remaining destructible blocks
        [](const Block& b){ return b.hp != -1; });
    if (!anyDestructible) {                                                // all destructible blocks cleared
        if (score > highScore) { highScore = score; saveHighScore(highScore); } // saves new high score
        state = GameState::LevelComplete; stateTimer = 0.f;               // transitions to level complete
    }
}

void Game::updateLifeLost(float dt) {                    // waits for player input or auto-advances after timeout
    stateTimer += dt;
    if (stateTimer > AUTO_ADVANCE_MS) resetAfterLifeLost(); // auto-resumes after 2.5 seconds
}

void Game::updateLevelComplete(float dt) {               // waits for player input or auto-advances after timeout
    stateTimer += dt;
    if (stateTimer > AUTO_ADVANCE_MS) loadNextLevel();   // auto-loads next level after 2.5 seconds
}

// ─── GAME — RENDER ────────────────────────────────────────────────────────────

void Game::render() {                                    // dispatches to per-state render function
    switch (state) {
        case GameState::Title:
            renderTitle(); break;
        case GameState::Playing:
            renderGame(); break;
        case GameState::LifeLost:
            renderGame(); renderOverlay(153);             // ~60% dark overlay over frozen game
            drawText("BALL LOST!", CANVAS_W/2, CANVAS_H/2-30, fontMed, COL_RED, true);
            drawText(std::to_string(lives)+(lives==1?" LIFE REMAINING":" LIVES REMAINING"),
                     CANVAS_W/2, CANVAS_H/2+10, fontSmall, COL_HUD, true);
            drawBlink("PRESS SPACE", CANVAS_H/2+55, fontSmall, COL_WHITE, 500.f);
            break;
        case GameState::LevelComplete:
            renderGame(); renderOverlay(165);             // ~65% dark overlay
            drawText("LEVEL "+std::to_string(level), CANVAS_W/2, CANVAS_H/2-40, fontMed, COL_GREEN, true);
            drawText("COMPLETE!",                    CANVAS_W/2, CANVAS_H/2-10, fontMed, COL_GREEN, true);
            drawText("SCORE: "+std::to_string(score),CANVAS_W/2, CANVAS_H/2+30, fontSmall, COL_WHITE, true);
            drawBlink("PRESS SPACE", CANVAS_H/2+65, fontSmall, COL_WHITE, 500.f);
            break;
        case GameState::GameOver:
            renderGame(); renderOverlay(191);             // ~75% dark overlay
            drawText("GAME OVER",                     CANVAS_W/2, CANVAS_H/2-30, fontLarge, COL_RED, true);
            drawText("SCORE: "+std::to_string(score), CANVAS_W/2, CANVAS_H/2+20, fontSmall, COL_HUD, true);
            if (score > 0 && score >= highScore)
                drawText("NEW BEST!", CANVAS_W/2, CANVAS_H/2+45, fontSmall, COL_GOLD, true); // new high score message
            drawBlink("PRESS SPACE", CANVAS_H/2+80, fontSmall, COL_WHITE, 500.f);
            break;
        case GameState::Paused:
            renderGame(); renderOverlay(127);             // 50% dark overlay
            drawText("PAUSED",        CANVAS_W/2, CANVAS_H/2-10, fontLarge, COL_WHITE, true);
            drawText("ESC TO RESUME", CANVAS_W/2, CANVAS_H/2+35, fontSmall, COL_HUD,   true);
            break;
    }
}

void Game::renderGame() {                                               // draws all live entities and HUD
    int ox = 0, oy = 0;                                                 // screen-shake offset, default zero
    if (shakeTimer > 0.f) {                                             // if shake is still active
        shakeTimer -= 16.f;                                             // decrements shake timer (~1 frame at 60fps)
        ox = (std::rand() % (SHAKE_MAGNITUDE*2+1)) - SHAKE_MAGNITUDE;  // random horizontal offset
        oy = (std::rand() % (SHAKE_MAGNITUDE*2+1)) - SHAKE_MAGNITUDE;  // random vertical offset
    }
    if (ox || oy) { SDL_Rect vp={ox,oy,CANVAS_W,CANVAS_H}; SDL_RenderSetViewport(renderer,&vp); } // shift viewport for shake

    SDL_SetRenderDrawColor(renderer, COL_BG.r, COL_BG.g, COL_BG.b, 255); // sets background color
    SDL_RenderClear(renderer);                                            // fills canvas with background

    for (auto& p : particles) p.draw(renderer);             // renders particles behind blocks
    for (auto& b : blocks)    b.draw(renderer);             // renders all block tiles
    for (auto& p : powerups)  p.draw(renderer, fontTiny);  // renders powerup pills with labels
    for (auto& l : lasers)    l.draw(renderer);             // renders laser beams
    paddle.draw(renderer);                                  // renders the paddle
    for (auto& b : balls)     b.draw(renderer);             // renders all balls

    if (ox || oy) SDL_RenderSetViewport(renderer, nullptr); // restores full-screen viewport after shake
    drawHUD();                                              // renders HUD on top (not affected by shake)
}

void Game::renderTitle() {                                               // draws animated title screen
    SDL_SetRenderDrawColor(renderer, COL_BG.r, COL_BG.g, COL_BG.b, 255); // background color
    SDL_RenderClear(renderer);                                           // clears canvas

    float t = titleAnimTimer / 1000.f;                                   // seconds elapsed for sine calculations
    for (int r = 0; r < 5; r++) {                                        // draws 5 rows of decorative animated blocks
        for (int c = 0; c < BLOCK_COLS; c++) {                           // draws 10 columns
            int bx = BLOCK_OFFSET_LEFT + c*(BLOCK_W+BLOCK_PAD);         // block x position
            int by = BLOCK_OFFSET_TOP + 20 + r*(BLOCK_H+BLOCK_PAD);    // block base y
            int drawY = by + (int)(std::sin(t + c*0.5f) * 6.f);         // sine-wave animated y offset
            Uint8 alpha = (Uint8)(255.f*(0.4f + 0.3f*std::sin(t*2.f+r+c))); // pulsing opacity
            SDL_Color col = NORMAL_COLORS[(r*BLOCK_COLS+c) % NORMAL_COLOR_COUNT]; // cycles color palette
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);   // enables blending for alpha
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, alpha); // sets pulsing color
            SDL_Rect rc = {bx, drawY, BLOCK_W, BLOCK_H};                 // block rect at animated position
            SDL_RenderFillRect(renderer, &rc);                            // fills decorative block
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);            // restores blend mode

    drawText("BREAKOUT", CANVAS_W/2, (int)(CANVAS_H*0.55f), fontLarge, {79,195,247,255}, true); // title in cyan
    drawBlink("PRESS SPACE TO START",     (int)(CANVAS_H*0.70f), fontSmall, COL_WHITE, 600.f);  // blinking prompt
    drawText("ARROW KEYS / A-D TO MOVE", CANVAS_W/2, (int)(CANVAS_H*0.78f), fontTiny, {120,120,120,255}, true); // hint
    drawText("SPACE TO LAUNCH BALL",     CANVAS_W/2, (int)(CANVAS_H*0.83f), fontTiny, {120,120,120,255}, true); // hint
    drawText("ESC TO PAUSE",             CANVAS_W/2, (int)(CANVAS_H*0.88f), fontTiny, {120,120,120,255}, true); // hint
    if (highScore > 0)
        drawText("BEST: "+std::to_string(highScore), CANVAS_W/2, (int)(CANVAS_H*0.93f), fontSmall, COL_GOLD, true);
}

void Game::renderOverlay(Uint8 alpha) {                                  // draws semi-transparent black overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);           // enables alpha blending
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);                    // black at specified opacity
    SDL_RenderFillRect(renderer, nullptr);                               // nullptr fills the entire rendering target
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);            // restores blend mode
}

void Game::drawHUD() {                                                   // renders score, level, and life hearts at top
    // Zero-padded score on the left
    std::string sc = std::to_string(score);
    while (sc.size() < 6) sc = "0" + sc;                                // pads score to 6 digits
    drawText("SCORE "+sc, 8, 6, fontSmall, COL_HUD, false);             // draws score left-aligned

    drawText("LVL "+std::to_string(level), CANVAS_W/2, 6, fontSmall, COL_HUD, true); // level centered

    for (int i = 0; i < lives; i++)                                      // draws one heart per remaining life
        drawText("\u2665", CANVAS_W-14-i*22, 6, fontSmall, COL_LIFE, false); // hearts from right edge inward

    if (highScore > 0)                                                   // dim gray best score below level
        drawText("BEST "+std::to_string(highScore), CANVAS_W/2, 22, fontTiny, {100,100,100,255}, true);
}

void Game::drawText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color, bool centered) const {
    if (!font || text.empty()) return;                                   // guard: skip if no font or empty string
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), color); // renders UTF-8 text to CPU surface
    if (!surf) return;                                                   // guard: skip if surface creation failed
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);    // uploads surface to GPU texture
    if (tex) {                                                           // proceeds if texture creation succeeded
        int tw, th;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);               // queries texture pixel dimensions
        int drawX = centered ? x - tw/2 : x;                            // offsets x for centered alignment
        SDL_Rect dst = {drawX, y, tw, th};                               // destination rectangle on screen
        SDL_RenderCopy(renderer, tex, nullptr, &dst);                    // draws text texture
        SDL_DestroyTexture(tex);                                         // frees GPU texture
    }
    SDL_FreeSurface(surf);                                               // frees CPU surface
}

void Game::drawBlink(const std::string& text, int y, TTF_Font* font, SDL_Color color, float periodMs) {
    if (blinkOn(periodMs)) drawText(text, CANVAS_W/2, y, font, color, true); // draws only during visible blink phase
}

bool Game::blinkOn(float periodMs) const {                               // true during the visible half of each blink period
    return ((int)(blinkTimer / periodMs) % 2) == 0;                     // even periods = visible, odd = invisible
}

// ─── GAME — PERSISTENCE ───────────────────────────────────────────────────────

int Game::loadHighScore() const {                        // reads saved high score from disk
    std::ifstream f(HS_FILE);                            // opens high score file
    if (!f.is_open()) return 0;                          // returns 0 if file doesn't exist
    int hs = 0; f >> hs; return hs;                     // reads and returns the integer value
}

void Game::saveHighScore(int s) const {                  // writes score to disk
    std::ofstream f(HS_FILE);                            // opens (or creates) the high score file
    if (f.is_open()) f << s;                             // writes score as plain integer
}

// ─── GAME — EVENT HANDLING ────────────────────────────────────────────────────

void Game::handleEvent(const SDL_Event& ev) {            // processes a single SDL input event
    if (ev.type != SDL_KEYDOWN) return;                  // only handles key-press events
    SDL_Keycode key = ev.key.keysym.sym;                 // extracts virtual key code

    if (key == SDLK_SPACE) {                             // Space bar pressed
        switch (state) {
            case GameState::Title:         startNewGame();       break; // starts a new game from title
            case GameState::Playing:                                     // during gameplay: launch ball (laser fires via held key)
                if (!paddle.laserActive)                                 // only launches if laser not active
                    for (auto& b : balls) if (b.onPaddle) { b.onPaddle=false; b.vx=0.f; b.vy=-b.speed; } // launches attached balls
                break;
            case GameState::LifeLost:      resetAfterLifeLost(); break; // resumes immediately
            case GameState::LevelComplete: loadNextLevel();      break; // advances immediately
            case GameState::GameOver:      state=GameState::Title; blinkTimer=0.f; break; // returns to title
            default: break;
        }
    }

    if (key == SDLK_ESCAPE) {                            // Escape key pressed
        if      (state == GameState::Playing) state = GameState::Paused;  // pauses game
        else if (state == GameState::Paused)  { state = GameState::Playing; stateTimer = 0.f; } // resumes game
    }
}
