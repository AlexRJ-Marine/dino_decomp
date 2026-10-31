// Retail mouse-cursor textures (`Data/Graphics/Cursors/*.ini`).
//
// The park and menus use real cursor TGAs:
//   CursorMain.ini       -> CursorMain_wood.tga (16x16, default)
//   CursorSell.ini       -> money.tga          (32x32, sell state)
//   CursorInsertFence.ini-> put_in_enclos.tga  (32x32, fence placement)
// The .ini files are simple `[GRAPH] Path/Sequence/SizeX/SizeY` records; the
// port only needs the sequence image (one frame, Speed 1). The retail cursor
// is a colour cursor with the hotspot at the top-left (no hotspot field).
#pragma once

#include <string>

namespace dino::app {

enum class CursorMode { Main, Sell, Fence };

// Loads the three retail cursor images. Missing images fall back to the SDL
// default cursor. Safe to call once after the window exists.
void init_cursors(const std::string& asset_root);
void set_cursor_mode(CursorMode mode);
void shutdown_cursors();

}  // namespace dino::app
