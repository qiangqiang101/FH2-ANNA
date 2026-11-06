// Routes: the game's own, to the nearest race, bucket list, festival, barn find, bonus board.
#pragma once
#include "common.h"

namespace fh2anna {

enum class Route { Race, BucketList, Festival, BarnFind, BonusBoard };

// The game's CareerNavigationModule.
bool hook_routes();
bool routes_available();
bool set_route(Route r);            // the game thread
void start_nearest_search(uint8_t* status);   // "what should I do?": the nearest of them
void nearest_search_tick(uint8_t* status);    // each HUD update

} // namespace fh2anna
