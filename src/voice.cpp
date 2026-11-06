// Voice commands, through Windows' offline speech recognition (SAPI 5's in-process recogniser):
// a grammar of "ANNA, <entry>" for the entries that can be chosen now (fh2anna.cpp keeps it
// current), heard on a thread of its own; what's heard is chosen on the HUD's thread, as from
// the menu. Saying "ANNA" first is what keeps game sound and talk from choosing anything.
//
// The recogniser is Windows' for the game's language; without one (Windows has them for some
// languages only), the English one with the English labels, if there's that.
#include "voice.h"

#include <sapi.h>

#include <atomic>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

namespace fh2anna {
namespace {

std::mutex g_mu;
std::vector<VoiceCommand> g_commands;   // guarded by g_mu
std::string g_language = "EN";          // guarded by g_mu
unsigned g_version     = 0;             // guarded by g_mu: bumped when they change
std::deque<std::string> g_heard;        // guarded by g_mu

std::atomic<bool> g_running{false};
std::atomic<bool> g_awake{false};   // ANNA's awake: commands without "ANNA" too
std::thread g_thread;
std::mutex g_thread_mu;   // starting and stopping

std::wstring wide_text(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// The game's language codes as Windows' language ids (SAPI's recognisers' "Language" attribute).
const wchar_t* lcid_of(const std::string& code) {
    static const struct {
        const char* code;
        const wchar_t* lcid;
    } k[] = {{"EN", L"409"}, {"GB", L"809"}, {"DE", L"407"}, {"FR", L"40C"}, {"ES", L"C0A"}, {"MX", L"80A"}, {"IT", L"410"},
             {"JP", L"411"}, {"KO", L"412"}, {"CHT", L"404"}, {"BR", L"416"}, {"PL", L"415"}, {"RU", L"419"}, {"NL", L"413"},
             {"CZ", L"405"}, {"HU", L"40E"}};
    for (const auto& e : k)
        if (code == e.code) return e.lcid;
    return L"409";
}

// "ANNA" as it's written for each language's recogniser.
const wchar_t* wake_word(const std::string& code) {
    if (code == "JP") return L"アンナ";
    if (code == "KO") return L"안나";
    if (code == "CHT") return L"安娜";
    if (code == "RU") return L"Анна";
    return L"Anna";
}

template <class T> void release(T*& p) {
    if (p) p->Release(), p = nullptr;
}

ISpObjectToken* recogniser_for(const wchar_t* lcid) {
    ISpObjectTokenCategory* category = nullptr;
    IEnumSpObjectTokens* tokens      = nullptr;
    ISpObjectToken* token            = nullptr;
    const std::wstring want          = std::wstring(L"Language=") + lcid;
    if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&category))) &&
        SUCCEEDED(category->SetId(SPCAT_RECOGNIZERS, FALSE)) && SUCCEEDED(category->EnumTokens(want.c_str(), nullptr, &tokens)))
        if (tokens->Next(1, &token, nullptr) != S_OK) token = nullptr;
    release(tokens);
    release(category);
    return token;
}

void clear_event(SPEVENT& e) {
    if (e.elParamType == SPET_LPARAM_IS_OBJECT || e.elParamType == SPET_LPARAM_IS_TOKEN) {
        if (e.lParam) reinterpret_cast<IUnknown*>(e.lParam)->Release();
    } else if (e.elParamType == SPET_LPARAM_IS_POINTER || e.elParamType == SPET_LPARAM_IS_STRING) {
        if (e.lParam) CoTaskMemFree(reinterpret_cast<void*>(e.lParam));
    }
    e = {};
}

struct Listener {
    ISpRecognizer* recogniser = nullptr;
    ISpRecoContext* context   = nullptr;
    ISpRecoGrammar* grammar   = nullptr;
    SPSTATEHANDLE rule        = nullptr;   // "ANNA" and a command
    SPSTATEHANDLE awake_rule  = nullptr;   // a command, while ANNA's awake
    std::string language;                 // the recogniser's ("EN" when it's the English fallback)
    std::vector<std::string> keys;        // the grammar's commands' entries, by their property value
    bool woke = false;                    // "ANNA" heard, the rest of what's said not yet

    void stop() {
        woke = false;
        if (grammar) grammar->SetRuleState(nullptr, nullptr, SPRS_INACTIVE);
        release(grammar);
        release(context);
        release(recogniser);
        rule = awake_rule = nullptr;
        awake = false;
        keys.clear();
    }

