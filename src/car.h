// The player's car: its headlights and the parts it animates (roof, hood, trunk, doors...).
#pragma once
#include "common.h"

namespace fh2anna {

// Open All / Close All are the parts before PopupLights.
enum class Part { Roof, Hood, Trunk, DoorLF, DoorRF, DoorLR, DoorRR, Storage, StorageM, PopupLights, PopupL, PopupR, Spoiler, Count };

// The game thread (the HUD's update) only.
void* player_car();                     // this frame's (not kept: the game may replace it)
bool headlights_on(void* car);
void set_headlights(void* car, bool on);   // and the pop-up lights with them
void apply_headlights(void* car);           // each update of the car (the game rewrites them)
// While autodrive's on: the headlights on while it's dark round the car (a tunnel, the night),
// then as they were; not if the player chose them in ANNA. Each HUD update.
void dark_headlights_tick(void* car, bool driving);
bool part_supported(void* car, Part p); // the car animates it
bool part_open(void* car, Part p);      // as we last left it
void set_part(void* car, Part p, bool open);
void log_car_events(void* car);         // every animation event the car has, to the log (once a car)

bool hook_headlight_beams();            // headlights on shine in daylight too (from start)
bool hook_headlight_reads();            // ours in place wherever the game reads them (from start)
void note_headlights(void* car);        // at the car update's start: another mod switched them?

float car_speed(void* car);             // metres a second, or -1 if it can't be read
void close_doors_if_moving(void* car);  // the doors, hood, trunk and storage ANNA opened, shut once it moves

} // namespace fh2anna
