// simulation.cpp — 1/8192 Probability Visual Simulation (C++ / SDL2)
// Demonstrates the 1-in-8192 probability event through 5 interactive visualizations.
// Controls: LEFT/RIGHT or 1-5 = cycle mode | UP/DOWN = speed | R = reset | ESC/Q = quit

#include <SDL2/SDL.h>              // SDL2 core: window, renderer, events
#include <SDL2/SDL_ttf.h>          // SDL2_ttf: TrueType font rendering
#include <algorithm>               // std::min, std::max, std::max_element
#include <cmath>                   // std::pow, std::floor
#include <cstring>                 // std::string helpers (via string)
#include <deque>                   // std::deque: rolling history window
#include <random>                  // std::mt19937, std::uniform_int_distribution
#include <set>                     // std::set: epoch hit cells
#include <sstream>                 // std::ostringstream: number formatting
#include <string>                  // std::string
#include <vector>                  // std::vector: hit history

// ── Constants ──────────────────────────────────────────────────────────────────
static const int    ODDS       = 8192;    // event fires with probability 1/ODDS
static const int    WIN_W      = 800;     // window width in pixels
static const int    WIN_H      = 600;     // window height in pixels
static const int    FPS        = 60;      // target frames per second
static const int    MAX_HIST   = 2000;    // max data points for convergence graph
static const int    N_BUCKETS  = 30;      // number of bars in the histogram
static const int    MIN_SPD    = 1;       // minimum simulation speed (trials/frame)
static const int    MAX_SPD    = 100000;  // maximum simulation speed (trials/frame)
static const int    GRID_COLS  = 128;     // grid columns (128 × 64 = 8192)
static const int    GRID_ROWS  = 64;      // grid rows

