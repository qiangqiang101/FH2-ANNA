// FH2 ANNA: the ANNA menu Forza Horizon 2 left on its HUD, woken up, with FH3's controls.
//
//   D-pad down (a tap) / C       open ANNA (a hold of D-pad down is still the game's: the telemetry)
//   D-pad up / down,  I / K      move the highlight
//   D-pad right,      L          choose: a list opens, anything else ANNA shows as chosen, closes and does
//   D-pad left,       J          back, or close
// While ANNA is open the d-pad is ANNA's: the game and Extra Options don't see it.
//
// What's known (24 Nov 2015 build):
//  - The driving HUD (A_Hud.xaml) has a HUDAnnaControl, bound to HUDStatusAnna (0x148 bytes).
//  - HUDAnnaComponent's update (0x74D670) keeps HUDStatusAnna at +0xD8. Each frame it compares
//    AnnaDisplayMode (+0x13C, read by 0xAABDF0) with the Options/Map setting and sets it
//    (0xAACCD0) when they differ. 0: hidden, 2: the minimap only, 3: the minimap and ANNA.
//  - HUDStatusAnna+0x130: ObservableCollection<AnnaCommandItem> (OptionsList): its list at
//    +0x50, the count at list+0x30, the items at [[list+0x28]+0x28].
//  - AnnaCommandItem (0x48 bytes): Command, an AVUI String*, at +0x38; IsSelected, a byte, at
//    +0x40 (property 0xAAC210); 0xAA14A0(item, property) raises PropertyChanged.
//  - HUDStatusAnna's own functions, as HUDAnnaComponent uses them (0x755600 / 0x755700):
//    0xA99F60(status) empties the list; 0xA987B0(status, String*&) adds a command and, the first
//    time, raises its ShowFull event (the control wakes, with ANNA's sound); 0xAA77F0(status,
//    String*&) selects the command with that text and raises Selected; 0xAA9BE0(status) raises
//    ShowMinimized (asleep again). The String*& ones release the String they're given (a
//    TRefCountedPtr by value): the caller adds a reference first.
#include "actions.h"
#include "car.h"
#include "drive.h"
#include "cinematic.h"
#include "common.h"
#include "fh2anna.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <string>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace fh2anna {

const FH2ModApi* g_api = nullptr;
FH2Mod* g_mod          = nullptr;
uint8_t* g_base        = nullptr;

bool readable(const void* p, std::size_t n) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (!p || VirtualQuery(p, &mbi, sizeof mbi) != sizeof mbi || mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    return static_cast<const char*>(p) + n <= static_cast<const char*>(mbi.BaseAddress) + mbi.RegionSize;
}

void logf(const char* fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    g_api->log(g_mod, line);
}

namespace {

constexpr uintptr_t RVA_GET_MODE_CALL = 0x74D7FD;   // HUDAnnaComponent update: call 0xAABDF0(status)
constexpr uintptr_t RVA_SET_MODE_CALL = 0x74D80F;   // ... call 0xAACCD0(status, mode)
constexpr uintptr_t RVA_ITEM_NOTIFY   = 0xAA14A0;   // (item, property)
constexpr uintptr_t RVA_SELECTED_PROP = 0xAAC210;   // AnnaCommandItem::IsSelected's property
constexpr uintptr_t RVA_AVUI_STRING   = 0x1663220;  // String* (String* out, const char*, int length or -1)
constexpr uintptr_t RVA_STATUS_CLEAR  = 0xA99F60;   // (status)
constexpr uintptr_t RVA_STATUS_ADD    = 0xA987B0;   // (status, String*&), releases the String
constexpr uintptr_t RVA_STATUS_SELECT = 0xAA77F0;   // (status, String*&), releases the String
constexpr uintptr_t RVA_STATUS_SLEEP  = 0xAA9BE0;   // (status)
constexpr uintptr_t STATUS_ITEMS      = 0x130;
constexpr uintptr_t ITEM_SELECTED     = 0x40;
constexpr int kModeMapOnly            = 2;   // Options/Map's "on" on PC
constexpr int kModeAnna               = 3;   // the map and ANNA (asleep until opened)
constexpr DWORD kChosenMs             = 700;   // ANNA shows a choice this long, then closes
constexpr DWORD kIdleMs               = 10000; // and closes by itself after this long untouched

using GetModeFn   = int (*)(uint8_t* status);
using SetModeFn   = void (*)(uint8_t* status, int mode);
using NotifyFn    = void (*)(uint8_t* item, void* prop);
using PropFn      = void* (*)();
using StringFn    = void* (*)(void* out, const char* text, int length);
using StatusFn    = void (*)(uint8_t* status);
using StatusStrFn = void (*)(uint8_t* status, uint8_t** text);

GetModeFn o_get_mode = nullptr;
SetModeFn o_set_mode = nullptr;

uint8_t* avui_string(const char* text) {
    auto* s = new uint8_t[0x80]();   // ours for good: one reference of ours is never let go
    game<StringFn>(RVA_AVUI_STRING)(s, text, -1);
    InterlockedIncrement(reinterpret_cast<volatile LONG*>(s + 0x10));
    return s;
}

// Calls a HUDStatusAnna function that takes a String*& and releases it: with a reference for it.
void with_text(uintptr_t rva, uint8_t* status, uint8_t* text) {
    InterlockedIncrement(reinterpret_cast<volatile LONG*>(text + 0x10));
    uint8_t* t = text;
    game<StatusStrFn>(rva)(status, &t);
}

// ---- the menu -------------------------------------------------------------------------------
// Entries are ANNA's own and other mods' (include/fh2anna.h), in the order they were added.
struct Entry {
    std::string key, parent, label;   // parent "": the top list
    FH2AnnaChooseFn on_choose = nullptr;   // null: a list
    FH2AnnaShownFn shown      = nullptr;
    FH2AnnaLabelFn label_now  = nullptr;
    void* user                = nullptr;
    bool stays_open           = false;
    FH2AnnaShownFn enabled    = nullptr;
};
std::mutex g_entries_mu;
std::vector<Entry> g_entries;   // guarded by g_entries_mu; never shrinks

// Greyed-out entries: their item's text is kOff and their label, which the row template draws as
// the label, grey, with a trigger for each such text that FH2 ANNA puts in the control's XAML as
// it's read (disabled_triggers): the labels of the entries that can be greyed out by then.
const char kOff[] = "FH2ANNA_OFF:";
std::mutex g_off_mu;
std::vector<std::string> g_off_labels;   // guarded by g_off_mu

bool can_grey(const std::string& label) {
    std::scoped_lock lk{g_off_mu};
    for (const std::string& l : g_off_labels)
        if (l == label) return true;
    return false;
}

std::string xml_escaped(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            default: o += c;
        }
    }
    return o;
}

