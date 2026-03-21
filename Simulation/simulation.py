# simulation.py — 1/8192 Probability Visual Simulation (Python / pygame)
# Demonstrates the 1-in-8192 probability event through 5 interactive visualizations.
# Controls: LEFT/RIGHT or 1-5 = cycle mode | UP/DOWN = speed | R = reset | ESC/Q = quit

import pygame                    # main graphics and event library
import random                    # for simulating random rolls
import math                      # for math.pow, math.log
import sys                       # for sys.exit()
from collections import deque    # efficient fixed-length rolling window

# ── Constants ──────────────────────────────────────────────────────────────────
ODDS      = 8192          # the event occurs with probability 1 / ODDS
WIN_W     = 800           # window width in pixels
WIN_H     = 600           # window height in pixels
FPS       = 60            # target frames per second
MAX_HIST  = 2000          # maximum data points stored for the convergence graph
N_BUCKETS = 30            # number of bars in the geometric histogram
MIN_SPD   = 1             # slowest simulation speed (trials per frame)
MAX_SPD   = 100000        # fastest simulation speed (trials per frame)

# ── Colors (R, G, B) ───────────────────────────────────────────────────────────
BG     = ( 10,  10,  20)  # deep navy: main background
WHITE  = (255, 255, 255)  # white: primary text
GRAY   = (120, 120, 140)  # gray: secondary labels
DARK   = ( 25,  25,  45)  # dark navy: unvisited grid cells
GREEN  = ( 50, 220,  80)  # green: at-or-above-expected indicator
RED    = (220,  60,  60)  # red: below-expected indicator
GOLD   = (255, 200,   0)  # gold: hit highlight
LBLUE  = ( 80, 140, 255)  # light blue: convergence/histogram line
ORANGE = (255, 140,   0)  # orange: theoretical PMF curve
TEAL   = ( 40, 200, 180)  # teal: reference line

# ── Mode metadata ──────────────────────────────────────────────────────────────
MODES = [                              # (title, short description) per mode
    ("Stats Dashboard",     "Numeric overview of the simulation state"),
    ("8192 Grid",           "Cell grid — one cell per trial in the current epoch"),
    ("Convergence Graph",   "Running hit rate converging toward 1/8192"),
    ("Geometric Histogram", "Distribution of wait-times between hits"),
    ("Live Roller",         "Slow-motion view of individual rolls"),
]

# ── Simulation state (module-level globals) ────────────────────────────────────
total_trials = 0                        # cumulative number of trials executed
total_hits   = 0                        # cumulative number of successful rolls (roll == 0)
streak       = 0                        # trials elapsed since the most recent hit
hit_history  = []                       # list of wait-times (streak length per hit)
rate_history = deque(maxlen=MAX_HIST)   # rolling window of observed hit-rate values
last_roll    = 0                        # value of the most recent roll (0 – ODDS-1)
flash_timer  = 0                        # frames remaining in the current hit-flash
epoch        = 0                        # index of the current 8192-trial epoch
epoch_hits   = set()                    # grid cell indices that were hits this epoch

# ── UI state ───────────────────────────────────────────────────────────────────
cur_mode = 0    # index of the active visualization (0 – 4)
speed    = 500  # trials to simulate per frame (user-adjustable)


# ── Simulation functions ───────────────────────────────────────────────────────

def reset():
    """Reset all simulation state to initial zero values."""
    global total_trials, total_hits, streak, hit_history   # declare globals to modify
    global rate_history, last_roll, flash_timer, epoch, epoch_hits
    total_trials = 0                         # clear trial counter
    total_hits   = 0                         # clear hit counter
    streak       = 0                         # clear current no-hit streak
    hit_history  = []                        # clear recorded wait-times
    rate_history = deque(maxlen=MAX_HIST)    # reset the rolling rate window
    last_roll    = 0                         # clear displayed roll value
    flash_timer  = 0                         # cancel any active flash
    epoch        = 0                         # restart at epoch zero
    epoch_hits   = set()                     # clear grid hit markers