// ── Colors (helper to build SDL_Color inline) ─────────────────────────────────
static SDL_Color mkc(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {  // construct an SDL_Color
    SDL_Color c; c.r = r; c.g = g; c.b = b; c.a = a; return c;   // fill and return
}
// Named color constants used throughout the draw functions
static const SDL_Color C_BG     = mkc( 10,  10,  20);  // deep navy background
static const SDL_Color C_WHITE  = mkc(255, 255, 255);  // white text
static const SDL_Color C_GRAY   = mkc(120, 120, 140);  // gray labels
static const SDL_Color C_DARK   = mkc( 25,  25,  45);  // dark navy (unvisited cells)
static const SDL_Color C_GREEN  = mkc( 50, 220,  80);  // green: at/above expected
static const SDL_Color C_RED    = mkc(220,  60,  60);  // red: below expected
static const SDL_Color C_GOLD   = mkc(255, 200,   0);  // gold: hit highlight
static const SDL_Color C_LBLUE  = mkc( 80, 140, 255);  // light blue: graph line
static const SDL_Color C_ORANGE = mkc(255, 140,   0);  // orange: theoretical PMF
static const SDL_Color C_TEAL   = mkc( 40, 200, 180);  // teal: reference line

// ── Mode names ─────────────────────────────────────────────────────────────────
static const char* MODE_NAMES[] = {       // display names for each visualization mode
    "Stats Dashboard",                     // mode 0: numeric overview
    "8192 Grid",                           // mode 1: epoch cell grid
    "Convergence Graph",                   // mode 2: running rate vs theory
    "Geometric Histogram",                 // mode 3: wait-time distribution
    "Live Roller",                         // mode 4: slow-motion roll view
};
static const int NUM_MODES = 5;           // total number of modes

// ── Random number generator ────────────────────────────────────────────────────
static std::mt19937                                rng(std::random_device{}());  // Mersenne Twister seeded from hardware entropy
static std::uniform_int_distribution<int>         dist(0, ODDS - 1);            // uniform distribution over [0, ODDS-1]

// ── Simulation state (file-scope globals) ─────────────────────────────────────
static long long         totalTrials  = 0;     // cumulative number of trials executed
static long long         totalHits    = 0;     // cumulative number of successful rolls (roll == 0)
static long long         streak       = 0;     // trials since the most recent hit
static std::vector<int>  hitHistory;           // recorded wait-times (one entry per hit)
static std::deque<double> rateHistory;         // rolling window of observed hit-rate values
static int               lastRoll     = 0;     // value of the most recently simulated roll
static int               flashTimer   = 0;     // frames remaining in the hit-flash animation
static int               epoch        = 0;     // index of the current 8192-trial epoch
static std::set<int>     epochHits;            // grid cell indices that were hits this epoch

// ── UI state ───────────────────────────────────────────────────────────────────
static int curMode = 0;    // index of the active visualization (0–4)
static int speed   = 500;  // trials to simulate per frame (adjustable by user)

// ── Fonts (loaded once in main, used in draw functions) ───────────────────────
static TTF_Font* fontBig = nullptr;  // 72pt: large roll number in Live Roller
static TTF_Font* fontMed = nullptr;  // 26pt: section headers and stats
static TTF_Font* fontSm  = nullptr;  // 15pt: labels and hints


// ── Number formatting helper ───────────────────────────────────────────────────

// Format an integer with thousands separators (e.g. 8192 → "8,192")
static std::string fmtInt(long long n) {
    std::string s   = std::to_string(n < 0 ? -n : n);  // convert absolute value to string
    std::string out;                                      // build result here
    int start = (int)s.size() % 3;                       // how many digits in the first group
    if (start > 0) out += s.substr(0, start);            // prepend first group (may be < 3 digits)
    for (int i = start; i < (int)s.size(); i += 3) {    // iterate remaining groups of 3
        if (!out.empty()) out += ',';                     // add comma separator before each group
        out += s.substr(i, 3);                            // append this 3-digit group
    }
    if (n < 0) out = "-" + out;                          // re-add minus sign if negative
    return out;                                           // return the formatted string
}

// Format a double with a fixed number of decimal places
static std::string fmtDbl(double v, int decimals = 8) {
    std::ostringstream ss;                   // use a string stream for formatting
    ss.precision(decimals);                  // set decimal precision
    ss << std::fixed << v;                   // write fixed-point decimal
    return ss.str();                         // return formatted string
}


// ── Simulation functions ───────────────────────────────────────────────────────

// Reset all simulation state to initial zero values
static void resetState() {
    totalTrials = 0;                         // clear trial counter
    totalHits   = 0;                         // clear hit counter
    streak      = 0;                         // clear current streak
    hitHistory.clear();                      // clear all recorded wait-times
    rateHistory.clear();                     // clear rolling rate window
    lastRoll    = 0;                         // clear the displayed roll value
    flashTimer  = 0;                         // cancel any active flash
    epoch       = 0;                         // restart at epoch zero
    epochHits.clear();                       // clear grid hit markers
}

// Simulate n trials and update all global state
static void simulate(int n) {
    for (int i = 0; i < n; ++i) {                       // execute each of the n trials
        int roll  = dist(rng);                            // draw a uniform random roll in [0, ODDS-1]
        lastRoll  = roll;                                 // store for Live Roller display
        int cell  = (int)(totalTrials % ODDS);           // grid cell index within this epoch
        ++totalTrials;                                    // increment total trial counter
        ++streak;                                         // extend current no-hit streak
        if (roll == 0) {                                  // success: the 1/8192 event fired
            ++totalHits;                                  // count this hit
            hitHistory.push_back((int)streak);            // record the wait-time
            streak = 0;                                   // reset streak counter
            epochHits.insert(cell);                       // mark this grid cell as a hit
            flashTimer = FPS / 2;                         // trigger a 30-frame hit flash
        }
        if (totalTrials % ODDS == 0) {                   // completed a full 8192-trial epoch
            ++epoch;                                      // advance epoch index
            epochHits.clear();                            // clear hit markers for new epoch
        }
        double rate = (totalTrials > 0)                  // compute observed hit rate
                        ? (double)totalHits / (double)totalTrials
                        : 0.0;
        if ((int)rateHistory.size() >= MAX_HIST)         // enforce max history length
            rateHistory.pop_front();                      // remove oldest entry
        rateHistory.push_back(rate);                     // append latest rate snapshot
    }
}


// ── Rendering helpers ──────────────────────────────────────────────────────────

// Set the renderer draw color from an SDL_Color
static void setColor(SDL_Renderer* r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);  // set RGBA draw color
}

// Fill a rectangle with a specific color
static void fillRect(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c) {
    setColor(r, c);                            // apply fill color
    SDL_Rect rect = {x, y, w, h};             // build the SDL_Rect
    SDL_RenderFillRect(r, &rect);              // fill the rectangle
}