// The AVUI Strings of the labels shown, by text (each made once, kept).
std::vector<std::pair<std::string, uint8_t*>> g_texts;   // the HUD's thread
uint8_t* text_for(const std::string& label) {
    for (auto& [t, s] : g_texts)
        if (t == label) return s;
    g_texts.emplace_back(label, avui_string(label.c_str()));
    return g_texts.back().second;
}

int add_entry(const FH2AnnaEntry* e) {
    if (!e || e->size < sizeof(uint32_t) * 2 || !e->key || !*e->key || !e->label) return 0;
    std::scoped_lock lk{g_entries_mu};
    for (const Entry& o : g_entries)
        if (o.key == e->key) return 0;
    Entry n;
    n.key       = e->key;
    n.parent    = e->parent ? e->parent : "";
    n.label     = e->label;
    n.on_choose = e->on_choose;
    n.shown     = e->shown;
    n.label_now = e->label_now;
    n.user      = e->user;
    n.stays_open = e->size >= offsetof(FH2AnnaEntry, stays_open) + sizeof(int) && e->stays_open;
    n.enabled    = e->size >= offsetof(FH2AnnaEntry, enabled) + sizeof(void*) ? e->enabled : nullptr;
    g_entries.push_back(std::move(n));
    return 1;
}

// The HUD's thread only.
struct Menu {
    bool open       = false;
    std::string list;               // the list showing ("" the top)
    std::vector<Entry> shown;       // its entries, as shown (copies)
    std::vector<std::string> texts; // and their labels (kOff and the label: greyed out)
    std::vector<bool> off;          // greyed out: choosing them does nothing
    std::vector<uint8_t*> items;    // the list's items for them (the list holds them)
    int highlighted = 0;
    bool chosen     = false;        // shown[chosen_at] was chosen: done once ANNA has closed
    Entry chosen_entry;
    DWORD close_at  = 0;
    DWORD touched   = 0;
} g_menu;
std::atomic<bool> g_open{false};    // for the input update's thread
uint8_t* g_status = nullptr;        // the HUD's ANNA, for ANNA's own entries

std::vector<Entry> entries_of(const std::string& parent) {
    std::scoped_lock lk{g_entries_mu};
    std::vector<Entry> out;
    for (const Entry& e : g_entries)
        if (e.parent == parent) out.push_back(e);
    return out;
}

// Whether an entry shows now: its own say, and a list only with something in it.
bool visible(const Entry& e, int depth = 0) {
    if (e.shown && !e.shown(e.user, e.key.c_str())) return false;
    if (e.on_choose) return true;
    if (depth > 8) return false;
    for (const Entry& c : entries_of(e.key))
        if (visible(c, depth + 1)) return true;
    return false;
}

// Whether a shown entry can be chosen: its own say, and a list only when something in it can.
bool enabled_now(const Entry& e, int depth = 0) {
    if (e.on_choose) return !e.enabled || e.enabled(e.user, e.key.c_str());
    if (depth > 8) return true;
    for (const Entry& c : entries_of(e.key))
        if (visible(c, depth + 1) && enabled_now(c, depth + 1)) return true;
    return false;
}

// At most kRows entries show; the list scrolls to keep the highlighted one in them. Above and
// below them, a row that says there's more that way: these texts, which the control's row template
// (tools/port_anna_xaml.py) draws as the pause menu's scroll arrow instead, on a shorter row. When
// the list scrolls both rows are there (kMoreNone, drawn as nothing, at an end with no more), so the
// rows' heights, and the arc their left ends follow, are the same however far it has scrolled.
constexpr int kRows = 6;
const char kMoreUp[]   = "FH2ANNA_MORE_UP";
const char kMoreDown[] = "FH2ANNA_MORE_DOWN";
const char kMoreNone[] = "FH2ANNA_MORE_NONE";
int g_first        = 0;   // the first entry showing (the HUD's thread)

