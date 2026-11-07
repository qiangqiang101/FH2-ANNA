// FH2 ANNA's own indicators, hazards and autodrive (as Extra Options does them, with its own
// state: keep Extra Options' bindings for these at None so the two don't both drive the car).
//
// Hooks are inline, on the functions' code: Extra Options replaces the vtable slots that point
// at them and calls them on from its own, so ours still run.
//
// Indicators: the car's +0x51BB hazards, +0x51BC left, +0x51BD right (the game's setters are
// 0xF755C0 / 0xF76F10 / 0xF774E0). Each car update (0xF5C900, from the update 0x2A9400) turns
// them into the lights' blink; the game resets them now and then, so while one is on all three
// are written before every update of the player's car. Left / right turn off once the steering
// has turned (|steer| >= 0.15) and come back (<= 0.05), as a stalk does; hazards stay on.
//
// Autodrive's cinematic camera: cinematic.cpp, from here (3 s in, as Extra Options' FH3 branch).
//
// Autodrive: the game's AI driver drives the sat-nav's route. 0xF79F10(car, ai) hands the car
// to the AI (its driver mode, car +0x4D30, 1 the player's, 0 the AI's) and back; the route's
// tracker (SatNavRouteTrackingComponent) has to be following it: 0x6BEC10(tracker) begins,
// 0x6A5FA0(tracker) ends. The sat-nav (SatNavComponent, its update 0x6B4D30) is active while
// +0x160; its route state **(+0x150): the route at +0x118 (valid while its +8 is 1), arrived
// at +0x8A. Starting and stopping happen in the sat-nav's update, as Extra Options does. It
// stops when the player touches the gas, the brake or the steering (read with the game's own
// GetAnalog, so FH2 Mouse & Keyboard's keys count), on arrival, or when anything changes.
//
// A route the game set (an event's, a race's, a road trip's) isn't the sat-nav's: it's in the
// car's CarRouteTrackingComponent bindings (car +0x4F70, vtable 0x2F5D850), the Event binding's
// route at +0x10 and, in an event, a road trip's GroupSatNav (+0x88) or PersonalSatNav (+0x60),
// each valid while its +8 is 1. The game tracks those itself (checkpoints, laps), so the AI just
// takes the car. That's driven from the player's car update, in free roam or while the game's
// presentation (0x354F4F8 +0x50 -> 0xF9AE70, its +0x3CC) is a driving one (3, 4, 6, 9, 12), as
// Extra Options' "autodrive in events" does; while there's such a route it's the one driven.
#include "drive.h"
#include "car.h"
#include "cinematic.h"

#include <atomic>
#include <cmath>

