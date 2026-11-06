// Autodrive's cinematic camera, as Extra Options' FH3 branch does it (fh2/features/autodrive.c,
// autodrive_game.c, autodrive_ui.c), with its addresses.
//
// The camera: the primary view (the player manager 0x34ABA60 +0x48 the simulation, +0x98, +0x2D0
// the views, the first) gives its camera with its vtable +0x38: a camera director (vtable
// 0x2C6C840). Its groups (+0x188 .. +0x190, 64 bytes each, the camera at +0x30) are the camera
// modes; the current one comes from 0x399FF0(director + 0x1B0), and 0xFE5780(director, group)
// changes it. Group 13 is the replay's "relative" cameras (+0x2F0): a list of shots (+0x188 ->
// +8 .. +0x10, 0x4D0 bytes each, its label at +0x20), the one shown at +0x748 (a double).
// 0xFE5E30(director, label) picks the replay shot with that label and switches to group 13:
// label 17 is a slow orbit, 19 another, and the two take turns as each one's animation ends.
//
// The flash: 0xFDDAE0(view, 3, 0.6) is the game's white flash between cameras.
//
// The HUD: the UI draws through Application::Render's wait (0x1140ED0), draw (0x15B51A0) and
// present (0x15B5110), each reached only through a vtable slot (and the base classes' copies:
// Extra Options takes those slots, so the functions are hooked, inline); while hidden,
// they do nothing but let go of what they're handed. That hides every page, the HUD's and the
// menus': the pause and an event's start stop the autodrive (drive.cpp), which brings it back.
#include "cinematic.h"
#include "lang.h"

#include <atomic>
#include <string>
#include <vector>

