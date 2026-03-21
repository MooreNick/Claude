cmake_minimum_required(VERSION 3.16) # requires CMake 3.16+ for modern target-based SDL2 support
project(Breakout CXX)               # names the project "Breakout" and specifies C++ as the language

set(CMAKE_CXX_STANDARD 17)          # requires C++17 for structured bindings, std::optional, and other modern features
set(CMAKE_CXX_STANDARD_REQUIRED ON) # makes C++17 a hard requirement rather than a preference

# ── Dependency: SDL2 ──────────────────────────────────────────────────────────
# Install via vcpkg: vcpkg install sdl2 sdl2-ttf
# Or on Ubuntu/Debian: sudo apt install libsdl2-dev libsdl2-ttf-dev
# Or on macOS (Homebrew): brew install sdl2 sdl2_ttf
find_package(SDL2 REQUIRED)         # locates the SDL2 library; errors if not found
find_package(SDL2_ttf REQUIRED)     # locates SDL2_ttf for TrueType font rendering; errors if not found

# ── Source files ──────────────────────────────────────────────────────────────
add_executable(breakout             # defines the output executable named "breakout"
    src/main.cpp                    # entry point: creates Game and calls run()
    src/Game.cpp                    # state machine, game loop, collision, update, render
    src/Ball.cpp                    # ball movement, wall bounce, drawing
    src/Paddle.cpp                  # paddle movement, powerup timers, drawing
    src/Block.cpp                   # block hit detection, score value, drawing
    src/Powerup.cpp                 # powerup falling, collection detection, drawing
    src/Laser.cpp                   # laser projectile movement and drawing
    src/Particle.cpp                # particle physics, fade-out, drawing
)

target_include_directories(breakout PRIVATE src) # adds src/ to the include path so headers can be included without path prefix

# ── Link SDL2 and SDL2_ttf ────────────────────────────────────────────────────
# Modern SDL2 CMake config exposes SDL2::SDL2 and SDL2_ttf::SDL2_ttf targets.
# Older installations may use SDL2_LIBRARIES / SDL2_TTF_LIBRARIES variables instead.
if(TARGET SDL2::SDL2)                           # checks whether modern CMake targets are available
    target_link_libraries(breakout              # links using modern namespaced targets (vcpkg / CMake 3.20+ installs)
        SDL2::SDL2                              # SDL2 main library
        SDL2_ttf::SDL2_ttf                      # SDL2_ttf font library
    )
else()                                          # fallback for older SDL2 CMake find modules
    target_include_directories(breakout PRIVATE ${SDL2_INCLUDE_DIRS}) # adds SDL2 headers to include path
    target_link_libraries(breakout ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES}) # links using variable-style targets
endif()

# ── Windows-specific: copy SDL2 DLLs next to the executable ──────────────────
if(WIN32)                                       # only runs this block on Windows builds
    add_custom_command(TARGET breakout POST_BUILD # runs after the executable is built
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  # copies files only if they have changed
            $<TARGET_RUNTIME_DLLS:breakout>            # expands to list of runtime DLLs needed by the target
            $<TARGET_FILE_DIR:breakout>                # destination: same directory as the built executable
        COMMAND_EXPAND_LISTS                           # needed to correctly expand generator expressions above
    )
endif()
