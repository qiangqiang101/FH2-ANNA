// What the pause menu does, for ANNA. Tune Car: the pause menu opens as with Start, and its tile's
// own handler runs on it. Matchmaking: the event the pause menu's online tiles end in, sent
// straight from the pause flow, no menu.
#pragma once
#include "common.h"

namespace fh2anna {

enum class PauseTile { TuneCar };
enum class Session { RoadTrip, FreeRoam, PrivateRoadTrip, PrivateFreeRoam };

bool hook_pause();                  // at start
bool pause_available();             // the pause flow's waiting for Start (driving)
void press_pause_tile(PauseTile t); // any thread: done from the next frames' updates
void start_matchmaking(Session s);  // any thread: done from the pause flow's next update
void pause_tick();                  // each input update (the main loop)

} // namespace fh2anna
