#include "Config.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

    #define MOST_LINE                        512
    #define MOST_KPH                         1000
    #define DEFAULT_DEF_PLAY                 "IG"
    #define TOML_EXTENSION                   ".toml"

    const BYTE kByteOrderMark[] = { 0xEF, 0xBB, 0xBF };

    char*           Trim(char* text);
    void            StripComment(char* line);
    bool            ReadQuoted(const char* text, char* out, size_t size);
    bool            ReadSwitch(const char* value, bool* out);
    void            ReadSpeed(const char* value, float* out);
    void            CopyText(char* out, size_t size, const char* text);
    Config::Section SectionOf(const char* header);
    bool            SplitKey(char* text, char* key, size_t size, char** value);

    void ApplyRoot(const char* key, const char* value) {
        Config::Settings& s = Config::gSettings;
        if (strcmp(key, "Unused Theme 1") == 0)               ReadSwitch(value, &s.UnusedTheme[0]);
        else if (strcmp(key, "Unused Theme 2") == 0)          ReadSwitch(value, &s.UnusedTheme[1]);
        else if (strcmp(key, "Unused Theme 3") == 0)          ReadSwitch(value, &s.UnusedTheme[2]);
        else if (strcmp(key, "Unused Cooldown Theme") == 0)   ReadSwitch(value, &s.UnusedCooldownTheme);
        else if (strcmp(key, "Unused Busted Theme") == 0)     ReadSwitch(value, &s.UnusedBustedTheme);
        else if (strcmp(key, "Song Ending Transitions") == 0) ReadSwitch(value, &s.SongEndingTransitions);
    }

    void ApplySpeech(Config::Section section, const char* key, const char* value) {
        Config::Settings& s = Config::gSettings;
        if (section == Config::SECTION_INTERRUPT_RAM && strcmp(key, "MinPlayerSpeed") == 0)           ReadSpeed(value, &s.InterruptRamMinPlayerSpeed);
        else if (section == Config::SECTION_INTERRUPT_RAM_HIGH && strcmp(key, "MinPlayerSpeed") == 0) ReadSpeed(value, &s.InterruptRamHighMinPlayerSpeed);
        else if (section == Config::SECTION_CALL_FOR_EV && strcmp(key, "UnitsDown") == 0)             ReadSwitch(value, &s.CallForEVUnitsDown);
        else if (section == Config::SECTION_CALL_FOR_EV && strcmp(key, "HeliDown") == 0)              ReadSwitch(value, &s.CallForEVHeliDown);
    }

    void ApplySong(Config::Song* song, const char* key, const char* value) {
        if (strcmp(key, "PathEvent") == 0) {
            char* end = NULL;
            unsigned long number = strtoul(value, &end, 0);
            if (end != value && *end == '\0') song->PathEvent = (DWORD)number;
            return;
        }
        char text[SONG_TEXT] = "";
        if (!ReadQuoted(value, text, sizeof(text))) return;
        if (strcmp(key, "Album") == 0)         CopyText(song->Album, sizeof(song->Album), text);
        else if (strcmp(key, "Artist") == 0)   CopyText(song->Artist, sizeof(song->Artist), text);
        else if (strcmp(key, "SongName") == 0) CopyText(song->SongName, sizeof(song->SongName), text);
        else if (strcmp(key, "DefPlay") == 0)  CopyText(song->DefPlay, sizeof(song->DefPlay), text);
    }

    Config::Song* AddSong() {
        Config::Settings& s = Config::gSettings;
        if (s.SongCount >= MOST_SONGS) return NULL;
        Config::Song* song = &s.Songs[s.SongCount++];
        memset(song, 0, sizeof(*song));
        CopyText(song->DefPlay, sizeof(song->DefPlay), DEFAULT_DEF_PLAY);
        return song;
    }

    void DropIncompleteSongs() {
        Config::Settings& s = Config::gSettings;
        int kept = 0;
        for (int i = 0; i < s.SongCount; ++i) {
            if (s.Songs[i].PathEvent == 0 || s.Songs[i].SongName[0] == '\0') continue;
            if (kept != i) s.Songs[kept] = s.Songs[i];
            ++kept;
        }
        s.SongCount = kept;
    }

}

namespace Config {

    Settings gSettings;

