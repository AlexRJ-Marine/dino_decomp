// Dino Island portable port - application entry point.
//
// The application object lives in `game.h`; this file only parses the command
// line and runs it.

#include "game.h"

int main(int argc, char** argv) {
    return dino::app::Game(argc, argv).run();
}
