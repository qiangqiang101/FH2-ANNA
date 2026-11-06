// Autodrive's cinematic camera (Extra Options' FH3 branch's, ported): a few seconds into an
// autodrive, slow orbiting replay shots of the car with the HUD hidden; the camera and the HUD
// come back when it ends.
#pragma once
#include "common.h"

namespace fh2anna {

void cinematic_settings();          // its ini value and Mods page row (from fh2mod_init)
bool hook_cinematic();              // the UI's drawing, to hide it (from start)
bool cinematic_wanted();            // the setting's on

bool cinematic_start();             // the shots, the HUD hidden, a flash: false if the camera can't
void cinematic_tick();              // the next shot when one ends
void cinematic_stop(bool flash);    // the camera and the HUD back (a flash if asked and it was on)
bool cinematic_on();

} // namespace fh2anna
