// The player's car: its headlights and the parts it animates.
//
// The car: the player manager (0x34ABA60) -> 0x21E0C0(manager, 0), the local player ->
// 0x2DA060(player, &car), a reference (released with the car's vtable +0x18); the player's car
// has the vtable 0x2C5CB80. (As Extra Options' autodrive finds it.)
//
// Headlights: +0x51A1 is "the driver sets them" (0xF74EE0 sets it), +0x51A2 on / off then
// (0xF74EF0); otherwise the game's own choice, +0x51B4.
//
// Parts: the car's animation graph (car +0x4D40 -> +0x1C8) has named events, "roof:open",
// "doorLF:close", "hood:toggle"..., the clips ForzaVista plays. 0xF5D300(car, name) sends one;
// the events are a std::unordered_map<std::string, ...> at graph +0x268 (its list's head node
// at +0x268, its size at +0x270), looked up with 0x16FD4D0(map, &node, &key). 0x16FBE90(graph, 0) puts the graph at
// full detail first, so a part far from the camera still moves. FH3's ANNA does the same with
// FH3's addresses.
#include "car.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <string>

namespace fh2anna {
namespace {

constexpr uintptr_t RVA_PLAYER_MANAGER = 0x34ABA60;
constexpr uintptr_t RVA_LOCAL_PLAYER   = 0x21E0C0;   // Player* (manager, int index)
constexpr uintptr_t RVA_PLAYER_CAR     = 0x2DA060;   // (player, Car** out), add-ref'd
constexpr uintptr_t RVA_CAR_VTABLE     = 0x2C5CB80;
constexpr uintptr_t RVA_SET_LIGHTS_ON  = 0xF74EF0;   // (car, bool)
constexpr uintptr_t RVA_SET_LIGHTS_OWN = 0xF74EE0;   // (car, bool): the driver's, not the game's
constexpr uintptr_t RVA_CAR_EVENT      = 0xF5D300;   // (car, const char* name)
constexpr uintptr_t RVA_FIND_EVENT     = 0x16FD4D0;  // (map, Node** out, const std::string* key)
constexpr uintptr_t RVA_GRAPH_DETAIL   = 0x16FBE90;  // (graph, int level), 0: full
constexpr uintptr_t CAR_ANIMATOR       = 0x4D40;
constexpr uintptr_t ANIMATOR_GRAPH     = 0x1C8;
constexpr uintptr_t GRAPH_EVENTS       = 0x268;
constexpr uintptr_t CAR_LIGHTS_OWN = 0x51A1, CAR_LIGHTS_ON = 0x51A2, CAR_LIGHTS_AUTO = 0x51B4;

// The pop-up lights are two parts, "headlightL" and "headlightR", moved together.
const char* const kPartNames[] = {"roof", "hood", "trunk", "doorLF", "doorRF", "doorLR", "doorRR", "storage", "storageM", "headlightL", "headlightL", "headlightR", "active_aero"};
void* g_lights_car = nullptr;   // the car whose headlights are ours
bool g_lights_on   = false;

// MSVC's std::string, as the game's map keys are.
struct MsvcString {
    union {
        char text[16];
        const char* pointer;
    };
    std::size_t size, capacity;
};

using PlayerFn   = void* (*)(void* manager, int index);
using CarFn      = void (*)(void* player, void** out);
using ReleaseFn  = void (*)(void* obj);
using LightsFn   = void (*)(void* car, uint8_t on);
using EventFn    = void (*)(void* car, const char* name);
using FindFn     = void* (*)(void* map, void** out, const MsvcString* key);
using DetailFn   = void (*)(void* graph, int level);

void* g_parts_car    = nullptr;   // whose parts g_parts_open is
unsigned g_parts_open = 0;
void* g_logged_car   = nullptr;

uint8_t* graph_of(void* car) {
    uint8_t* animator = car ? *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(car) + CAR_ANIMATOR) : nullptr;
    return animator ? *reinterpret_cast<uint8_t**>(animator + ANIMATOR_GRAPH) : nullptr;
}

bool has_event_raw(uint8_t* graph, const char* name) {
    __try {
        uintptr_t map[2];
        if (!readable(graph + GRAPH_EVENTS, sizeof map)) return false;
        std::memcpy(map, graph + GRAPH_EVENTS, sizeof map);
        if (!map[0] || map[1] > 65536) return false;
        // A std::string: up to 15 characters in place, longer ones ("active_aero:toggle") by pointer.
        MsvcString key{};
        key.size = std::strlen(name);
        if (key.size < sizeof key.text) key.capacity = 15, std::memcpy(key.text, name, key.size);
        else key.capacity = key.size, key.pointer = name;
        void* node = nullptr;
        game<FindFn>(RVA_FIND_EVENT)(graph + GRAPH_EVENTS, &node, &key);
        return node && node != reinterpret_cast<void*>(map[0]);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool has_event(void* car, const char* part, const char* action) {
    const std::string name = std::string(part) + ":" + action;
    uint8_t* graph         = graph_of(car);
    return graph && has_event_raw(graph, name.c_str());
}

void send_event_raw(void* car, uint8_t* graph, const char* name) {
    __try {
        game<DetailFn>(RVA_GRAPH_DETAIL)(graph, 0);
        game<EventFn>(RVA_CAR_EVENT)(car, name);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        logf("sending %s crashed", name);
    }
}

constexpr unsigned kPopupBits = 1u << static_cast<int>(Part::PopupLights) | 1u << static_cast<int>(Part::PopupL) |
                                1u << static_cast<int>(Part::PopupR);
void* g_events_car = nullptr;   // the car the last event went to

bool popups_raw(void* car) {
    return has_event(car, "headlightL", "toggle") || (has_event(car, "headlightL", "open") && has_event(car, "headlightL", "close"));
}

// Another car: all closed, but for pop-up lights the game has up (its headlights on, at night).
void own_parts(void* car) {
    if (car == g_parts_car) return;
    g_parts_car  = car;
    g_parts_open = headlights_on(car) && popups_raw(car) ? kPopupBits : 0;
}

// The keys of the event map: an MSVC std::unordered_map, its entries a std::list (head node at
// +0x268, count at +0x270): nodes {next, prev, value}, the value a std::string key first.
void collect_events(uint8_t* head, std::string& out) {
    uint8_t* node = *reinterpret_cast<uint8_t**>(head);   // the first
    int guard     = 0;
    while (node && node != head && guard++ < 2000) {
        const auto* key  = reinterpret_cast<const MsvcString*>(node + 0x10);
        const char* text = key->capacity >= 16 ? key->pointer : key->text;
        if (key->size < 64 && readable(text, key->size)) out.append(text, key->size).append(" ");
        node = *reinterpret_cast<uint8_t**>(node);
    }
}

bool collect_events_guarded(uint8_t* head, std::string& out) {
    __try {
        collect_events(head, out);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void* player_car_raw() {
    __try {
        void* manager = *reinterpret_cast<void**>(g_base + RVA_PLAYER_MANAGER);
        if (!manager) return nullptr;
        void* player = game<PlayerFn>(RVA_LOCAL_PLAYER)(manager, 0);
        if (!player) return nullptr;
        void* car = nullptr;
        game<CarFn>(RVA_PLAYER_CAR)(player, &car);
        if (!car) return nullptr;
        void** vt = *reinterpret_cast<void***>(car);
        // Ours for this frame: the game keeps its own reference.
        reinterpret_cast<ReleaseFn>(vt[3])(car);
        return vt == reinterpret_cast<void**>(g_base + RVA_CAR_VTABLE) ? car : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

} // namespace

void* player_car() { return player_car_raw(); }

bool headlights_on(void* car) {
    const auto* c = static_cast<const uint8_t*>(car);
    if (c && car == g_lights_car) return g_lights_on;
    return c && (c[CAR_LIGHTS_OWN] ? c[CAR_LIGHTS_ON] : c[CAR_LIGHTS_AUTO]) != 0;
}

// Autodrive's, in the dark (dark_headlights_tick): what was there before, put back after.
struct DarkLights {
    void* car = nullptr;   // ours are on for the dark
    uint8_t own = 0, on = 0;
    DWORD at    = 0;       // it's been the other way since
    bool taken  = false;   // something else switched ours: theirs till autodrive ends
    float outside = -1.f;  // the light as it was outside, last
    DWORD outside_tick = 0;
} g_dark;

void own_headlights(void* car, bool on) {
    g_lights_car = car;
    g_lights_on  = on;
    apply_headlights(car);
    if (part_supported(car, Part::PopupLights)) set_part(car, Part::PopupLights, on);
}

void set_headlights(void* car, bool on) {
    if (!car) return;
    g_dark.car = nullptr;   // the player's from now on
    own_headlights(car, on);
    logf("car: headlights %s", on ? "on" : "off");
}

// ---- the headlights' beams in daylight -------------------------------------------------------
// (As Extra Options' FH3 branch, fh2/features/headlight_beams.c.) The car's "headlights on" turns
// its lights up, but each light preset has its own daylight threshold (two floats): many cars'
// beams and glows only show in the dark. While the renderer uploads the lights (the beams
// 0x10F6C40(system, graphics), the glows 0x10F6970, each called from one place, 0x1308390's), the
// player's car's headlight (amount type 13) presets get a threshold of 0, put back after.
//
// The car's renderer: car +0x4D38 -> +8 (vtable 0x2EC1450 or 0x2EC2B70); its context +0x10 ->
// +0x380 the light manager, whose +8 (beams) or +0x18 (glows) is the system being uploaded; its
// asset +0x1278 -> +0x4BE0 .. +0x4BE8 the light instances (u16 type at +4, mapping index at
// +0x50); the manager's mappings +0x28 (24 each; +0 the beam's index, +4 the glow's), the
// system's lights (+0 / +0x678, 64 each; the preset at +0x30 / +0xC (low 16 bits)), its presets
// (+0x38 / +0x6B0, 0xC0 / 0x120 each; the threshold at +0x9C / +0x44). The beams' system caches
// its presets for the GPU: +0x100 set makes it upload them again (when ours change).
constexpr uintptr_t RVA_UPLOAD_BEAMS = 0x10F6C40;
constexpr uintptr_t RVA_UPLOAD_GLOWS = 0x10F6970;
constexpr uintptr_t RVA_RENDERER_VT_A = 0x2EC1450, RVA_RENDERER_VT_B = 0x2EC2B70;
std::atomic<void*> g_beams_renderer{nullptr};   // the player's car's, while ours are on

using UploadFn = void (*)(void* system, void* graphics);
UploadFn o_upload_beams = nullptr, o_upload_glows = nullptr;

template <class T> bool peek(const void* p, T* out) {
    if (!p || !readable(p, sizeof(T))) return false;
    *out = *static_cast<const T*>(p);
    return true;
}

uint8_t* record(uint8_t* owner, unsigned offset, unsigned index, size_t stride) {
    uintptr_t begin, end;
    if (!owner || !peek(owner + offset, &begin) || !peek(owner + offset + 8, &end) || !begin || end < begin ||
        (end - begin) % stride || (end - begin) / stride > 100000 || index >= (end - begin) / stride)
        return nullptr;
    return reinterpret_cast<uint8_t*>(begin + index * stride);
}

struct Threshold {
    float* at;
    float saved[2];
};

unsigned collect(void* system, bool glow, Threshold out[32]) {
    auto* car = static_cast<uint8_t*>(g_beams_renderer.load());
    void* vt;
    uint8_t *context, *manager, *backing, *asset;
    uintptr_t begin, end;
    if (!car || !peek(car, &vt) || (vt != g_base + RVA_RENDERER_VT_A && vt != g_base + RVA_RENDERER_VT_B) ||
        !peek(car + 0x10, &context) || !peek(context + 0x380, &manager) || !peek(manager + (glow ? 0x18 : 8), &backing) ||
        backing != system || !peek(car + 0x1278, &asset) || !peek(asset + 0x4BE0, &begin) || !peek(asset + 0x4BE8, &end) || !begin ||
        end < begin || (end - begin) % 8 || (end - begin) / 8 > 512)
        return 0;
    unsigned count = 0;
    for (uintptr_t at = begin; at < end && count < 32; at += 8) {
        uint8_t* instance;
        uint16_t type;
        unsigned map_index;
        if (!peek(reinterpret_cast<void*>(at), &instance) || !peek(instance + 4, &type) || type != 13 || !peek(instance + 0x50, &map_index))
            continue;
        uint8_t* mapping = record(manager, 0x28, map_index, 24);
        unsigned index, preset;
        if (!mapping || !peek(mapping + (glow ? 4 : 0), &index)) continue;
        uint8_t* light = record(backing, glow ? 0x678 : 0, index, 64);
        if (!light || !peek(light + (glow ? 0xC : 0x30), &preset)) continue;
        if (glow) preset &= 0xFFFF;
        uint8_t* definition = record(backing, glow ? 0x6B0 : 0x38, preset, glow ? 0x120 : 0xC0);
        auto* threshold     = definition ? reinterpret_cast<float*>(definition + (glow ? 0x44 : 0x9C)) : nullptr;
        if (!threshold || !readable(threshold, 8)) continue;
        bool seen = false;
        for (unsigned i = 0; i < count; ++i) seen = seen || out[i].at == threshold;
        if (seen || !std::isfinite(threshold[0]) || !std::isfinite(threshold[1])) continue;
        out[count++] = {threshold, {threshold[0], threshold[1]}};
    }
    return count;
}

void upload(void* system, void* graphics, bool glow) {
    static Threshold previous[32];
    static unsigned previous_count = 0;
    static void* previous_system   = nullptr;
    Threshold t[32];
    unsigned count = 0;
    __try {
        count = collect(system, glow, t);
        if (!glow) {
            // the beams' presets are cached for the GPU: uploaded again when ours change (on, off, another car)
            bool changed = system != previous_system || count != previous_count;
            for (unsigned i = 0; !changed && i < count; ++i)
                changed = previous[i].at != t[i].at || previous[i].saved[0] != t[i].saved[0] || previous[i].saved[1] != t[i].saved[1];
            if (changed && (count || previous_count)) static_cast<uint8_t*>(system)[0x100] = 1;
            if (changed) logf("headlights' beams: %u presets of the car's (were %u)%s", count, previous_count, count ? ", lit in daylight" : "");
            previous_system = system, previous_count = count;
            for (unsigned i = 0; i < count; ++i) previous[i] = t[i];
        }
        for (unsigned i = 0; i < count; ++i) t[i].at[0] = t[i].at[1] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        count = 0;
    }
    (glow ? o_upload_glows : o_upload_beams)(system, graphics);
    for (unsigned i = 0; i < count; ++i) t[i].at[0] = t[i].saved[0], t[i].at[1] = t[i].saved[1];
}
void d_upload_beams(void* system, void* graphics) { upload(system, graphics, false); }
void d_upload_glows(void* system, void* graphics) { upload(system, graphics, true); }

// The car's renderer, for the uploads, while our headlights are on.
void beams_for(void* car, bool on) {
    void *proxy = nullptr, *actual = nullptr, *vt = nullptr;
    void* now = nullptr;
    if (on && car && peek(static_cast<uint8_t*>(car) + 0x4D38, &proxy) && peek(static_cast<uint8_t*>(proxy) + 8, &actual) && peek(actual, &vt) &&
        (vt == g_base + RVA_RENDERER_VT_A || vt == g_base + RVA_RENDERER_VT_B))
        now = actual;
    g_beams_renderer = now;
}

// The car's update sets them from the game's own choice every frame: ours go in before and after.
void write_lights(void* car) {
    game<LightsFn>(RVA_SET_LIGHTS_ON)(car, g_lights_on ? 1 : 0);
    game<LightsFn>(RVA_SET_LIGHTS_OWN)(car, 1);
}

void apply_headlights(void* car) {
    if (!car || car != g_lights_car) {
        g_beams_renderer = nullptr;   // another car: the game's own
        return;
    }
    beams_for(car, g_lights_on);
    write_lights(car);
}

// Others switch them too: Extra Options' headlight key keeps its own state written before and
// after every update of the car, around ours; and the game may. What's there when the update
// starts, before ours goes in, is theirs: when that changes, to something other than ours, they
// switched the lights, and they're theirs from then on (ours again the next time ANNA's chosen).
uint16_t g_foreign = 0xFFFF;   // their own / on bytes as last seen (the car update's thread)

void note_headlights(void* car) {
    if (!car || car != g_lights_car) {
        g_foreign = 0xFFFF;
        return;
    }
    const auto* c       = static_cast<const uint8_t*>(car);
    const uint16_t now  = static_cast<uint16_t>(c[CAR_LIGHTS_OWN] | c[CAR_LIGHTS_ON] << 8);
    const uint16_t ours = static_cast<uint16_t>(1 | (g_lights_on ? 1 : 0) << 8);
    if (g_foreign != 0xFFFF && now != g_foreign && now != ours) {
        g_lights_car     = nullptr;
        g_beams_renderer = nullptr;
        g_foreign        = 0xFFFF;
        logf("car: the headlights were switched elsewhere (Extra Options?): theirs now");
        return;
    }
    g_foreign = now;
}

// ---- the headlights in the dark (autodrive) --------------------------------------------------
// The light where the car is: a scale in the game's global lighting (0x366101C, a float, beside
// the sun's direction and the sky's colour) that drops to a fifth or less in a tunnel (0.0003
// outside, 0.00006 inside, by day) and is back as the car comes out; the sky's own light (the
// renderer's ambient colour) only follows the time of day. It's compared with what it was outside
// (`outside`, the brightest lately): dark below kDark of it for kDarkFor, light again above kLight
// of it for kLightFor (no flicker at a tunnel's mouth).
constexpr uintptr_t RVA_ZONE_LIGHT = 0x366101C;
constexpr float kDark              = 0.4f, kLight = 0.7f;
constexpr DWORD kDarkFor           = 300, kLightFor = 1000;
constexpr float kOutsideHalf       = 20.f;   // s

float zone_light() {   // -1 if it can't be read
    float v;
    if (!peek(g_base + RVA_ZONE_LIGHT, &v) || !std::isfinite(v) || v < 0.f || v > 1000.f) return -1.f;
    return v;
}

void dark_lights_off(void* car) {   // back as they were, if they're still ours for the dark
    if (car && car == g_dark.car && car == g_lights_car) {
        g_lights_car     = nullptr;
        g_beams_renderer = nullptr;
        game<LightsFn>(RVA_SET_LIGHTS_ON)(car, g_dark.on);
        game<LightsFn>(RVA_SET_LIGHTS_OWN)(car, g_dark.own);
        if (part_supported(car, Part::PopupLights) && !headlights_on(car)) set_part(car, Part::PopupLights, false);
        logf("car: headlights back as they were (it's light)");
    }
    g_dark.car = nullptr;
}

void dark_headlights_tick(void* car, bool driving) {
    if (g_dark.car && (!driving || car != g_dark.car)) {
        dark_lights_off(g_dark.car == car ? car : nullptr);
        return;
    }
    if (!driving || !car) {
        g_dark.at = 0, g_dark.taken = false, g_dark.outside = -1.f;
        return;
    }
    if (g_dark.car && car != g_lights_car) {   // switched elsewhere (note_headlights)
        g_dark.car = nullptr, g_dark.taken = true;
        return;
    }
    if (g_dark.taken) return;
    const float light = zone_light();
    if (light <= 0.f) return;
    const bool on    = g_dark.car != nullptr;
    const DWORD tick = GetTickCount();
    // outside: the brightest lately, fading by half in kOutsideHalf (the light fades in over a
    // moment at a tunnel's mouth: following it would follow it down); kept while ours are on
    if (g_dark.outside <= 0.f) g_dark.outside = light;
    if (!on) {
        const float dt  = static_cast<float>(tick - g_dark.outside_tick) / 1000.f;
        g_dark.outside  = std::max(light, g_dark.outside * std::exp2(-std::min(dt, 5.f) / kOutsideHalf));
    }
    g_dark.outside_tick = tick;
    const bool other = on ? light > g_dark.outside * kLight : light < g_dark.outside * kDark;   // the other way to now
    const DWORD now  = GetTickCount();
    if (!other) {
        g_dark.at = 0;
        return;
    }
    if (!g_dark.at) g_dark.at = now;
    if (now - g_dark.at < (on ? kLightFor : kDarkFor)) return;
    g_dark.at = 0;
    if (on) return dark_lights_off(car);
    if (car == g_lights_car) return;   // the player chose them in ANNA: theirs
    // even if the game has them on (its AI driver does in a tunnel): many cars' beams only show in
    // the dark by their presets' daylight thresholds, and ours shine in daylight too
    const auto* c = static_cast<const uint8_t*>(car);
    g_dark.own = c[CAR_LIGHTS_OWN], g_dark.on = c[CAR_LIGHTS_ON];
    own_headlights(car, true);
    g_dark.car = car;
    logf("car: headlights on (it's dark: %.6f, %.6f outside)", light, g_dark.outside);
}

// Where the game reads them for the lights: the car's update (inside ours), and also 0xF5C900
// (car, float) and 0xF7BCC0 (car), called from outside it, where Extra Options' are in place.
// Ours go in just before those read them.
constexpr uintptr_t RVA_READ_LIGHTS = 0xF5C900;
constexpr uintptr_t RVA_ANIM_LIGHTS = 0xF7BCC0;
using ReadLightsFn = uintptr_t (*)(void* car, float x);
using AnimLightsFn = uintptr_t (*)(void* car);
ReadLightsFn o_read_lights = nullptr;
AnimLightsFn o_anim_lights = nullptr;

uintptr_t d_read_lights(void* car, float x) {
    if (car && car == g_lights_car) write_lights(car);
    return o_read_lights(car, x);
}
uintptr_t d_anim_lights(void* car) {
    if (car && car == g_lights_car) write_lights(car);
    return o_anim_lights(car);
}

bool part_supported(void* car, Part p) {
    const char* n = kPartNames[static_cast<int>(p)];
    return has_event(car, n, "toggle") || (has_event(car, n, "open") && has_event(car, n, "close"));
}

bool part_open(void* car, Part p) {
    own_parts(car);
    return (g_parts_open >> static_cast<int>(p)) & 1;
}

void set_part(void* car, Part p, bool open) {
    own_parts(car);
    const int i = static_cast<int>(p);
    if (part_open(car, p) == open) return;
    uint8_t* graph = graph_of(car);
    if (!graph) return;
    auto send = [&](const char* n, bool to_open) {
        // open / close when the car has them, so a part another animation moved still goes where asked
        const char* action = has_event(car, n, to_open ? "open" : "close") ? (to_open ? "open" : "close")
                             : has_event(car, n, "toggle")                 ? "toggle"
                                                                           : nullptr;
        if (!action) return;
        const std::string name = std::string(n) + ":" + action;
        send_event_raw(car, graph, name.c_str());
        logf("car: %s", name.c_str());
    };
    const bool first = car != g_events_car;
    g_events_car     = car;
    for (const char* n : {kPartNames[i], p == Part::PopupLights ? "headlightR" : static_cast<const char*>(nullptr)})
        if (n) send(n, open);
    // The first event a car's animation graph gets puts the pop-up lights the game had up down
    // again (the graph starts from its own pose): up again, those that are.
    if (first) {
        if (p != Part::PopupLights && p != Part::PopupL && part_open(car, Part::PopupL)) send("headlightL", true);
        if (p != Part::PopupLights && p != Part::PopupR && part_open(car, Part::PopupR)) send("headlightR", true);
    }
    // the pair and each side move the same lights
    auto mark = [](Part q, bool v) { g_parts_open = (g_parts_open & ~(1u << static_cast<int>(q))) | (v ? 1u << static_cast<int>(q) : 0u); };
    mark(p, open);
    if (p == Part::PopupLights) mark(Part::PopupL, open), mark(Part::PopupR, open);
    else if (p == Part::PopupL || p == Part::PopupR)
        mark(Part::PopupLights, part_open(car, Part::PopupL) && part_open(car, Part::PopupR));
}

bool hook_headlight_beams() {
    FH2Hook* a = g_api->hook_function(g_mod, "ANNA: the headlights' beams (daylight)", g_base + RVA_UPLOAD_BEAMS,
                                      reinterpret_cast<void*>(&d_upload_beams), reinterpret_cast<void**>(&o_upload_beams));
    FH2Hook* b = a ? g_api->hook_function(g_mod, "ANNA: the headlights' glows (daylight)", g_base + RVA_UPLOAD_GLOWS,
                                          reinterpret_cast<void*>(&d_upload_glows), reinterpret_cast<void**>(&o_upload_glows))
                   : nullptr;
    if (!a || !b) return false;
    g_api->enable_hooks(g_mod, a);
    g_api->enable_hooks(g_mod, b);
    return true;
}

bool hook_headlight_reads() {
    FH2Hook* a = g_api->hook_function(g_mod, "ANNA: the headlights' state (read)", g_base + RVA_READ_LIGHTS,
                                      reinterpret_cast<void*>(&d_read_lights), reinterpret_cast<void**>(&o_read_lights));
    FH2Hook* b = a ? g_api->hook_function(g_mod, "ANNA: the headlights' state (animation)", g_base + RVA_ANIM_LIGHTS,
                                          reinterpret_cast<void*>(&d_anim_lights), reinterpret_cast<void**>(&o_anim_lights))
                   : nullptr;
    if (!a || !b) return false;
    g_api->enable_hooks(g_mod, a);
    g_api->enable_hooks(g_mod, b);
    return true;
}

// ---- doors shut when it drives off -----------------------------------------------------------
// The car's speed (as Extra Options reads it for "standing still"): its target (car +0x20, vtable
// 0x2C5CE28, whose +0x10 is 0xF646B0) gives its velocity with 0xF646B0(target, float[4]): the
// physics body's (+0x4BE8, its vtable +0x288). Metres a second, or -1.
constexpr uintptr_t RVA_TARGET_VT    = 0x2C5CE28;
constexpr uintptr_t RVA_GET_VELOCITY = 0xF646B0;
constexpr float kMovingSpeed         = 2.0f;   // about 7 km/h
using VelocityFn = void* (*)(void* target, float* out);

float speed_raw(void* car) {
    __try {
        auto* target = static_cast<uint8_t*>(car) + 0x20;
        void *vt, *body, *body_vt;
        uintptr_t getter;
        if (!peek(target, &vt) || vt != g_base + RVA_TARGET_VT || !peek(static_cast<uint8_t*>(vt) + 0x10, &getter) ||
            getter != reinterpret_cast<uintptr_t>(g_base + RVA_GET_VELOCITY) || !peek(target + 0x4BE8, &body) || !peek(body, &body_vt) ||
            !peek(static_cast<uint8_t*>(body_vt) + 0x288, &getter) || getter < reinterpret_cast<uintptr_t>(g_base) + 0x1000 ||
            getter >= reinterpret_cast<uintptr_t>(g_base) + 0x1DCC000)
            return -1;
        alignas(16) float v[4] = {};
        game<VelocityFn>(RVA_GET_VELOCITY)(target, v);
        if (!std::isfinite(v[0]) || !std::isfinite(v[1]) || !std::isfinite(v[2])) return -1;
        return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

float car_speed(void* car) { return car ? speed_raw(car) : -1; }

// What ANNA opened that shuts when the car moves: the doors, the hood, the trunk, the storage.
// Not the roof (a convertible drives open), the pop-up lights (they're the headlights') or the
// spoiler.
void close_doors_if_moving(void* car) {
    if (!car || !g_parts_open || car_speed(car) < kMovingSpeed) return;
    bool closed = false;
    for (Part p : {Part::Hood, Part::Trunk, Part::DoorLF, Part::DoorRF, Part::DoorLR, Part::DoorRR, Part::Storage, Part::StorageM})
        if (part_open(car, p)) set_part(car, p, false), closed = true;
    if (closed) logf("car: moving, the doors shut");
}

void log_car_events(void* car) {
    if (!car || car == g_logged_car) return;
    g_logged_car   = car;
    uint8_t* graph = graph_of(car);
    std::string events;
    if (graph && collect_events_guarded(*reinterpret_cast<uint8_t**>(graph + GRAPH_EVENTS), events))
        logf("the car's animation events: %s", events.empty() ? "(none)" : events.c_str());
}

} // namespace fh2anna