// ANNA's list: entries g_first .. g_first + kRows of the list.
void fill_window(uint8_t* status) {
    game<StatusFn>(RVA_STATUS_CLEAR)(status);
    g_menu.items.clear();
    const int n       = static_cast<int>(g_menu.texts.size());
    const bool up     = n > kRows;   // the marker rows
    const bool down   = n > kRows;
    if (up) with_text(RVA_STATUS_ADD, status, text_for(g_first > 0 ? kMoreUp : kMoreNone));
    for (int i = g_first; i < n && i < g_first + kRows; ++i) with_text(RVA_STATUS_ADD, status, text_for(g_menu.texts[i]));
    if (down) with_text(RVA_STATUS_ADD, status, text_for(g_first + kRows < n ? kMoreDown : kMoreNone));
    void* collection = *reinterpret_cast<void**>(status + STATUS_ITEMS);
    uint8_t* lst     = collection ? *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(collection) + 0x50) : nullptr;
    const int count  = lst ? *reinterpret_cast<int*>(lst + 0x30) : 0;
    uint8_t** arr    = lst ? *reinterpret_cast<uint8_t***>(*reinterpret_cast<uint8_t**>(lst + 0x28) + 0x28) : nullptr;
    for (int i = up ? 1 : 0; arr && i < count - (down ? 1 : 0); ++i) g_menu.items.push_back(arr[i]);   // the entries' rows
}

// `index` in the whole list.
void highlight(int index) {
    g_menu.highlighted = index;
    const int first    = index < g_first ? index : index >= g_first + kRows ? index - kRows + 1 : g_first;
    if (first != g_first && g_status) g_first = first, fill_window(g_status);
    void* prop = game<PropFn>(RVA_SELECTED_PROP)();
    for (int i = 0; i < static_cast<int>(g_menu.items.size()); ++i) {
        g_menu.items[i][ITEM_SELECTED] = i == index - g_first;
        game<NotifyFn>(RVA_ITEM_NOTIFY)(g_menu.items[i], prop);
    }
}

// The next entry that isn't greyed out from `from`, `step` (1 down, -1 up) at a time, round the
// list; `from` itself if every other one is (with step 0: `from` unless it's greyed out).
int next_enabled(int from, int step) {
    const int n = static_cast<int>(g_menu.off.size());
    if (!n) return 0;
    if (!step) {
        if (from < n && !g_menu.off[from]) return from;
        step = 1;
    }
    for (int k = 1; k <= n; ++k) {
        const int i = ((from + step * k) % n + n) % n;
        if (!g_menu.off[i]) return i;
    }
    return from < n ? from : 0;
}

void show_list(uint8_t* status, const std::string& list) {
    g_menu.list = list;
    g_menu.shown.clear();
    g_menu.texts.clear();
    g_menu.off.clear();
    for (const Entry& e : entries_of(list)) {
        if (!visible(e)) continue;
        const bool off = !enabled_now(e) && can_grey(e.label);
        const char* now = e.label_now ? e.label_now(e.user, e.key.c_str()) : nullptr;
        g_menu.shown.push_back(e);
        g_menu.texts.push_back(off ? kOff + e.label : now && *now ? now : e.label);
        g_menu.off.push_back(off);
    }
    g_status = status;
    g_first  = 0;
    fill_window(status);
    highlight(next_enabled(0, 0));   // the first that can be chosen
}

void open_menu(uint8_t* status) {
    g_menu.open     = true;
    g_menu.chosen   = false;
    g_menu.close_at = 0;
    g_menu.touched  = GetTickCount();
    g_open          = true;
    show_list(status, "");
}

