// Voice commands: "ANNA, Enable Auto Drive", heard through the microphone with Windows' own
// offline speech recognition (SAPI), as Kinect heard them on the Xbox.
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace fh2anna {

struct VoiceCommand {
    std::string phrase;      // what's said after "ANNA", in the game's language (UTF-8)
    std::string english;     // in English: when Windows has no recogniser for that language
    std::string key;         // the entry it chooses
};

void voice_enable(bool on);                                   // any thread: starts or stops listening
void voice_set_commands(std::vector<VoiceCommand> commands,   // the HUD's thread, when they change
                        const char* language);                // the game's ("EN", "DE", "CHT"...)
bool voice_take(std::string& key);                            // the HUD's thread: a command heard, or:
inline constexpr const char* kVoiceWake   = "voice:wake";     //   "ANNA" heard, more maybe coming
inline constexpr const char* kVoiceOpen   = "voice:open";     //   "ANNA" alone
inline constexpr const char* kVoiceMissed = "voice:missed";   //   "ANNA" and something it doesn't know
inline constexpr const char* kVoiceBack   = "voice:back";     //   "back" (ANNA awake)
inline constexpr const char* kVoiceClose  = "voice:close";    //   "close" (ANNA awake)
void voice_set_awake(bool awake);                             // the HUD's thread: ANNA's listening or open

} // namespace fh2anna