namespace fh2anna {
namespace {

constexpr uintptr_t RVA_PLAYER_MANAGER = 0x34ABA60;
constexpr uintptr_t RVA_DIRECTOR_VT    = 0x2C6C840;
constexpr uintptr_t RVA_RELATIVE_VT    = 0x2C6CB70;
constexpr uintptr_t RVA_ANIM_PLAYER_VT = 0x2E779D8;
constexpr uintptr_t RVA_VIEW_VT_A      = 0x2E6E890;
constexpr uintptr_t RVA_VIEW_VT_B      = 0x2C66188;
constexpr uintptr_t RVA_CURRENT_GROUP  = 0x399FF0;   // int* (director + 0x1B0)
constexpr uintptr_t RVA_SET_GROUP      = 0xFE5780;   // (director, group)
constexpr uintptr_t RVA_REPLAY_SHOT    = 0xFE5E30;   // (director, label)
constexpr uintptr_t RVA_RELATIVE_SLOT4 = 0x3979A0;   // the relative cameras' vtable +0x20
constexpr uintptr_t RVA_FLASH          = 0xFDDAE0;   // (view, 3, 0.6f)
constexpr uintptr_t RVA_FREE_OBJECT    = 0x257730;
constexpr int kGroupRelative = 13;
constexpr unsigned char kShotA = 17, kShotB = 19;
constexpr size_t kShot = 0x4D0;

using GroupFn  = int* (*)(void* list);
using SetFn    = void (*)(void* director, int group);
using ShotFn   = void (*)(void* director, unsigned char label);
using FlashFn  = void (*)(void* view, int kind, float seconds);
using GetFn    = void* (*)(void* self);
using FreeFn   = void (*)(void* obj);

template <class T> bool get(const void* p, T* out) {
    if (!p || !readable(p, sizeof(T))) return false;
    *out = *static_cast<const T*>(p);
    return true;
}
bool ptr(const void* p, uintptr_t off, void** out) { return p && get(static_cast<const uint8_t*>(p) + off, out); }

uintptr_t method(void* obj, unsigned slot) {
    void* vt;
    uintptr_t fn;
    if (!get(obj, &vt) || !get(static_cast<uint8_t*>(vt) + slot, &fn)) return 0;
    return fn > reinterpret_cast<uintptr_t>(g_base) + 0x1000 && fn < reinterpret_cast<uintptr_t>(g_base) + 0x1DCC000 ? fn : 0;
}
bool has_vtable(void* obj, uintptr_t rva) {
    void* vt;
    return get(obj, &vt) && vt == g_base + rva;
}

void* primary_view() {
    void *mgr, *sim, *m, *views, *view;
    if (!get(g_base + RVA_PLAYER_MANAGER, &mgr) || !ptr(mgr, 0x48, &sim) || !ptr(sim, 0x98, &m) || !ptr(m, 0x2D0, &views) ||
        !get(views, &view))
        return nullptr;
    return view;
}

void* director() {
    void* view          = primary_view();
    const uintptr_t get_camera = view ? method(view, 0x38) : 0;
    if (!get_camera) return nullptr;
    void* camera = reinterpret_cast<GetFn>(get_camera)(view);
    return has_vtable(camera, RVA_DIRECTOR_VT) ? camera : nullptr;
}

bool flash(void* camera) {
    void* view = primary_view();
    void* view_camera;
    unsigned char busy;
    if ((!has_vtable(view, RVA_VIEW_VT_A) && !has_vtable(view, RVA_VIEW_VT_B)) || !camera || director() != camera) return false;
    if (!ptr(view, 0x40, &view_camera) || !method(view_camera, 0x178) || !get(static_cast<uint8_t*>(view) + 0xB1, &busy) || busy)
        return false;
    game<FlashFn>(RVA_FLASH)(view, 3, 0.6f);
    return true;
}

int group_of(void* camera) {
    void* head;
    size_t count;
    int value;
    if (!has_vtable(camera, RVA_DIRECTOR_VT) || !ptr(camera, 0x1B0, &head) || !get(static_cast<uint8_t*>(camera) + 0x1D0, &count) || !count)
        return -1;
    int* current = game<GroupFn>(RVA_CURRENT_GROUP)(static_cast<uint8_t*>(camera) + 0x1B0);
    return get(current, &value) ? value : -1;
}

bool set_group(void* camera, int group) {
    uint8_t *groups, *end;
    void* target;
    if (group < 0 || group > 31 || !has_vtable(camera, RVA_DIRECTOR_VT) || !ptr(camera, 0x188, reinterpret_cast<void**>(&groups)) ||
        !ptr(camera, 0x190, reinterpret_cast<void**>(&end)) || end < groups || static_cast<size_t>(end - groups) / 64 <= static_cast<unsigned>(group) ||
        !ptr(groups + static_cast<size_t>(group) * 64, 0x30, &target))
        return false;
    game<SetFn>(RVA_SET_GROUP)(camera, group);
    return group_of(camera) == group;
}

bool shots(void* camera, void** relative, uint8_t** begin, size_t* count) {
    void* data;
    uint8_t* end;
    if (!has_vtable(camera, RVA_DIRECTOR_VT) || !ptr(camera, 0x2F0, relative) || !ptr(*relative, 0x188, &data) ||
        !ptr(data, 8, reinterpret_cast<void**>(begin)) || !ptr(data, 0x10, reinterpret_cast<void**>(&end)) || end <= *begin ||
        static_cast<size_t>(end - *begin) % kShot || static_cast<size_t>(end - *begin) / kShot > 512)
        return false;
    *count = static_cast<size_t>(end - *begin) / kShot;
    return true;
}

bool has_shot(uint8_t* entries, size_t count, unsigned char label) {
    for (size_t i = 0; i < count; ++i) {
        unsigned char l;
        if (!get(entries + i * kShot + 0x20, &l)) return false;
        if (l == label) return true;
    }
    return false;
}

struct Saved {
    void* camera   = nullptr;
    void* relative = nullptr;
    int previous_group        = 0;
    double previous_selection = 0, selection = 0;
    unsigned char label       = 0;
    bool cycling              = false;
} g_saved;   // the sat-nav's update's thread

bool owns_camera() {
    void* relative;
    double selection;
    return g_saved.camera && director() == g_saved.camera && group_of(g_saved.camera) == kGroupRelative &&
           ptr(g_saved.camera, 0x2F0, &relative) && relative == g_saved.relative &&
           get(static_cast<uint8_t*>(relative) + 0x748, &selection) && selection == g_saved.selection;
}

bool shot_finished(void* relative, uint8_t* entry, size_t selection) {
    uint8_t* player = static_cast<uint8_t*>(relative) + 0x190;
    uint8_t *keys, *end;
    void* config;
    unsigned type, active;
    float elapsed, first, last;
    if (!has_vtable(relative, RVA_RELATIVE_VT) || !has_vtable(player, RVA_ANIM_PLAYER_VT) || !get(entry + 0x24, &type) || type != 0 ||
        !get(static_cast<uint8_t*>(relative) + 0x740, &active) || active != selection || !ptr(player, 0x188, &config) ||
        config != entry + 0x80 || !ptr(player, 0x190, reinterpret_cast<void**>(&keys)) || !ptr(player, 0x198, reinterpret_cast<void**>(&end)) ||
        end <= keys || static_cast<size_t>(end - keys) % 64 || static_cast<size_t>(end - keys) / 64 > 4096 || !get(keys, &first) ||
        !get(end - 64, &last) || !get(player + 0x288, &elapsed) || !(first >= 0 && last > first && last <= 600) ||
        !(elapsed >= first && elapsed <= last + 1))
        return false;
    return elapsed >= last - 0.001f;
}

bool slow_spin() {
    void* camera = director();
    void* relative;
    uint8_t *entries, *groups, *groups_end;
    size_t count;
    double previous_selection;
    const int previous = group_of(camera);
    if ((previous != 0 && previous != 1) || !shots(camera, &relative, &entries, &count) ||
        !get(static_cast<uint8_t*>(relative) + 0x748, &previous_selection) ||
        !(previous_selection >= 0 && previous_selection < static_cast<double>(count)) ||
        !ptr(camera, 0x188, reinterpret_cast<void**>(&groups)) || !ptr(camera, 0x190, reinterpret_cast<void**>(&groups_end)) ||
        groups_end <= groups || static_cast<size_t>(groups_end - groups) % 64 || static_cast<size_t>(groups_end - groups) / 64 < 19 ||
        static_cast<size_t>(groups_end - groups) / 64 > 32)
        return false;
    void* target;
    if (!has_shot(entries, count, kShotA) || !ptr(groups + kGroupRelative * 64, 0x30, &target) || target != relative) return false;
    g_saved = {camera, relative, previous, previous_selection, 0, kShotA, true};
    game<ShotFn>(RVA_REPLAY_SHOT)(camera, kShotA);
    double selection;
    unsigned char label = 0;
    const bool selected = get(static_cast<uint8_t*>(relative) + 0x748, &selection) && selection >= 0 &&
                          selection < static_cast<double>(count) && get(entries + static_cast<size_t>(selection) * kShot + 0x20, &label) &&
                          label == kShotA && group_of(camera) == kGroupRelative;
    if (!selected) {
        set_group(camera, previous);
        *reinterpret_cast<double*>(static_cast<uint8_t*>(relative) + 0x748) = previous_selection;
        g_saved = {};
        return false;
    }
    g_saved.selection = selection;
    return true;
}

void cycle() {
    if (!g_saved.cycling) return;
    void* relative;
    uint8_t* entries;
    size_t count;
    if (!owns_camera() || !shots(g_saved.camera, &relative, &entries, &count) ||
        !(g_saved.selection >= 0 && g_saved.selection < static_cast<double>(count)) ||
        g_saved.selection != static_cast<double>(static_cast<size_t>(g_saved.selection))) {
        g_saved.cycling = false;
        return;
    }
    uint8_t* entry = entries + static_cast<size_t>(g_saved.selection) * kShot;
    unsigned char label;
    if (!get(entry + 0x20, &label) || label != g_saved.label) {
        g_saved.cycling = false;
        return;
    }
    if (!shot_finished(relative, entry, static_cast<size_t>(g_saved.selection))) return;
    const unsigned char next = g_saved.label == kShotA ? kShotB : kShotA;
    void* active;
    if (!has_shot(entries, count, next) || !ptr(g_saved.camera, 0x208, &active) || active != relative ||
        method(relative, 0x20) != reinterpret_cast<uintptr_t>(g_base + RVA_RELATIVE_SLOT4)) {
        g_saved.cycling = false;
        logf("autodrive: no next cinematic shot; this one stays");
        return;
    }
    game<ShotFn>(RVA_REPLAY_SHOT)(g_saved.camera, next);
    double selection;
    if (!get(static_cast<uint8_t*>(relative) + 0x748, &selection) || !(selection >= 0 && selection < static_cast<double>(count)) ||
        !get(entries + static_cast<size_t>(selection) * kShot + 0x20, &label) || label != next || group_of(g_saved.camera) != kGroupRelative) {
        g_saved.cycling = false;
        logf("autodrive: the next cinematic shot didn't take; this one stays");
        return;
    }
    g_saved.selection = selection;
    g_saved.label     = label;
}

bool restore() {
    bool restored = false;
    if (owns_camera() && set_group(g_saved.camera, g_saved.previous_group)) {
        *reinterpret_cast<double*>(static_cast<uint8_t*>(g_saved.relative) + 0x748) = g_saved.previous_selection;
        restored = true;
    }
    g_saved = {};
    return restored;
}

// ---- the HUD ---------------------------------------------------------------------------------
std::atomic<bool> g_hidden{false};
std::atomic<bool> g_hud_hooked{false};
std::atomic<bool> g_on{false};
std::atomic<bool> g_wanted{true};
std::wstring g_ini;

using WaitFn    = void (*)(void* renderer, void* surface, void* parameters);
using DrawFn    = void (*)(void* app, void* screen, void* transform);
using PresentFn = void (*)(void* app, void* transform);
constexpr uintptr_t RVA_UI_WAIT    = 0x1140ED0;   // (renderer, surface, parameters)
constexpr uintptr_t RVA_UI_DRAW    = 0x15B51A0;   // (application, screen, transform)
constexpr uintptr_t RVA_UI_PRESENT = 0x15B5110;   // (application, transform)
WaitFn o_wait       = nullptr;
DrawFn o_draw       = nullptr;
PresentFn o_present = nullptr;

void d_wait(void* renderer, void* surface, void* parameters) {
    if (!g_hidden.load()) return o_wait(renderer, surface, parameters);
    // the graphics wrapper's Release is at +0x18
    if (void* owned = *static_cast<void**>(surface)) (*reinterpret_cast<void (***)(void*)>(owned))[3](owned);
}
void d_draw(void* app, void* screen, void* transform) {
    if (!g_hidden.load()) return o_draw(app, screen, transform);
    // Render takes the screen it's handed even when it draws nothing
    auto* root = *static_cast<uint8_t**>(screen);
    if (root && InterlockedDecrement(reinterpret_cast<volatile LONG*>(root + 0x10)) == 0) game<FreeFn>(RVA_FREE_OBJECT)(root);
}
void d_present(void* app, void* transform) {
    if (!g_hidden.load()) o_present(app, transform);
}

bool start_guarded() {
    __try {
        return slow_spin();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_saved = {};
        return false;
    }
}
void cycle_guarded() {
    __try {
        cycle();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_saved.cycling = false;
    }
}
bool restore_guarded(void** camera) {
    __try {
        *camera = g_saved.camera;
        return restore();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_saved = {};
        return false;
    }
}
void flash_guarded(void* camera) {
    __try {
        flash(camera);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void setting_changed(void*, const char*, int value) {
    g_wanted = value != 0;
    WritePrivateProfileStringW(L"Autodrive", L"Cinematic", value ? L"1" : L"0", g_ini.c_str());
}

} // namespace

void cinematic_settings() {
    g_ini   = std::wstring(g_api->azimuth_dir) + L"\\fh2-mods\\FH2ANNA.ini";
    g_wanted = GetPrivateProfileIntW(L"Autodrive", L"Cinematic", 1, g_ini.c_str()) != 0;
    if (g_api->api_version < 4) return;
    FH2Setting s{};
    s.size        = sizeof s;
    s.key         = "AutodriveCinematic";
    s.label       = tr("Autodrive cinematic camera");
    s.description = tr("A few seconds into ANNA's autodrive, the camera turns to slow orbiting shots of the car and the HUD hides. Taking over brings them back.");
    s.value       = g_wanted.load() ? 1 : 0;
    s.on_change   = setting_changed;
    g_api->add_setting(g_mod, &s);
}

bool hook_cinematic() {
    // The functions themselves, inline: Extra Options takes their vtable slots (for its own
    // autodrive's) and calls them on, so these still run. Nothing but those slots reaches them.
    struct F {
        uintptr_t rva;
        const char* name;
        void* detour;
        void** out;
    } fns[] = {
        {RVA_UI_WAIT, "ANNA: the UI's render wait (cinematic)", reinterpret_cast<void*>(&d_wait), reinterpret_cast<void**>(&o_wait)},
        {RVA_UI_DRAW, "ANNA: the UI's draw (cinematic)", reinterpret_cast<void*>(&d_draw), reinterpret_cast<void**>(&o_draw)},
        {RVA_UI_PRESENT, "ANNA: the UI's present (cinematic)", reinterpret_cast<void*>(&d_present), reinterpret_cast<void**>(&o_present)},
    };
    std::vector<FH2Hook*> made;
    for (const F& f : fns) {
        FH2Hook* h = g_api->hook_function(g_mod, f.name, g_base + f.rva, f.detour, f.out);
        if (!h) {
            logf("the UI's drawing isn't as expected: the cinematic camera keeps the HUD");
            return false;   // the ones made stay off
        }
        made.push_back(h);
    }
    for (FH2Hook* h : made) g_api->enable_hooks(g_mod, h);
    g_hud_hooked = true;
    return true;
}

bool cinematic_wanted() { return g_wanted.load(); }
bool cinematic_on() { return g_on.load(); }

bool cinematic_start() {
    if (g_on.load() || !start_guarded()) return false;
    g_on = true;
    if (g_hud_hooked.load()) g_hidden = true;
    flash_guarded(g_saved.camera);
    logf("autodrive: cinematic camera");
    return true;
}

void cinematic_tick() {
    if (g_on.load()) cycle_guarded();
}

void cinematic_stop(bool with_flash) {
    if (!g_on.exchange(false)) return;
    g_hidden      = false;
    void* camera  = nullptr;
    const bool ok = restore_guarded(&camera);
    if (with_flash && ok) flash_guarded(camera);
    logf("autodrive: the camera's back%s", ok ? "" : " (it had changed: left as it is)");
}

} // namespace fh2anna