// Draw a rectangle outline with a specific color
static void drawRect(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c) {
    setColor(r, c);                            // apply outline color
    SDL_Rect rect = {x, y, w, h};             // build the SDL_Rect
    SDL_RenderDrawRect(r, &rect);              // draw the outline
}

// Draw a single line segment
static void drawLine(SDL_Renderer* r, int x1, int y1, int x2, int y2, SDL_Color c) {
    setColor(r, c);                            // apply line color
    SDL_RenderDrawLine(r, x1, y1, x2, y2);    // draw the line
}

// Draw a horizontal dashed line from (x1,y) to (x2,y)
static void drawDashedH(SDL_Renderer* r, int x1, int x2, int y, SDL_Color c, int dash = 8) {
    setColor(r, c);                            // apply dash color
    for (int x = x1; x < x2; x += dash * 2) {           // advance by dash + gap (equal width)
        int end = std::min(x + dash, x2);                // end of this segment (clamped)
        SDL_RenderDrawLine(r, x, y, end, y);             // draw one dash segment
    }
}

// Render a text string onto the renderer at (x, y); if cx=true, center on x
static void renderText(SDL_Renderer* r, TTF_Font* f,
                        const std::string& text, int x, int y,
                        SDL_Color color, bool cx = false) {
    if (!f || text.empty()) return;                          // guard: skip if no font or empty string
    SDL_Surface* surf = TTF_RenderText_Blended(f, text.c_str(), color);  // render to surface
    if (!surf) return;                                       // guard: skip on render failure
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);  // upload surface to GPU texture
    SDL_Rect dst = {x, y, surf->w, surf->h};                // destination rectangle
    if (cx) dst.x = x - surf->w / 2;                        // center-align: shift left by half width
    SDL_FreeSurface(surf);                                   // release the CPU surface
    if (tex) {                                               // only blit if texture creation succeeded
        SDL_RenderCopy(r, tex, nullptr, &dst);               // blit texture onto the renderer
        SDL_DestroyTexture(tex);                             // release the GPU texture
    }
}

// Draw a filled progress bar with a border and a percentage label inside
static void drawBar(SDL_Renderer* r, int x, int y, int w, int h,
                    double fillFrac, SDL_Color fillCol, SDL_Color borderCol,
                    TTF_Font* labelFont, const std::string& label) {
    fillRect(r, x, y, w, h, C_DARK);                        // draw the empty track
    int filled = (int)(w * std::min(std::max(fillFrac, 0.0), 1.0));  // clamped fill width
    if (filled > 0) fillRect(r, x, y, filled, h, fillCol);  // draw filled portion
    drawRect(r, x, y, w, h, borderCol);                     // draw border over fill
    if (!label.empty()) renderText(r, labelFont, label, x + w / 2, y + 2, C_WHITE, true);  // centered label
}


// ── Mode 0: Stats Dashboard ────────────────────────────────────────────────────

static void drawStats(SDL_Renderer* r) {
    double p      = 1.0 / ODDS;                                            // theoretical probability per trial
    double actual = (totalTrials > 0) ? (double)totalHits / totalTrials : 0.0;  // observed rate
    double cum    = (totalTrials > 0)                                      // cumulative P(≥1 hit)
                      ? 1.0 - std::pow(1.0 - p, (double)totalTrials)
                      : 0.0;

    renderText(r, fontMed, "Stats Dashboard", WIN_W / 2, 10, C_WHITE, true);  // title

    // ── Main counters ──────────────────────────────────────────────────────────
    renderText(r, fontMed, "Total Trials:    " + fmtInt(totalTrials), 70,  75, C_WHITE);  // trial count
    renderText(r, fontMed, "Total Hits:      " + fmtInt(totalHits),   70, 113, C_GREEN);  // hit count
    renderText(r, fontMed, "Current Streak:  " + fmtInt(streak),      70, 151, C_GRAY);   // streak

    // ── Rate comparison ────────────────────────────────────────────────────────
    renderText(r, fontMed, "Expected Rate:   1/" + std::to_string(ODDS)
               + "  =  " + fmtDbl(p), 70, 205, C_TEAL);                           // theoretical rate
    SDL_Color rateCol = (actual >= p) ? C_GREEN : C_RED;                           // above/below color
    renderText(r, fontMed, "Actual Rate:         " + fmtDbl(actual), 70, 243, rateCol);  // observed rate

    // ── Cumulative probability bar ─────────────────────────────────────────────
    renderText(r, fontSm, "Probability of at least 1 hit by now:", 70, 300, C_GRAY);  // bar label
    std::string pctLabel = fmtDbl(cum * 100.0, 4) + "%";                              // percentage text
    drawBar(r, 70, 325, 660, 28, cum, C_TEAL, C_GRAY, fontSm, pctLabel);             // draw bar

    // ── Hit history summary ────────────────────────────────────────────────────
    if (!hitHistory.empty()) {                              // only if any hits recorded
        double sum = 0;                                     // accumulator for average
        for (int w : hitHistory) sum += w;                  // sum all wait-times
        double avg = sum / hitHistory.size();               // compute average
        renderText(r, fontSm, "Avg wait: " + fmtDbl(avg, 1)
                   + "  (expected " + fmtInt(ODDS) + ")", 70, 390, C_GRAY);   // average wait
        renderText(r, fontSm, "Last hit after: " + fmtInt(hitHistory.back())
                   + " trials", 70, 418, C_GRAY);                               // most recent wait
        renderText(r, fontSm, "Hits recorded:  " + fmtInt((long long)hitHistory.size()),
                   70, 446, C_GRAY);                                             // hit count
    } else {                                                // no hits yet
        renderText(r, fontSm, "No hits recorded yet -- keep simulating!", 70, 390, C_GRAY);
    }
}


