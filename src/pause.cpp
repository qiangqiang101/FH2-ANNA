// Pause menu tiles, pressed for ANNA.
//
// The pause flow (stateflow pause.xml, "pause.root") waits in a WaitForPause behaviour (vtable
// 0x2D2F6F8) whose transitions are at +0x28 on, 0x30 apart, onPause first. Its update (vtable
// slot 14, 0x964810), each frame while it waits, takes onPause when Start is pressed:
// 0x1925260(behaviour, &transition). That reads the state flow's context from the thread's
// (TLS +0x148), there only while the flow updates: so ANNA's goes in from the same update.
// The pause menu is the page PauseMenuTiled (vtable 0x2E13E88); its update (slot 153, 0xDCE660)
// reads its buttons each frame, and from there ANNA's tile is pressed: its handler, called with
// the page (and, for the road trip's, the button: null takes the same path as a press):
//   PART_Button_TuneCar               0xDBFD60  "tune_car": ui_screens.tune_menu_without_upgrades
//   PART_Button_PublicOnlineRoadtrip  0xDBEDE0  online_public_roadtrip  (matchmaking: RoadTrip, public)
//   PART_Button_PublicOnlineFreeroam  0xDBED10  online_public_freeroam  (FreeRoam, public)
//   PART_Button_PrivateSession        0xDDF330  the private session's own choices
// The handlers check for themselves what the tile would (offline, in an event...) and say so.
// (ANNA presses only Tune Car's: the online ones are the event below, without the menu.)
//
// Matchmaking: the online tiles' flow ends in the behaviour NetworkFireStartMatchmakingEvent
// (stateflow network_matchmaking.xml fire_start_matchmaking; its OnEnter 0x8BC120), which:
//   the argument: 0x5DD1F0(out, public, island, location -1) for a road trip,
//                 0x5DD1C0(out, public, island) for free roam (12 bytes);
//   wraps it as the event's data: 0x6FE4D0(out {type, holder}, &argument), the holder let go
//                 with its vtable +0x18;
//   the world: 0x19242B0() the state flow's context (the thread's, while it updates), and
//                 0x8679F0(context, &shared_ptr) its world, a reference of ours;
//   sends it: 0x5499D0(world, &StartMatchmakingEvent 0x34BDAF8, &data).
// ANNA does the same from the pause flow's update (the context is there then).
#include "pause.h"

#include <atomic>
#include <cstring>

namespace fh2anna {
namespace {

constexpr uintptr_t RVA_WAIT_UPDATE = 0x964810;
constexpr uintptr_t RVA_PAGE_UPDATE = 0xDCE660;
constexpr uintptr_t RVA_TRANSITION  = 0x1925260;
constexpr uintptr_t WAIT_ON_PAUSE   = 0x28;
constexpr uintptr_t RVA_TILE[]      = {0xDBFD60};
const char* const kTileNames[]      = {"Tune Car"};
constexpr uintptr_t RVA_MM_ROADTRIP = 0x5DD1F0;
constexpr uintptr_t RVA_MM_FREEROAM = 0x5DD1C0;
constexpr uintptr_t RVA_EVENT_DATA  = 0x6FE4D0;
constexpr uintptr_t RVA_FLOW_CTX    = 0x19242B0;
constexpr uintptr_t RVA_CTX_WORLD   = 0x8679F0;
constexpr uintptr_t RVA_SEND_EVENT  = 0x5499D0;
constexpr uintptr_t RVA_MM_EVENT    = 0x34BDAF8;   // StartMatchmakingEvent
const char* const kSessionNames[]   = {"Online Road Trip", "Online Free Roam", "Private Road Trip", "Private Free Roam"};

struct SharedRef {
    void* obj;
    uint8_t* counter;
};
using RoadArgFn = void* (*)(void* out, uint8_t is_public, uint8_t island, int location);
using FreeArgFn = void* (*)(void* out, uint8_t is_public, uint8_t island);
using DataFn    = void* (*)(void* out, const void* argument);
using CtxFn     = void* (*)();
using WorldFn   = void* (*)(void* context, SharedRef* out);
using SendFn    = void (*)(void* world, const void* symbol, void* data);
using RelFn     = void (*)(void* self);

std::atomic<int> g_session{-1};   // the matchmaking asked for

void release_ref(SharedRef& s) {
    if (!s.counter) return;
    void** vt = *reinterpret_cast<void***>(s.counter);
    if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(s.counter + 8)) == 0) {
        reinterpret_cast<RelFn>(vt[0])(s.counter);
        if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(s.counter + 0xC)) == 0) reinterpret_cast<RelFn>(vt[1])(s.counter);
    }
    s = {};
}

