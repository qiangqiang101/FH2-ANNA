// Routes: the game's own CareerNavigationModule, the module ANNA's voice commands used on the
// Xbox (stateflow anna_flow.xml: ModuleMethodCall module_id="CareerNavigationModule"
// method="SetDestinationToNearest..."). Its instance is caught as it's made (0x462930).
#include "actions.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <atomic>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <string>

namespace fh2anna {
namespace {

// ---- routes ---------------------------------------------------------------------------------
constexpr uintptr_t RVA_NAV_CTOR         = 0x462930;   // CareerNavigationModule::CareerNavigationModule(this)
constexpr uintptr_t RVA_NAV_DTOR         = 0x4685D0;   // ... ~CareerNavigationModule(this)
constexpr uintptr_t RVA_NEAREST_EVENT    = 0x4825B0;   // SetDestinationToNearestCareerEvent(this)
constexpr uintptr_t RVA_NEAREST_BUCKET   = 0x4823E0;   // SetDestinationToNearestBucketList(this)
constexpr uintptr_t RVA_NEAREST_FESTIVAL = 0x4829E0;   // SetDestinationToNearestRoadtripLocation(this)
using NavFn    = void (*)(void* nav);
using CtorFn   = void* (*)(void* self);   // a constructor gives back `this`, a destructor whatever's in rax
CtorFn o_nav_ctor = nullptr, o_nav_dtor = nullptr;
std::atomic<void*> g_nav{nullptr};

void* d_nav_ctor(void* self) {
    void* r = o_nav_ctor(self);
    g_nav   = self;
    return r;
}
void* d_nav_dtor(void* self) {
    void* expected = self;
    g_nav.compare_exchange_strong(expected, nullptr);
    return o_nav_dtor(self);
}

uintptr_t route_rva(Route r) {
    switch (r) {
        case Route::Race: return RVA_NEAREST_EVENT;
        case Route::BucketList: return RVA_NEAREST_BUCKET;
        case Route::Festival: return RVA_NEAREST_FESTIVAL;
        default: break;
    }
    return 0;
}

// Bonus boards ("DiscountBoard"): 0x482740(nav, Result* out) routes to the nearest one discovered
// and not yet smashed; Result {Entity*, counter*, bool found at +0x10}, its reference ours.
// Barn finds: 0x715A70(unused, SharedPtr<World>* ctx) routes to the nearest rumour's hint (as
// ANNA's barn-find recommendation does); ctx is consumed, made with 0x1908670(nav + 8, &ctx),
// the nav module's world.
constexpr uintptr_t RVA_NEAREST_BOARD = 0x482740;
constexpr uintptr_t RVA_NEAREST_BARN  = 0x715A70;
constexpr uintptr_t RVA_LOCK_WORLD    = 0x1908670;
struct SharedRef {
    void* obj;
    uint8_t* counter;
};
using BoardFn = void* (*)(void* nav, void* out);
using BarnFn  = void (*)(void* unused, SharedRef* ctx);
using LockFn  = void* (*)(void* weak, SharedRef* out);
using RelFn   = void (*)(void* self);

void release_ref(SharedRef& s) {
    if (!s.counter) return;
    void** vt = *reinterpret_cast<void***>(s.counter);
    // A std::shared_ptr's control block: the strong count (+8); at 0 the object goes and its own
    // weak reference with it (+0xC), and at 0 the block.
    if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(s.counter + 8)) == 0) {
        reinterpret_cast<RelFn>(vt[0])(s.counter);
        if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(s.counter + 0xC)) == 0) reinterpret_cast<RelFn>(vt[1])(s.counter);
    }
    s = {};
}

bool board_route(void* nav) {
    alignas(16) uint8_t out[0x18] = {};
    game<BoardFn>(RVA_NEAREST_BOARD)(nav, out);
    auto* ref        = reinterpret_cast<SharedRef*>(out);
    const bool found = out[0x10] != 0;
    release_ref(*ref);
    return found;
}

void barn_route(void* nav) {
    SharedRef ctx{};
    game<LockFn>(RVA_LOCK_WORLD)(static_cast<uint8_t*>(nav) + 8, &ctx);
    if (ctx.obj) game<BarnFn>(RVA_NEAREST_BARN)(nullptr, &ctx);   // it lets go of ctx
}

