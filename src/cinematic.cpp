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
// The field of view: each update the director asks its active camera (+0x208) for its FOV (its
// vtable +0x70) and keeps it (+0x2A0, its own +0x70's answer, 0xFE62BB). The relative cameras'
// (0x392FC0: a camera they wrap's, +0x738, or their own +0x7B8) is hooked in their vtable's slot:
// while the cinematic camera is on, its relative camera's is 30% narrower (its value is the
// other way round: larger is narrower).
//
// The HUD and the letterbox: the HUD page's own XAML (anna_xaml.cpp, letterbox()): while ANNA's
// display mode is kModeCinematic (fh2anna.cpp sets it while cinematic_on()), the HUD's controls
// hide and two black bars slide in at the top and the bottom.
#include "cinematic.h"
#include "lang.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <iterator>
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
constexpr int kGroupRelative = 13;
constexpr unsigned char kShotA = 17, kShotB = 19;
constexpr size_t kShot = 0x4D0;

using GroupFn  = int* (*)(void* list);
using SetFn    = void (*)(void* director, int group);
using ShotFn   = void (*)(void* director, unsigned char label);
using FlashFn  = void (*)(void* view, int kind, float seconds);
using GetFn    = void* (*)(void* self);

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

// ---- the field of view ------------------------------------------------------------------------
constexpr uintptr_t RVA_RELATIVE_FOV = 0x392FC0;   // the relative cameras' vtable +0x70
constexpr float kFovScale            = 1.f / 0.7f; // 30% narrower
using FovFn            = float (*)(void* camera);
FovFn o_relative_fov   = nullptr;
std::atomic<void*> g_narrow{nullptr};   // the relative camera that's narrower now

float d_relative_fov(void* camera) {
    const float fov = o_relative_fov(camera);
    return camera && camera == g_narrow.load() && std::isfinite(fov) ? fov * kFovScale : fov;
}

std::atomic<bool> g_on{false};

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

// Its button: FH2ANNA.ini's [Autodrive] Cinematic, a pad button's name (the d-pad's and the
// triggers' aren't: ANNA, the radio and the car have those), or None. Before, it was 1 / 0 (the
// camera on by itself a few seconds in, or not): either is LB now, written back as such.
struct Button {
    const char* name;
    uint16_t bit;
};
const Button kButtons[] = {{"LB", 0x0100}, {"RB", 0x0200}, {"A", 0x1000}, {"B", 0x2000}, {"X", 0x4000},
                           {"Y", 0x8000}, {"View", 0x0020}, {"L3", 0x0040}, {"R3", 0x0080}};
// Which: an index into kButtons, or kNone. Read from the game's threads, set from the ini and the
// Mods page's row.
constexpr int kNone   = static_cast<int>(std::size(kButtons));
std::atomic<int> g_button{0};
std::wstring g_cinematic_ini;

void set_button(int i) {
    g_button = i;
    WritePrivateProfileStringW(L"Autodrive", L"Cinematic", i == kNone ? L"None" : std::wstring(kButtons[i].name, kButtons[i].name + std::strlen(kButtons[i].name)).c_str(),
                               g_cinematic_ini.c_str());
}

} // namespace

void cinematic_settings() {
    g_cinematic_ini   = std::wstring(g_api->azimuth_dir) + L"\\fh2-mods\\FH2ANNA.ini";
    wchar_t value[32] = {};
    GetPrivateProfileStringW(L"Autodrive", L"Cinematic", L"", value, 32, g_cinematic_ini.c_str());
    std::string v;
    for (const wchar_t* c = value; *c; ++c) v += *c < 0x80 ? static_cast<char>(*c) : '?';
    int found = -1;
    if (!_stricmp(v.c_str(), "None")) found = kNone;
    for (int i = 0; i < kNone; ++i)
        if (!_stricmp(v.c_str(), kButtons[i].name)) found = i;
    if (found < 0) {   // missing, the old 1 / 0, or not a button: LB
        if (!v.empty() && v != "0" && v != "1") logf("FH2ANNA.ini: Cinematic=%s isn't a button (LB, RB, A, B, X, Y, View, L3, R3, None): LB", v.c_str());
        set_button(0);
    } else {
        g_button = found;
    }
    logf("autodrive's cinematic camera: %s", cinematic_button() ? cinematic_button_name() : "none");
    // Its row on the Mods page: the buttons by name, as the ini has them (choices: API 9; on an
    // older platform it'd be an On/Off row, so there it's the ini's alone).
    if (g_api->api_version < 9) return;
    static const char* choices[kNone + 1];
    for (int i = 0; i < kNone; ++i) choices[i] = kButtons[i].name;
    choices[kNone] = "None";
    FH2Setting s{};
    s.size         = sizeof s;
    s.key          = "CinematicButton";
    s.label        = tr("Cinematic camera button");
    s.description  = tr("While Auto Drive is on, this button switches to slow cinematic shots and back. None: no cinematic camera.");
    s.kind         = FH2_SETTING_CHOICE;
    s.choices      = choices;
    s.choice_count = kNone + 1;
    s.value        = g_button.load();
    s.on_change    = [](void*, const char*, int value) {
        if (value < 0 || value > kNone) return;
        set_button(value);
        logf("autodrive's cinematic camera: %s", cinematic_button() ? cinematic_button_name() : "none");
    };
    g_api->add_setting(g_mod, &s);
}

uint16_t cinematic_button() {
    const int i = g_button.load();
    return i < kNone ? kButtons[i].bit : 0;
}
const char* cinematic_button_name() {
    const int i = g_button.load();
    return i < kNone ? kButtons[i].name : "None";
}
int cinematic_button_index() {
    const int i = g_button.load();
    return i < kNone ? i : -1;
}

bool hook_cinematic() {
    // The relative cameras' FOV, in their vtable's slot (as long as it's still the game's own).
    auto** slot = reinterpret_cast<void**>(g_base + RVA_RELATIVE_VT + 0x70);
    FH2Hook* fov = *slot == g_base + RVA_RELATIVE_FOV
                       ? g_api->hook_pointer(g_mod, "ANNA: the replay cameras' FOV (cinematic)", slot, reinterpret_cast<void*>(&d_relative_fov),
                                             reinterpret_cast<void**>(&o_relative_fov))
                       : nullptr;
    if (!fov || !g_api->enable_hooks(g_mod, fov)) {
        logf("the replay cameras' FOV isn't as expected: the cinematic camera keeps its own");
        return false;
    }
    return true;
}

bool cinematic_on() { return g_on.load(); }

bool cinematic_start() {
    if (g_on.load() || !start_guarded()) return false;
    g_on = true;
    if (o_relative_fov) g_narrow = g_saved.relative;
    flash_guarded(g_saved.camera);
    logf("autodrive: cinematic camera");
    return true;
}

void cinematic_tick() {
    if (g_on.load()) cycle_guarded();
}

void cinematic_stop(bool with_flash) {
    if (!g_on.exchange(false)) return;
    g_narrow      = nullptr;
    void* camera  = nullptr;
    const bool ok = restore_guarded(&camera);
    if (with_flash && ok) flash_guarded(camera);
    logf("autodrive: the camera's back%s", ok ? "" : " (it had changed: left as it is)");
}

} // namespace fh2anna