    // The game's language's recogniser, or the English one: false if neither (or no microphone).
    bool start(const std::string& game_language) {
        stop();
        ISpObjectToken* token = recogniser_for(lcid_of(game_language));
        language              = game_language;
        if (!token && game_language != "EN" && game_language != "GB") token = recogniser_for(L"409"), language = "EN";
        if (!token) token = recogniser_for(L"809"), language = "EN";
        if (!token) {
            logf("voice: Windows has no speech recogniser for %s or English: no voice commands", game_language.c_str());
            return false;
        }
        IUnknown* audio = nullptr;
        HRESULT hr      = CoCreateInstance(CLSID_SpInprocRecognizer, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&recogniser));
        if (SUCCEEDED(hr)) hr = recogniser->SetRecognizer(token);
        if (SUCCEEDED(hr)) hr = CoCreateInstance(CLSID_SpMMAudioIn, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&audio));   // the default microphone
        if (SUCCEEDED(hr)) hr = recogniser->SetInput(audio, TRUE);
        release(audio);
        release(token);
        if (SUCCEEDED(hr)) hr = recogniser->CreateRecoContext(&context);
        if (SUCCEEDED(hr)) hr = context->SetNotifyWin32Event();
        if (SUCCEEDED(hr)) hr = context->SetInterest(SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_FALSE_RECOGNITION) | SPFEI(SPEI_HYPOTHESIS),
                                                        SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_FALSE_RECOGNITION) | SPFEI(SPEI_HYPOTHESIS));
        if (SUCCEEDED(hr)) hr = context->CreateGrammar(1, &grammar);
        if (SUCCEEDED(hr)) hr = grammar->GetRule(L"FH2ANNA", 0, SPRAF_TopLevel | SPRAF_Active | SPRAF_Dynamic, TRUE, &rule);
        if (SUCCEEDED(hr)) hr = grammar->GetRule(L"FH2ANNA_AWAKE", 0, SPRAF_TopLevel | SPRAF_Dynamic, TRUE, &awake_rule);
        if (FAILED(hr)) {
            logf("voice: the speech recogniser or the microphone couldn't be opened (0x%08lX; Windows' microphone privacy "
                 "setting for desktop apps?): no voice commands",
                 static_cast<unsigned long>(hr));
            stop();
            return false;
        }
        logf("voice: listening (%s recogniser), for \"ANNA\" and a command", language.c_str());
        return true;
    }

    // Two rules, the same commands in each: "ANNA" and a command (always listened for), and a
    // command alone, while ANNA's awake (listening after "ANNA", or open): "Set Route To
    // Nearest", then "Race". A command is its label, or its list's and its own ("Set Route To
    // Nearest Bonus Board"), in English with a plural too ("Bonus Boards") and after any of the
    // ways people lead into one ("set GPS to the nearest", "take me to the nearest"...).
    void add_commands(SPSTATEHANDLE from, const std::vector<VoiceCommand>& commands, int& failed) {
        SPSTATEHANDLE body = nullptr;
        if (FAILED(grammar->CreateNewState(from, &body))) return;
        grammar->AddWordTransition(from, body, nullptr, nullptr, SPWT_LEXICAL, 1.0f, nullptr);   // nothing first
        if (language == "EN") {
            static const wchar_t* const lead_ins[] = {
                L"please", L"set GPS to", L"set GPS to nearest", L"set GPS to the nearest", L"set the GPS to the nearest",
                L"set route to", L"set route to nearest", L"set route to the nearest", L"set a route to the nearest",
                L"route to the nearest", L"route me to the nearest", L"take me to", L"take me to the nearest",
                L"navigate to", L"navigate to the nearest", L"go to the nearest", L"find the nearest", L"nearest",
                L"start matchmaking for", L"start", L"join", L"find", L"turn", L"switch"};
            for (const wchar_t* w : lead_ins) grammar->AddWordTransition(from, body, w, L" ", SPWT_LEXICAL, 1.0f, nullptr);
        }
        for (std::size_t k = 0; k < commands.size(); ++k) {
            const VoiceCommand& c   = commands[k];
            const std::string& said = language == "EN" ? c.english : c.phrase;
            if (said.empty()) continue;
            std::vector<std::wstring> ways = {wide_text(said)};
            if (language == "EN" && said.back() != 's' && said.back() != 'S') ways.push_back(wide_text(said + "s"));
            bool any = false;
            for (const std::wstring& w : ways) any = add(body, w.c_str(), static_cast<LONG>(k)) || any;
            if (!any) ++failed;
        }
    }

    bool add(SPSTATEHANDLE from, const wchar_t* words, LONG value) {
        SPPROPERTYINFO p{};
        p.pszName     = L"command";
        p.ulId        = 1;
        p.vValue.vt   = VT_I4;
        p.vValue.lVal = value;
        return SUCCEEDED(grammar->AddWordTransition(from, nullptr, words, L" ", SPWT_LEXICAL, 1.0f, &p));
    }

    void set_commands(const std::vector<VoiceCommand>& commands) {
        if (!grammar) return;
        grammar->SetRuleState(nullptr, nullptr, SPRS_INACTIVE);
        grammar->ClearRule(rule);
        grammar->ClearRule(awake_rule);
        keys.clear();
        for (const VoiceCommand& c : commands) keys.push_back(c.key);
        keys.push_back(kVoiceBack);
        keys.push_back(kVoiceClose);
        const LONG back = static_cast<LONG>(keys.size() - 2), close = static_cast<LONG>(keys.size() - 1);
        int failed = 0;
        SPSTATEHANDLE after_anna = nullptr;
        if (FAILED(grammar->CreateNewState(rule, &after_anna))) return;
        grammar->AddWordTransition(rule, after_anna, wake_word(language), L" ", SPWT_LEXICAL, 1.0f, nullptr);
        add(rule, wake_word(language), -1);   // "ANNA" alone: ANNA listens
        add_commands(after_anna, commands, failed);
        add_commands(awake_rule, commands, failed);
        if (language == "EN") {
            for (const wchar_t* w : {L"back", L"go back"}) add(awake_rule, w, back);
            for (const wchar_t* w : {L"close", L"cancel", L"never mind", L"go away"}) add(awake_rule, w, close);
        }
        grammar->Commit(0);
        grammar->SetRuleState(L"FH2ANNA", nullptr, SPRS_ACTIVE);
        apply_awake(true);
        if (failed) logf("voice: %d of %zu commands the recogniser couldn't take", failed, commands.size());
    }

    bool awake = false;   // the awake rule's on
    void apply_awake(bool force = false) {
        const bool now = g_awake.load();
        if (!grammar || (!force && now == awake)) return;
        awake = now;
        grammar->SetRuleState(L"FH2ANNA_AWAKE", nullptr, now ? SPRS_ACTIVE : SPRS_INACTIVE);
    }

    void heard(const char* what) {
        std::scoped_lock lk{g_mu};
        g_heard.push_back(what);
    }

    void drain() {
        if (!context) return;
        SPEVENT e{};
        ULONG fetched = 0;
        while (SUCCEEDED(context->GetEvents(1, &e, &fetched)) && fetched) {
            // what's being said, as far as it's heard (all the grammar has starts with "ANNA")
            if (e.eEventId == SPEI_HYPOTHESIS && !woke) woke = true, heard(kVoiceWake);
            if (e.eEventId == SPEI_FALSE_RECOGNITION) {
                logf("voice: heard something, but not a command it knows");
                if (woke) heard(kVoiceMissed);
                woke = false;
            }
            if (e.eEventId == SPEI_RECOGNITION && e.elParamType == SPET_LPARAM_IS_OBJECT && e.lParam) {
                auto* result   = reinterpret_cast<ISpRecoResult*>(e.lParam);
                SPPHRASE* what = nullptr;
                bool done      = false;
                if (SUCCEEDED(result->GetPhrase(&what)) && what) {
                    const bool sure  = what->Rule.Confidence >= SP_NORMAL_CONFIDENCE;
                    const long at    = what->pProperties ? what->pProperties->vValue.lVal : -2;   // -1: "ANNA" alone
                    const bool known = at >= 0 && at < static_cast<long>(keys.size());
                    if (at == -1) heard(kVoiceOpen), done = true;
                    else if (sure && known) heard(keys[at].c_str()), done = true;
                    else if (known) logf("voice: heard %s, not sure enough (said clearly?)", keys[at].c_str());
                    CoTaskMemFree(what);
                }
                if (!done && woke) heard(kVoiceMissed);
                woke = false;
            }
            clear_event(e);
        }
    }
};

