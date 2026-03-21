import pygame  # imports the pygame library for graphics, sound, and input
import sys  # imports sys for clean program exit
import math  # imports math for trigonometric functions used in ball angle calculations
import random  # imports random for block layout generation and powerup drops
import json  # imports json for saving and loading the high score to a file
import os  # imports os to construct the high score file path relative to this script

# ─── CONSTANTS ────────────────────────────────────────────────────────────────

CANVAS_W = 480  # width of the game window in pixels
CANVAS_H = 640  # height of the game window in pixels
FPS = 60  # target frames per second for the game loop

PADDLE_W = 80  # default paddle width in pixels
PADDLE_H = 12  # paddle height in pixels
PADDLE_Y = 600  # vertical position of the paddle from the top of the window
PADDLE_SPEED = 6  # pixels the paddle moves per frame when a key is held

BALL_RADIUS = 7  # radius of the ball in pixels
BALL_BASE_SPEED = 4.5  # starting ball speed in pixels per frame

BLOCK_COLS = 10  # number of block columns in the grid
BLOCK_ROWS_BASE = 5  # number of block rows at level 1
BLOCK_ROWS_MAX = 8  # maximum number of block rows at high levels
BLOCK_W = 40  # width of each block in pixels
BLOCK_H = 18  # height of each block in pixels
BLOCK_PAD = 4  # gap between adjacent blocks in pixels
BLOCK_OFFSET_TOP = 60  # distance from window top to first block row
BLOCK_OFFSET_LEFT = 20  # distance from window left to first block column

POWERUP_CHANCE = 0.18  # probability (0–1) a destroyed block drops a powerup
POWERUP_SPEED = 2  # pixels per frame the powerup falls downward
LASER_COOLDOWN_MS = 400  # milliseconds between laser shots
LASER_SPEED = 12  # pixels per frame lasers travel upward
WIDE_DURATION_MS = 8000  # milliseconds the wide-paddle powerup lasts
SLOW_DURATION_MS = 6000  # milliseconds the slow-ball powerup lasts
LASER_DURATION_MS = 10000  # milliseconds the laser powerup lasts
LIVES_START = 3  # number of lives the player starts with

NORMAL_COLORS = [  # color palette for single-hit blocks (RGB tuples)
    (79, 195, 247),   # light blue
    (129, 199, 132),  # green
    (255, 183, 77),   # amber
    (206, 147, 216),  # purple
    (77, 208, 225),   # cyan
]
COLOR_HP2 = (239, 154, 154)  # color for two-hit blocks (red-pink)
COLOR_INDESTRUCTIBLE = (120, 144, 156)  # color for indestructible blocks (gray)
COLOR_BG = (10, 10, 26)  # very dark blue background color
COLOR_WHITE = (255, 255, 255)  # white for ball and general text
COLOR_HUD = (224, 224, 224)  # light gray for HUD text
COLOR_LIFE = (244, 143, 177)  # pink for life heart icons
COLOR_RED = (239, 83, 80)  # red for game over and ball lost messages
COLOR_GREEN = (129, 199, 132)  # green for level complete messages
COLOR_GOLD = (255, 183, 77)  # gold for high score text

# game state identifiers
STATE_TITLE = "title"  # title/main menu screen
STATE_PLAYING = "playing"  # active gameplay
STATE_LIFE_LOST = "life_lost"  # brief pause after ball falls off bottom
STATE_LEVEL_COMPLETE = "level_complete"  # all blocks cleared, advancing to next level
STATE_GAME_OVER = "game_over"  # no lives remaining
STATE_PAUSED = "paused"  # player pressed ESC to pause

# high score file path (same folder as this script)
HS_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "breakout_hs.json")  # constructs absolute path for the save file


# ─── HIGH SCORE PERSISTENCE ───────────────────────────────────────────────────

def load_high_score():  # reads the saved high score from disk; returns 0 if not found
    try:  # attempts to open and parse the save file
        with open(HS_FILE) as f:  # opens the high score file for reading
            return json.load(f).get("hs", 0)  # extracts the "hs" value, defaulting to 0
    except Exception:  # catches file-not-found or parse errors silently
        return 0  # returns zero if any error occurs


def save_high_score(score):  # writes the given score to the high score save file
    try:  # attempts to write the file
        with open(HS_FILE, "w") as f:  # opens the file for writing (creates it if missing)
            json.dump({"hs": score}, f)  # writes the score as a JSON object
    except Exception:  # silently ignores any write errors (e.g. read-only filesystem)
        pass  # nothing to do if write fails


# ─── ENTITY CLASSES ───────────────────────────────────────────────────────────

class Ball:  # represents a single ball moving around the arena
    def __init__(self, x, y, vx, vy, speed):  # initializes ball position, velocity, and speed scalar
        self.x = float(x)  # horizontal center position as float for sub-pixel precision
        self.y = float(y)  # vertical center position as float for sub-pixel precision
        self.vx = float(vx)  # horizontal velocity component in pixels per frame
        self.vy = float(vy)  # vertical velocity component in pixels per frame
        self.speed = float(speed)  # scalar speed used to renormalize velocity after deflections
        self.radius = BALL_RADIUS  # collision and drawing radius
        self.on_paddle = True  # True while ball is glued to paddle before launch
        self.active = True  # False means this ball should be removed from the list

    def update(self):  # moves the ball by its velocity each frame
        if self.on_paddle:  # skip movement if ball is still attached to the paddle
            return  # do nothing while glued to paddle
        self.x += self.vx  # advance horizontal position
        self.y += self.vy  # advance vertical position

    def normalize_speed(self):  # restores velocity magnitude to the stored speed scalar
        mag = math.hypot(self.vx, self.vy)  # computes current speed magnitude
        if mag == 0:  # guards against division by zero
            return  # nothing to normalize if velocity is zero
        self.vx = (self.vx / mag) * self.speed  # scales horizontal component to target speed
        self.vy = (self.vy / mag) * self.speed  # scales vertical component to target speed

    def draw(self, surface):  # renders the ball as a white circle with a subtle glow
        glow_surf = pygame.Surface((self.radius * 4, self.radius * 4), pygame.SRCALPHA)  # creates transparent surface for glow
        pygame.draw.circle(glow_surf, (160, 196, 255, 60), (self.radius * 2, self.radius * 2), self.radius * 2)  # draws soft glow circle
        surface.blit(glow_surf, (int(self.x) - self.radius * 2, int(self.y) - self.radius * 2))  # blits glow behind ball
        pygame.draw.circle(surface, COLOR_WHITE, (int(self.x), int(self.y)), self.radius)  # draws the solid white ball