def simulate(n):
    """Execute n trials and update all global simulation state."""
    global total_trials, total_hits, streak, hit_history   # declare globals to write
    global rate_history, last_roll, flash_timer, epoch, epoch_hits
    for _ in range(n):                                    # run each of the n trials
        roll         = random.randint(0, ODDS - 1)        # uniform roll in [0, ODDS-1]
        last_roll    = roll                               # store for the Live Roller display
        cell         = total_trials % ODDS               # which grid cell within this epoch
        total_trials += 1                                # increment the global trial counter
        streak       += 1                                # extend the current no-hit streak
        if roll == 0:                                    # success: the 1/8192 event fired
            total_hits   += 1                            # count this hit
            hit_history.append(streak)                   # record the wait-time
            streak        = 0                            # reset the streak counter
            epoch_hits.add(cell)                         # mark this grid cell as a hit
            flash_timer   = FPS // 2                     # trigger a 30-frame hit flash
        if total_trials % ODDS == 0:                     # completed a full 8192-trial epoch
            epoch     += 1                               # advance the epoch counter
            epoch_hits = set()                           # clear hit markers for the new epoch
        rate_history.append(                             # record the current observed rate
            total_hits / total_trials                    # = hits / total (avoids div-by-zero after first trial)
        )


# ── Drawing helpers ────────────────────────────────────────────────────────────

def draw_text(surf, text, x, y, font, color=WHITE, cx=False):
    """Render a text string onto surf; if cx=True, center horizontally at x."""
    img  = font.render(text, True, color)   # create the rendered text surface
    rect = img.get_rect()                   # get its bounding rectangle
    if cx:                                  # center-align mode
        rect.centerx = x                   # move center to x
    else:                                   # left-align mode
        rect.x = x                         # left edge at x
    rect.y = y                             # set the vertical position
    surf.blit(img, rect)                   # composite text onto the target surface


def draw_bar(surf, x, y, w, h, fill_frac, fill_col, border_col, label_font):
    """Draw a horizontal progress bar; fill_frac in [0,1]."""
    pygame.draw.rect(surf, DARK,       (x, y, w, h))                         # empty track
    filled = int(w * min(max(fill_frac, 0.0), 1.0))                          # clamp and scale
    if filled > 0:                                                            # draw fill only if non-zero
        pygame.draw.rect(surf, fill_col, (x, y, filled, h))                  # filled portion
    pygame.draw.rect(surf, border_col, (x, y, w, h), 1)                     # border


def draw_dashed_hline(surf, x1, x2, y, color, dash=8):
    """Draw a horizontal dashed line from x1 to x2 at height y."""
    x = x1                                  # start at left edge
    while x < x2:                           # march across the full width
        end = min(x + dash, x2)            # end of this dash segment (clamped)
        pygame.draw.line(surf, color, (x, y), (end, y), 1)  # draw one dash
        x += dash * 2                       # skip forward by dash + gap (equal dash/gap)


# ── Mode 1: Stats Dashboard ────────────────────────────────────────────────────

