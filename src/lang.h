// ANNA's text in the game's language: the text language Extra Options or the game chose, read
// from the game's string tables (as Extra Options' own plugins read it).
#pragma once
#include "common.h"

#include <string>

namespace fh2anna {

// The ini the language seen last is kept in (for the Mods page's rows, written before the game
// has a language): from fh2mod_init, before tr() is used.
void lang_init(const std::wstring& ini);
// `english` in the game's language now; itself when there's no translation (another mod's label).
const char* tr(const char* english);
std::wstring wide(const char* utf8);

} // namespace fh2anna