// ── Mode 1: 8192 Grid ─────────────────────────────────────────────────────────

static void drawGrid(SDL_Renderer* r) {
    renderText(r, fontMed,                                        // title with epoch number
        "8192 Grid  --  Epoch " + std::to_string(epoch + 1),
        WIN_W / 2, 8, C_WHITE, true);

    const int MAR    = 14;                                        // pixel margin around grid
    const int cell_w = (WIN_W - 2 * MAR) / GRID_COLS;            // cell pixel width (= 6)
    const int cell_h = (WIN_H - 52 - MAR) / GRID_ROWS;           // cell pixel height (≈ 8)
    const int off_x  = MAR;                                       // grid left edge
    const int off_y  = 44;                                        // grid top edge
    const int pos    = (int)(totalTrials % ODDS);                 // progress within current epoch

    for (int i = 0; i < ODDS; ++i) {                             // iterate all 8192 cells
        int col  = i % GRID_COLS;                                 // column of cell i
        int row  = i / GRID_COLS;                                 // row of cell i
        int cx   = off_x + col * cell_w + 1;                     // cell left pixel (with 1px gap)
        int cy   = off_y + row * cell_h + 1;                     // cell top pixel (with 1px gap)
        int cw   = cell_w - 1;                                    // cell width minus gap
        int ch   = cell_h - 1;                                    // cell height minus gap

        SDL_Color color;                                          // color for this cell
        if (epochHits.count(i)) color = C_GOLD;                  // gold: hit this epoch
        else if (i < pos)       color = mkc(40, 40, 80);         // dim blue: visited, no hit
        else                    color = C_DARK;                   // dark: not yet reached

        fillRect(r, cx, cy, cw, ch, color);                      // paint the cell
    }

    // ── Bottom status bar ──────────────────────────────────────────────────────
    std::string status = "Hits this epoch: " + std::to_string((int)epochHits.size())
                       + "   |   Progress: " + fmtInt(pos) + " / " + fmtInt(ODDS);
    renderText(r, fontSm, status, WIN_W / 2, WIN_H - 22, C_GRAY, true);  // bottom summary
}


// ── Mode 2: Convergence Graph ──────────────────────────────────────────────────

