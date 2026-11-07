// FH2 ANNA's own indicators, hazards and autodrive.
#pragma once
#include "common.h"

namespace fh2anna {

enum class Signal { Off, Hazards, Left, Right };

bool hook_drive();                 // the car update, the sat-nav and its route tracking
void note_local_car(void* car);    // the player's car this frame (from the HUD's update)

Signal signal_on();
void set_signal(Signal s);         // the same one again turns it off

bool autodrive_possible();         // a route to drive (the sat-nav's or the game's), and the car the player's
bool autodriving();
void toggle_autodrive();           // starts (at the sat-nav's next update) or stops

bool in_free_roam();               // not in an event or a race (as the player's car last saw)

} // namespace fh2anna