namespace fh2anna {
namespace {

constexpr uintptr_t RVA_CAR_UPDATE      = 0x2A9400;   // (car, float dt, float frame_dt, unsigned frame)
constexpr uintptr_t RVA_SATNAV_UPDATE   = 0x6B4D30;   // (satnav, float dt)
constexpr uintptr_t RVA_SATNAV_OFF      = 0x6C0C70;   // (satnav)
constexpr uintptr_t RVA_TRACKER_UPDATE  = 0x6B4E40;   // (tracker, float dt)
constexpr uintptr_t RVA_TRACKER_DTOR    = 0x6A26E0;   // (tracker, ...)
constexpr uintptr_t RVA_PAUSE_ENTER     = 0x8E98E0;   // (pause state): the pause menu opens
constexpr uintptr_t RVA_EVENT_ENTER     = 0x912860;   // (event context): an event starts
constexpr uintptr_t RVA_SET_AI          = 0xF79F10;   // (car, bool ai)
constexpr uintptr_t RVA_BEGIN_ROUTE     = 0x6BEC10;   // (tracker)
constexpr uintptr_t RVA_END_ROUTE       = 0x6A5FA0;   // (tracker)
constexpr uintptr_t RVA_LOCK_WEAK       = 0x3EB620;   // (weak*, shared* out {obj, counter})
constexpr uintptr_t RVA_CLEAR_REWIND    = 0x99C4C0;   // (rewind, 1, 1)
constexpr uintptr_t RVA_REWIND          = 0x34D5710;
constexpr uintptr_t RVA_REWIND_RECORDER = 0x34D5700;
constexpr uintptr_t RVA_PLAYER_MANAGER  = 0x34ABA60;
constexpr uintptr_t RVA_EVENT_CONTEXT   = 0x2129A0;   // (holder, shared* out)
constexpr uintptr_t RVA_CONTEXT_HOLDER  = 0x34C0F40;
constexpr uintptr_t RVA_GET_ANALOG      = 0x106F040;  // float (input, context, control)
constexpr uintptr_t RVA_INPUTS          = 0x365D7C8;
constexpr uintptr_t INPUTS_CURRENT      = 0xAB28;
constexpr uintptr_t RVA_PRESENTATIONS   = 0x354F4F8;
constexpr uintptr_t RVA_PRESENTATION    = 0xF9AE70;   // (manager, shared* out)
constexpr uintptr_t RVA_ROUTE_BINDINGS_VTABLE = 0x2F5D850;
constexpr uintptr_t CAR_ROUTE_BINDINGS  = 0x4F70;
constexpr uintptr_t CAR_DRIVER = 0x4D10, CAR_DRIVER_MODE = 0x4D30, CAR_SIGNALS = 0x51BB;
constexpr int kModePlayer = 1, kModeAi = 0;

using CarUpdateFn = uintptr_t (*)(void* car, float dt, float frame_dt, unsigned frame);
using UpdateFn    = uintptr_t (*)(void* self, float dt);
using OneFn       = uintptr_t (*)(void* self);
using DtorFn      = uintptr_t (*)(void* self, uintptr_t flags);
using SetAiFn     = void (*)(void* car, uint8_t ai);
using TrackerFn   = void (*)(void* tracker);
using LockFn      = void* (*)(void* weak, void** out);
using RewindFn    = void (*)(void* rewind, uint8_t a, uint8_t b);
using ContextFn   = void* (*)(void* holder, void** out);
using AnalogFn    = float (*)(void* input, int context, int control);
using ReleaseFn   = void (*)(void* self);

CarUpdateFn o_car_update = nullptr;
UpdateFn o_satnav_update = nullptr, o_tracker_update = nullptr;
OneFn o_satnav_off       = nullptr;
DtorFn o_tracker_dtor    = nullptr;
OneFn o_pause_enter      = nullptr;
OneFn o_event_enter      = nullptr;

std::atomic<void*> g_local_car{nullptr};

// ---- the player's inputs --------------------------------------------------------------------
float analog(int control) {
    uint8_t* all = *reinterpret_cast<uint8_t**>(g_base + RVA_INPUTS);
    void* input  = all ? *reinterpret_cast<void**>(all + INPUTS_CURRENT) : nullptr;
    return input ? game<AnalogFn>(RVA_GET_ANALOG)(input, 0, control) : 0.f;
}
float steering() { return analog(2); }

// ---- signals --------------------------------------------------------------------------------
std::atomic<int> g_signal{0};
void* g_signal_car = nullptr;        // whose they are (the car update's thread)
bool g_turned      = false;
std::atomic<bool> g_clear{false};    // a signal went off: clear the bytes once

void write_signals(uint8_t* car, int s) {
    car[CAR_SIGNALS]     = s == static_cast<int>(Signal::Hazards);
    car[CAR_SIGNALS + 1] = s == static_cast<int>(Signal::Left);
    car[CAR_SIGNALS + 2] = s == static_cast<int>(Signal::Right);
}

void signals_tick(uint8_t* car) {
    if (car != g_signal_car) {   // another car: its signals are its own
        g_signal_car = car;
        g_signal     = 0;
        g_turned     = false;
        return;
    }
    const int s = g_signal.load();
    if (g_clear.exchange(false) && !s) write_signals(car, 0);
    if (!s) return;
    if (s != static_cast<int>(Signal::Hazards)) {
        const float steer = std::fabs(steering());
        if (steer >= 0.15f) g_turned = true;
        else if (g_turned && steer <= 0.05f) {
            g_signal = 0, g_turned = false;
            write_signals(car, 0);
            return;
        }
    }
    write_signals(car, s);
    static bool said = false;
    if (!said) said = true, logf("signals: writing to the player's car");
}

// ---- autodrive ------------------------------------------------------------------------------
struct Shared {
    void* obj;
    uint8_t* counter;
};
void release_shared(Shared& s) {
    if (!s.counter) return;
    void** vt = *reinterpret_cast<void***>(s.counter);
    // A std::shared_ptr's control block: the strong count (+8); at 0 the object goes and its own
    // weak reference with it (+0xC), and at 0 the block.
    if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(s.counter + 8)) == 0) {
        reinterpret_cast<ReleaseFn>(vt[0])(s.counter);
        if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(s.counter + 0xC)) == 0) reinterpret_cast<ReleaseFn>(vt[1])(s.counter);
    }
    s = {};
}

