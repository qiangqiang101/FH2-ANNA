// Autodrive's cinematic camera (Extra Options' FH3 branch's, ported): while ANNA drives, its button
// (LB unless FH2ANNA.ini's [Autodrive] Cinematic says another, or None) turns it on (drive.cpp),
// slow orbiting replay shots of the car with the HUD hidden; the camera and the HUD come back with
// the button again or when the drive ends.
#pragma once
#include "common.h"

namespace fh2anna {

bool hook_cinematic();              // the replay cameras' FOV (from start)

void cinematic_settings();          // its button, from the ini, and its Mods page row (from fh2mod_init)
uint16_t cinematic_button();        // its XInput bit; 0: None, no cinematic camera
const char* cinematic_button_name();   // "LB", "RB", "A"... (FH2's ControllerIcons/Controller_<name>.png)
int cinematic_button_index();       // 0 LB, 1 RB, 2 A, 3 B, 4 X, 5 Y, 6 View, 7 L3, 8 R3; -1 None

bool cinematic_start();             // the shots, the HUD hidden, a flash: false if the camera can't
void cinematic_tick();              // the next shot when one ends
void cinematic_stop(bool flash);    // the camera and the HUD back (a flash if asked and it was on)
bool cinematic_on();

} // namespace fh2anna
