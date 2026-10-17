// HARDCORE HENRY DRIVE — shared world types.
// Lives in a header (not the .ino) so the struct is declared before the
// Arduino IDE's auto-generated function prototypes. Otherwise genBlock's
// prototype references BRect before it exists -> "'BRect' not declared".
#ifndef TOPDRIVE_WORLD_H
#define TOPDRIVE_WORLD_H

struct BRect { int x, y, w, h; };  // world-space rectangle (buildings)

// Per-car steering state (rising-edge intersection detection + turn target)
struct SteerSt { bool inInter; bool hasTarget; float tx, ty; };

#endif