    Settings::Settings()
        : UnusedCooldownTheme(true),
          UnusedBustedTheme(true),
          SongEndingTransitions(true),
          InterruptRamMinPlayerSpeed(48.0f),
          InterruptRamHighMinPlayerSpeed(48.0f),
          CallForEVUnitsDown(true),
          CallForEVHeliDown(true),
          SongCount(0) {
        for (int i = 0; i < RESTORED_THEMES; ++i) UnusedTheme[i] = true;
        memset(Songs, 0, sizeof(Songs));
    }

    void Load(HMODULE module) {
        char path[MAX_PATH] = "";
        DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
        char* dot = (length > 0 && length < MAX_PATH) ? strrchr(path, '.') : NULL;
        if (dot == NULL || dot < strrchr(path, '\\') || (size_t)(dot - path) + sizeof(TOML_EXTENSION) > MAX_PATH) return;
        strcpy(dot, TOML_EXTENSION);

        FILE* file = fopen(path, "r");
        if (file == NULL) return;
        char line[MOST_LINE];
        Section section = SECTION_ROOT;
        Song* song = NULL;
        while (fgets(line, sizeof(line), file) != NULL) {
            char* text = line;
            if (memcmp(text, kByteOrderMark, sizeof(kByteOrderMark)) == 0) text += sizeof(kByteOrderMark);
            StripComment(text);
            text = Trim(text);
            if (*text == '\0') continue;
            if (*text == '[') {
                section = SectionOf(text);
                song = (section == SECTION_SONG) ? AddSong() : NULL;
                continue;
            }
            char key[SONG_TEXT] = "";
            char* value = NULL;
            if (!SplitKey(text, key, sizeof(key), &value)) continue;
            if (section == SECTION_ROOT)       ApplyRoot(key, value);
            else if (section == SECTION_SONG)  { if (song != NULL) ApplySong(song, key, value); }
            else                               ApplySpeech(section, key, value);
        }
        fclose(file);
        DropIncompleteSongs();
    }

}

namespace {

    char* Trim(char* text) {
        while (*text == ' ' || *text == '\t') ++text;
        size_t length = strlen(text);
        while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t' || text[length - 1] == '\r' || text[length - 1] == '\n')) text[--length] = '\0';
        return text;
    }

    void StripComment(char* line) {
        bool quoted = false;
        for (char* at = line; *at != '\0'; ++at) {
            if (*at == '\\' && quoted && at[1] != '\0') {
                ++at;
                continue;
            }
            if (*at == '"') quoted = !quoted;
            if (*at == '#' && !quoted) {
                *at = '\0';
                return;
            }
        }
    }

    bool ReadQuoted(const char* text, char* out, size_t size) {
        if (*text != '"' || size == 0) return false;
        size_t used = 0;
        for (const char* at = text + 1; *at != '\0'; ++at) {
            char c = *at;
            if (c == '"') {
                out[used] = '\0';
                return true;
            }
            if (c == '\\' && at[1] != '\0') c = *++at;
            if (used + 1 < size) out[used++] = c;
        }
        return false;
    }

    bool ReadSwitch(const char* value, bool* out) {
        if (_stricmp(value, "true") == 0) *out = true;
        else if (_stricmp(value, "false") == 0) *out = false;
        else return false;
        return true;
    }

    void ReadSpeed(const char* value, float* out) {
        char* end = NULL;
        double number = strtod(value, &end);
        if (end != value && number >= 0 && number <= MOST_KPH) *out = (float)number;
    }

    void CopyText(char* out, size_t size, const char* text) {
        strncpy(out, text, size - 1);
        out[size - 1] = '\0';
    }

    Config::Section SectionOf(const char* header) {
        if (strcmp(header, "[[Song]]") == 0)         return Config::SECTION_SONG;
        if (strcmp(header, "[InterruptRam]") == 0)     return Config::SECTION_INTERRUPT_RAM;
        if (strcmp(header, "[InterruptRamHigh]") == 0) return Config::SECTION_INTERRUPT_RAM_HIGH;
        if (strcmp(header, "[CallForEV]") == 0)        return Config::SECTION_CALL_FOR_EV;
        return Config::SECTION_OTHER;
    }

    bool SplitKey(char* text, char* key, size_t size, char** value) {
        if (*text == '"') {
            if (!ReadQuoted(text, key, size)) return false;
            char* close = strchr(text + 1, '"');
            char* equals = (close != NULL) ? strchr(close + 1, '=') : NULL;
            if (equals == NULL) return false;
            *value = Trim(equals + 1);
            return true;
        }
        char* equals = strchr(text, '=');
        if (equals == NULL) return false;
        *equals = '\0';
        CopyText(key, size, Trim(text));
        *value = Trim(equals + 1);
        return true;
    }

}
