#include "breakout.h"  // includes all game classes, constants, and SDL2 headers
#include <iostream>    // std::cerr for printing fatal errors to the console
#include <stdexcept>   // std::exception for catching SDL initialization failures

// main is the program entry point.
// It constructs the Game (which initializes SDL2, creates the window/renderer, and loads fonts),
// then calls run() to enter the game loop.
int main() {
    try {                          // wraps everything in a try block to catch fatal init errors
        Game game;                 // constructs the game: SDL2 init, window, renderer, fonts, high score
        game.run();                // enters the game loop; returns only when the window is closed
    } catch (const std::exception& e) {             // catches any std::exception thrown during init
        std::cerr << "Fatal: " << e.what() << "\n"; // prints the error to stderr
        return 1;                  // returns non-zero exit code to signal failure
    }
    return 0;                      // returns zero on successful exit
}
