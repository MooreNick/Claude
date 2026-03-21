#include "Paddle.h"      // includes the Paddle class declaration
#include <algorithm>     // includes std::max and std::min for boundary clamping

Paddle::Paddle()                              // default constructor; initializes all paddle state
    : baseWidth(static_cast<float>(PADDLE_W)) // stores the original width as a float for comparison later
    , width(static_cast<float>(PADDLE_W))     // sets current width to the default paddle width
    , height(PADDLE_H)                        // sets fixed height from the constant
    , x(static_cast<float>((CANVAS_W - PADDLE_W) / 2)) // centers the paddle horizontally on the canvas
    , y(PADDLE_Y)                             // places the paddle at the fixed vertical position
    , speed(PADDLE_SPEED)                     // sets movement speed from the constant
    , wideTimer(0.f)                          // no wide powerup active at start
    , laserActive(false)                      // laser disabled at start
    , laserTimer(0.f)                         // no laser powerup active at start
    , laserCooldownTimer(0.f)                 // laser can fire immediately once activated
{}

float Paddle::centerX() const {               // returns the horizontal midpoint of the paddle
    return x + width / 2.0f;                  // left edge plus half the width gives the center
}

SDL_Rect Paddle::rect() const {               // returns an SDL_Rect representing the paddle's bounding box
    return SDL_Rect{                           // constructs the rect using current position and size
        static_cast<int>(x),                  // left edge (cast from float to int for SDL)
        y,                                    // top edge (already an int)
        static_cast<int>(width),              // current width (cast from float to int)
        height                                // fixed height
    };
}

void Paddle::update(float dt, const Uint8* keys) { // moves paddle and ticks powerup timers each frame
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) { // checks if left arrow or 'A' is held
        x -= speed;                                         // moves paddle leftward by one speed unit
    }
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) { // checks if right arrow or 'D' is held
        x += speed;                                          // moves paddle rightward by one speed unit
    }
    // clamp paddle so it stays fully within the canvas bounds
    x = std::max(0.0f, std::min(static_cast<float>(CANVAS_W) - width, x)); // prevents paddle from going off either edge

    if (wideTimer > 0.f) {             // if wide powerup is still counting down
        wideTimer -= dt;               // subtracts elapsed time from the wide duration
        if (wideTimer <= 0.f) {        // if the wide timer just expired
            width = baseWidth;         // restores paddle to its original width
        }
    }

    if (laserCooldownTimer > 0.f) {    // if laser is still on cooldown from the last shot
        laserCooldownTimer -= dt;      // subtracts elapsed time from the cooldown
    }

    if (laserTimer > 0.f) {            // if laser powerup is still active
        laserTimer -= dt;              // subtracts elapsed time from the laser duration
        if (laserTimer <= 0.f) {       // if the laser timer just expired
            laserActive = false;       // disables laser mode
        }
    }
}

void Paddle::activateWide() {                          // applies the wide-paddle powerup
    width = baseWidth * WIDE_FACTOR;                   // expands paddle to 180% of its original width
    wideTimer = WIDE_DURATION_MS;                      // starts the countdown timer
}

void Paddle::activateLaser() {                         // applies the laser powerup
    laserActive = true;                                // enables laser firing
    laserTimer  = LASER_DURATION_MS;                   // starts the countdown timer
}

void Paddle::draw(SDL_Renderer* renderer) const {      // renders the paddle as a vertically gradient-shaded rectangle
    SDL_Rect r = rect();                               // gets the current bounding rect

    // choose top and bottom gradient colors based on whether laser is active
    SDL_Color top    = laserActive ? SDL_Color{255, 138, 101, 255} : SDL_Color{220, 220, 220, 255}; // orange or light gray
    SDL_Color bottom = laserActive ? SDL_Color{191,  54,  12, 255} : SDL_Color{158, 158, 158, 255}; // dark orange or dark gray

    for (int i = 0; i < r.h; i++) {                   // iterates each horizontal pixel row of the paddle for gradient
        float t = static_cast<float>(i) / r.h;        // interpolation factor: 0.0 at top, 1.0 at bottom
        Uint8 cr = static_cast<Uint8>(top.r + (bottom.r - top.r) * t); // interpolates red channel
        Uint8 cg = static_cast<Uint8>(top.g + (bottom.g - top.g) * t); // interpolates green channel
        Uint8 cb = static_cast<Uint8>(top.b + (bottom.b - top.b) * t); // interpolates blue channel
        SDL_SetRenderDrawColor(renderer, cr, cg, cb, 255);              // sets the draw color for this row
        SDL_RenderDrawLine(renderer, r.x, r.y + i, r.x + r.w - 1, r.y + i); // draws a horizontal line across the paddle
    }

    if (laserActive) {                                  // if laser is active, draw a red outline glow
        SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255); // bright red border color
        SDL_RenderDrawRect(renderer, &r);               // draws the rectangle outline
    }
}