def draw_stats(surf, fonts):
    """Render the numeric statistics overview."""
    _, med, sm = fonts                                   # unpack fonts (med and small used)
    p      = 1.0 / ODDS                                  # theoretical probability per trial
    actual = (total_hits / total_trials) if total_trials else 0.0  # observed hit rate
    cum    = 1.0 - (1.0 - p) ** total_trials if total_trials else 0.0  # P(≥1 hit by now)

    draw_text(surf, "Stats Dashboard", WIN_W // 2, 10, med, WHITE, cx=True)  # section title

    # ── Main counters ──────────────────────────────────────────────────────────
    draw_text(surf, f"Total Trials:      {total_trials:>14,}", 70,  75, med, WHITE)   # total trial count
    draw_text(surf, f"Total Hits:        {total_hits:>14,}",   70, 113, med, GREEN)   # total hits in green
    draw_text(surf, f"Current Streak:    {streak:>14,}",       70, 151, med, GRAY)    # trials since last hit

    # ── Rate comparison ────────────────────────────────────────────────────────
    draw_text(surf, f"Expected Rate:     1/{ODDS}  =  {p:.8f}", 70, 205, med, TEAL)            # theoretical
    col = GREEN if actual >= p else RED                                                          # above/below color
    draw_text(surf, f"Actual Rate:                  {actual:.8f}", 70, 243, med, col)           # observed

    # ── Cumulative probability bar ─────────────────────────────────────────────
    draw_text(surf, "Probability of at least 1 hit by now:", 70, 300, sm, GRAY)   # bar label
    draw_bar(surf, 70, 325, 660, 28, cum, TEAL, GRAY, sm)                         # progress bar
    draw_text(surf, f"{cum * 100:.4f}%", 70 + 660 // 2, 329, sm, WHITE, cx=True) # percentage inside bar

    # ── Hit history summary ────────────────────────────────────────────────────
    if hit_history:                                            # only if any hits have occurred
        avg = sum(hit_history) / len(hit_history)              # average wait-time between hits
        draw_text(surf, f"Avg wait between hits: {avg:,.1f}  (expected ≈ {ODDS:,})", 70, 390, sm, GRAY)
        draw_text(surf, f"Last hit after:        {hit_history[-1]:,} trials", 70, 418, sm, GRAY)
        draw_text(surf, f"Hits recorded:         {len(hit_history):,}", 70, 446, sm, GRAY)  # hit count
    else:                                                      # no hits yet
        draw_text(surf, "No hits recorded yet — keep simulating!", 70, 390, sm, GRAY)  # placeholder


# ── Mode 2: 8192 Grid ──────────────────────────────────────────────────────────

def draw_grid(surf, fonts):
    """Render the 128×64 epoch grid — every cell represents one trial."""
    _, med, sm = fonts                             # unpack fonts
    draw_text(surf, f"8192 Grid  —  Epoch {epoch + 1}", WIN_W // 2, 8, med, WHITE, cx=True)  # title

    COLS   = 128                                   # 128 columns × 64 rows = 8192 cells
    ROWS   = 64                                    # 64 rows
    MAR    = 14                                    # pixel margin around the grid
    cell_w = (WIN_W - 2 * MAR) // COLS            # pixel width of each cell (= 6)
    cell_h = (WIN_H - 52 - MAR) // ROWS           # pixel height of each cell (≈ 8)
    off_x  = MAR                                   # grid left edge offset
    off_y  = 44                                    # grid top edge offset (below title)
    pos    = total_trials % ODDS                   # trial index within the current epoch

    for i in range(ODDS):                          # iterate all 8192 cells
        col  = i % COLS                            # column index of cell i
        row  = i // COLS                           # row index of cell i
        rect = (                                   # pixel rect for this cell
            off_x + col * cell_w + 1,             # left edge + 1px gap between cells
            off_y + row * cell_h + 1,             # top edge + 1px gap
            cell_w - 1,                            # width minus gap
            cell_h - 1,                            # height minus gap
        )
        if i in epoch_hits:                        # this cell was a hit this epoch
            color = GOLD                           # light up gold
        elif i < pos:                              # this cell was visited but not a hit
            color = (40, 40, 80)                  # dim blue-gray
        else:                                      # this cell hasn't been reached yet
            color = DARK                           # very dark background
        pygame.draw.rect(surf, color, rect)        # paint the cell

    # ── Bottom status bar ──────────────────────────────────────────────────────
    draw_text(surf,
        f"Hits this epoch: {len(epoch_hits)}   |   Progress: {pos:,} / {ODDS:,}",
        WIN_W // 2, WIN_H - 22, sm, GRAY, cx=True)  # epoch progress summary


# ── Mode 3: Convergence Graph ──────────────────────────────────────────────────

def draw_convergence(surf, fonts):
    """Plot the running hit rate over time alongside the 1/8192 theoretical line."""
    _, med, sm = fonts                             # unpack fonts
    draw_text(surf, "Convergence Graph", WIN_W // 2, 8, med, WHITE, cx=True)  # title

    PX, PY = 60, 45                                # plot area top-left corner
    PW     = WIN_W - PX - 25                       # plot area width
    PH     = WIN_H - PY - 65                       # plot area height
    p_theo = 1.0 / ODDS                            # the theoretical rate we expect to converge toward

    pygame.draw.line(surf, GRAY, (PX, PY),      (PX, PY + PH),      1)  # y-axis
    pygame.draw.line(surf, GRAY, (PX, PY + PH), (PX + PW, PY + PH), 1)  # x-axis
    draw_text(surf, "Rate",   PX - 5,      PY - 14,     sm, GRAY)         # y-axis label
    draw_text(surf, "Time →", PX + PW - 30, PY + PH + 5, sm, GRAY)       # x-axis label

    if not rate_history:                           # nothing to draw yet
        draw_text(surf, "Simulating…", WIN_W // 2, WIN_H // 2, med, GRAY, cx=True)
        return

    # ── Y-axis scale ────────────────────────────────────────────────────────────
    y_max = max(max(rate_history), p_theo * 4)     # top of y range; always shows theoretical
    y_scl = PH / y_max if y_max > 0 else 1         # pixels per unit of rate

    # ── Dashed theoretical reference line ─────────────────────────────────────
    ref_y = PY + PH - int(p_theo * y_scl)          # y pixel for the theoretical rate
    draw_dashed_hline(surf, PX, PX + PW, ref_y, TEAL, dash=8)            # dashed teal line
    draw_text(surf, f"1/{ODDS}", PX + PW + 2, ref_y - 7, sm, TEAL)       # label at right edge

    # ── Running rate line ──────────────────────────────────────────────────────
    pts    = list(rate_history)                     # snapshot for this frame
    n      = len(pts)                               # number of data points
    if n < 2:                                       # need 2+ points to draw a line
        return
    x_step = PW / (n - 1)                          # horizontal pixels between consecutive points
    prev   = None                                   # previous (x, y) for connecting segments
    for i, rate in enumerate(pts):                  # iterate each recorded rate value
        sx = int(PX + i * x_step)                  # screen x position
        sy = int(PY + PH - rate * y_scl)           # screen y position (higher rate = higher up)
        sy = max(PY, min(PY + PH, sy))             # clamp to plot bounds
        if prev:                                    # connect to the previous point
            pygame.draw.line(surf, LBLUE, prev, (sx, sy), 1)  # draw graph segment
        prev = (sx, sy)                             # advance previous point

    # ── Current rate label ─────────────────────────────────────────────────────
    cur_rate = rate_history[-1]                                              # most recent rate
    col      = GREEN if cur_rate >= p_theo else RED                          # above/below color
    draw_text(surf, f"Now: {cur_rate:.8f}", PX + 4, PY + 4, sm, col)       # current rate top-left
    draw_text(surf, f"Expected: {p_theo:.8f}", PX + 4, PY + 22, sm, TEAL)  # expected rate below


# ── Mode 4: Geometric Histogram ───────────────────────────────────────────────

def draw_histogram(surf, fonts):
    """Bar chart of observed wait-times with the theoretical geometric PMF overlay."""
    _, med, sm = fonts                             # unpack fonts
    draw_text(surf, "Geometric Distribution Histogram", WIN_W // 2, 8, med, WHITE, cx=True)

    if len(hit_history) < 2:                       # need multiple hits for a meaningful histogram
        msg = f"Waiting for hits…  ({total_hits} recorded, need ≥ 2)"
        draw_text(surf, msg, WIN_W // 2, WIN_H // 2, med, GRAY, cx=True)
        return

    # ── Bucket observed wait-times ─────────────────────────────────────────────
    mx_wait  = max(hit_history)                    # longest observed wait
    bkt_max  = max(mx_wait, ODDS * 3)              # x-axis upper bound: at least 3× expected
    bkt_sz   = max(1, bkt_max // N_BUCKETS)        # trials per bucket
    counts   = [0] * N_BUCKETS                     # initialize all bucket counts to zero
    for w in hit_history:                          # assign each wait-time to a bucket
        b = min(int(w / bkt_sz), N_BUCKETS - 1)   # compute bucket index (clamped)
        counts[b] += 1                             # increment that bucket's count

    max_ct = max(counts) if any(counts) else 1     # tallest bar for y-axis scaling

    # ── Plot area ──────────────────────────────────────────────────────────────
    PX, PY = 60, 45                                # top-left of plot area
    PW     = WIN_W - PX - 25                       # plot width
    PH     = WIN_H - PY - 70                       # plot height
    bw     = PW // N_BUCKETS                       # pixel width of each bar

    pygame.draw.line(surf, GRAY, (PX, PY),      (PX, PY + PH),      1)  # y-axis
    pygame.draw.line(surf, GRAY, (PX, PY + PH), (PX + PW, PY + PH), 1)  # x-axis
    draw_text(surf, "Freq",              PX - 5,       PY - 14,     sm, GRAY)         # y label
    draw_text(surf, "Trials to hit →",  PX + PW//3,  PY + PH + 5,  sm, GRAY)         # x label

    # ── Draw observed bars ─────────────────────────────────────────────────────
    for i, ct in enumerate(counts):                        # one bar per bucket
        bh = int((ct / max_ct) * PH) if max_ct else 0     # bar height proportional to count
        bx = PX + i * bw                                   # bar left x
        by = PY + PH - bh                                  # bar top y (bottom-aligned)
        pygame.draw.rect(surf, LBLUE, (bx + 1, by, bw - 2, bh))  # draw observed bar

    # ── Theoretical geometric PMF overlay ─────────────────────────────────────
    p_hit  = 1.0 / ODDS                             # probability of success per trial
    n_hits = len(hit_history)                        # total hits recorded (for scaling)
    prev   = None                                    # previous point for the curve line
    for i in range(N_BUCKETS):                       # one curve point per bucket
        mid   = (i + 0.5) * bkt_sz                  # midpoint wait-time of this bucket
        pmf   = ((1 - p_hit) ** (mid - 1)) * p_hit  # geometric PMF: P(X = mid)
        exp_n = pmf * bkt_sz * n_hits                # expected count = PMF × bucket_width × sample_size
        eph   = int((exp_n / max_ct) * PH)           # pixel height for expected count
        pt_x  = PX + i * bw + bw // 2               # x center of this bucket
        pt_y  = PY + PH - eph                        # y of PMF curve point
        if prev:                                     # connect to previous point
            pygame.draw.line(surf, ORANGE, prev, (pt_x, pt_y), 2)  # orange PMF curve
        prev = (pt_x, pt_y)                          # advance previous point

    # ── Legend ─────────────────────────────────────────────────────────────────
    pygame.draw.rect(surf, LBLUE,  (PX + 5, PY + 8,  12, 12))             # blue swatch
    draw_text(surf, "Observed",         PX + 22, PY + 6,  sm, LBLUE)      # observed label
    pygame.draw.rect(surf, ORANGE, (PX + 5, PY + 28, 12, 12))             # orange swatch
    draw_text(surf, "Geometric PMF",    PX + 22, PY + 26, sm, ORANGE)     # PMF label
    draw_text(surf, f"{n_hits} hits  |  bucket = {bkt_sz:,} trials", PX + 5, PY + 48, sm, GRAY)


# ── Mode 5: Live Roller ────────────────────────────────────────────────────────

def draw_roller(surf, fonts):
    """Slow-motion individual roll display with a gold flash on each hit."""
    big, med, sm = fonts                            # unpack fonts (big used for roll number)

    # ── Hit flash: gold overlay fading over 30 frames ─────────────────────────
    if flash_timer > 0:                             # flash is active
        alpha   = int(180 * flash_timer / (FPS // 2))           # alpha fades as timer counts down
        overlay = pygame.Surface((WIN_W, WIN_H), pygame.SRCALPHA)  # transparent overlay surface
        overlay.fill((255, 200, 0, alpha))          # semi-transparent gold
        surf.blit(overlay, (0, 0))                  # composite over the current scene

    draw_text(surf, "Live Roller", WIN_W // 2, 14, med, WHITE, cx=True)    # title

    # ── Large roll number ──────────────────────────────────────────────────────
    roll_col = GOLD if last_roll == 0 else WHITE    # gold on hit, white otherwise
    draw_text(surf, f"{last_roll:,}", WIN_W // 2, WIN_H // 2 - 80, big, roll_col, cx=True)   # huge number
    draw_text(surf, "(hit when roll = 0)", WIN_W // 2, WIN_H // 2 + 8, sm, GRAY, cx=True)    # hint

    # ── Stats row ──────────────────────────────────────────────────────────────
    draw_text(surf, f"Trials since last hit:  {streak:,}",
              WIN_W // 2, WIN_H // 2 + 52, med, GRAY, cx=True)                  # current streak
    draw_text(surf, f"Total Hits: {total_hits:,}   |   Total Trials: {total_trials:,}",
              WIN_W // 2, WIN_H // 2 + 90, sm, GRAY, cx=True)                   # totals

    # ── Recent hit list ────────────────────────────────────────────────────────
    draw_text(surf, "Last 5 hits (trials waited):", 70, WIN_H - 145, sm, GRAY)  # section header
    recent = list(reversed(hit_history[-5:]))        # last 5 waits, newest first
    for j, w in enumerate(recent):                   # iterate up to 5 entries
        label = f"  Hit #{len(hit_history) - j}:  after {w:,} trials"           # formatted label
        draw_text(surf, label, 70, WIN_H - 123 + j * 22, sm, GOLD)              # draw in gold

    # ── Speed note ─────────────────────────────────────────────────────────────
    eff = min(speed, 10)                             # mode 5 caps at 10 trials/frame
    draw_text(surf, f"Speed: {eff}/frame (capped at 10 in this mode)",
              WIN_W // 2, WIN_H - 22, sm, GRAY, cx=True)                        # bottom note


# ── Persistent HUD ─────────────────────────────────────────────────────────────

def draw_hud(surf, fonts):
    """Overlay the always-visible speed indicator and control hints."""
    _, _, sm = fonts                                              # only small font needed
    draw_text(surf, f"Speed: {speed:,}/frame", 5, 5, sm, GRAY)  # top-left speed readout
    draw_text(surf,                                               # bottom-center mode indicator
        f"[ Mode {cur_mode + 1}/5: {MODES[cur_mode][0]} ]",
        WIN_W // 2, WIN_H - 36, sm, GRAY, cx=True)
    draw_text(surf,                                               # bottom-center control hints
        "◄► / 1-5: mode   |   UP/DN: speed   |   R: reset   |   ESC/Q: quit",
        WIN_W // 2, WIN_H - 18, sm, (70, 70, 90), cx=True)


# ── Main ───────────────────────────────────────────────────────────────────────

def main():
    """Initialize pygame and run the event/simulation/draw loop."""
    global cur_mode, speed, flash_timer              # allow UI state changes in the loop

    pygame.init()                                    # initialize all pygame subsystems
    screen = pygame.display.set_mode((WIN_W, WIN_H)) # create the display window
    pygame.display.set_caption(                      # set initial window title
        f"1/{ODDS} Probability Simulation  —  {MODES[cur_mode][0]}")
    clock = pygame.time.Clock()                      # clock for capping the frame rate

    # ── Fonts ──────────────────────────────────────────────────────────────────
    font_big = pygame.font.SysFont("consolas", 72, bold=True)   # large: roll number in Live Roller
    font_med = pygame.font.SysFont("consolas", 26, bold=False)  # medium: section headers and stats
    font_sm  = pygame.font.SysFont("consolas", 15, bold=False)  # small: labels and hints
    fonts    = (font_big, font_med, font_sm)                     # bundle as a tuple

    # ── Mode draw dispatch ─────────────────────────────────────────────────────
    draw_fns = [draw_stats, draw_grid, draw_convergence, draw_histogram, draw_roller]

    running = True                                   # main loop control flag
    while running:                                   # ── main loop ──

        # ── Events ──────────────────────────────────────────────────────────────
        for event in pygame.event.get():             # drain the event queue
            if event.type == pygame.QUIT:            # window close button clicked
                running = False                      # exit cleanly

            elif event.type == pygame.KEYDOWN:       # a key was pressed
                k = event.key                        # shorthand alias

                if k in (pygame.K_ESCAPE, pygame.K_q):      # ESC or Q: quit
                    running = False

                elif k == pygame.K_r:                        # R: reset everything
                    reset()

                elif k == pygame.K_LEFT:                     # LEFT: previous mode
                    cur_mode = (cur_mode - 1) % len(MODES)
                    pygame.display.set_caption(
                        f"1/{ODDS} Probability Simulation  —  {MODES[cur_mode][0]}")

                elif k == pygame.K_RIGHT:                    # RIGHT: next mode
                    cur_mode = (cur_mode + 1) % len(MODES)
                    pygame.display.set_caption(
                        f"1/{ODDS} Probability Simulation  —  {MODES[cur_mode][0]}")

                elif k == pygame.K_1: cur_mode = 0           # 1 key: Stats Dashboard
                elif k == pygame.K_2: cur_mode = 1           # 2 key: 8192 Grid
                elif k == pygame.K_3: cur_mode = 2           # 3 key: Convergence Graph
                elif k == pygame.K_4: cur_mode = 3           # 4 key: Geometric Histogram
                elif k == pygame.K_5: cur_mode = 4           # 5 key: Live Roller

                elif k == pygame.K_UP:                       # UP: double the speed
                    speed = min(speed * 2, MAX_SPD)
                elif k == pygame.K_DOWN:                     # DOWN: halve the speed
                    speed = max(speed // 2, MIN_SPD)

        # ── Simulate ─────────────────────────────────────────────────────────────
        n = min(speed, 10) if cur_mode == 4 else speed  # Live Roller caps at 10/frame
        simulate(n)                                      # run this frame's batch of trials

        if flash_timer > 0:                              # tick down the hit-flash animation
            flash_timer -= 1                             # one frame closer to fade-out

        # ── Draw ─────────────────────────────────────────────────────────────────
        screen.fill(BG)                                  # clear screen with background color
        draw_fns[cur_mode](screen, fonts)                # call the active mode's draw function
        draw_hud(screen, fonts)                          # overlay the persistent HUD
        pygame.display.flip()                            # swap the back-buffer to the screen
        clock.tick(FPS)                                  # cap the frame rate at FPS

    pygame.quit()                                    # shut down pygame subsystems
    sys.exit(0)                                      # exit the process


if __name__ == "__main__":                           # only run when executed directly (not imported)
    main()                                           # enter the main function
