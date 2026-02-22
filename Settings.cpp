module;

#include <windows.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
//#include <iostream>
#include <algorithm>
#include <cctype>
#include <regex>

export module Settings;

const std::string SETTINGS_NOT_FOUND    = "Settings file not found or invalid settings format, using defaults...\n";
const std::string DEFAULT_SETTINGS      = "heading_color: #FFFFFF; total_timer_idle_color: #006400; total_timer_active_color: #39FF14; segment_timer_idle_color: #4169E1; segment_timer_active_color: #00BFFF; splits_maps_color: #FFFFFF; splits_times_color: #FFFFFF; total_color: #FFD700; total_time_color: #FFD700;category: Default Settings;segment_time: ON;show_splits: OFF;splits_total: OFF;two_decimal_points: OFF;timer_start_split: F9;timer_reset: F8;timer_skip: F10;timer_undo: F11;splits_table: [];";

// NEVER WRITE THIS DIRECTLY DUE TO UB, SINCE MULTIPLE THREADS WILL ACCESS THIS
export struct Settings_s {

    bool    segment_time        = true;
    bool    show_splits         = false;
    bool    splits_total        = false;
    bool    two_decimal_points  = false;

    WORD    timer_start_split   = VK_F9;
    WORD    timer_reset         = VK_F8;
    WORD    timer_skip          = VK_F10;
    WORD    timer_undo          = VK_F11;

    std::string category = "";

    std::string heading_color = "";

    std::string total_timer_idle_color      = "";
    std::string total_timer_active_color    = "";

    std::string segment_timer_idle_color    = "";
    std::string segment_timer_active_color  = "";

    std::string splits_maps_color   = "";
    std::string splits_times_color  = "";

    std::string total_color         = "";
    std::string total_time_color    = "";

    std::vector<std::pair<std::string, std::string>> splits = {{"", ""}};

} settings;