// 0: sent, 1: no world, 2: crashed
int send_matchmaking(int session) {
    __try {
        void* context = game<CtxFn>(RVA_FLOW_CTX)();
        if (!context) return 1;
        SharedRef world{};
        game<WorldFn>(RVA_CTX_WORLD)(context, &world);
        if (!world.obj) return release_ref(world), 1;
        const bool is_public = session == static_cast<int>(Session::RoadTrip) || session == static_cast<int>(Session::FreeRoam);
        const bool road      = session == static_cast<int>(Session::RoadTrip) || session == static_cast<int>(Session::PrivateRoadTrip);
        alignas(16) uint8_t argument[16] = {};
        if (road) game<RoadArgFn>(RVA_MM_ROADTRIP)(argument, is_public, 0, -1);
        else game<FreeArgFn>(RVA_MM_FREEROAM)(argument, is_public, 0);
        void* data[2] = {};
        game<DataFn>(RVA_EVENT_DATA)(data, argument);
        game<SendFn>(RVA_SEND_EVENT)(world.obj, g_base + RVA_MM_EVENT, data);
        if (data[1]) reinterpret_cast<RelFn>((*reinterpret_cast<void***>(data[1]))[3])(data[1]);
        release_ref(world);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 2;
    }
}
constexpr DWORD kSettleMs           = 500;    // the pause menu's on screen and taking the pad
constexpr DWORD kGiveUpMs           = 5000;
constexpr DWORD kWaitingMs          = 1000;   // the pause flow's update seen this recently: it's waiting

using UpdateFn = uintptr_t (*)(void* self);
using FireFn   = void (*)(void* behaviour, void* transition);
using TileFn   = void (*)(void* page, void* button);

UpdateFn o_wait_update = nullptr, o_page_update = nullptr;

std::atomic<DWORD> g_wait_seen{0};   // the pause flow's update last ran
std::atomic<int> g_pending{-1};      // the tile asked for
std::atomic<int> g_stage{0};         // 0: the pause menu to open, 1: opening
std::atomic<DWORD> g_asked_at{0}, g_opened_at{0};
std::atomic<bool> g_page_logged{false};

void drop(int tile, const char* why) {
    logf("%s: %s", kTileNames[tile], why);
    g_pending = -1;
    g_stage   = 0;
}