class Paddle:  # represents the player-controlled paddle at the bottom of the screen
    def __init__(self):  # initializes the paddle centered horizontally
        self.base_width = PADDLE_W  # remembers original width to restore after wide powerup
        self.width = PADDLE_W  # current width in pixels (can change from powerup)
        self.height = PADDLE_H  # fixed height of the paddle
        self.x = float((CANVAS_W - PADDLE_W) // 2)  # starting horizontal position (centered)
        self.y = PADDLE_Y  # fixed vertical position near the bottom
        self.speed = PADDLE_SPEED  # movement speed in pixels per frame
        self.wide_timer = 0  # milliseconds remaining for wide-paddle effect
        self.laser_active = False  # whether laser powerup is currently active
        self.laser_timer = 0  # milliseconds remaining for laser powerup
        self.laser_cooldown_timer = 0  # milliseconds until next laser shot is allowed

    @property
    def center_x(self):  # computed property returning horizontal center of the paddle
        return self.x + self.width / 2  # adds half the width to the left edge

    @property
    def rect(self):  # returns a pygame.Rect for collision checks and rendering
        return pygame.Rect(int(self.x), self.y, self.width, self.height)  # constructs rect from current position and size

    def update(self, dt, keys):  # moves paddle based on held keys and ticks powerup timers
        if keys[pygame.K_LEFT] or keys[pygame.K_a]:  # checks if left movement key is held
            self.x -= self.speed  # moves paddle left
        if keys[pygame.K_RIGHT] or keys[pygame.K_d]:  # checks if right movement key is held
            self.x += self.speed  # moves paddle right
        self.x = max(0.0, min(float(CANVAS_W - self.width), self.x))  # clamps paddle within canvas bounds

        if self.wide_timer > 0:  # if wide powerup is still active
            self.wide_timer -= dt  # counts down wide timer
            if self.wide_timer <= 0:  # if wide timer just expired
                self.width = self.base_width  # restores paddle to original width

        if self.laser_cooldown_timer > 0:  # if laser is still on cooldown
            self.laser_cooldown_timer -= dt  # counts down cooldown timer

        if self.laser_timer > 0:  # if laser powerup is still active
            self.laser_timer -= dt  # counts down laser duration
            if self.laser_timer <= 0:  # if laser powerup just expired
                self.laser_active = False  # disables the laser

    def activate_wide(self):  # applies the wide-paddle powerup
        self.width = int(self.base_width * 1.8)  # expands paddle to 180% of normal width
        self.wide_timer = WIDE_DURATION_MS  # sets the duration timer

    def activate_laser(self):  # applies the laser powerup
        self.laser_active = True  # enables laser firing
        self.laser_timer = LASER_DURATION_MS  # sets the laser duration timer

    def draw(self, surface):  # renders the paddle as a rounded rectangle with gradient effect
        color = (255, 138, 101) if self.laser_active else (220, 220, 220)  # orange if laser active, white otherwise
        dark_color = (191, 54, 12) if self.laser_active else (158, 158, 158)  # darker shade for gradient effect
        r = self.rect  # gets the paddle's current bounding rectangle
        for i in range(self.height):  # draws each horizontal pixel row with interpolated color for gradient
            t = i / self.height  # 0.0 at top, 1.0 at bottom
            row_color = (  # interpolates between top and bottom colors
                int(color[0] + (dark_color[0] - color[0]) * t),  # red channel
                int(color[1] + (dark_color[1] - color[1]) * t),  # green channel
                int(color[2] + (dark_color[2] - color[2]) * t),  # blue channel
            )
            pygame.draw.line(surface, row_color, (r.x, r.y + i), (r.x + r.width - 1, r.y + i))  # draws horizontal line
        if self.laser_active:  # draws a glow border when laser is active
            pygame.draw.rect(surface, (255, 100, 100), r, 1)  # red border outline


class Block:  # represents a single block that the ball can destroy
    def __init__(self, x, y, hp, color):  # initializes block at grid position with hit points and color
        self.x = x  # left edge of the block
        self.y = y  # top edge of the block
        self.width = BLOCK_W  # block width
        self.height = BLOCK_H  # block height
        self.hp = hp  # current hit points (-1 = indestructible, 1 = normal, 2 = tough)
        self.max_hp = hp  # original hit points for damage visual calculation
        self.color = color  # fill color as an RGB tuple
        self.active = True  # False means the block has been destroyed
        self.score_value = 0 if hp == -1 else hp * 10  # points awarded when destroyed

    @property
    def rect(self):  # returns a pygame.Rect for collision checks and rendering
        return pygame.Rect(self.x, self.y, self.width, self.height)  # constructs rect from block position

    def hit(self):  # called when a ball or laser contacts this block
        if self.hp == -1:  # indestructible blocks cannot be damaged
            return  # do nothing
        self.hp -= 1  # reduces hit points by one
        if self.hp <= 0:  # if all hit points are gone
            self.active = False  # marks block for removal

    def draw(self, surface):  # renders the block with color and a border
        if not self.active:  # skip drawing destroyed blocks
            return  # nothing to draw
        brightness = 1.0 if self.max_hp == -1 else self.hp / self.max_hp  # computes health ratio for dimming effect
        alpha = int(255 * (0.35 + 0.65 * brightness))  # computes opacity based on health (dims when damaged)
        r, g, b = self.color  # unpacks the block's RGB color
        draw_color = (  # builds the dimmed color for damaged blocks
            int(r * (alpha / 255)),  # scales red channel by alpha
            int(g * (alpha / 255)),  # scales green channel by alpha
            int(b * (alpha / 255)),  # scales blue channel by alpha
        )
        pygame.draw.rect(surface, draw_color, self.rect)  # draws the solid block rectangle
        pygame.draw.rect(surface, (40, 40, 40), self.rect, 1)  # draws a dark border for definition


class Powerup:  # represents a powerup pill falling from a destroyed block
    def __init__(self, x, y, ptype):  # initializes powerup at given position with a type string
        self.x = float(x)  # left edge of the powerup pill
        self.y = float(y)  # top edge of the powerup pill
        self.width = 34  # width of the powerup pill
        self.height = 16  # height of the powerup pill
        self.vy = POWERUP_SPEED  # falling speed in pixels per frame
        self.ptype = ptype  # powerup type string (e.g. "wide", "laser")
        self.active = True  # False means collected or fell off screen

    @property
    def rect(self):  # returns a pygame.Rect for collision checks
        return pygame.Rect(int(self.x), int(self.y), self.width, self.height)  # current bounding rect

    def update(self):  # moves the powerup downward each frame
        self.y += self.vy  # advances vertical position by falling speed
        if self.y > CANVAS_H + 20:  # if powerup has fallen below the screen
            self.active = False  # deactivates it for removal

    def draw(self, surface, font):  # renders the powerup as a colored pill with a label
        colors = {  # maps each powerup type to a distinct RGB color
            "wide":     (165, 214, 167),  # green for wide paddle
            "multiball":(255, 241, 118),  # yellow for multi-ball
            "slow":     (128, 222, 234),  # cyan for slow ball
            "life":     (244, 143, 177),  # pink for extra life
            "laser":    (255, 138, 101),  # orange for laser
        }
        labels = {  # short labels displayed inside each pill
            "wide": "WID", "multiball": "x3", "slow": "SLW", "life": "+1", "laser": "LZR"
        }
        color = colors.get(self.ptype, COLOR_WHITE)  # gets pill color, defaults to white
        r = self.rect  # gets bounding rect
        pygame.draw.rect(surface, color, r, border_radius=7)  # draws rounded pill
        pygame.draw.rect(surface, (0, 0, 0), r, 1, border_radius=7)  # draws dark border
        label = labels.get(self.ptype, "?")  # gets label text
        text_surf = font.render(label, True, (0, 0, 0))  # renders label in black
        surface.blit(text_surf, text_surf.get_rect(center=r.center))  # draws label centered in pill


class Laser:  # represents a laser beam fired upward from the paddle
    def __init__(self, x, y):  # initializes laser at given position
        self.x = float(x)  # horizontal center of the laser beam
        self.y = float(y)  # top edge of the laser beam
        self.width = 3  # width of the laser beam in pixels
        self.height = 14  # height of the laser beam in pixels
        self.vy = -LASER_SPEED  # upward velocity (negative = moving up the screen)
        self.active = True  # False means this laser has hit something or left the screen

    def update(self):  # moves the laser upward each frame
        self.y += self.vy  # advances vertical position upward
        if self.y + self.height < 0:  # if laser has traveled off the top of the screen
            self.active = False  # deactivates it for removal

    def draw(self, surface):  # renders the laser as a bright red rectangle
        pygame.draw.rect(surface, (255, 68, 68), pygame.Rect(int(self.x - self.width // 2), int(self.y), self.width, self.height))  # draws laser beam


class Particle:  # represents a single visual particle spawned from block destruction
    def __init__(self, x, y, color):  # initializes particle at a position with a given color
        self.x = float(x)  # horizontal position
        self.y = float(y)  # vertical position
        self.vx = (random.random() - 0.5) * 5  # random horizontal velocity
        self.vy = (random.random() - 0.5) * 5 - 2  # random upward-biased vertical velocity
        self.size = random.uniform(2, 6)  # random size between 2 and 6 pixels
        self.color = color  # particle color matching the destroyed block
        self.alpha = 255  # starting opacity (fades to 0 over lifetime)
        self.life = random.randint(300, 500)  # random lifetime in milliseconds
        self.max_life = self.life  # stores original lifetime for alpha calculation
        self.active = True  # False means this particle should be removed

    def update(self, dt):  # moves and fades the particle each frame
        self.x += self.vx  # advances horizontal position
        self.y += self.vy  # advances vertical position
        self.vy += 0.1  # applies gravity (gradually increases downward velocity)
        self.life -= dt  # counts down particle lifetime
        self.alpha = max(0, int(255 * self.life / self.max_life))  # fades out as lifetime runs out
        if self.life <= 0:  # if lifetime has expired
            self.active = False  # marks particle for removal

    def draw(self, surface):  # renders the particle as a small colored square
        if self.alpha <= 0:  # skip invisible particles
            return  # nothing to draw
        s = pygame.Surface((int(self.size), int(self.size)), pygame.SRCALPHA)  # creates a small transparent surface
        s.fill((*self.color, self.alpha))  # fills it with the particle color at current opacity
        surface.blit(s, (int(self.x - self.size / 2), int(self.y - self.size / 2)))  # blits particle at its position


# ─── LEVEL GENERATION ─────────────────────────────────────────────────────────

def generate_level(lvl):  # creates a random block layout for the given level number
    result = []  # list to collect newly created Block objects
    rows = min(BLOCK_ROWS_BASE + lvl // 2, BLOCK_ROWS_MAX)  # increases row count every 2 levels, capped at max
    for r in range(rows):  # iterates over each row
        for c in range(BLOCK_COLS):  # iterates over each column
            if random.random() < 0.15:  # 15% chance to skip this cell, creating visual gaps
                continue  # skip this grid position
            x = BLOCK_OFFSET_LEFT + c * (BLOCK_W + BLOCK_PAD)  # calculates block left edge position
            y = BLOCK_OFFSET_TOP + r * (BLOCK_H + BLOCK_PAD)  # calculates block top edge position
            roll = random.random()  # random roll to determine block type
            if lvl >= 6 and roll < 0.10:  # 10% chance of indestructible block from level 6 onward
                hp, color = -1, COLOR_INDESTRUCTIBLE  # indestructible block
            elif lvl >= 3 and roll < 0.30:  # 30% chance of two-hit block from level 3 onward
                hp, color = 2, COLOR_HP2  # tough block requiring two hits
            else:  # default: normal single-hit block
                hp = 1  # one hit to destroy
                color = random.choice(NORMAL_COLORS)  # random color from palette
            result.append(Block(x, y, hp, color))  # adds the block to the result list
    return result  # returns the complete list of blocks for this level


# ─── POWERUP SELECTION ────────────────────────────────────────────────────────

def random_powerup_type():  # selects a random powerup type using weighted probability
    weights = [  # list of (type, weight) tuples defining relative drop frequencies
        ("wide",      30),  # wide paddle is most common
        ("slow",      25),  # slow ball is second most common
        ("multiball", 20),  # multi-ball is moderately common
        ("laser",     15),  # laser is somewhat rare
        ("life",      10),  # extra life is rarest
    ]
    total = sum(w for _, w in weights)  # sums all weights to get total probability mass
    roll = random.uniform(0, total)  # picks a random value within the total range
    for ptype, weight in weights:  # iterates through weighted entries
        roll -= weight  # subtracts each weight from the roll
        if roll <= 0:  # if roll has reached zero or below
            return ptype  # returns this powerup type
    return "wide"  # fallback (should never reach here)


# ─── MAIN GAME CLASS ──────────────────────────────────────────────────────────

class Game:  # encapsulates all game state and logic
    def __init__(self):  # initializes pygame, creates window, loads fonts, and sets up initial state
        pygame.init()  # initializes all pygame modules
        self.screen = pygame.display.set_mode((CANVAS_W, CANVAS_H))  # creates the game window
        pygame.display.set_caption("BREAKOUT")  # sets the window title bar text
        self.clock = pygame.time.Clock()  # creates a clock object to control frame rate

        # load fonts — falls back gracefully if system fonts differ
        self.font_large = pygame.font.SysFont("Courier New", 28, bold=True)  # large retro-style font for title and game over
        self.font_med = pygame.font.SysFont("Courier New", 18, bold=True)  # medium font for level/state messages
        self.font_small = pygame.font.SysFont("Courier New", 11, bold=True)  # small font for HUD elements and hints
        self.font_tiny = pygame.font.SysFont("Courier New", 9, bold=True)  # tiny font for powerup labels

        self.high_score = load_high_score()  # loads the saved high score from disk
        self._reset_game_vars()  # initializes all mutable game state variables
        self.state = STATE_TITLE  # starts on the title screen
        self.title_anim_timer = 0  # animation timer for the title screen decoration
        self.blink_timer = 0  # timer used to drive blinking text effect

    def _reset_game_vars(self):  # resets all gameplay variables to their starting values
        self.level = 1  # resets level to 1
        self.lives = LIVES_START  # resets lives to starting amount
        self.score = 0  # resets score to zero
        self.balls = []  # clears ball list
        self.blocks = []  # clears block list
        self.powerups = []  # clears powerup list
        self.lasers = []  # clears laser list
        self.particles = []  # clears particle list
        self.paddle = None  # clears paddle (will be recreated)
        self.slow_active = False  # resets slow powerup state
        self.slow_timer = 0  # resets slow timer
        self.state_timer = 0  # resets general state timer
        self.shake_timer = 0  # resets screen shake timer

    def start_new_game(self):  # sets up all entities and transitions to playing state
        self._reset_game_vars()  # resets all variables to starting values
        self.paddle = Paddle()  # creates a fresh paddle
        self.blocks = generate_level(self.level)  # generates block layout for level 1
        self._create_ball_on_paddle()  # places a ball on the paddle ready for launch
        self.state = STATE_PLAYING  # transitions to gameplay state

    def _create_ball_on_paddle(self):  # creates a new ball attached to the paddle
        speed = min(BALL_BASE_SPEED + (self.level - 1) * 0.25, 9.0)  # scales speed with level, capped at 9
        b = Ball(self.paddle.center_x, self.paddle.y - BALL_RADIUS, 0, -speed, speed)  # creates ball above paddle center
        b.on_paddle = True  # marks ball as attached to paddle
        self.balls.append(b)  # adds ball to the active list

    def _reset_after_life_lost(self):  # prepares for play to resume after losing a life
        self.balls = []  # removes all existing balls
        self.powerups = []  # clears all falling powerups
        self.lasers = []  # clears all laser projectiles
        self.paddle.width = self.paddle.base_width  # resets paddle to normal width
        self.paddle.wide_timer = 0  # clears wide timer
        self.paddle.laser_active = False  # disables laser
        self.paddle.laser_timer = 0  # clears laser timer
        self.slow_active = False  # clears slow effect
        self.slow_timer = 0  # clears slow timer
        self._create_ball_on_paddle()  # places a fresh ball on the paddle
        self.state = STATE_PLAYING  # returns to playing state
        self.state_timer = 0  # resets state timer

    def _load_next_level(self):  # advances the game to the next level
        self.level += 1  # increments level counter
        self.balls = []  # clears all balls
        self.powerups = []  # clears all powerups
        self.lasers = []  # clears all lasers
        self.particles = []  # clears leftover particles
        self.paddle.width = self.paddle.base_width  # resets paddle width
        self.paddle.wide_timer = 0  # clears wide timer
        self.paddle.laser_active = False  # disables laser
        self.paddle.laser_timer = 0  # clears laser timer
        self.slow_active = False  # clears slow effect
        self.slow_timer = 0  # clears slow timer
        self.blocks = generate_level(self.level)  # generates new block layout
        self._create_ball_on_paddle()  # places a fresh ball on the paddle
        self.state = STATE_PLAYING  # returns to playing state
        self.state_timer = 0  # resets state timer

    # ─── COLLISION ────────────────────────────────────────────────────────────

    def _ball_paddle_collision(self, ball):  # handles ball bouncing off the paddle with angle control
        if ball.y + ball.radius < self.paddle.y:  # ball is above paddle; no collision
            return
        if ball.y - ball.radius > self.paddle.y + self.paddle.height:  # ball is below paddle; no collision
            return
        if ball.x + ball.radius < self.paddle.x:  # ball is left of paddle; no collision
            return
        if ball.x - ball.radius > self.paddle.x + self.paddle.width:  # ball is right of paddle; no collision
            return
        if ball.vy < 0:  # ball is already moving upward; skip to avoid sticking
            return
        ball.y = self.paddle.y - ball.radius  # pushes ball above paddle surface
        hit_pos = (ball.x - self.paddle.center_x) / (self.paddle.width / 2)  # normalizes hit position from -1 to +1
        hit_pos = max(-0.95, min(0.95, hit_pos))  # clamps to avoid nearly-horizontal angles
        angle = hit_pos * (math.pi / 3)  # maps hit position to ±60 degrees from vertical
        ball.vx = ball.speed * math.sin(angle)  # sets horizontal component based on angle
        ball.vy = -ball.speed * math.cos(angle)  # sets vertical component upward based on angle

    def _ball_block_collision(self, ball, block):  # tests and resolves ball-block collision; returns True if hit
        if not block.active:  # skip inactive blocks
            return False
        bL = ball.x - ball.radius  # ball left edge
        bR = ball.x + ball.radius  # ball right edge
        bT = ball.y - ball.radius  # ball top edge
        bB = ball.y + ball.radius  # ball bottom edge

        if bR < block.x or bL > block.x + block.width:  # no horizontal overlap
            return False
        if bB < block.y or bT > block.y + block.height:  # no vertical overlap
            return False

        # compute penetration depths to determine which face was hit
        overlap_l = bR - block.x  # penetration from block's left face
        overlap_r = (block.x + block.width) - bL  # penetration from block's right face
        overlap_t = bB - block.y  # penetration from block's top face
        overlap_b = (block.y + block.height) - bT  # penetration from block's bottom face

        min_x = min(overlap_l, overlap_r)  # smallest horizontal penetration
        min_y = min(overlap_t, overlap_b)  # smallest vertical penetration

        if min_x < min_y:  # horizontal face hit (left or right side)
            ball.vx *= -1  # reverses horizontal velocity
        else:  # vertical face hit (top or bottom)
            ball.vy *= -1  # reverses vertical velocity

        block.hit()  # damages the block (reduces hp, deactivates if zero)

        if not block.active:  # if this hit destroyed the block
            self.score += block.score_value  # adds points to score
            for _ in range(8):  # spawns 8 particles at block center
                self.particles.append(Particle(block.x + block.width // 2, block.y + block.height // 2, block.color))
            if random.random() < POWERUP_CHANCE:  # rolls for powerup drop
                ptype = random_powerup_type()  # selects a random powerup type
                self.powerups.append(Powerup(block.x + block.width // 2 - 17, block.y, ptype))  # spawns powerup at block center

        return True  # signals that collision occurred

    def _laser_block_collision(self, laser, block):  # tests and resolves laser-block collision; returns True if hit
        if not block.active or not laser.active:  # skip inactive entities
            return False
        lr = pygame.Rect(int(laser.x - laser.width // 2), int(laser.y), laser.width, laser.height)  # laser bounding rect
        if not lr.colliderect(block.rect):  # no overlap between laser and block
            return False
        laser.active = False  # destroys laser on contact
        block.hit()  # damages the block
        if not block.active:  # if this hit destroyed the block
            self.score += block.score_value  # awards points
            for _ in range(6):  # spawns 6 particles
                self.particles.append(Particle(block.x + block.width // 2, block.y + block.height // 2, block.color))
            if random.random() < POWERUP_CHANCE:  # rolls for powerup drop
                self.powerups.append(Powerup(block.x + block.width // 2 - 17, block.y, random_powerup_type()))
        return True  # signals collision occurred

    def _collect_powerup(self, pu):  # applies the effect of a collected powerup and deactivates it
        pu.active = False  # marks powerup as consumed
        if pu.ptype == "wide":  # wide paddle powerup
            self.paddle.activate_wide()  # expands the paddle
        elif pu.ptype == "slow":  # slow ball powerup
            if not self.slow_active:  # only slows if not already slowed
                for b in self.balls:  # slows all active balls
                    b.speed *= 0.7  # reduces speed scalar
                    b.normalize_speed()  # reapplies speed to velocity
            self.slow_active = True  # marks slow as active
            self.slow_timer = SLOW_DURATION_MS  # resets the slow duration timer
        elif pu.ptype == "multiball":  # multi-ball powerup
            src = next((b for b in self.balls if not b.on_paddle and b.active), None)  # finds an in-flight ball to clone
            if src:  # if there is an active flying ball
                b1 = Ball(src.x, src.y, src.vx + 2, src.vy, src.speed)  # new ball deflected slightly right
                b1.on_paddle = False  # immediately in flight
                b2 = Ball(src.x, src.y, src.vx - 2, src.vy, src.speed)  # new ball deflected slightly left
                b2.on_paddle = False  # immediately in flight
                b1.normalize_speed()  # corrects speed after deflection offset
                b2.normalize_speed()  # corrects speed after deflection offset
                self.balls.extend([b1, b2])  # adds both new balls to the list
        elif pu.ptype == "life":  # extra life powerup
            self.lives += 1  # increases player's life count
        elif pu.ptype == "laser":  # laser powerup
            self.paddle.activate_laser()  # enables laser firing on the paddle

    # ─── UPDATE ───────────────────────────────────────────────────────────────

    def update(self, dt):  # main update dispatcher; calls the correct update for current state
        self.blink_timer += dt  # advances blink timer every frame regardless of state
        if self.state == STATE_TITLE:  # title screen
            self.title_anim_timer += dt  # advances title animation timer
        elif self.state == STATE_PLAYING:  # active gameplay
            self._update_playing(dt)  # runs full gameplay update
        elif self.state == STATE_LIFE_LOST:  # life lost pause
            self.state_timer += dt  # ticks state timer
            if self.state_timer > 2500:  # auto-resumes after 2.5 seconds
                self._reset_after_life_lost()  # resets for next try
        elif self.state == STATE_LEVEL_COMPLETE:  # level complete pause
            self.state_timer += dt  # ticks state timer
            if self.state_timer > 2500:  # auto-advances after 2.5 seconds
                self._load_next_level()  # loads next level

    def _update_playing(self, dt):  # handles all gameplay logic for one frame
        self.state_timer += dt  # ticks general state timer

        # fire lasers when space is held and laser powerup is active
        keys = pygame.key.get_pressed()  # gets current keyboard state for laser fire check
        if keys[pygame.K_SPACE] and self.paddle.laser_active and self.paddle.laser_cooldown_timer <= 0:  # checks laser fire conditions
            self.lasers.append(Laser(self.paddle.x + self.paddle.width * 0.25, self.paddle.y - 2))  # fires from left quarter
            self.lasers.append(Laser(self.paddle.x + self.paddle.width * 0.75, self.paddle.y - 2))  # fires from right quarter
            self.paddle.laser_cooldown_timer = LASER_COOLDOWN_MS  # starts cooldown

        self.paddle.update(dt, keys)  # moves paddle and ticks powerup timers

        # track attached balls to paddle center
        for b in self.balls:  # iterates all balls
            if b.on_paddle:  # if ball is still attached to the paddle
                b.x = self.paddle.center_x  # snaps ball horizontally to paddle center
                b.y = self.paddle.y - b.radius  # positions ball just above paddle surface

        # move all entities
        for b in self.balls:  # moves each ball
            b.update()
        for p in self.powerups:  # moves each powerup downward
            p.update()
        for l in self.lasers:  # moves each laser upward
            l.update()
        for p in self.particles:  # updates each particle (position, fade, life)
            p.update(dt)

        # wall and ceiling collisions
        for ball in self.balls:  # checks each ball against walls
            if ball.on_paddle:  # skip attached balls
                continue
            if ball.x - ball.radius < 0:  # hit left wall
                ball.x = ball.radius  # pushes ball inside boundary
                ball.vx = abs(ball.vx)  # ensures ball moves rightward
            if ball.x + ball.radius > CANVAS_W:  # hit right wall
                ball.x = CANVAS_W - ball.radius  # pushes ball inside boundary
                ball.vx = -abs(ball.vx)  # ensures ball moves leftward
            if ball.y - ball.radius < 0:  # hit ceiling
                ball.y = ball.radius  # pushes ball below ceiling
                ball.vy = abs(ball.vy)  # ensures ball moves downward
            if ball.y - ball.radius > CANVAS_H:  # ball fell below the screen
                ball.active = False  # marks ball as lost

        # ball vs paddle collisions
        for ball in self.balls:  # checks each ball
            if not ball.on_paddle:  # only test in-flight balls
                self._ball_paddle_collision(ball)  # resolves paddle collision

        # ball vs block collisions
        for ball in self.balls:  # checks each ball
            if ball.on_paddle:  # skip attached balls
                continue
            for block in self.blocks:  # checks each block
                if self._ball_block_collision(ball, block):  # if collision occurred
                    break  # only one block hit per ball per frame

        # laser vs block collisions
        for laser in self.lasers:  # checks each laser
            for block in self.blocks:  # checks each block
                if self._laser_block_collision(laser, block):  # if collision occurred
                    break  # one block hit per laser

        # powerup collection by paddle
        paddle_rect = self.paddle.rect  # gets paddle bounding rect for overlap test
        for pu in self.powerups:  # checks each powerup
            if pu.active and pu.rect.colliderect(paddle_rect):  # if powerup overlaps paddle
                self._collect_powerup(pu)  # applies powerup effect

        # tick slow ball timer
        if self.slow_active:  # if slow powerup is currently active
            self.slow_timer -= dt  # counts down slow duration
            if self.slow_timer <= 0:  # if slow duration just expired
                self.slow_active = False  # clears slow state
                normal_speed = min(BALL_BASE_SPEED + (self.level - 1) * 0.25, 9.0)  # recalculates normal speed for this level
                for b in self.balls:  # restores all balls to normal speed
                    b.speed = normal_speed  # sets speed scalar
                    b.normalize_speed()  # applies scalar to velocity

        # remove inactive entities
        self.balls = [b for b in self.balls if b.active]  # keeps only active balls
        self.powerups = [p for p in self.powerups if p.active]  # keeps only active powerups
        self.lasers = [l for l in self.lasers if l.active]  # keeps only active lasers
        self.particles = [p for p in self.particles if p.active]  # keeps only living particles
        self.blocks = [b for b in self.blocks if b.active or b.hp == -1]  # keeps indestructible and intact blocks

        # check if all balls have been lost
        if not self.balls:  # no balls remain in play
            self.lives -= 1  # deducts a life
            if self.lives <= 0:  # all lives exhausted
                if self.score > self.high_score:  # new high score achieved
                    self.high_score = self.score  # updates high score
                    save_high_score(self.high_score)  # saves to disk
                self.state = STATE_GAME_OVER  # transitions to game over screen
            else:  # lives remain
                self.shake_timer = 400  # triggers screen shake
                self.state = STATE_LIFE_LOST  # transitions to life lost pause screen
            self.state_timer = 0  # resets state timer
            return  # exits update early

        # check if all destructible blocks are cleared
        if not any(b.hp != -1 for b in self.blocks):  # no destructible blocks remain
            if self.score > self.high_score:  # check for new high score
                self.high_score = self.score  # updates high score
                save_high_score(self.high_score)  # saves to disk
            self.state = STATE_LEVEL_COMPLETE  # transitions to level complete screen
            self.state_timer = 0  # resets state timer

    # ─── RENDER ───────────────────────────────────────────────────────────────

    def render(self):  # main render dispatcher; draws the correct screen for current state
        if self.state == STATE_TITLE:  # title screen
            self._render_title()  # draws animated title screen
        elif self.state == STATE_PLAYING:  # active gameplay
            self._render_game()  # draws game entities and HUD
        elif self.state == STATE_LIFE_LOST:  # life lost pause
            self._render_game()  # draws frozen game underneath
            self._render_overlay(0.6)  # semi-transparent dark overlay
            self._draw_centered("BALL LOST!", CANVAS_H // 2 - 30, self.font_med, COLOR_RED)  # loss message
            self._draw_centered(f"{self.lives} {'LIFE' if self.lives == 1 else 'LIVES'} REMAINING", CANVAS_H // 2 + 10, self.font_small, COLOR_HUD)  # remaining lives count
            self._draw_blink("PRESS SPACE", CANVAS_H // 2 + 55, self.font_small, COLOR_WHITE, 500)  # blinking continue prompt
        elif self.state == STATE_LEVEL_COMPLETE:  # level complete screen
            self._render_game()  # draws frozen game underneath
            self._render_overlay(0.65)  # dark overlay
            self._draw_centered(f"LEVEL {self.level}", CANVAS_H // 2 - 40, self.font_med, COLOR_GREEN)  # level number
            self._draw_centered("COMPLETE!", CANVAS_H // 2 - 10, self.font_med, COLOR_GREEN)  # completion text
            self._draw_centered(f"SCORE: {self.score}", CANVAS_H // 2 + 30, self.font_small, COLOR_WHITE)  # current score
            self._draw_blink("PRESS SPACE", CANVAS_H // 2 + 65, self.font_small, COLOR_WHITE, 500)  # blinking continue prompt
        elif self.state == STATE_GAME_OVER:  # game over screen
            self._render_game()  # draws frozen game underneath
            self._render_overlay(0.75)  # heavy overlay
            self._draw_centered("GAME OVER", CANVAS_H // 2 - 30, self.font_large, COLOR_RED)  # game over text
            self._draw_centered(f"SCORE: {self.score}", CANVAS_H // 2 + 20, self.font_small, COLOR_HUD)  # final score
            if self.score > 0 and self.score >= self.high_score:  # if this was a new high score
                self._draw_centered("NEW BEST!", CANVAS_H // 2 + 45, self.font_small, COLOR_GOLD)  # congratulatory message
            self._draw_blink("PRESS SPACE", CANVAS_H // 2 + 80, self.font_small, COLOR_WHITE, 500)  # blinking restart prompt
        elif self.state == STATE_PAUSED:  # paused screen
            self._render_game()  # draws frozen game underneath
            self._render_overlay(0.5)  # medium overlay
            self._draw_centered("PAUSED", CANVAS_H // 2 - 10, self.font_large, COLOR_WHITE)  # paused text
            self._draw_centered("ESC TO RESUME", CANVAS_H // 2 + 35, self.font_small, COLOR_HUD)  # resume hint

        pygame.display.flip()  # swaps the back buffer to the screen, making the frame visible

    def _render_game(self):  # draws all game entities; used by playing and overlay states
        # apply screen shake offset
        ox, oy = 0, 0  # default no offset
        if self.shake_timer > 0:  # if shake is currently active
            self.shake_timer -= self.clock.get_time()  # decrements shake timer
            ox = random.randint(-3, 3)  # random horizontal shake offset
            oy = random.randint(-3, 3)  # random vertical shake offset

        self.screen.fill(COLOR_BG)  # clears screen with background color

        # draw all entities offset by shake amount
        for p in self.particles:  # draws particle effects (behind blocks)
            p.draw(self.screen)
        for b in self.blocks:  # draws all blocks
            b.draw(self.screen)
        for p in self.powerups:  # draws falling powerups
            p.draw(self.screen, self.font_tiny)
        for l in self.lasers:  # draws laser beams
            l.draw(self.screen)
        if self.paddle:  # draws the paddle if it exists
            self.paddle.draw(self.screen)
        for b in self.balls:  # draws all balls
            b.draw(self.screen)

        self._draw_hud()  # draws HUD on top of everything

    def _draw_hud(self):  # renders the score, level, and lives display at the top
        score_surf = self.font_small.render(f"SCORE {self.score:06d}", True, COLOR_HUD)  # renders zero-padded score
        self.screen.blit(score_surf, (8, 6))  # draws score in top-left corner

        lvl_surf = self.font_small.render(f"LVL {self.level}", True, COLOR_HUD)  # renders level number
        self.screen.blit(lvl_surf, lvl_surf.get_rect(centerx=CANVAS_W // 2, top=6))  # draws level centered at top

        for i in range(self.lives):  # draws one heart icon per remaining life
            heart_surf = self.font_small.render("♥", True, COLOR_LIFE)  # renders pink heart
            self.screen.blit(heart_surf, (CANVAS_W - 14 - i * 22, 6))  # places hearts from right edge inward

        if self.high_score > 0:  # only shows high score if one exists
            hs_surf = self.font_tiny.render(f"BEST {self.high_score}", True, (100, 100, 100))  # renders dim gray high score
            self.screen.blit(hs_surf, hs_surf.get_rect(centerx=CANVAS_W // 2, top=22))  # draws below level number

    def _render_title(self):  # renders the animated title screen
        self.screen.fill(COLOR_BG)  # fills background
        t = self.title_anim_timer / 1000.0  # converts timer to seconds for sine calculations
        for r in range(5):  # draws 5 rows of decorative animated blocks
            for c in range(BLOCK_COLS):  # draws 10 columns
                x = BLOCK_OFFSET_LEFT + c * (BLOCK_W + BLOCK_PAD)  # block x position
                y_offset = int(math.sin(t + c * 0.5) * 6)  # sine wave vertical animation
                y = BLOCK_OFFSET_TOP + 20 + r * (BLOCK_H + BLOCK_PAD) + y_offset  # animated y position
                alpha = int(80 + 50 * math.sin(t * 2 + r + c))  # pulsing opacity value
                color = NORMAL_COLORS[(r * BLOCK_COLS + c) % len(NORMAL_COLORS)]  # cycles through color palette
                surf = pygame.Surface((BLOCK_W, BLOCK_H), pygame.SRCALPHA)  # transparent surface for alpha control
                surf.fill((*color, alpha))  # fills with color at pulsing alpha
                self.screen.blit(surf, (x, y))  # draws decorative block

        self._draw_centered("BREAKOUT", int(CANVAS_H * 0.55), self.font_large, (79, 195, 247))  # draws game title in cyan
        self._draw_blink("PRESS SPACE TO START", int(CANVAS_H * 0.70), self.font_small, COLOR_WHITE, 600)  # blinking start prompt
        self._draw_centered("ARROW KEYS / A-D TO MOVE", int(CANVAS_H * 0.78), self.font_tiny, (120, 120, 120))  # control hint
        self._draw_centered("SPACE TO LAUNCH BALL", int(CANVAS_H * 0.83), self.font_tiny, (120, 120, 120))  # launch hint
        self._draw_centered("ESC TO PAUSE", int(CANVAS_H * 0.88), self.font_tiny, (120, 120, 120))  # pause hint
        if self.high_score > 0:  # shows high score only if one exists
            self._draw_centered(f"BEST: {self.high_score}", int(CANVAS_H * 0.93), self.font_small, COLOR_GOLD)  # gold high score line

    def _render_overlay(self, alpha):  # draws a semi-transparent dark overlay over the current frame
        overlay = pygame.Surface((CANVAS_W, CANVAS_H), pygame.SRCALPHA)  # creates transparent surface same size as canvas
        overlay.fill((0, 0, 0, int(255 * alpha)))  # fills with black at specified opacity
        self.screen.blit(overlay, (0, 0))  # draws overlay covering the entire screen

    def _draw_centered(self, text, y, font, color):  # renders text centered horizontally at the given y coordinate
        surf = font.render(text, True, color)  # renders the text surface
        self.screen.blit(surf, surf.get_rect(centerx=CANVAS_W // 2, top=y))  # blits text centered horizontally

    def _draw_blink(self, text, y, font, color, period_ms):  # renders blinking text (visible half the time)
        if (self.blink_timer // period_ms) % 2 == 0:  # alternates visibility based on timer divided by period
            self._draw_centered(text, y, font, color)  # draws text when blink phase is "on"

    # ─── EVENT HANDLING ───────────────────────────────────────────────────────

    def handle_event(self, event):  # processes a single pygame event
        if event.type == pygame.QUIT:  # user clicked the window close button
            pygame.quit()  # shuts down pygame
            sys.exit()  # exits the program

        if event.type == pygame.KEYDOWN:  # a key was pressed this frame
            if event.key == pygame.K_SPACE:  # space bar pressed
                if self.state == STATE_TITLE:  # on title screen
                    self.start_new_game()  # begins a new game
                elif self.state == STATE_PLAYING:  # during gameplay
                    if not self.paddle.laser_active:  # if laser is not active, space launches the ball
                        for b in self.balls:  # iterates all balls
                            if b.on_paddle:  # only launches balls attached to the paddle
                                b.on_paddle = False  # detaches ball from paddle
                                b.vx = 0  # starts with no horizontal velocity
                                b.vy = -b.speed  # launches ball straight upward
                elif self.state == STATE_LIFE_LOST:  # on life lost pause screen
                    self._reset_after_life_lost()  # resumes immediately without waiting for timer
                elif self.state == STATE_LEVEL_COMPLETE:  # on level complete screen
                    self._load_next_level()  # advances to next level immediately
                elif self.state == STATE_GAME_OVER:  # on game over screen
                    self.state = STATE_TITLE  # returns to title screen
                    self.blink_timer = 0  # resets blink timer

            if event.key == pygame.K_ESCAPE:  # escape key pressed
                if self.state == STATE_PLAYING:  # if currently playing
                    self.state = STATE_PAUSED  # pauses the game
                elif self.state == STATE_PAUSED:  # if currently paused
                    self.state = STATE_PLAYING  # resumes the game
                    self.state_timer = 0  # resets state timer to avoid timer-based misfires

    # ─── MAIN LOOP ────────────────────────────────────────────────────────────

    def run(self):  # starts and runs the game loop until the window is closed
        while True:  # loops indefinitely until sys.exit() is called
            dt = self.clock.tick(FPS)  # waits to maintain target FPS and returns elapsed milliseconds

            for event in pygame.event.get():  # processes all pending pygame events
                self.handle_event(event)  # dispatches each event to the handler

            self.update(dt)  # updates game logic for this frame
            self.render()  # draws the current frame to the screen


# ─── ENTRY POINT ──────────────────────────────────────────────────────────────

if __name__ == "__main__":  # only runs when this script is executed directly (not imported)
    game = Game()  # creates the game instance (initializes pygame, window, fonts)
    game.run()  # starts the main game loop
