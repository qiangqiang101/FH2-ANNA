// FH3's ANNA textures, built into the DLL (src/resources.rc) and written where the game finds them:
// UI/textures/data_bound/FH2ANNA/<name>.swatchbin, which XAML names
// "/ForzaUI;component/Textures/Data_Bound/FH2ANNA/<name>.png". Written from fh2mod_init, before
// the game reads its files.
#include "common.h"

#include <string>

namespace fh2anna {

HMODULE g_self = nullptr;

bool resource(const char* name, const void** data, DWORD* size) {
    HRSRC res = FindResourceA(g_self, name, MAKEINTRESOURCEA(10));   // RT_RCDATA
    HGLOBAL mem = res ? LoadResource(g_self, res) : nullptr;
    *data       = mem ? LockResource(mem) : nullptr;
    *size       = *data ? SizeofResource(g_self, res) : 0;
    return *size != 0;
}

bool write_textures(HMODULE self) {
    g_self = self;
    static const char* const names[] = {"ANNA_Glow_On", "ANNA_Glow_ANNAOnly", "ANNA_Logo", "VerticalGradient",
                                        "Minimap_OpacityMask", "Minimap_OpacityMask_WithDestination",
                                        "Minimap_OpacityMask_MapOnly", "Minimap_OpacityMask_MapOnly_WithDestination"};
    bool all = true;
    for (const char* n : names) {
        const void* data = nullptr;
        DWORD size       = 0;
        const std::string file = std::string("UI/textures/data_bound/FH2ANNA/") + n + ".swatchbin";
        if (!resource((std::string("TEX_") + n).c_str(), &data, &size) || !g_api->set_file(g_mod, file.c_str(), data, size)) all = false;
    }
    if (!all) logf("couldn't write FH3's ANNA textures: ANNA's look is missing some");
    return all;
}

} // namespace fh2anna