bool fire_pause(void* wait) {
    __try {
        game<FireFn>(RVA_TRANSITION)(wait, static_cast<uint8_t*>(wait) + WAIT_ON_PAUSE);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool press(int tile, void* page) {
    __try {
        game<TileFn>(RVA_TILE[tile])(page, nullptr);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uintptr_t d_wait_update(void* self) {
    const uintptr_t r = o_wait_update(self);
    g_wait_seen       = GetTickCount();
    const int session = g_session.exchange(-1);
    if (session >= 0) {
        const int how = send_matchmaking(session);
        logf("%s: %s", kSessionNames[session], how == 0 ? "matchmaking started" : how == 1 ? "no state flow world; not started" : "starting matchmaking crashed");
    }
    const int tile    = g_pending.load();
    if (tile >= 0 && g_stage.load() == 0 && game_driving_input()) {
        if (!fire_pause(self)) return drop(tile, "opening the pause menu crashed"), r;
        g_opened_at = GetTickCount();
        g_stage     = 1;
    }
    return r;
}

uintptr_t d_page_update(void* page) {
    const uintptr_t r = o_page_update(page);
    if (!g_page_logged.exchange(true)) logf("the pause menu's update: seen");
    const int tile = g_pending.load();
    if (tile >= 0 && g_stage.load() == 1 && GetTickCount() - g_opened_at.load() >= kSettleMs) {
        g_pending = -1;
        g_stage   = 0;
        if (!press(tile, page)) logf("%s: the tile crashed", kTileNames[tile]);
        else logf("%s: pressed on the pause menu", kTileNames[tile]);
    }
    return r;
}

bool hook(const char* name, uintptr_t rva, const uint8_t* expect, std::size_t n, void* detour, void** original) {
    if (std::memcmp(g_base + rva, expect, n)) return false;
    FH2Hook* h = g_api->hook_function(g_mod, name, g_base + rva, detour, original);
    return h && g_api->enable_hooks(g_mod, h);
}

} // namespace

bool hook_pause() {
    static const uint8_t wait_update[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x70, 0x48, 0x8B};
    static const uint8_t page_update[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8B};
    static const uint8_t fire[]        = {0x48, 0x83, 0x7A, 0x10, 0x00, 0x8B, 0x0D};
    static const uint8_t tiles[][6]    = {{0x4C, 0x8B, 0xDC, 0x53, 0x48, 0x81}};
    // the matchmaking argument's makers, as they are
    static const uint8_t road_arg[]    = {0xB8, 0x01, 0x00, 0x00, 0x00, 0x41, 0xBA, 0x04, 0x00, 0x00, 0x00};
    static const uint8_t free_arg[]    = {0xB8, 0x02, 0x00, 0x00, 0x00, 0x41, 0xB9, 0x05, 0x00, 0x00, 0x00};
    static const uint8_t send[]        = {0xE9};   // a jump to the entity's event queue
    if (std::memcmp(g_base + RVA_TRANSITION, fire, sizeof fire)) return false;
    for (int i = 0; i < 1; ++i)
        if (std::memcmp(g_base + RVA_TILE[i], tiles[i], sizeof tiles[i])) return false;
    if (std::memcmp(g_base + RVA_MM_ROADTRIP, road_arg, sizeof road_arg) || std::memcmp(g_base + RVA_MM_FREEROAM, free_arg, sizeof free_arg) ||
        std::memcmp(g_base + RVA_SEND_EVENT, send, sizeof send))
        return false;
    return hook("ANNA: the pause flow (waiting for Start)", RVA_WAIT_UPDATE, wait_update, sizeof wait_update,
                reinterpret_cast<void*>(&d_wait_update), reinterpret_cast<void**>(&o_wait_update)) &&
           hook("ANNA: the pause menu (its update)", RVA_PAGE_UPDATE, page_update, sizeof page_update,
                reinterpret_cast<void*>(&d_page_update), reinterpret_cast<void**>(&o_page_update));
}

bool pause_available() { return o_wait_update && GetTickCount() - g_wait_seen.load() < kWaitingMs; }

void start_matchmaking(Session s) { g_session = static_cast<int>(s); }

void press_pause_tile(PauseTile t) {
    g_stage    = 0;
    g_asked_at = GetTickCount();
    g_pending  = static_cast<int>(t);
}

// Gives up on a tile the pause menu never got to.
void pause_tick() {
    const int tile = g_pending.load();
    if (tile < 0) return;
    const DWORD now = GetTickCount();
    if (g_stage.load() == 0 && now - g_asked_at.load() > kGiveUpMs) drop(tile, "the pause flow isn't waiting for Start; not done");
    else if (g_stage.load() == 1 && now - g_opened_at.load() > kGiveUpMs) drop(tile, "the pause menu didn't open");
}

} // namespace fh2anna