const std::unordered_map<std::string, DWORD> KEY_MAP = {

        // Letters
        {"A", 'A'}, {"B", 'B'}, {"C", 'C'}, {"D", 'D'}, {"E", 'E'},
        {"F", 'F'}, {"G", 'G'}, {"H", 'H'}, {"I", 'I'}, {"J", 'J'},
        {"K", 'K'}, {"L", 'L'}, {"M", 'M'}, {"N", 'N'}, {"O", 'O'},
        {"P", 'P'}, {"Q", 'Q'}, {"R", 'R'}, {"S", 'S'}, {"T", 'T'},
        {"U", 'U'}, {"V", 'V'}, {"W", 'W'}, {"X", 'X'}, {"Y", 'Y'},
        {"Z", 'Z'},

        // Numbers
        {"0", '0'}, {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'},
        {"5", '5'}, {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'},

        // Function keys
        {"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4},
        {"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8},
        {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},

        // Modifiers
        {"SHIFT", VK_SHIFT}, {"CTRL", VK_CONTROL}, {"ALT", VK_MENU},
        {"CAPSLOCK", VK_CAPITAL}, {"TAB", VK_TAB}, {"SPACE", VK_SPACE},

        // Navigation
        {"UP", VK_UP}, {"DOWN", VK_DOWN}, {"LEFT", VK_LEFT}, {"RIGHT", VK_RIGHT},
        {"HOME", VK_HOME}, {"END", VK_END}, {"PGUP", VK_PRIOR}, {"PGDN", VK_NEXT},
        {"INSERT", VK_INSERT}, {"DELETE", VK_DELETE},

        // Symbols (main keyboard)
        {"-", VK_OEM_MINUS}, {"EQUALS", VK_OEM_PLUS}, {"=", VK_OEM_PLUS},
        {"[", VK_OEM_4}, {"]", VK_OEM_6},
        {"\\", VK_OEM_5}, {";", VK_OEM_1}, {"'", VK_OEM_7},
        {",", VK_OEM_COMMA}, {".", VK_OEM_PERIOD}, {"/", VK_OEM_2},
        {"`", VK_OEM_3},

        // Numpad
        {"NUM0", VK_NUMPAD0}, {"NUM1", VK_NUMPAD1}, {"NUM2", VK_NUMPAD2},
        {"NUM3", VK_NUMPAD3}, {"NUM4", VK_NUMPAD4}, {"NUM5", VK_NUMPAD5},
        {"NUM6", VK_NUMPAD6}, {"NUM7", VK_NUMPAD7}, {"NUM8", VK_NUMPAD8},
        {"NUM9", VK_NUMPAD9},
        {"NUMPLUS", VK_ADD}, {"+", VK_ADD},
        {"NUMMINUS", VK_SUBTRACT},
        {"NUMDEL", VK_DELETE},
        {"NUMENTER", VK_RETURN},

        // Special
        {"ESC", VK_ESCAPE}, {"BACKSPACE", VK_BACK}, {"ENTER", VK_RETURN},
        {"PRINTSCREEN", VK_SNAPSHOT}, {"PAUSE", VK_PAUSE}, {"MENU", VK_APPS},

        // Mouse placeholders
        {"MOUSE1", 1}, {"MOUSE2", 2}, {"MOUSE3", 3},
        {"MOUSE4", 4}, {"MOUSE5", 5},

};


std::string trim(std::string s) {

    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [](unsigned char ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
        [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    return s;

}

bool isValidHexColor(const std::string& color) {

    static const std::regex pattern("^#[0-9A-Fa-f]{6}$");
    return std::regex_match(color, pattern);

}


export std::string loadSettings() {

    std::ifstream file("Settings.txt");

    if (!file) {

        // std::cerr << SETTINGS_NOT_FOUND;
        return DEFAULT_SETTINGS;
    }


    std::ostringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

export void setupSettings(std::string settingsStr) {

    bool validSettings = true;
    std::string loadedSettings = trim(settingsStr);

    std::string splitsBlock;

    size_t tablePos = loadedSettings.find("splits_table");
    if (tablePos != std::string::npos) {

        size_t openBracket  = loadedSettings.find('[', tablePos);
        size_t closeBracket = loadedSettings.find(']', openBracket);

        if (openBracket != std::string::npos &&
            closeBracket != std::string::npos) {

            splitsBlock = loadedSettings.substr(
                openBracket + 1,
                closeBracket - openBracket - 1
            );

            // remove entire splits_table expression from main string
            loadedSettings.erase(tablePos,
                closeBracket - tablePos + 2); // +2 for ];
            } else validSettings = false;

    } else validSettings = false;

    std::stringstream ss(splitsBlock);
    std::string row;

    while (std::getline(ss, row, ',')) {

        row = trim(row);
        if (row.empty())
            continue;

        std::stringstream rowStream(row);

        std::string name;
        std::string time;

        if (std::getline(rowStream, name, '=') &&
            std::getline(rowStream, time)) {

            settings.splits.emplace_back(
                trim(name),
                trim(time)
            );
        } else validSettings = false;

    }

    std::stringstream mainSS(loadedSettings);
    std::string token;

    while (std::getline(mainSS, token, ';')) {

        token = trim(token);
        if (token.empty())
            continue;

        std::stringstream tokenStream(token);
        std::string key, value;

        if (std::getline(tokenStream, key, ':') &&
            std::getline(tokenStream, value)) {

            key   = trim(key);
            value = trim(value);

            // Assign to Settings_s fields
            if (key == "segment_time") {

                settings.segment_time = (value == "ON");

            } else if (key == "show_splits") {

                settings.show_splits = (value == "ON");

            } else if (key == "splits_total") {

                settings.splits_total = (value == "ON");

            } else if (key == "two_decimal_points") {

                settings.two_decimal_points = (value == "ON");

            } else if (key == "timer_start_split") {

                if (KEY_MAP.find(value) != KEY_MAP.end()) settings.timer_start_split = KEY_MAP.at(value);
                else validSettings = false;

            } else if (key == "timer_reset") {

                if (KEY_MAP.find(value) != KEY_MAP.end()) settings.timer_reset = KEY_MAP.at(value);
                else validSettings = false;

            } else if (key == "timer_skip") {

                if (KEY_MAP.find(value) != KEY_MAP.end()) settings.timer_skip = KEY_MAP.at(value);
                else validSettings = false;

            } else if (key == "timer_undo") {

                if (KEY_MAP.find(value) != KEY_MAP.end()) settings.timer_undo = KEY_MAP.at(value);
                else validSettings = false;

            } else if (key == "category") {

                settings.category = value;

            } else if (key == "heading_color") {

                if (isValidHexColor(value)) settings.heading_color = value;
                else validSettings = false;

            } else if (key == "total_timer_idle_color") {

                if (isValidHexColor(value)) settings.total_timer_idle_color = value;
                else validSettings = false;

            } else if (key == "total_timer_active_color") {

                if (isValidHexColor(value)) settings.total_timer_active_color = value;
                else validSettings = false;

            } else if (key == "segment_timer_idle_color") {

                if (isValidHexColor(value)) settings.segment_timer_idle_color = value;
                else validSettings = false;

            } else if (key == "segment_timer_active_color") {

                if (isValidHexColor(value)) settings.segment_timer_active_color = value;
                else validSettings = false;

            } else if (key == "splits_maps_color") {

                if (isValidHexColor(value)) settings.splits_maps_color = value;
                else validSettings = false;

            } else if (key == "splits_times_color") {

                if (isValidHexColor(value)) settings.splits_times_color = value;
                else validSettings = false;

            } else if (key == "total_color") {

                if (isValidHexColor(value)) settings.total_color = value;
                else validSettings = false;

            } else if (key == "total_time_color") {

                if (isValidHexColor(value)) settings.total_time_color = value;
                else validSettings = false;

            } else validSettings = false;

        } else validSettings = false;

    }

    if (!validSettings) {

        //std::cerr << SETTINGS_NOT_FOUND;
        setupSettings(DEFAULT_SETTINGS);
        return;

    }

}




/*

segment_time: ON;
show_splits: ON;
splits_total: OFF;

timer_start_split: F9;
timer_reset: F8;
timer_skip: F10;
timer_undo: F11;


splits_table: [

cordon 	 = 	1:36.5,
landfill =	1:02.7,
bar	     =	54.1,
military =	59.8,
radar	 =	1:18.9,
pripyat  = 	1:24.2,
cnpp	 =	1:14.9,
sarc	 =	20.3

];

Pressing the start/split key, prints the time to the table the moment
that key got pressed

Pressing the Skip key, erades any time, including the preset time from the
table settings file.

Undo reverses both the jump via split and skip back to the time preset in the
table settings file.

Reset resets the timer back to zero

*/