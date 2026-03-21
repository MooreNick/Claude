#include "Game.h"      // includes the Game class which owns all SDL resources and game logic
#include <iostream>    // includes std::cerr for printing fatal error messages to the console
#include <stdexcept>   // includes std::exception for catching runtime errors from Game's constructor

// main is the program entry point.
// It constructs the Game object (which initializes SDL, creates the window and renderer,
// and loads fonts), then calls run() to enter the main game loop.
// Any fatal initialization errors are caught and printed to stderr.
int main() {
    try {                       // wraps construction and run() in a try block to catch SDL init failures
        Game game;              // constructs the game: initializes SDL2, SDL_ttf, window, renderer, and fonts
        game.run();             // enters the game loop; returns only when the player closes the window
    } catch (const std::exception& e) {          // catches any std::exception thrown during init or runtime
        std::cerr << "Fatal error: " << e.what() << std::endl; // prints the error message to stderr
        return 1;               // returns a non-zero exit code to signal failure to the OS
    }
    return 0;                   // returns zero to signal successful execution to the OS
}
