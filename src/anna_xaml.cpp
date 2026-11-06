// FH3's look for FH2's ANNA control (Controls/HUDAnnaControl.xaml), as the game reads it.
//
// The XAML is edited as the game reads it, as Extra Options and FH2 Mouse & Keyboard edit the
// pages they change: the loader (the call at 0x16EBB37) gets a file's bytes with
// 0xAABEC0(reader, Ptr* out), a resource object with the text at +0x28 and its length at +0x30.
// That function is hooked; for the loader's call (or Extra Options', which takes that call over
// and calls it from its own DLL) a resource of ours with the edited text is handed back.
//
// The look (FH3's ANNA): closed, the d-pad and "ANNA" under the minimap; open, a ring round the
// minimap, the label outlined, and the list to the right of the map, grey with the highlighted
// entry in ANNA's blue and the d-pad's right beside it.
#include "common.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <vector>

#ifdef _MSC_VER
#include <intrin.h>
#define RETURN_ADDRESS() _ReturnAddress()
#else
#define RETURN_ADDRESS() __builtin_return_address(0)
#endif

namespace fh2anna {
namespace {

constexpr uintptr_t RVA_GET_RESOURCE     = 0x0AABEC0;   // Ptr* (reader, Ptr* out): the reader's data, add-ref'd
constexpr uintptr_t RVA_PAGE_XAML_RETURN = 0x16EBB3C;   // after the loader's call to it
constexpr uintptr_t RVA_GAME_ALLOCATE    = 0x03229C0;
constexpr uintptr_t RVA_GAME_FREE        = 0x031C7B0;
constexpr uintptr_t RVA_OBJECT_INIT      = 0x143EB80;
constexpr uintptr_t RVA_OBJECT_CLEANUP   = 0x1442050;
constexpr uintptr_t RVA_OBJECT_FREE      = 0x0257730;   // when an AVUI object's refcount (+0x10) reaches 0

struct XamlResource {
    void** vtable;
    uint8_t header[0x20];
    char* bytes;
    int length;
    int capacity;
};
static_assert(offsetof(XamlResource, bytes) == 0x28, "the resource's text");

using GetResourceFn = void** (*)(void* reader, void** out);
using AllocateFn    = void* (*)(std::size_t);
using ObjectFn      = void (*)(void* obj);
GetResourceFn o_get_resource = nullptr;
uintptr_t g_image_end        = 0;
std::atomic<bool> g_restyled{false};

std::mutex g_vt_mu;
struct VtCopy {
    void** from;
    void* slots[7];
};
std::vector<VtCopy*> g_vts;   // guarded by g_vt_mu, never freed

void* resource_delete(XamlResource* r, unsigned flags) {
    game<ObjectFn>(RVA_OBJECT_CLEANUP)(r);
    if (flags & 1) game<ObjectFn>(RVA_GAME_FREE)(r);
    return r;
}

// The original's vtable, with a destructor that frees what we allocated.
void** our_vtable(void** from) {
    std::scoped_lock lk{g_vt_mu};
    for (VtCopy* v : g_vts)
        if (v->from == from) return v->slots + 1;
    auto* v = new VtCopy{from, {}};
    std::memcpy(v->slots, from - 1, sizeof v->slots);   // [-1] is the RTTI locator
    v->slots[1] = reinterpret_cast<void*>(&resource_delete);
    g_vts.push_back(v);
    return v->slots + 1;
}

bool resource_text(void* obj, const char** bytes, int* length) {
    __try {
        auto* r = static_cast<XamlResource*>(obj);
        if (!readable(r, sizeof *r) || !r->bytes || r->length <= 0 || r->length > (8 << 20)) return false;
        if (!readable(r->bytes, static_cast<std::size_t>(r->length))) return false;
        *bytes  = r->bytes;
        *length = r->length;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void release(void* obj) {
    if (obj && InterlockedDecrement(reinterpret_cast<volatile LONG*>(static_cast<uint8_t*>(obj) + 0x10)) == 0)
        game<ObjectFn>(RVA_OBJECT_FREE)(obj);
}

bool replace_resource(void** out, const std::string& text) {
    auto* original             = static_cast<XamlResource*>(*out);
    const std::size_t capacity = text.size() + 1;
    auto* r = static_cast<XamlResource*>(game<AllocateFn>(RVA_GAME_ALLOCATE)(sizeof(XamlResource) + capacity));
    if (!r) return false;
    char* bytes = reinterpret_cast<char*>(r + 1);
    std::memcpy(bytes, text.data(), text.size());
    bytes[text.size()] = 0;
    __try {
        game<ObjectFn>(RVA_OBJECT_INIT)(r);
        r->vtable   = our_vtable(original->vtable);
        r->bytes    = bytes;
        r->length   = static_cast<int>(text.size());
        r->capacity = static_cast<int>(capacity);
        *out        = r;
        release(original);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool has(const char* bytes, int length, const char* what) {
    const std::size_t n = std::strlen(what);
    const char* end     = bytes + length;
    return std::search(bytes, end, what, what + n) != end;
}

void edit(void** out) {
    const char* bytes = nullptr;
    int length        = 0;
    if (!resource_text(*out, &bytes, &length)) return;
    if (!has(bytes, std::min(length, 2048), "x:Class=\"ForzaUI.HUDAnnaControl\"")) return;
    std::string text(bytes, static_cast<std::size_t>(length));
    // FH3's control, ported (assets/HUDAnnaControl.xaml); FH2's own, restyled, if that's missing.
    const void* port = nullptr;
    DWORD port_size  = 0;
    if (resource("ANNA_XAML", &port, &port_size)) {
        const char* p = static_cast<const char*>(port);
        if (port_size >= 3 && !std::memcmp(p, "ï»¿", 3)) p += 3, port_size -= 3;   // a BOM
        text.assign(p, port_size);
        static const char marker[] = "<!--FH2ANNA_OFF-->";   // greyed-out entries' triggers go here
        if (const std::size_t at = text.find(marker); at != std::string::npos) text.replace(at, sizeof marker - 1, disabled_triggers());
        if (replace_resource(out, text) && !g_restyled.exchange(true)) logf("the ANNA control is FH3's");
        return;
    }
    if (!restyle_anna(text)) {
        logf("the ANNA control's XAML isn't as expected: it keeps FH2's look");
        return;
    }
    if (replace_resource(out, text) && !g_restyled.exchange(true)) logf("the ANNA control has FH3's look");
}

void** d_get_resource(void* reader, void** out) {
    const auto from = reinterpret_cast<uintptr_t>(RETURN_ADDRESS());
    void** r        = o_get_resource(reader, out);
    // The loader's call, or Extra Options' (from its own DLL): nothing else outside the game calls it.
    const bool in_game = from >= reinterpret_cast<uintptr_t>(g_base) && from < g_image_end;
    if (r == out && out && *out && (!in_game || from == reinterpret_cast<uintptr_t>(g_base) + RVA_PAGE_XAML_RETURN))
        edit(out);
    return r;
}

// ---- the edit ------------------------------------------------------------------------------

bool replace_once(std::string& s, const std::string& from, const std::string& to) {
    const std::size_t at = s.find(from);
    if (at == std::string::npos) return false;
    s.replace(at, from.size(), to);
    return true;
}

// Inserts `what` after the first "<Storyboard>" following `anchor`.
bool into_storyboard(std::string& s, const std::string& anchor, const std::string& what) {
    const std::size_t a = s.find(anchor);
    if (a == std::string::npos) return false;
    const std::string open = "<Storyboard>";
    const std::size_t b    = s.find(open, a);
    if (b == std::string::npos) return false;
    s.insert(b + open.size(), what);
    return true;
}

// Replaces the element that starts with `start` (its whole text, to the matching end tag `end`).
bool replace_element(std::string& s, const std::string& start, const std::string& end, const std::string& with) {
    const std::size_t a = s.find(start);
    if (a == std::string::npos) return false;
    const std::size_t b = s.find(end, a);
    if (b == std::string::npos) return false;
    s.replace(a, b + end.size() - a, with);
    return true;
}

// FH3's colours (its HUDAnnaControl.xaml): the highlight, the ring when ANNA is open, and the
// faint ring and outline when it isn't.
const char kBlue[]   = "#FF00C6FF";   // A_Color_ItemHighlighted
const char kRing[]   = "#FF00D2FF";   // A_Color_Minimap_EdgeActive
const char kFaint[]  = "#1AFFFFFF";   // A_Color_Minimap_EdgeInactive

// A ring round the minimap (its Border: 304 x 214, 30 down).
std::string ring(const char* name, const char* colour, const char* opacity) {
    return std::string("\r\n            <Path x:Name=\"") + name +
           "\" Width=\"286\" Height=\"286\" Margin=\"0,30,0,0\" HorizontalAlignment=\"Center\" VerticalAlignment=\"Center\" "
           "Data=\"M1.5,143 A141.5,141.5 0 1 1 284.5,143 A141.5,141.5 0 1 1 1.5,143 Z\" Stroke=\"" + colour +
           "\" StrokeThickness=\"3\" Opacity=\"" + opacity + "\"/>";
}

// FH3's glow round the minimap (its ANNA_Glow_On, 310 square for a 288 ring), tinted, for when
// ANNA is open: the same size for our 286 ring, in the same place.
std::string glow() {
    return std::string("\r\n            <local:FZMaterial x:Name=\"FH2ANNA_Ring\" Width=\"308\" Height=\"308\" Margin=\"0,30,0,0\" "
                       "HorizontalAlignment=\"Center\" VerticalAlignment=\"Center\" Preset=\"{StaticResource A_FZMaterialPresetAdditive}\" "
                       "ImageSource=\"/ForzaUI;component/Textures/Data_Bound/FH2ANNA/ANNA_Glow_On.png\" Color=\"") +
           kRing + "\" Opacity=\"0\"/>";
}

std::string ring_opacity(const char* value) {
    return std::string("\r\n                        <DoubleAnimationUsingKeyFrames Storyboard.TargetProperty=\"(UIElement.Opacity)\" "
                       "Storyboard.TargetName=\"FH2ANNA_Ring\"><DiscreteDoubleKeyFrame KeyTime=\"0:0:0\" Value=\"") +
           value + "\"/></DoubleAnimationUsingKeyFrames>";
}

} // namespace

bool restyle_anna(std::string& s) {
    if (s.find("FH2ANNA_Ring") != std::string::npos) return false;   // already
    const std::string blue = kBlue;
    bool ok = true;
    // Colours: the highlight blue; the label without a box until ANNA is open, then outlined.
    ok = ok && replace_once(s, "<Color x:Key=\"A_Color_ItemSelected\">#FF55CD5F</Color>", "<Color x:Key=\"A_Color_ItemSelected\">" + blue + "</Color>");
    ok = ok && replace_once(s, "x:Key=\"A_Brush_ANNAButtonBackground\" Color=\"#19FFFFFF\"", "x:Key=\"A_Brush_ANNAButtonBackground\" Color=\"#00000000\"");
    ok = ok && replace_once(s, "x:Key=\"A_Brush_ANNAButtonBorder\" Color=\"#66FFFFFF\"",
                            "x:Key=\"A_Brush_ANNAButtonBorder\" Color=\"#00FFFFFF\"");   // closed: no box, as FH3
    ok = ok && replace_once(s, "x:Key=\"A_Brush_ANNAButtonBackgroundSelected\" Color=\"#19FFFFFF\"",
                            "x:Key=\"A_Brush_ANNAButtonBackgroundSelected\" Color=\"#B0101010\"");   // FH3's translucent dark
    // The label 15 up, so its box is the frame FH3's glow texture draws under the ring.
    ok = ok && replace_once(s, "<Grid Margin=\"23.267,0\" HorizontalAlignment=\"Center\" VerticalAlignment=\"Bottom\"",
                            "<Grid Margin=\"23.267,-15,23.267,15\" HorizontalAlignment=\"Center\" VerticalAlignment=\"Bottom\"");
    ok = ok && replace_once(s, "x:Key=\"A_Brush_ANNAButtonBorderSelected\" Color=\"#FFFFFFFF\"",
                            // none: FH3's glow texture has the label's outline in it
                            "x:Key=\"A_Brush_ANNAButtonBorderSelected\" Color=\"#00FFFFFF\"");
    // The entries: flat grey bars, the highlighted one blue with the d-pad's right beside it.
    ok = ok && replace_element(s, "<DataTemplate x:Key=\"A_DataTemplate_SpeechItem\">", "</DataTemplate>",
                               "<DataTemplate x:Key=\"A_DataTemplate_SpeechItem\">\r\n"
                               "                <DockPanel Margin=\"0,1.5,0,1.5\">\r\n"
                               "                    <local:FZMaterial x:Name=\"Button\" Color=\"{StaticResource A_Color_ItemSelected}\" Width=\"36\" Height=\"36\" Margin=\"3,0,0,0\" Visibility=\"Hidden\" DockPanel.Dock=\"Right\">\r\n"
                               "                        <local:FZMaterial ImageSource=\"{StaticResource A_BitmapImage_ControllerButton_DPadRight}\" Width=\"28\" Height=\"28\" HorizontalAlignment=\"Center\" VerticalAlignment=\"Center\"/>\r\n"
                               "                    </local:FZMaterial>\r\n"
                               "                    <local:FZMaterial x:Name=\"Item\" Color=\"#C0505050\" Height=\"36\">\r\n"
                               "                        <local:FZTextBlock Text=\"{Binding Command}\" Style=\"{StaticResource A_Style_FZTextBlock_HudMedium}\" Margin=\"8,-1,8,0\" Width=\"214\" VerticalAlignment=\"Center\" HorizontalAlignment=\"Left\"/>\r\n"
                               "                    </local:FZMaterial>\r\n"
                               "                </DockPanel>\r\n"
                               "                <DataTemplate.Triggers>\r\n"
                               "                    <DataTrigger Binding=\"{Binding IsSelected}\" Value=\"True\">\r\n"
                               "                        <Setter TargetName=\"Item\" Property=\"Color\" Value=\"{DynamicResource A_Color_ItemSelected}\"/>\r\n"
                               "                        <Setter TargetName=\"Button\" Property=\"Visibility\" Value=\"Visible\"/>\r\n"
                               "                    </DataTrigger>\r\n"
                               "                </DataTemplate.Triggers>\r\n"
                               "            </DataTemplate>");
    // The list: to the right of the map, in a Canvas that takes no room, without its "ANNA" title.
    ok = ok && replace_once(s,
                            "<StackPanel Orientation=\"Vertical\" x:Name=\"ANNAMain\" HorizontalAlignment=\"Center\" VerticalAlignment=\"Bottom\" Margin=\"0,0,0,24\" Style=\"{StaticResource A_Style_FrameworkElement_MapOnly}\">",
                            "<Canvas Height=\"0\" HorizontalAlignment=\"Left\" Panel.ZIndex=\"1\" Style=\"{StaticResource A_Style_FrameworkElement_MapOnly}\">\r\n"
                            // The list's middle on the map's: the ring's centre is 150 below the Canvas (the
                            // map's Grid starts 90 above it, the ring 30 into its 450). A 1000-high Grid with
                            // its middle there centres the list on it, however long it is.
                            "        <Grid Canvas.Left=\"430\" Canvas.Top=\"-350\" Height=\"1000\">\r\n"
                            "        <StackPanel Orientation=\"Vertical\" x:Name=\"ANNAMain\" VerticalAlignment=\"Center\">");
    ok = ok && replace_once(s, "<local:FZTextBlock Text=\"ANNA\" Style=\"{StaticResource A_Style_FZTextBlock_HudMedium}\"/>", "");
    if (ok) {
        const std::size_t list_end = s.find("</local:CommonListBox>", s.find("x:Name=\"ANNAMain\""));
        const std::size_t panel_end = list_end == std::string::npos ? list_end : s.find("</StackPanel>", list_end);
        ok = panel_end != std::string::npos;
        if (ok) s.insert(panel_end + std::strlen("</StackPanel>"), "\r\n        </Grid>\r\n        </Canvas>");
    }
    // The ring round the map, shown while ANNA is open.
    ok = ok && replace_once(s, "Fill=\"{StaticResource A_Brush_MapShadowBackground}\" Opacity=\"0\"/>",
                            "Fill=\"{StaticResource A_Brush_MapShadowBackground}\" Opacity=\"0\"/>" +
                            ring("FH2ANNA_RingFaint", kFaint, "1") + glow());
    ok = ok && into_storyboard(s, "<VisualState x:Name=\"ShowMinimized\">", ring_opacity("0"));
    ok = ok && into_storyboard(s, "<VisualState x:Name=\"Selected\">", ring_opacity("1"));
    ok = ok && into_storyboard(s, "<VisualState x:Name=\"ShowFull\">", ring_opacity("1"));
    ok = ok && replace_once(s, "<Storyboard x:Key=\"A_Storyboard_PlaySleepSoundAndMinimize\">",
                            "<Storyboard x:Key=\"A_Storyboard_PlaySleepSoundAndMinimize\">" + ring_opacity("0"));
    // The label: FH3's rounded outline, the d-pad and "ANNA", never "unavailable" (there's no Kinect to be).
    ok = ok && replace_once(s, "BorderThickness=\"2\" CornerRadius=\"6\" Height=\"32\"", "BorderThickness=\"3\" CornerRadius=\"9\" Height=\"34\" Width=\"156\"");
    ok = ok && replace_once(s,
                            "ImageSource=\"/ForzaUI;component/Textures/Anthem/HUD/ANNA_Mic_Icon_Available.png\" Color=\"White\" Visibility=\"{Binding IsAvailable, Converter={StaticResource A_FZBooleanToVisibilityConverter}}\"",
                            "ImageSource=\"{StaticResource A_BitmapImage_ControllerButton_DPadDown}\" Color=\"White\"");
    ok = ok && replace_once(s,
                            "ImageSource=\"/ForzaUI;component/Textures/Anthem/HUD/ANNA_Mic_Icon_Unavailable.png\" Color=\"White\" Visibility=\"{Binding IsAvailable, Converter={StaticResource A_FZBooleanToVisibilityConverter_Inverse}}\"",
                            "ImageSource=\"/ForzaUI;component/Textures/Anthem/HUD/ANNA_Mic_Icon_Unavailable.png\" Color=\"White\" Visibility=\"Collapsed\"");
    ok = ok && replace_once(s,
                            "Text=\"{local:Loc Anna.IDS_Label_Available}\" Style=\"{StaticResource A_Style_FZTextBlock_HudMedium}\" Visibility=\"{Binding IsAvailable, Converter={StaticResource A_FZBooleanToVisibilityConverter}}\"",
                            // kept (ANNA's animations colour it), hidden; FH3's logo beside it
                            "Text=\"\" Style=\"{StaticResource A_Style_FZTextBlock_HudMedium}\" Visibility=\"Collapsed\"/>\r\n"
                            "                    <local:FZMaterial ImageSource=\"/ForzaUI;component/Textures/Data_Bound/FH2ANNA/ANNA_Logo.png\" "
                            "Width=\"79\" Height=\"24\" Margin=\"2,3,0,0\" VerticalAlignment=\"Center\"");
    ok = ok && replace_once(s,
                            "Text=\"{local:Loc Anna.IDS_Label_Unavailable}\" Style=\"{StaticResource A_Style_FZTextBlock_HudMedium}\" Visibility=\"{Binding IsAvailable, Converter={StaticResource A_FZBooleanToVisibilityConverter_Inverse}}\"",
                            "Text=\"\" Style=\"{StaticResource A_Style_FZTextBlock_HudMedium}\" Visibility=\"Collapsed\"");
    // FH2's own soft highlight round the label (shown as ANNA wakes): FH3's outline is enough.
    ok = ok && replace_once(s, "x:Name=\"fZMaterial1\" Color=\"{StaticResource A_Color_ANNAButtonSelected}\" Margin=\"-16\" Opacity=\"0.2\"",
                            "x:Name=\"fZMaterial1\" Color=\"{StaticResource A_Color_ANNAButtonSelected}\" Margin=\"-16\" Opacity=\"0\"");
    // The map round, filling the ring (FH2's is a 304 x 214 ellipse), fading out just inside it,
    // and its N on a circle just inside the ring.
    ok = ok && replace_once(s, "<Border Width=\"304\" Height=\"214\" OpacityMask=\"{StaticResource A_Brush_MapOpacity}\"",
                            "<Border Width=\"266\" Height=\"266\" OpacityMask=\"{StaticResource A_Brush_MapOpacity}\"");
    ok = ok && replace_once(s, "<GradientStop Color=\"#FF000000\" Offset=\".7 \"/>", "<GradientStop Color=\"#FF000000\" Offset=\".85\"/>");
    ok = ok && replace_once(s, "Cosine=\"False\" ResultScale=\"-137\"", "Cosine=\"False\" ResultScale=\"-104\"");
    ok = ok && replace_once(s, "Cosine=\"True\" ResultScale=\"-92\"", "Cosine=\"True\" ResultScale=\"-104\"");
    return ok;
}

bool hook_anna_xaml() {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_base);
    const auto* nt  = reinterpret_cast<const IMAGE_NT_HEADERS64*>(g_base + dos->e_lfanew);
    g_image_end     = reinterpret_cast<uintptr_t>(g_base) + nt->OptionalHeader.SizeOfImage;
    static const uint8_t expect[][12] = {
        {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x8B, 0x15},   // allocate
        {0x48, 0x85, 0xC9, 0x74, 0x1D, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B},   // free
        {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8D},   // object init
        {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8D, 0x05, 0xB3, 0xD0, 0x9F},   // object cleanup
    };
    const uintptr_t at[] = {RVA_GAME_ALLOCATE, RVA_GAME_FREE, RVA_OBJECT_INIT, RVA_OBJECT_CLEANUP};
    for (int i = 0; i < 4; ++i)
        if (std::memcmp(g_base + at[i], expect[i], sizeof expect[i])) {
            logf("the game's XAML code isn't as expected: ANNA keeps FH2's look");
            return false;
        }
    FH2Hook* h = g_api->hook_function(g_mod, "ANNA: the control's XAML", g_base + RVA_GET_RESOURCE,
                                      reinterpret_cast<void*>(&d_get_resource), reinterpret_cast<void**>(&o_get_resource));
    return h && g_api->enable_hooks(g_mod, h);
}

} // namespace fh2anna