std::atomic<uint8_t*> g_satnav{nullptr};
std::atomic<uint8_t*> g_tracker{nullptr};
std::atomic<bool> g_want{false};     // start / stop, at the sat-nav's next update
struct Drive {
    uint8_t* car     = nullptr;
    uint8_t* tracker = nullptr;
    void* route      = nullptr;
    float elapsed    = 0;       // seconds of it
    bool cinematic_tried = false;
    bool event       = false;   // the game's route (the car's update), not the sat-nav's
} g_drive;                           // the sat-nav's and the player's car's updates (one thread)
std::atomic<bool> g_driving{false};
std::atomic<void*> g_event_car{nullptr};   // the car with a route the game set, as last seen
std::atomic<bool> g_event_ready{false};    // ... and it may be driven now
std::atomic<bool> g_roam{true};            // free roam, as the player's car's update last saw

void* route_of(uint8_t* satnav, bool* arrived) {
    *arrived = false;
    if (!satnav || !satnav[0x160]) return nullptr;
    uint8_t** holder = *reinterpret_cast<uint8_t***>(satnav + 0x150);
    uint8_t* rc      = holder ? *holder : nullptr;
    if (!rc) return nullptr;
    uint8_t* route = *reinterpret_cast<uint8_t**>(rc + 0x118);
    if (!route || route[8] != 1) return nullptr;
    *arrived = rc[0x8A] != 0;
    return route;
}

int driver_mode(uint8_t* car) {
    void** holder = *reinterpret_cast<void***>(car + CAR_DRIVER);
    if (!holder || !*holder) return -1;
    return *reinterpret_cast<int*>(car + CAR_DRIVER_MODE);
}

bool set_ai(uint8_t* car, bool ai) {
    if (driver_mode(car) != (ai ? kModePlayer : kModeAi)) return false;
    game<SetAiFn>(RVA_SET_AI)(car, ai ? 1 : 0);
    return driver_mode(car) == (ai ? kModeAi : kModePlayer);
}

// Free roam, the game running: no events, no pause. `why` says what isn't, for the log.
int g_context = -1;   // the event context's type as last read (2: free roam)
bool free_roam(const char** why = nullptr) {
    auto no = [&](const char* w) {
        if (why) *why = w;
        return false;
    };
    uint8_t* mgr = *reinterpret_cast<uint8_t**>(g_base + RVA_PLAYER_MANAGER);
    uint8_t* sim = mgr ? *reinterpret_cast<uint8_t**>(mgr + 0x48) : nullptr;
    if (!sim) return no("no simulation");
    if (sim[0x28]) return no("paused");
    void* holder = *reinterpret_cast<void**>(g_base + RVA_CONTEXT_HOLDER);
    if (!holder) return no("no event context");
    Shared sh{};
    game<ContextFn>(RVA_EVENT_CONTEXT)(holder, reinterpret_cast<void**>(&sh));
    g_context = sh.obj ? *reinterpret_cast<int*>(static_cast<uint8_t*>(sh.obj) + 0x60) : -1;
    release_shared(sh);
    return g_context == 2 || no("not free roam");
}

// The game's presentation of what's on (-1: unknown).
int presentation_mode() {
    uint8_t* services = *reinterpret_cast<uint8_t**>(g_base + RVA_PRESENTATIONS);
    uint8_t* manager  = services ? *reinterpret_cast<uint8_t**>(services + 0x50) : nullptr;
    if (!manager || !*reinterpret_cast<void**>(manager + 8)) return -1;
    Shared sh{};
    game<ContextFn>(RVA_PRESENTATION)(manager, reinterpret_cast<void**>(&sh));
    const int mode = sh.obj ? *reinterpret_cast<int*>(static_cast<uint8_t*>(sh.obj) + 0x3CC) : -1;
    release_shared(sh);
    return mode;
}

