cmake_minimum_required(VERSION 3.16)   # requires CMake 3.16+ for modern target-based SDL2 support
project(Simulation CXX)               # names the project "Simulation", C++ language

set(CMAKE_CXX_STANDARD 17)            # require C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)   # make C++17 a hard requirement

# ── Point CMake at the SDL2 libraries bundled in the breakout-game folder ───────
# These paths assume you run CMake from Simulation/cpp/build/ (one level below CMakeLists.txt).
# Adjust the paths below if your directory layout differs.
# The x86_64-w64-mingw32 subdirectory contains the 64-bit MinGW build of SDL2.
set(SDL2_DIR
    "${CMAKE_SOURCE_DIR}/../../breakout-game/breakout-cpp/libs/SDL2-2.32.10/x86_64-w64-mingw32/lib/cmake/SDL2"
    CACHE PATH "Path to SDL2Config.cmake directory")

set(SDL2_ttf_DIR
    "${CMAKE_SOURCE_DIR}/../../breakout-game/breakout-cpp/libs/SDL2_ttf-2.24.0/x86_64-w64-mingw32/lib/cmake/SDL2_ttf"
    CACHE PATH "Path to SDL2_ttfConfig.cmake directory")

# ── Find SDL2 and SDL2_ttf ────────────────────────────────────────────────────
find_package(SDL2     REQUIRED)        # locate SDL2; errors if not found at SDL2_DIR
find_package(SDL2_ttf REQUIRED)        # locate SDL2_ttf; errors if not found at SDL2_ttf_DIR

# ── Define the executable ─────────────────────────────────────────────────────
add_executable(simulation              # output executable named "simulation"
    src/simulation.cpp                 # single source file containing all code
)

target_include_directories(simulation PRIVATE src)  # add src/ to the include search path

# ── Link SDL2 and SDL2_ttf ────────────────────────────────────────────────────
# Modern SDL2 cmake config exposes SDL2::SDL2 and SDL2_ttf::SDL2_ttf targets.
# Older installs may expose only the variable-style SDL2_LIBRARIES instead.
if(TARGET SDL2::SDL2)                              # modern namespaced targets available
    target_link_libraries(simulation
        SDL2::SDL2                                 # SDL2 main library
        SDL2_ttf::SDL2_ttf                         # SDL2_ttf font library
    )
else()                                             # fallback for older SDL2 cmake modules
    target_include_directories(simulation PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(simulation ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES})
endif()

# ── Windows: copy SDL2 DLLs next to the executable ────────────────────────────
if(WIN32)                                          # only on Windows
    add_custom_command(TARGET simulation POST_BUILD  # run after build completes
        COMMAND ${CMAKE_COMMAND} -E copy_if_different  # copy only if changed
            $<TARGET_RUNTIME_DLLS:simulation>          # all runtime DLLs the target needs
            $<TARGET_FILE_DIR:simulation>              # destination: same folder as simulation.exe
        COMMAND_EXPAND_LISTS                           # required for generator expression expansion
    )
endif()
