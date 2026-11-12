// Shared by FH2ANNA's source files.
#pragma once
#include "fh2mod.h"

#include <windows.h>

#include <cstdint>
#include <string>

namespace fh2anna {

extern const FH2ModApi* g_api;
extern FH2Mod* g_mod;
extern uint8_t* g_base;

template <class F> F game(uintptr_t rva) { return reinterpret_cast<F>(g_base + rva); }

void logf(const char* fmt, ...);

// Readable memory, for checking what the game hands us before reading it.
bool readable(const void* p, std::size_t n);

// fh2anna.cpp: the game takes driving input (no menu, map or pause has it), as last read.
bool game_driving_input();
// A mod holds the driving input (fh2anna.h's hold_input): autodrive goes on as if the game had it.
bool input_held();
// The cinematic camera's button is down (the pad as read, before ANNA hides it from the game).
bool cinematic_held();
// The row template's triggers for greyed-out entries (as the control's XAML is read), and from
// then on only those can be greyed out.
std::string disabled_triggers();
// The autodrive prompts' text (the row template's FH2ANNA_TEXT_ tokens), in the game's language.
void hint_texts(std::string& xaml);

// anna_xaml.cpp: FH3's look for the HUD's ANNA control, as its XAML is read.
bool hook_anna_xaml();
bool restyle_anna(std::string& xaml);

// textures.cpp: FH3's ANNA textures, into the game's media (from fh2mod_init).
bool write_textures(HMODULE self);
bool resource(const char* name, const void** data, DWORD* size);   // one built into the DLL

} // namespace fh2anna