static void drawConvergence(SDL_Renderer* r) {
    renderText(r, fontMed, "Convergence Graph", WIN_W / 2, 8, C_WHITE, true);  // title

    const int PX = 60, PY = 45;                   // plot top-left corner
    const int PW = WIN_W - PX - 25;               // plot width
    const int PH = WIN_H - PY - 65;               // plot height
    const double p_theo = 1.0 / ODDS;             // theoretical probability (reference line)

    drawLine(r, PX, PY, PX, PY + PH, C_GRAY);         // y-axis
    drawLine(r, PX, PY + PH, PX + PW, PY + PH, C_GRAY); // x-axis
    renderText(r, fontSm, "Rate",   PX - 5,       PY - 16,    C_GRAY);  // y-axis label
    renderText(r, fontSm, "Time", PX + PW - 25, PY + PH + 5,  C_GRAY);  // x-axis label

    if (rateHistory.empty()) {                         // no data yet
        renderText(r, fontMed, "Simulating...", WIN_W / 2, WIN_H / 2, C_GRAY, true);
        return;
    }

    // ── Y-axis scale ────────────────────────────────────────────────────────────
    double y_max = *std::max_element(rateHistory.begin(), rateHistory.end());  // peak observed rate
    y_max = std::max(y_max, p_theo * 4.0);             // always show at least 4× theoretical
    double y_scl = (y_max > 0) ? (double)PH / y_max : 1.0;  // pixels per unit of rate

    // ── Dashed theoretical reference line ─────────────────────────────────────
    int ref_y = PY + PH - (int)(p_theo * y_scl);      // y pixel of the theoretical rate
    drawDashedH(r, PX, PX + PW, ref_y, C_TEAL, 8);    // dashed teal reference line
    renderText(r, fontSm, "1/" + std::to_string(ODDS), PX + PW + 2, ref_y - 7, C_TEAL);  // right label

    // ── Running rate line ──────────────────────────────────────────────────────
    int n = (int)rateHistory.size();                   // number of data points available
    if (n < 2) return;                                 // need at least 2 to draw a segment
    double x_step = (double)PW / (n - 1);             // horizontal pixels per step
    int prev_sx = -1, prev_sy = -1;                    // previous screen coordinates
    for (int i = 0; i < n; ++i) {                     // iterate each rate value
        double rate = rateHistory[i];                  // rate value at this index
        int sx = (int)(PX + i * x_step);              // screen x
        int sy = (int)(PY + PH - rate * y_scl);       // screen y (inverted: higher = up)
        sy = std::max(PY, std::min(PY + PH, sy));      // clamp to plot area
        if (prev_sx >= 0) {                            // connect to previous point
            drawLine(r, prev_sx, prev_sy, sx, sy, C_LBLUE);  // graph line segment
        }
        prev_sx = sx; prev_sy = sy;                    // advance previous point
    }

    // ── Current rate label ─────────────────────────────────────────────────────
    double cur = rateHistory.back();                           // most recent rate
    SDL_Color col = (cur >= p_theo) ? C_GREEN : C_RED;        // color based on comparison
    renderText(r, fontSm, "Now:      " + fmtDbl(cur),      PX + 4, PY + 4,  col);   // current
    renderText(r, fontSm, "Expected: " + fmtDbl(p_theo),   PX + 4, PY + 22, C_TEAL); // expected
}


// ── Mode 3: Geometric Histogram ───────────────────────────────────────────────