void listen() {
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        logf("voice: COM didn't start: no voice commands");
        return;
    }
    Listener l;
    unsigned seen      = ~0u;
    std::string spoken = "";   // the game's language the recogniser was chosen for
    bool working       = false;
    while (g_running.load()) {
        std::vector<VoiceCommand> commands;
        std::string language;
        unsigned version;
        {
            std::scoped_lock lk{g_mu};
            version  = g_version;
            language = g_language;
            if (version != seen) commands = g_commands;
        }
        if (language != spoken) {
            spoken  = language;
            working = l.start(language);
            seen    = ~0u;   // a new recogniser: the commands again
            std::scoped_lock lk{g_mu};
            commands = g_commands;
        }
        if (working && version != seen) l.set_commands(commands), seen = version;
        if (working) l.apply_awake();
        const HANDLE notify = working && l.context ? l.context->GetNotifyEventHandle() : nullptr;
        if (notify) {
            if (MsgWaitForMultipleObjects(1, &notify, FALSE, 200, QS_ALLINPUT) == WAIT_OBJECT_0) l.drain();
        } else {
            Sleep(200);
        }
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) TranslateMessage(&m), DispatchMessageW(&m);
    }
    l.stop();
    CoUninitialize();
    logf("voice: stopped listening");
}

} // namespace

void voice_enable(bool on) {
    std::scoped_lock lk{g_thread_mu};
    if (on == g_running.load()) return;
    if (on) {
        g_running = true;
        g_thread  = std::thread(listen);
    } else {
        g_running = false;
        if (g_thread.joinable()) g_thread.join();
        std::scoped_lock lk2{g_mu};
        g_heard.clear();
    }
}

void voice_set_commands(std::vector<VoiceCommand> commands, const char* language) {
    std::scoped_lock lk{g_mu};
    g_commands = std::move(commands);
    g_language = language ? language : "EN";
    ++g_version;
}

void voice_set_awake(bool awake) { g_awake = awake; }

bool voice_take(std::string& key) {
    std::scoped_lock lk{g_mu};
    if (g_heard.empty()) return false;
    key = std::move(g_heard.front());
    g_heard.pop_front();
    return true;
}

} // namespace fh2anna