bool run_guarded(FH2AnnaChooseFn fn, void* user, const char* key) {
    __try {
        fn(user, key);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void close_menu(uint8_t* status) {
    g_menu.items.clear();   // before the list lets them go
    g_menu.shown.clear();
    game<StatusFn>(RVA_STATUS_SLEEP)(status);
    game<StatusFn>(RVA_STATUS_CLEAR)(status);
    g_menu.open     = false;
    g_menu.close_at = 0;
    g_open          = false;
    if (g_menu.chosen) {
        g_menu.chosen  = false;
        const Entry& e = g_menu.chosen_entry;
        logf("chose %s", e.key.c_str());
        if (!run_guarded(e.on_choose, e.user, e.key.c_str())) logf("%s crashed", e.key.c_str());
    }
}

void choose(uint8_t* status) {
    if (g_menu.highlighted >= static_cast<int>(g_menu.shown.size())) return;
    const Entry& e = g_menu.shown[g_menu.highlighted];
    if (g_menu.off[g_menu.highlighted]) return;   // greyed out
    if (!e.on_choose) {   // a list
        const std::string key = e.key;
        show_list(status, key);
        return;
    }
    with_text(RVA_STATUS_SELECT, status, text_for(g_menu.texts[g_menu.highlighted]));   // ANNA shows it chosen
    if (e.stays_open) {   // done now, and the list again, its labels as they are now, on the same entry
        const Entry chosen    = e;
        const int at          = g_menu.highlighted;
        logf("chose %s", chosen.key.c_str());
        if (!run_guarded(chosen.on_choose, chosen.user, chosen.key.c_str())) logf("%s crashed", chosen.key.c_str());
        show_list(status, g_menu.list);
        if (at < static_cast<int>(g_menu.shown.size())) highlight(next_enabled(at, 0));   // or the next that can be
        return;
    }
    g_menu.chosen       = true;
    g_menu.chosen_entry = e;
    g_menu.close_at     = GetTickCount() + kChosenMs;
}

void back(uint8_t* status) {
    if (g_menu.list.empty()) {
        close_menu(status);
        return;
    }
    const std::string was = g_menu.list;
    std::string parent;
    {
        std::scoped_lock lk{g_entries_mu};
        for (const Entry& e : g_entries)
            if (e.key == was) parent = e.parent;
    }
    show_list(status, parent);
    for (int i = 0; i < static_cast<int>(g_menu.shown.size()); ++i)   // back on the list it came from
        if (g_menu.shown[i].key == was) highlight(next_enabled(i, 0));
}

// ---- ANNA's own entries ---------------------------------------------------------------------
const char* const kPartKeys[]   = {"car.doors.roof", "car.doors.hood", "car.doors.trunk", "car.doors.lf",
                                   "car.doors.rf", "car.doors.lr", "car.doors.rr", "car.doors.storage",
                                   "car.doors.storagem", "car.doors.popup", "car.doors.popupl", "car.doors.popupr",
                                   "car.doors.spoiler"};
const char* const kPartLabels[] = {"Roof", "Hood", "Trunk", "Front Left Door", "Front Right Door", "Rear Left Door",
                                   "Rear Right Door", "Storage", "Second Storage", "Pop-up Headlights", "Left Pop-up Headlight",
                                   "Right Pop-up Headlight", "Spoiler"};
constexpr int kParts            = static_cast<int>(Part::Count);
constexpr int kDoorParts        = static_cast<int>(Part::PopupLights);   // Open All / Close All: not the lights

int part_index(const char* key) {
    for (int i = 0; i < kParts; ++i)
        if (!std::strcmp(key, kPartKeys[i])) return i;
    return -1;
}

// "Hide hood and trunk" (the Mods page, FH2ANNA.ini): for cars whose trunk is invisible or whose
// hood has no engine under it, the Doors list (and Open All) leaves them alone.
std::atomic<bool> g_hide_hood_trunk{false};

// A part the Doors list offers on this car.
bool offered(void* car, int i) {
    const Part p = static_cast<Part>(i);
    if (g_hide_hood_trunk.load() && (p == Part::Hood || p == Part::Trunk)) return false;
    return part_supported(car, p);
}

int car_shown(void*, const char*) { return player_car() != nullptr; }
int part_shown(void*, const char* key) {
    const int i = part_index(key);
    return i >= 0 && offered(player_car(), i);
}
int all_shown(void*, const char*) {
    void* car = player_car();
    int n     = 0;
    for (int i = 0; i < kDoorParts; ++i) n += offered(car, i);
    return n >= 2;
}
bool any_open(void* car) {
    for (int i = 0; i < kDoorParts; ++i)
        if (offered(car, i) && part_open(car, static_cast<Part>(i))) return true;
    return false;
}

std::string g_label;   // label_now's text, until it's copied
const char* part_label(void*, const char* key) {
    const int i = part_index(key);
    g_label     = std::string(part_open(player_car(), static_cast<Part>(i)) ? "Close " : "Open ") + kPartLabels[i];
    return g_label.c_str();
}
const char* all_label(void*, const char*) { return any_open(player_car()) ? "Close All" : "Open All"; }
const char* headlights_label(void*, const char*) { return headlights_on(player_car()) ? "Headlights Off" : "Headlights On"; }
const char* const kSignalNames[] = {"", "Hazard Lights", "Left Indicator", "Right Indicator"};
const char* signal_label(void* user, const char*) {
    const int s = static_cast<int>(reinterpret_cast<uintptr_t>(user));
    g_label     = std::string(kSignalNames[s]) + (static_cast<int>(signal_on()) == s ? " Off" : " On");
    return g_label.c_str();
}
void choose_signal(void* user, const char*) { set_signal(static_cast<Signal>(reinterpret_cast<uintptr_t>(user))); }
int autodrive_enabled(void*, const char*) { return autodrive_possible(); }
const char* autodrive_label(void*, const char*) { return autodriving() ? "Disable Auto Drive" : "Enable Auto Drive"; }
void choose_autodrive(void*, const char*) { toggle_autodrive(); }

void choose_part(void*, const char* key) {
    void* car = player_car();
    if (!car) return;
    const Part p = static_cast<Part>(part_index(key));
    set_part(car, p, !part_open(car, p));
}
void choose_all(void*, const char*) {
    void* car = player_car();
    if (!car) return;
    const bool open = !any_open(car);
    for (int i = 0; i < kDoorParts; ++i)
        if (offered(car, i)) set_part(car, static_cast<Part>(i), open);
}
void choose_headlights(void*, const char*) {
    void* car = player_car();
    if (car) set_headlights(car, !headlights_on(car));
}
void choose_route(void* user, const char*) { set_route(static_cast<Route>(reinterpret_cast<uintptr_t>(user))); }

std::wstring g_ini;   // fh2-mods\FH2ANNA.ini

void hood_trunk_changed(void*, const char*, int value) {
    g_hide_hood_trunk = value != 0;
    WritePrivateProfileStringW(L"Doors", L"HideHoodTrunk", value ? L"1" : L"0", g_ini.c_str());
}

// "Close doors when moving" (on unless the Mods page says off): see close_doors_if_moving.
std::atomic<bool> g_close_moving{true};

void close_moving_changed(void*, const char*, int value) {
    g_close_moving = value != 0;
    WritePrivateProfileStringW(L"Doors", L"CloseWhenMoving", value ? L"1" : L"0", g_ini.c_str());
}

void hood_trunk_setting() {
    g_ini             = std::wstring(g_api->azimuth_dir) + L"\\fh2-mods\\FH2ANNA.ini";
    g_hide_hood_trunk = GetPrivateProfileIntW(L"Doors", L"HideHoodTrunk", 0, g_ini.c_str()) != 0;
    if (g_api->api_version < 4) return;
    FH2Setting s{};
    s.size        = sizeof s;
    s.key         = "HideHoodTrunk";
    s.label       = "Hide hood and trunk in ANNA";
    s.description = "Car > Doors leaves out the hood and the trunk, and Open All leaves them shut: for cars with an invisible trunk or a fake engine under the hood.";
    s.value       = g_hide_hood_trunk.load() ? 1 : 0;
    s.on_change   = hood_trunk_changed;
    g_api->add_setting(g_mod, &s);
    g_close_moving = GetPrivateProfileIntW(L"Doors", L"CloseWhenMoving", 1, g_ini.c_str()) != 0;
    FH2Setting m{};
    m.size        = sizeof m;
    m.key         = "CloseWhenMoving";
    m.label       = "Close doors when moving";
    m.description = "The doors, hood, trunk and storage ANNA opened shut by themselves once the car drives off. The roof stays as it is.";
    m.value       = g_close_moving.load() ? 1 : 0;
    m.on_change   = close_moving_changed;
    g_api->add_setting(g_mod, &m);
}

void add_own_entries() {
    auto add = [](const char* key, const char* parent, const char* label, FH2AnnaChooseFn choose = nullptr,
                  FH2AnnaShownFn shown = nullptr, FH2AnnaLabelFn now = nullptr, void* user = nullptr, FH2AnnaShownFn enabled = nullptr) {
        FH2AnnaEntry e{};
        e.size = sizeof e;
        e.key = key, e.parent = parent, e.label = label, e.on_choose = choose, e.shown = shown, e.label_now = now, e.user = user;
        e.enabled = enabled;
        e.stays_open = parent && (!std::strcmp(parent, "car.doors") || !std::strcmp(parent, "car.lights"));   // they flip back and forth
        add_entry(&e);
    };
    add("car", nullptr, "Car", nullptr, car_shown);
    // without a route (none set on the GPS) it shows greyed out
    add("car.autodrive", "car", "Enable Auto Drive", choose_autodrive, car_shown, autodrive_label, nullptr, autodrive_enabled);
    add("car.lights", "car", "Lights");
    add("car.lights.head", "car.lights", "Headlights On", choose_headlights, car_shown, headlights_label);
    add("car.lights.hazards", "car.lights", "Hazard Lights On", choose_signal, car_shown, signal_label, reinterpret_cast<void*>(Signal::Hazards));
    add("car.lights.left", "car.lights", "Left Indicator On", choose_signal, car_shown, signal_label, reinterpret_cast<void*>(Signal::Left));
    add("car.lights.right", "car.lights", "Right Indicator On", choose_signal, car_shown, signal_label, reinterpret_cast<void*>(Signal::Right));
    add("car.doors", "car", "Doors");
    add("car.doors.all", "car.doors", "Open All", choose_all, all_shown, all_label);
    for (int i = 0; i < kParts; ++i) add(kPartKeys[i], "car.doors", kPartLabels[i], choose_part, part_shown, part_label);
    add("route", nullptr, "Set Route To Nearest");
    add("route.race", "route", "Race", choose_route, nullptr, nullptr, reinterpret_cast<void*>(Route::Race));
    add("route.bucketlist", "route", "Bucket List", choose_route, nullptr, nullptr, reinterpret_cast<void*>(Route::BucketList));
    add("route.festival", "route", "Festival", choose_route, nullptr, nullptr, reinterpret_cast<void*>(Route::Festival));
    add("route.barnfind", "route", "Barn Find", choose_route, nullptr, nullptr, reinterpret_cast<void*>(Route::BarnFind));
    add("route.bonusboard", "route", "Bonus Board", choose_route, nullptr, nullptr, reinterpret_cast<void*>(Route::BonusBoard));
}

// ---- input ----------------------------------------------------------------------------------
// The game's input update (0x1075490, once a frame from the main loop) reads each player's pad
// into its input; the current player's is the inputs' (0x365D7C8) +0xAB28. In an input: this
// frame's state at +0x70 and the last frame's at +0x128 (what was just pressed is what's down
// now and wasn't then); in each, the buttons at +0x8 (XInput's bits, and the game's own for the
// d-pad, as FH2 Mouse & Keyboard found them), the d-pad as a direction at +0x10 ({dir: 8 none,
// angle, length, x, y}) and XInput's wButtons as read at +0x50 (what Extra Options reads).
constexpr uintptr_t RVA_INPUT_UPDATE = 0x1075490;
constexpr uintptr_t RVA_INPUTS       = 0x365D7C8;
constexpr uintptr_t INPUTS_CURRENT   = 0xAB28;
constexpr uintptr_t INPUT_STATE      = 0x70;
constexpr uintptr_t INPUT_LAST_STATE = 0x128;
constexpr uintptr_t STATE_BUTTONS    = 0x8;
constexpr uintptr_t STATE_DPAD       = 0x10;
constexpr uintptr_t STATE_XINPUT     = 0x50;
constexpr uint16_t kPadUp = 0x1, kPadDown = 0x2, kPadLeft = 0x4, kPadRight = 0x8, kDpad = 0xF;
constexpr uint16_t kOpenPad = kPadDown;   // ANNA's button, as FH3's
constexpr DWORD kTapMs = 250;   // a longer hold is the game's (D-pad down's telemetry wants a hold)

// The game's own bits for each d-pad direction, with XInput's.
uint64_t game_bits(uint16_t dpad) {
    uint64_t b = dpad;
    if (dpad & (kPadUp | kPadDown)) b |= 0x140000000ull;
    if (dpad & (kPadLeft | kPadRight)) b |= 0xC0000000ull;
    return b;
}

using InputUpdateFn = uintptr_t (*)(void* inputs, float dt);
InputUpdateFn o_input_update = nullptr;

// What the input asks the menu to do, for the HUD's update to do it.
enum : uint32_t { kAskOpen = 1, kAskBack = 2, kAskUp = 4, kAskDown = 8, kAskChoose = 16, kAskToggle = 32, kAskClose = 64 };
std::atomic<uint32_t> g_ask{0};

// Takes d-pad directions from the game, and from what reads the pad's buttons (Extra Options).
void hide(uint8_t* input, uint16_t dpad) {
    for (uintptr_t state : {INPUT_STATE, INPUT_LAST_STATE}) {
        uint8_t* s = input + state;
        *reinterpret_cast<uint64_t*>(s + STATE_BUTTONS) &= ~game_bits(dpad);
        *reinterpret_cast<uint16_t*>(s + STATE_XINPUT) &= static_cast<uint16_t>(~dpad);
        int* dir = reinterpret_cast<int*>(s + STATE_DPAD);
        // the d-pad's direction too (0 up, 2 left, 4 down, 6 right) when it's the one taken
        const int taken = dpad == kPadUp ? 0 : dpad == kPadLeft ? 2 : dpad == kPadDown ? 4 : dpad == kPadRight ? 6 : -1;
        if (dpad == kDpad || *dir == taken) {
            float* f = reinterpret_cast<float*>(dir + 1);
            *dir = 8, f[0] = f[1] = f[2] = f[3] = 0;
        }
    }
}

// The game is taking driving input (not a menu, the map or a pause): the front of the input's
// context queue (a std::deque at +0x38: {proxy, blocks, capacity, start, count}, two 8-byte
// entries a block) has bit 1 in its permissions (+4), as Extra Options reads it.
bool driving_input(uint8_t* input) {
    struct {
        void* proxy;
        uint8_t** blocks;
        std::size_t capacity, start, count;
    } q;
    std::memcpy(&q, input + 0x38, sizeof q);
    if (!q.count || !q.blocks || !q.capacity || q.capacity > 4096 || (q.capacity & (q.capacity - 1))) return false;
    uint8_t* block = q.blocks[(q.start / 2) & (q.capacity - 1)];
    return block && (*reinterpret_cast<uint32_t*>(block + (q.start % 2) * 8 + 4) & 1);
}

std::atomic<bool> g_driving_input{false};   // as last read (the input update's thread)

void read_pad(uint8_t* input) {
    static uint16_t last     = 0;
    static DWORD up_at       = 0;   // ANNA's button went down (closed): hidden till it's a hold
    uint16_t now = (*reinterpret_cast<uint16_t*>(input + INPUT_STATE + STATE_XINPUT) |
                    *reinterpret_cast<uint16_t*>(input + INPUT_STATE + STATE_BUTTONS)) & kDpad;
    const uint16_t press   = now & ~last;
    const uint16_t release = last & ~now;
    last = now;
    // A d-pad button still held when ANNA closed (D-pad left, say) stays ANNA's till it's let go:
    // else the game takes it as a press then (the radio's station).
    static bool was_open   = false;
    static uint16_t keep   = 0;
    if (was_open && !g_open.load()) keep |= now;
    was_open = g_open.load();
    keep &= now;
    if (keep) hide(input, keep);
    g_driving_input = driving_input(input);
    if (!g_driving_input.load()) {   // a menu has the pad: ANNA leaves it alone, and closes
        if (g_open.load()) g_ask |= kAskClose;
        up_at = 0;
        return;
    }
    if (g_open.load()) {
        if (press & kPadUp) g_ask |= kAskUp;
        if (press & kPadDown) g_ask |= kAskDown;
        if (press & kPadRight) g_ask |= kAskChoose;
        if (press & kPadLeft) g_ask |= kAskBack;
        up_at = 0;
        hide(input, kDpad);
    } else {
        if (press & kOpenPad) up_at = GetTickCount();
        if (up_at) {
            const DWORD held = GetTickCount() - up_at;
            if (release & kOpenPad) {
                if (held < kTapMs) g_ask |= kAskOpen;
                up_at = 0;
            } else if (held < kTapMs) {
                hide(input, kOpenPad);   // not yet the game's: it may be a tap
            } else {
                up_at = 0;          // a hold: the game's from now
            }
        }
    }
}

bool read_pad_guarded(uint8_t* input) {
    __try {
        read_pad(input);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uintptr_t d_input_update(void* inputs, float dt) {
    const uintptr_t r = o_input_update(inputs, dt);
    static bool failed = false;
    uint8_t* all       = *reinterpret_cast<uint8_t**>(g_base + RVA_INPUTS);
    uint8_t* input     = all ? *reinterpret_cast<uint8_t**>(all + INPUTS_CURRENT) : nullptr;
    if (!failed && input && !read_pad_guarded(input)) failed = true, logf("crashed reading the pad; the d-pad doesn't work ANNA");
    return r;
}

// ---- each HUD update ------------------------------------------------------------------------
// The pad's hook, outermost of the input update's: a keyboard mod (FH2Mouse) holds the d-pad's
// buttons for its keys in its own hook there, and ANNA reads them after it (and takes them from
// the game after it). The platform runs the most recently enabled hook first; mods hook as they
// start, so once the HUD is up (long after) ours is switched off and on again.
FH2Hook* g_pad_hook = nullptr;

void pad_hook_outermost() {
    static bool done = false;
    if (done || !g_pad_hook) return;
    done = true;
    g_api->disable_hooks(g_mod, g_pad_hook);
    g_api->enable_hooks(g_mod, g_pad_hook);
}

void tick(uint8_t* status) {
    pad_hook_outermost();
    static uint8_t* seen = nullptr;
    if (status != seen) {   // a new HUD: its list is empty, ANNA asleep
        seen = status;
        g_menu.items.clear();
        g_menu.shown.clear();
        g_menu.open = false, g_menu.chosen = false, g_menu.close_at = 0;
        g_open = false;
        g_ask  = 0;
    }
    g_status  = status;
    void* car = player_car();
    note_local_car(car);
    log_car_events(car);
    if (g_close_moving.load()) close_doors_if_moving(car);
    nearest_search_tick(status);
    // The update only sets the mode when it differs from the Options/Map setting (2 on PC), so the
    // first time it's set here; after that the update's own sets are kept at it (d_set_mode).
    if (o_get_mode(status) == kModeMapOnly) o_set_mode(status, kModeAnna);
    uint32_t ask = g_ask.exchange(0);
    if (o_get_mode(status) != kModeAnna) {   // the map is off: so is ANNA
        if (g_menu.open) close_menu(status);
        return;
    }
    if (!g_menu.open) {
        if (ask & (kAskOpen | kAskToggle)) open_menu(status);
        return;
    }
    const DWORD now = GetTickCount();
    if (ask & kAskClose) {   // a menu took the pad
        close_menu(status);
        return;
    }
    if (g_menu.close_at) {   // a choice: ANNA shows it, then closes and does it
        if (static_cast<LONG>(now - g_menu.close_at) >= 0) close_menu(status);
        return;
    }
    if (ask) g_menu.touched = now;
    if ((ask & kAskToggle) || now - g_menu.touched > kIdleMs) {
        close_menu(status);
        return;
    }
    if (ask & kAskBack) {
        back(status);
        return;
    }
    const int n = static_cast<int>(g_menu.shown.size());
    // up and down pass over what's greyed out
    if (n && (ask & kAskUp)) highlight(next_enabled(g_menu.highlighted, -1));
    if (n && (ask & kAskDown)) highlight(next_enabled(g_menu.highlighted, 1));
    if (n && (ask & kAskChoose)) choose(status);
}

bool tick_guarded(uint8_t* status) {
    __try {
        tick(status);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

int d_get_mode(uint8_t* status) {
    static bool failed = false;
    if (!failed && status && !tick_guarded(status)) failed = true, g_open = false, logf("crashed in ANNA's update; ANNA is off");
    return o_get_mode(status);
}

// With the minimap on, ANNA's label shows under it as in FH3, its list only while open. The map
// off is off.
void d_set_mode(uint8_t* status, int mode) {
    if (mode == kModeMapOnly) mode = kModeAnna;
    if (mode == o_get_mode(status)) return;
    o_set_mode(status, mode);
}

// ---- the interface for other mods (include/fh2anna.h) -----------------------------------------
std::atomic<bool> g_ready{false};   // ANNA works (its hooks are in)

int api_is_open() { return g_open.load() ? 1 : 0; }
int api_uses_key(uint32_t) { return 0; }   // ANNA takes no keys: the pad only
const FH2AnnaApi kApi = {sizeof(FH2AnnaApi), &api_is_open, &api_uses_key, &add_entry};

DWORD WINAPI start(LPVOID) {
    g_base = static_cast<uint8_t*>(g_api->wait_for_code(0xFFFFFFFFu));
    if (!g_base) return 0;
    if (!hook_routes()) logf("the navigation module isn't as expected: no routes");
    if (!hook_drive()) logf("some of the car's code isn't as expected: indicators or autodrive may not work");
    if (!hook_headlight_reads()) logf("the game's headlight reads aren't as expected: Extra Options' headlight key may hold ANNA's off");
    if (!hook_headlight_beams()) logf("the lights' upload isn't as expected: headlights on may only show in the dark");
    FH2Hook* a = g_api->hook_call(g_mod, "ANNA: display mode (read)", g_base + RVA_GET_MODE_CALL,
                                  reinterpret_cast<void*>(&d_get_mode), reinterpret_cast<void**>(&o_get_mode));
    FH2Hook* b = g_api->hook_call(g_mod, "ANNA: display mode (set)", g_base + RVA_SET_MODE_CALL,
                                  reinterpret_cast<void*>(&d_set_mode), reinterpret_cast<void**>(&o_set_mode));
    if (!a || !b) {
        logf("the HUD's ANNA code isn't as expected; off");
        return 0;
    }
    g_api->enable_hooks(g_mod, a);
    g_api->enable_hooks(g_mod, b);
    if (FH2Hook* p = g_api->hook_function(g_mod, "ANNA: the pad", g_base + RVA_INPUT_UPDATE, reinterpret_cast<void*>(&d_input_update),
                                          reinterpret_cast<void**>(&o_input_update)))
        g_api->enable_hooks(g_mod, p), g_pad_hook = p;
    else {
        logf("the game's input update isn't as expected: nothing opens ANNA");
        return 0;
    }
    if (!hook_cinematic()) logf("autodrive's cinematic camera keeps the HUD on");
    if (!hook_anna_xaml()) logf("couldn't hook the ANNA control's XAML: ANNA keeps FH2's look");
    g_ready = true;
    logf("ready: a tap of D-pad down opens ANNA (keys: a keyboard mod's, on the pad's buttons)");
    return 0;
}

} // namespace

bool game_driving_input() { return g_driving_input.load(); }

std::string disabled_triggers() {
    std::vector<std::string> labels;
    {
        std::scoped_lock lk{g_entries_mu};
        // an entry that can be greyed out, or a list with one in it (greyed out when all in it are)
        auto can_be = [](auto& self, const Entry& e, int depth) -> bool {
            if (e.on_choose) return e.enabled != nullptr;
            if (depth > 8) return false;
            for (const Entry& c : g_entries)
                if (c.parent == e.key && self(self, c, depth + 1)) return true;
            return false;
        };
        for (const Entry& e : g_entries)
            if (can_be(can_be, e, 0) && std::find(labels.begin(), labels.end(), e.label) == labels.end()) labels.push_back(e.label);
    }
    std::string x;
    for (const std::string& l : labels) {
        const std::string text = xml_escaped(l), command = xml_escaped(kOff + l);
        x += "<DataTrigger Binding=\"{Binding Command}\" Value=\"" + command + "\">\n"
             "    <Setter TargetName=\"Label\" Property=\"Text\" Value=\"" + text + "\" />\n"
             "    <Setter TargetName=\"Label\" Property=\"Color\" Value=\"#70FFFFFF\" />\n"
             "</DataTrigger>\n"
             "<MultiDataTrigger>\n"
             "    <MultiDataTrigger.Conditions>\n"
             "        <Condition Binding=\"{Binding IsSelected}\" Value=\"True\" />\n"
             "        <Condition Binding=\"{Binding Command}\" Value=\"" + command + "\" />\n"
             "    </MultiDataTrigger.Conditions>\n"
             "    <Setter TargetName=\"Back\" Property=\"Fill\" Value=\"#C0585858\" />\n"
             "    <Setter TargetName=\"Label\" Property=\"Color\" Value=\"#A0FFFFFF\" />\n"
             "    <Setter TargetName=\"Dpad\" Property=\"Visibility\" Value=\"Hidden\" />\n"
             "</MultiDataTrigger>\n";
    }
    std::scoped_lock lk{g_off_mu};
    g_off_labels = labels;
    return x;
}

} // namespace fh2anna

using namespace fh2anna;

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    if (api->api_version < 3) return 1;
    g_api = api;
    FH2ModInfo info{};
    info.size             = sizeof info;
    info.name             = "FH2 ANNA";
    info.version          = FH2ANNA_VERSION;
    info.platform_version = FH2MOD_PLATFORM_VERSION;
    info.check_updates    = 0;
    info.description      = "ANNA, the in-car assistant Forza Horizon 2 left on its HUD: a menu while you drive, that mods can add to.";
    g_mod                 = api->register_mod(&info);
    if (!g_mod) return 1;
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&fh2mod_init), &self);
    write_textures(self);
    cinematic_settings();
    hood_trunk_setting();
    add_own_entries();
    api->publish_interface(g_mod, FH2ANNA_INTERFACE, 4, &kApi);
    if (HANDLE t = CreateThread(nullptr, 0, start, nullptr, 0, nullptr)) CloseHandle(t);
    return 0;
}