static void drawHistogram(SDL_Renderer* r) {
    renderText(r, fontMed, "Geometric Distribution Histogram", WIN_W / 2, 8, C_WHITE, true);

    if ((int)hitHistory.size() < 2) {                  // need multiple hits for a distribution
        renderText(r, fontMed,
            "Waiting for hits... (" + std::to_string(totalHits) + " so far, need 2+)",
            WIN_W / 2, WIN_H / 2, C_GRAY, true);
        return;
    }

    // ── Bucket the observed wait-times ─────────────────────────────────────────
    int mx_wait = *std::max_element(hitHistory.begin(), hitHistory.end());  // longest observed wait
    int bkt_max = std::max(mx_wait, ODDS * 3);         // x-axis covers at least 3× expected
    int bkt_sz  = std::max(1, bkt_max / N_BUCKETS);    // trials per bucket
    std::vector<int> counts(N_BUCKETS, 0);             // initialize all bucket counts to zero
    for (int w : hitHistory) {                         // assign each wait to a bucket
        int b = std::min(w / bkt_sz, N_BUCKETS - 1);  // bucket index (clamped)
        ++counts[b];                                    // increment this bucket
    }
    int max_ct = *std::max_element(counts.begin(), counts.end());  // tallest bar (y-scale)
    if (max_ct == 0) return;                           // nothing to draw

    // ── Plot area ──────────────────────────────────────────────────────────────
    const int PX = 60, PY = 45;                        // plot top-left
    const int PW = WIN_W - PX - 25;                    // plot width
    const int PH = WIN_H - PY - 70;                    // plot height
    const int bw = PW / N_BUCKETS;                     // pixel width per bar

    drawLine(r, PX, PY,      PX,       PY + PH, C_GRAY);  // y-axis
    drawLine(r, PX, PY + PH, PX + PW, PY + PH, C_GRAY);  // x-axis
    renderText(r, fontSm, "Freq",           PX - 5,       PY - 16,    C_GRAY);   // y label
    renderText(r, fontSm, "Trials to hit", PX + PW / 3,  PY + PH + 5, C_GRAY);  // x label

    // ── Draw observed bars ─────────────────────────────────────────────────────
    for (int i = 0; i < N_BUCKETS; ++i) {              // one bar per bucket
        int bh = (int)((double)counts[i] / max_ct * PH);  // bar height proportional to count
        int bx = PX + i * bw;                          // bar left edge
        int by = PY + PH - bh;                         // bar top (bottom-aligned)
        fillRect(r, bx + 1, by, bw - 2, bh, C_LBLUE); // observed bar in light blue
    }

    // ── Theoretical geometric PMF overlay ─────────────────────────────────────
    double p_hit  = 1.0 / ODDS;                        // success probability per trial
    int    n_hits = (int)hitHistory.size();             // total hits (for expected-count scaling)
    int    prev_x = -1, prev_y = -1;                   // previous PMF curve point
    for (int i = 0; i < N_BUCKETS; ++i) {              // one curve point per bucket
        double mid   = (i + 0.5) * bkt_sz;             // midpoint of this bucket in trial space
        double pmf   = std::pow(1.0 - p_hit, mid - 1) * p_hit;  // geometric PMF: P(X=mid)
        double exp_n = pmf * bkt_sz * n_hits;           // expected count in this bucket
        int    eph   = (int)(exp_n / max_ct * PH);      // pixel height for expected count
        int    pt_x  = PX + i * bw + bw / 2;           // x center of this bucket
        int    pt_y  = PY + PH - eph;                   // y of PMF curve point
        if (prev_x >= 0) {                              // connect to previous PMF point
            drawLine(r, prev_x, prev_y, pt_x, pt_y, C_ORANGE);  // orange curve segment
        }
        prev_x = pt_x; prev_y = pt_y;                  // advance previous point
    }

    // ── Legend ─────────────────────────────────────────────────────────────────
    fillRect(r, PX + 5, PY + 8,  12, 12, C_LBLUE);                         // blue swatch
    renderText(r, fontSm, "Observed",      PX + 22, PY + 6,  C_LBLUE);     // observed label
    fillRect(r, PX + 5, PY + 28, 12, 12, C_ORANGE);                        // orange swatch
    renderText(r, fontSm, "Geometric PMF", PX + 22, PY + 26, C_ORANGE);    // PMF label
    renderText(r, fontSm,                                                   // sample size
        std::to_string(n_hits) + " hits  |  bucket=" + fmtInt(bkt_sz) + " trials",
        PX + 5, PY + 48, C_GRAY);
}


// ── Mode 4: Live Roller ────────────────────────────────────────────────────────

static void drawRoller(SDL_Renderer* r) {
    // ── Hit flash: gold overlay fading over 30 frames ─────────────────────────
    if (flashTimer > 0) {                                     // flash is active
        Uint8 alpha = (Uint8)(180 * flashTimer / (FPS / 2)); // alpha decreases over time
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);   // enable alpha blending
        SDL_SetRenderDrawColor(r, 255, 200, 0, alpha);        // semi-transparent gold
        SDL_Rect full = {0, 0, WIN_W, WIN_H};                 // full-window rect
        SDL_RenderFillRect(r, &full);                         // fill with flash color
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);    // restore blending mode
    }

    renderText(r, fontMed, "Live Roller", WIN_W / 2, 14, C_WHITE, true);  // title

    // ── Large roll number ──────────────────────────────────────────────────────
    SDL_Color rollCol = (lastRoll == 0) ? C_GOLD : C_WHITE;               // gold on hit
    renderText(r, fontBig, fmtInt(lastRoll), WIN_W / 2, WIN_H / 2 - 80, rollCol, true);  // huge number
    renderText(r, fontSm, "(hit when roll = 0)", WIN_W / 2, WIN_H / 2 + 8, C_GRAY, true); // hint

    // ── Stats row ──────────────────────────────────────────────────────────────
    renderText(r, fontMed, "Trials since last hit:  " + fmtInt(streak),
               WIN_W / 2, WIN_H / 2 + 52, C_GRAY, true);                  // current streak
    renderText(r, fontSm,
        "Total Hits: " + fmtInt(totalHits) + "   |   Total Trials: " + fmtInt(totalTrials),
        WIN_W / 2, WIN_H / 2 + 90, C_GRAY, true);                         // totals

    // ── Recent hit list ────────────────────────────────────────────────────────
    renderText(r, fontSm, "Last 5 hits (trials waited):", 70, WIN_H - 145, C_GRAY);  // header
    int numRecent = (int)std::min((int)hitHistory.size(), 5);              // up to 5 entries
    for (int j = 0; j < numRecent; ++j) {                                 // newest first
        int idx  = (int)hitHistory.size() - 1 - j;                        // index from end
        std::string lbl = "  Hit #" + std::to_string(idx + 1)
                        + ":  after " + fmtInt(hitHistory[idx]) + " trials"; // formatted label
        renderText(r, fontSm, lbl, 70, WIN_H - 123 + j * 22, C_GOLD);    // draw in gold
    }

    // ── Speed note ─────────────────────────────────────────────────────────────
    int eff = std::min(speed, 10);                                         // capped speed for this mode
    renderText(r, fontSm,
        "Speed: " + std::to_string(eff) + "/frame (capped at 10 in this mode)",
        WIN_W / 2, WIN_H - 22, C_GRAY, true);                             // bottom note
}