// A route the game set for the car (an event's, a race's), or null.
void* event_route(uint8_t* car, bool roam) {
    uint8_t* bindings = *reinterpret_cast<uint8_t**>(car + CAR_ROUTE_BINDINGS);
    if (!bindings || *reinterpret_cast<uint8_t**>(bindings) != g_base + RVA_ROUTE_BINDINGS_VTABLE) return nullptr;
    static const uintptr_t kOffsets[] = {0x10, 0x88, 0x60};   // Event; in events, a road trip's too
    for (size_t i = 0; i < (roam ? 1u : 3u); ++i) {
        void** property = *reinterpret_cast<void***>(bindings + kOffsets[i]);
        uint8_t* route  = property ? static_cast<uint8_t*>(*property) : nullptr;
        if (route && route[8] == 1) return route;
    }
    return nullptr;
}

// The game's route may be driven: free roam, or a race / event with the car's controls the player's.
bool event_allowed(bool roam) {
    if (roam) return true;
    switch (presentation_mode()) {
        case 3: case 4: case 6: case 9: case 12: return game_driving_input();
        default: return false;
    }
}

// The tracker follows this sat-nav (its weak pointer at +0x98), and isn't following a route yet.
bool tracker_ready(uint8_t* tracker, uint8_t* satnav) {
    if (!tracker || !*reinterpret_cast<void**>(tracker + 0x130) || *reinterpret_cast<void**>(tracker + 0x128)) return false;
    Shared sh{};
    game<LockFn>(RVA_LOCK_WEAK)(tracker + 0x98, reinterpret_cast<void**>(&sh));
    const bool mine = sh.obj == satnav;
    release_shared(sh);
    return mine;
}

// `flash`: the player ended it, or it got there: the camera comes back with a flash.
void stop_driving(const char* why, bool flash = false) {
    if (!g_drive.car) return;
    cinematic_stop(flash);
    set_ai(g_drive.car, false);
    if (g_drive.tracker) game<TrackerFn>(RVA_END_ROUTE)(g_drive.tracker);
    if (*reinterpret_cast<void**>(g_base + RVA_REWIND_RECORDER))
        game<RewindFn>(RVA_CLEAR_REWIND)(*reinterpret_cast<void**>(g_base + RVA_REWIND), 1, 1);
    g_drive   = {};
    g_driving = false;
    logf("autodrive off (%s)", why);
}

void start_driving(uint8_t* satnav) {
    uint8_t* car     = static_cast<uint8_t*>(g_local_car.load());
    uint8_t* tracker = g_tracker.load();
    bool arrived     = false;
    void* route      = route_of(satnav, &arrived);
    if (!car || !route || arrived) return logf("autodrive: no route to drive");
    if (const char* why = nullptr; !free_roam(&why)) return logf("autodrive: only in free roam (%s, context %d)", why, g_context);
    if (driver_mode(car) != kModePlayer) return logf("autodrive: the car isn't the player's to give");
    if (!tracker_ready(tracker, satnav)) return logf("autodrive: the route isn't being tracked");
    game<TrackerFn>(RVA_BEGIN_ROUTE)(tracker);
    if (!*reinterpret_cast<void**>(tracker + 0x128)) return logf("autodrive: the route's tracking didn't begin");
    if (!set_ai(car, true)) {
        game<TrackerFn>(RVA_END_ROUTE)(tracker);
        return logf("autodrive: the AI didn't take the car");
    }
    g_drive   = {car, tracker, route, 0, false, false};
    g_driving = true;
    logf("autodrive on");
}

void start_event_drive(uint8_t* car, void* route, bool allowed) {
    if (!allowed) return logf("autodrive: not here (context %d, presentation %d)", g_context, presentation_mode());
    if (driver_mode(car) != kModePlayer) return logf("autodrive: the car isn't the player's to give");
    if (!set_ai(car, true)) return logf("autodrive: the AI didn't take the car");
    g_drive   = {car, nullptr, route, 0, false, true};
    g_driving = true;
    logf("autodrive on (the game's route, context %d)", g_context);
}

constexpr float kCinematicAfter = 3;   // seconds, as Extra Options