bool call_route(Route r, void* nav) {
    __try {
        if (r == Route::BonusBoard) return board_route(nav);
        if (r == Route::BarnFind) return barn_route(nav), true;
        game<NavFn>(route_rva(r))(nav);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// "What should I do?": each kind of place in turn, its route's length read from the HUD (the
// distance under the minimap, HUDStatusAnna+0x128, an AVUI String: its text at +0x28, length
// at +0x30) once it's had time to be worked out; then the nearest again.
constexpr uintptr_t STATUS_DISTANCE = 0x128;
constexpr DWORD kRouteSettleMs      = 600;
const Route kNearestOf[]            = {Route::Race, Route::BucketList, Route::Festival, Route::BarnFind, Route::BonusBoard};
struct Search {
    int next      = -1;   // the kind being measured, or -1
    DWORD read_at = 0;
    std::wstring before;  // the distance before it was set
    double best   = 1e30;
    int best_kind = -1;
} g_search;

bool distance_raw(uint8_t* status, wchar_t* out, int cap) {
    __try {
        uint8_t* s = *reinterpret_cast<uint8_t**>(status + STATUS_DISTANCE);
        if (!s || !readable(s, 0x38)) return false;
        const wchar_t* text = *reinterpret_cast<const wchar_t**>(s + 0x28);
        const int length    = *reinterpret_cast<int*>(s + 0x30);
        if (!text || length <= 0 || length >= cap || !readable(text, length * sizeof(wchar_t))) return false;
        std::memcpy(out, text, length * sizeof(wchar_t));
        out[length] = 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::wstring distance_text(uint8_t* status) {
    wchar_t buf[64] = {};
    return distance_raw(status, buf, 64) ? std::wstring(buf) : std::wstring();
}

// "1.2 km", "350 m", "0.8 mi", "900 ft" in metres; -1 if it isn't a distance.
double metres(const std::wstring& t) {
    std::size_t i = 0;
    while (i < t.size() && !iswdigit(t[i])) ++i;
    if (i == t.size()) return -1;
    std::wstring num;
    for (; i < t.size() && (iswdigit(t[i]) || t[i] == L'.' || t[i] == L','); ++i) num += t[i] == L',' ? L'.' : t[i];
    const double v = wcstod(num.c_str(), nullptr);
    std::wstring unit;
    for (; i < t.size(); ++i)
        if (iswalpha(t[i])) unit += static_cast<wchar_t>(towlower(t[i]));
    if (unit.rfind(L"km", 0) == 0) return v * 1000;
    if (unit.rfind(L"mi", 0) == 0) return v * 1609.344;
    if (unit.rfind(L"ft", 0) == 0) return v * 0.3048;
    if (unit.rfind(L"m", 0) == 0) return v;
    return -1;
}

} // namespace

// ---- the API --------------------------------------------------------------------------------

bool hook_routes() {
    static const uint8_t ctor_bytes[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20};
    static const uint8_t dtor_bytes[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x48, 0x89, 0x7C, 0x24, 0x18};
    if (std::memcmp(g_base + RVA_NAV_CTOR, ctor_bytes, sizeof ctor_bytes) ||
        std::memcmp(g_base + RVA_NAV_DTOR, dtor_bytes, sizeof dtor_bytes))
        return false;
    FH2Hook* a = g_api->hook_function(g_mod, "ANNA: the navigation module (made)", g_base + RVA_NAV_CTOR,
                                      reinterpret_cast<void*>(&d_nav_ctor), reinterpret_cast<void**>(&o_nav_ctor));
    FH2Hook* b = g_api->hook_function(g_mod, "ANNA: the navigation module (gone)", g_base + RVA_NAV_DTOR,
                                      reinterpret_cast<void*>(&d_nav_dtor), reinterpret_cast<void**>(&o_nav_dtor));
    if (!a || !b) return false;
    g_api->enable_hooks(g_mod, a);
    g_api->enable_hooks(g_mod, b);
    return true;
}

bool routes_available() { return g_nav.load() != nullptr; }

bool set_route(Route r) {
    void* nav = g_nav.load();
    if (!nav) {
        logf("the navigation module isn't there yet: no route");
        return false;
    }
    if (!call_route(r, nav)) {
        logf("no route (none to go to, or it crashed)");
        return false;
    }
    return true;
}

void start_nearest_search(uint8_t* status) {
    g_search        = Search{};
    g_search.next   = 0;
    g_search.before = distance_text(status);
    set_route(kNearestOf[0]);
    g_search.read_at = GetTickCount() + kRouteSettleMs;
}

void nearest_search_tick(uint8_t* status) {
    if (g_search.next < 0 || static_cast<LONG>(GetTickCount() - g_search.read_at) < 0) return;
    const std::wstring now = distance_text(status);
    const double m         = now != g_search.before ? metres(now) : -1;   // unchanged: no route for that kind
    if (m >= 0 && m < g_search.best) g_search.best = m, g_search.best_kind = g_search.next;
    g_search.before = now;
    if (++g_search.next < static_cast<int>(sizeof kNearestOf / sizeof kNearestOf[0])) {
        set_route(kNearestOf[g_search.next]);
        g_search.read_at = GetTickCount() + kRouteSettleMs;
        return;
    }
    g_search.next = -1;
    if (g_search.best_kind < 0) {
        logf("what should I do: nothing to drive to");
        return;
    }
    set_route(kNearestOf[g_search.best_kind]);
    logf("what should I do: the nearest is %d (%.0f m)", g_search.best_kind, g_search.best);
}

} // namespace fh2anna