// ── Persistent HUD ─────────────────────────────────────────────────────────────

static void drawHUD(SDL_Renderer* r) {
    renderText(r, fontSm,                                                        // top-left speed
        "Speed: " + fmtInt(speed) + "/frame", 5, 5, C_GRAY);
    renderText(r, fontSm,                                                        // bottom mode indicator
        "[ Mode " + std::to_string(curMode + 1) + "/5: " + MODE_NAMES[curMode] + " ]",
        WIN_W / 2, WIN_H - 36, C_GRAY, true);
    renderText(r, fontSm,                                                        // control hints
        "<> / 1-5: mode   |   UP/DN: speed   |   R: reset   |   ESC/Q: quit",
        WIN_W / 2, WIN_H - 18, mkc(70, 70, 90), true);
}


// ── Main ───────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char* /*argv*/[]) {
    // ── SDL2 initialization ────────────────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {             // initialize SDL2 video subsystem
        SDL_Log("SDL_Init failed: %s", SDL_GetError());  // log error if init fails
        return 1;                                    // return non-zero on failure
    }
    if (TTF_Init() != 0) {                           // initialize SDL2_ttf font subsystem
        SDL_Log("TTF_Init failed: %s", TTF_GetError());  // log error if init fails
        SDL_Quit();                                  // clean up SDL2 before exit
        return 1;                                    // return non-zero on failure
    }

    // ── Create window and renderer ─────────────────────────────────────────────
    SDL_Window* window = SDL_CreateWindow(           // create the display window
        "1/8192 Probability Simulation",             // window title
        SDL_WINDOWPOS_CENTERED,                      // center horizontally on screen
        SDL_WINDOWPOS_CENTERED,                      // center vertically on screen
        WIN_W, WIN_H,                                // window dimensions
        SDL_WINDOW_SHOWN);                           // make it immediately visible
    if (!window) {                                   // guard against window creation failure
        SDL_Log("CreateWindow failed: %s", SDL_GetError());
        TTF_Quit(); SDL_Quit(); return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(     // create the accelerated renderer
        window, -1,                                  // -1: use the first available driver
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);  // GPU-accelerated, vsync on
    if (!renderer) {                                 // guard against renderer creation failure
        SDL_Log("CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window); TTF_Quit(); SDL_Quit(); return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);  // start with blending disabled

    // ── Load fonts ─────────────────────────────────────────────────────────────
    // Try Consolas first (Windows), then fall back to Courier New
    const char* fontPaths[] = {
        "C:/Windows/Fonts/consola.ttf",   // Consolas (Windows default monospace)
        "C:/Windows/Fonts/cour.ttf",      // Courier New (universal fallback)
    };
    const char* fontPath = nullptr;       // will hold the path that loaded successfully
    for (const char* p : fontPaths) {     // try each font path in order
        FILE* f = fopen(p, "r");          // test if file exists
        if (f) { fclose(f); fontPath = p; break; }  // found it — stop searching
    }
    if (!fontPath) {                      // no font found
        SDL_Log("Could not find a system font. Please copy a .ttf to the executable directory.");
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); TTF_Quit(); SDL_Quit();
        return 1;
    }
    fontBig = TTF_OpenFont(fontPath, 72);  // load large font for Live Roller number
    fontMed = TTF_OpenFont(fontPath, 26);  // load medium font for headers and stats
    fontSm  = TTF_OpenFont(fontPath, 15);  // load small font for labels and hints
    if (!fontBig || !fontMed || !fontSm) { // guard against any font load failure
        SDL_Log("TTF_OpenFont failed: %s", TTF_GetError());
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); TTF_Quit(); SDL_Quit();
        return 1;
    }

    // ── Main loop ──────────────────────────────────────────────────────────────
    bool running = true;                  // loop control flag
    Uint32 frameStart;                    // timestamp at the start of each frame

    while (running) {                     // ── main loop ──
        frameStart = SDL_GetTicks();      // record frame start time for capping FPS

        // ── Events ──────────────────────────────────────────────────────────────
        SDL_Event ev;                                      // event struct reused each iteration
        while (SDL_PollEvent(&ev)) {                       // drain the event queue
            if (ev.type == SDL_QUIT) running = false;      // window close button

            else if (ev.type == SDL_KEYDOWN) {             // key pressed
                SDL_Keycode k = ev.key.keysym.sym;         // shorthand alias

                if (k == SDLK_ESCAPE || k == SDLK_q)      // ESC or Q: quit
                    running = false;

                else if (k == SDLK_r)                      // R: reset simulation
                    resetState();

                else if (k == SDLK_LEFT)                   // LEFT: previous mode
                    curMode = (curMode - 1 + NUM_MODES) % NUM_MODES;
                else if (k == SDLK_RIGHT)                  // RIGHT: next mode
                    curMode = (curMode + 1) % NUM_MODES;

                else if (k == SDLK_1) curMode = 0;         // 1: Stats Dashboard
                else if (k == SDLK_2) curMode = 1;         // 2: 8192 Grid
                else if (k == SDLK_3) curMode = 2;         // 3: Convergence Graph
                else if (k == SDLK_4) curMode = 3;         // 4: Geometric Histogram
                else if (k == SDLK_5) curMode = 4;         // 5: Live Roller

                else if (k == SDLK_UP)                     // UP: double the speed
                    speed = std::min(speed * 2, MAX_SPD);
                else if (k == SDLK_DOWN)                   // DOWN: halve the speed
                    speed = std::max(speed / 2, MIN_SPD);
            }
        }

        // ── Simulate ─────────────────────────────────────────────────────────────
        int n = (curMode == 4) ? std::min(speed, 10) : speed;  // Live Roller caps at 10/frame
        simulate(n);                                             // run this frame's trials

        if (flashTimer > 0) --flashTimer;                       // tick down the hit-flash

        // ── Draw ─────────────────────────────────────────────────────────────────
        SDL_SetRenderDrawColor(renderer, C_BG.r, C_BG.g, C_BG.b, 255);  // set background color
        SDL_RenderClear(renderer);                               // clear the screen

        switch (curMode) {                                       // dispatch to active mode
            case 0: drawStats(renderer);      break;            // mode 0: Stats Dashboard
            case 1: drawGrid(renderer);       break;            // mode 1: 8192 Grid
            case 2: drawConvergence(renderer); break;           // mode 2: Convergence Graph
            case 3: drawHistogram(renderer);  break;            // mode 3: Geometric Histogram
            case 4: drawRoller(renderer);     break;            // mode 4: Live Roller
        }
        drawHUD(renderer);                                       // overlay persistent HUD

        SDL_RenderPresent(renderer);                             // swap back-buffer to screen

        // ── Frame cap ─────────────────────────────────────────────────────────────
        Uint32 elapsed = SDL_GetTicks() - frameStart;           // ms used this frame
        Uint32 target  = 1000 / FPS;                            // ms per frame at target FPS
        if (elapsed < target)                                    // if we finished early
            SDL_Delay(target - elapsed);                         // sleep for remaining time
    }

    // ── Cleanup ────────────────────────────────────────────────────────────────
    TTF_CloseFont(fontBig);              // release large font
    TTF_CloseFont(fontMed);              // release medium font
    TTF_CloseFont(fontSm);              // release small font
    SDL_DestroyRenderer(renderer);      // release the renderer
    SDL_DestroyWindow(window);          // release the window
    TTF_Quit();                         // shut down SDL2_ttf
    SDL_Quit();                         // shut down SDL2
    return 0;                           // return success
}