// What both kinds of drive check each update, after their own. Why it ends; with a flash when
// the player took over or it got there.
void driving_tick(float dt) {
    if (g_drive.car != g_local_car.load()) return stop_driving("another car");
    if (driver_mode(g_drive.car) != kModeAi) return stop_driving("the AI let go");
    if (analog(0) > 0.1f || analog(1) > 0.1f) return stop_driving("gas / brake", true);
    if (std::fabs(steering()) > 0.15f) return stop_driving("steering", true);
    if (cinematic_on() && !game_driving_input()) return stop_driving("a menu opened");   // it'd be hidden
    if (dt > 0 && dt <= 1) g_drive.elapsed += dt;
    if (!g_drive.cinematic_tried && cinematic_wanted() && g_drive.elapsed >= kCinematicAfter) {
        g_drive.cinematic_tried = true;
        if (!cinematic_start()) logf("autodrive: the cinematic camera isn't available here");
    } else {
        cinematic_tick();
    }
}

// The sat-nav's update: its route (the one the player set).
void drive_tick(uint8_t* satnav, float dt) {
    if (g_drive.event) return;   // the car's update has it
    // While the game set a route for the car, that's the one: the car's update starts it.
    if (!g_drive.car && g_event_car.load() && g_event_car.load() == g_local_car.load()) return;
    if (g_want.exchange(false)) {
        if (g_drive.car) stop_driving("asked", true);
        else start_driving(satnav);
    }
    if (!g_drive.car) return;
    bool arrived = false;
    void* route  = route_of(satnav, &arrived);
    if (arrived) return stop_driving("arrived", true);
    if (route != g_drive.route) return stop_driving("the route changed", true);
    if (!free_roam()) return stop_driving("not free roam");
    driving_tick(dt);
}

// The player's car's update: the route the game set (an event's, a race's).
void event_tick(uint8_t* car, float dt) {
    const bool roam = free_roam();
    g_roam          = roam || g_context == 2;   // paused in free roam is still free roam
    if (g_drive.car && !g_drive.event) {   // a sat-nav drive's on
        g_event_car = nullptr, g_event_ready = false;
        return;
    }
    void* route     = event_route(car, roam);
    const bool ok   = route && event_allowed(roam);
    g_event_car     = route ? car : nullptr;
    g_event_ready   = ok;
    static int was = -1;   // what's logged: free roam, a game's route, it may be driven
    const int now  = (g_roam.load() ? 1 : 0) | (route ? 2 : 0) | (ok ? 4 : 0);
    if (now != was) {
        was = now;
        logf("autodrive: %s, %s the game's route%s (context %d, presentation %d)", g_roam.load() ? "free roam" : "an event",
             route ? "with" : "without", route ? (ok ? ", drivable" : ", not drivable here") : "", g_context, presentation_mode());
    }
    if (!g_drive.car) {
        if (route && g_want.exchange(false)) start_event_drive(car, route, ok);
        return;
    }
    if (g_want.exchange(false)) return stop_driving("asked", true);
    if (car != g_drive.car) return stop_driving("another car");
    if (!route) return stop_driving("the route went", true);
    if (route != g_drive.route) return stop_driving("the route changed", true);
    if (!ok) return stop_driving("not here");
    driving_tick(dt);
}

// ---- the hooks ------------------------------------------------------------------------------
bool signals_guarded(uint8_t* car) {
    __try {
        signals_tick(car);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool event_guarded(uint8_t* car, float dt) {
    __try {
        event_tick(car, dt);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool drive_guarded(uint8_t* satnav, float dt) {
    __try {
        drive_tick(satnav, dt);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
void stop_guarded(const char* why) {
    __try {
        stop_driving(why);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_drive   = {};
        g_driving = false;
    }
}

uintptr_t d_car_update(void* car, float dt, float frame_dt, unsigned frame) {
    static bool failed = false;
    const bool mine = car && car == g_local_car.load();
    if (!failed && mine && !signals_guarded(static_cast<uint8_t*>(car))) failed = true, logf("the signals crashed; off");
    static bool event_failed = false;
    if (!event_failed && mine && !event_guarded(static_cast<uint8_t*>(car), dt)) {
        event_failed = true, g_event_car = nullptr, g_event_ready = false;
        if (g_drive.event) g_drive = {}, g_driving = false;
        logf("autodrive on the game's routes crashed; off");
    }
    if (mine) note_headlights(car), apply_headlights(car);
    const uintptr_t r = o_car_update(car, dt, frame_dt, frame);
    if (mine) apply_headlights(car);
    return r;
}

uintptr_t d_satnav_update(void* satnav, float dt) {
    const uintptr_t r = o_satnav_update(satnav, dt);
    static bool failed = false;
    g_satnav = static_cast<uint8_t*>(satnav);
    if (!failed && !drive_guarded(static_cast<uint8_t*>(satnav), dt)) failed = true, g_driving = false, logf("autodrive crashed; off");
    return r;
}

uintptr_t d_satnav_off(void* satnav) {
    if (satnav == g_satnav.load() && !g_drive.event) {
        stop_guarded("the sat-nav went");
        g_satnav = nullptr;
    }
    return o_satnav_off(satnav);
}

uintptr_t d_tracker_update(void* tracker, float dt) {
    g_tracker = static_cast<uint8_t*>(tracker);
    return o_tracker_update(tracker, dt);
}

uintptr_t d_tracker_dtor(void* tracker, uintptr_t flags) {
    if (tracker == g_drive.tracker) stop_guarded("the tracker went");
    uint8_t* expected = static_cast<uint8_t*>(tracker);
    g_tracker.compare_exchange_strong(expected, nullptr);
    return o_tracker_dtor(tracker, flags);
}

// The pause and an event's start end it (as Extra Options): the cinematic camera's hidden HUD
// would hide the pause menu too.
uintptr_t d_pause_enter(void* self) {
    stop_guarded("paused");
    return o_pause_enter(self);
}

uintptr_t d_event_enter(void* self) {
    stop_guarded("an event");
    return o_event_enter(self);
}

} // namespace

bool hook_drive() {
    struct H {
        const char* name;
        uintptr_t rva;
        void* detour;
        void** original;
    } hooks[] = {
        {"ANNA: the car's update (signals)", RVA_CAR_UPDATE, reinterpret_cast<void*>(&d_car_update), reinterpret_cast<void**>(&o_car_update)},
        {"ANNA: the sat-nav's update (autodrive)", RVA_SATNAV_UPDATE, reinterpret_cast<void*>(&d_satnav_update), reinterpret_cast<void**>(&o_satnav_update)},
        {"ANNA: the sat-nav going (autodrive)", RVA_SATNAV_OFF, reinterpret_cast<void*>(&d_satnav_off), reinterpret_cast<void**>(&o_satnav_off)},
        {"ANNA: the route tracking's update", RVA_TRACKER_UPDATE, reinterpret_cast<void*>(&d_tracker_update), reinterpret_cast<void**>(&o_tracker_update)},
        {"ANNA: the route tracking going", RVA_TRACKER_DTOR, reinterpret_cast<void*>(&d_tracker_dtor), reinterpret_cast<void**>(&o_tracker_dtor)},
        {"ANNA: the pause (autodrive)", RVA_PAUSE_ENTER, reinterpret_cast<void*>(&d_pause_enter), reinterpret_cast<void**>(&o_pause_enter)},
        {"ANNA: an event's start (autodrive)", RVA_EVENT_ENTER, reinterpret_cast<void*>(&d_event_enter), reinterpret_cast<void**>(&o_event_enter)},
    };
    bool all = true;
    for (H& h : hooks) {
        FH2Hook* k = g_api->hook_function(g_mod, h.name, g_base + h.rva, h.detour, h.original);
        if (k) g_api->enable_hooks(g_mod, k);
        else all = false;
    }
    return all;
}

void note_local_car(void* car) { g_local_car = car; }

Signal signal_on() { return static_cast<Signal>(g_signal.load()); }

void set_signal(Signal s) {
    const int now = g_signal.load();
    const int to  = now == static_cast<int>(s) ? 0 : static_cast<int>(s);
    g_turned      = false;
    g_signal      = to;
    if (!to) g_clear = true;
}

bool autodrive_possible() {
    if (g_driving.load()) return true;
    void* car = g_local_car.load();
    if (car && g_event_car.load() == car) return g_event_ready.load();
    bool arrived = false;
    return g_satnav.load() && route_of(g_satnav.load(), &arrived) && !arrived;
}

bool autodriving() { return g_driving.load(); }

bool in_free_roam() { return g_roam.load(); }

void toggle_autodrive() { g_want = true; }

} // namespace fh2anna
