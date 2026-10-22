// InputSystem_7c unit U3: ASCIICode (the character of each key) and InputStrings (the names of the key, button,
// axis and pov codes). Both are singletons built on first use. The tables are the ones the original constructors
// write, store by store (ASCIICode 0x1000caa0, InputStrings 0x1000d0e0); the lookups 0x100013f0..0x10001713.
#include <jet/InputSystem.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

// what the original does for `pstring = "text"`: drop the old node, intern the new text
void SetText(PString& s, const char* text)
{
    s.~PString();
    new (&s) PString(text);
}

struct AsciiInit {
    unsigned char key;
    unsigned short normal, shifted;
    unsigned char kind;
};
const AsciiInit kAscii[] = {
    {0x01, 0x1b, 0x00, 0}, {0x03, 0x31, 0x21, 2}, {0x04, 0x32, 0x40, 2}, {0x05, 0x33, 0x23, 2},
    {0x06, 0x34, 0x24, 2}, {0x07, 0x35, 0x25, 2}, {0x08, 0x36, 0x5e, 2}, {0x09, 0x37, 0x26, 2},
    {0x0a, 0x38, 0x2a, 2}, {0x0b, 0x39, 0x28, 2}, {0x0c, 0x30, 0x29, 2}, {0x0d, 0x2d, 0x5f, 2},
    {0x0e, 0x3d, 0x2b, 2}, {0x0f, 0x08, 0x00, 0}, {0x10, 0x09, 0x00, 0}, {0x11, 0x71, 0x51, 1},
    {0x12, 0x77, 0x57, 1}, {0x13, 0x65, 0x45, 1}, {0x14, 0x72, 0x52, 1}, {0x15, 0x74, 0x54, 1},
    {0x16, 0x79, 0x59, 1}, {0x17, 0x75, 0x55, 1}, {0x18, 0x69, 0x49, 1}, {0x19, 0x6f, 0x4f, 1},
    {0x1a, 0x70, 0x50, 1}, {0x1b, 0x5b, 0x7b, 2}, {0x1c, 0x5d, 0x7d, 2}, {0x1d, 0x0d, 0x00, 0},
    {0x1f, 0x61, 0x41, 1}, {0x20, 0x73, 0x53, 1}, {0x21, 0x64, 0x44, 1}, {0x22, 0x66, 0x46, 1},
    {0x23, 0x67, 0x47, 1}, {0x24, 0x68, 0x48, 1}, {0x25, 0x6a, 0x4a, 1}, {0x26, 0x6b, 0x4b, 1},
    {0x27, 0x6c, 0x4c, 1}, {0x28, 0x3b, 0x3a, 2}, {0x29, 0x27, 0x22, 2}, {0x2a, 0x60, 0x7e, 2},
    {0x2c, 0x5c, 0x7c, 2}, {0x2d, 0x7a, 0x5a, 1}, {0x2e, 0x78, 0x58, 1}, {0x2f, 0x63, 0x43, 1},
    {0x30, 0x76, 0x56, 1}, {0x31, 0x62, 0x42, 1}, {0x32, 0x6e, 0x4e, 1}, {0x33, 0x6d, 0x4d, 1},
    {0x34, 0x2c, 0x3c, 2}, {0x35, 0x2e, 0x3e, 2}, {0x36, 0x2f, 0x3f, 2}, {0x38, 0x2a, 0x00, 3},
    {0x3a, 0x20, 0x20, 1}, {0x48, 0x37, 0x00, 4}, {0x49, 0x38, 0x00, 4}, {0x4a, 0x39, 0x00, 4},
    {0x4b, 0x2d, 0x00, 3}, {0x4c, 0x34, 0x00, 4}, {0x4d, 0x35, 0x00, 4}, {0x4e, 0x36, 0x00, 4},
    {0x4f, 0x2b, 0x00, 3}, {0x50, 0x31, 0x00, 4}, {0x51, 0x32, 0x00, 4}, {0x52, 0x33, 0x00, 4},
    {0x53, 0x30, 0x00, 4}, {0x54, 0x2e, 0x00, 4}, {0x5a, 0x0d, 0x00, 0}, {0x5b, 0x2f, 0x00, 3},
    {0x67, 0x08, 0x00, 0},
};

struct NameInit {
    unsigned short slot;
    const char* text;
};
// in the original's order (BC_32 twice)
const NameInit kNames[] = {
    {1, "KC_Esc"}, {2, "KC_Pause"}, {3, "KC_1"}, {4, "KC_2"}, {5, "KC_3"}, {6, "KC_4"}, {7, "KC_5"}, {8, "KC_6"},
    {9, "KC_7"}, {10, "KC_8"}, {11, "KC_9"}, {12, "KC_0"}, {13, "KC_Minus"}, {14, "KC_Equals"}, {15, "KC_Back"},
    {16, "KC_Tab"}, {17, "KC_Q"}, {18, "KC_W"}, {19, "KC_E"}, {20, "KC_R"}, {21, "KC_T"}, {22, "KC_Y"}, {23, "KC_U"},
    {24, "KC_I"}, {25, "KC_O"}, {26, "KC_P"}, {27, "KC_OpenBracket"}, {28, "KC_CloseBracket"}, {29, "KC_Enter"},
    {30, "KC_LeftCtrl"}, {31, "KC_A"}, {32, "KC_S"}, {33, "KC_D"}, {34, "KC_F"}, {35, "KC_G"}, {36, "KC_H"},
    {37, "KC_J"}, {38, "KC_K"}, {39, "KC_L"}, {40, "KC_Semicolon"}, {41, "KC_Apostrophe"}, {42, "KC_Tilde"},
    {43, "KC_LeftShift"}, {44, "KC_BackSlash"}, {45, "KC_Z"}, {46, "KC_X"}, {47, "KC_C"}, {48, "KC_V"}, {49, "KC_B"},
    {50, "KC_N"}, {51, "KC_M"}, {52, "KC_Comma"}, {53, "KC_Dot"}, {54, "KC_Slash"}, {55, "KC_RightShift"},
    {56, "KC_KeypadStar"}, {57, "KC_LeftAlt"}, {58, "KC_Space"}, {59, "KC_CapsLock"}, {60, "KC_F1"}, {61, "KC_F2"},
    {62, "KC_F3"}, {63, "KC_F4"}, {64, "KC_F5"}, {65, "KC_F6"}, {66, "KC_F7"}, {67, "KC_F8"}, {68, "KC_F9"},
    {69, "KC_F10"}, {70, "KC_NumLock"}, {71, "KC_ScrollLock"}, {72, "KC_Keypad7"}, {73, "KC_Keypad8"},
    {74, "KC_Keypad9"}, {75, "KC_KeypadMinus"}, {76, "KC_Keypad4"}, {77, "KC_Keypad5"}, {78, "KC_Keypad6"},
    {79, "KC_KeypadPlus"}, {80, "KC_Keypad1"}, {81, "KC_Keypad2"}, {82, "KC_Keypad3"}, {83, "KC_Keypad0"},
    {84, "KC_KeypadDot"}, {85, "KC_F11"}, {86, "KC_F12"}, {87, "KC_F13"}, {88, "KC_F14"}, {89, "KC_F15"},
    {90, "KC_KeypadEnter"}, {91, "KC_KeypadSlash"}, {92, "KC_RightCtrl"}, {93, "KC_RightAlt"}, {94, "KC_Home"},
    {95, "KC_Up"}, {96, "KC_PgUp"}, {97, "KC_Left"}, {98, "KC_Right"}, {99, "KC_End"}, {100, "KC_Down"},
    {101, "KC_PgDn"}, {102, "KC_Ins"}, {103, "KC_Del"}, {104, "KC_LeftWin"}, {105, "KC_RightWin"},
    {106, "KC_AppMenu"}, {107, "KC_SysRq"}, {256, "BC_1"}, {257, "BC_2"}, {258, "BC_3"}, {259, "BC_4"},
    {260, "BC_5"}, {261, "BC_6"}, {262, "BC_7"}, {263, "BC_8"}, {264, "BC_9"}, {265, "BC_10"}, {266, "BC_11"},
    {267, "BC_12"}, {268, "BC_13"}, {269, "BC_14"}, {270, "BC_15"}, {271, "BC_16"}, {272, "BC_17"}, {273, "BC_18"},
    {274, "BC_19"}, {275, "BC_20"}, {276, "BC_21"}, {277, "BC_22"}, {278, "BC_23"}, {279, "BC_24"}, {280, "BC_25"},
    {281, "BC_26"}, {282, "BC_27"}, {283, "BC_28"}, {284, "BC_29"}, {285, "BC_30"}, {286, "BC_31"}, {287, "BC_32"},
    {287, "BC_32"}, {288, "BC_Left"}, {289, "BC_Right"}, {290, "BC_Middle"}, {291, "BC_X1"}, {292, "BC_X2"},
    {293, "AC_X"}, {294, "AC_Y"}, {295, "AC_Z"}, {296, "AC_RX"}, {297, "AC_RY"}, {298, "AC_RZ"}, {299, "AC_U"},
    {300, "AC_V"}, {301, "PC_1"}, {302, "PC_2"}, {303, "PC_3"}, {304, "PC_4"}, {305, "N"}, {306, "NE"}, {307, "E"},
    {308, "SE"}, {309, "S"}, {310, "SW"}, {311, "W"}, {312, "NW"}, {313, "C"},
};

}  // namespace

// ---- ASCIICode
ASCIICode::ASCIICode()
{
    std::memset(entries, 0, sizeof(entries));
    for (const AsciiInit& a : kAscii) {
        entries[a.key].normal = a.normal;
        entries[a.key].shifted = a.shifted;
        entries[a.key].kind = a.kind;
    }
}

const ASCIICode& ASCIICode::Get()
{
    static ASCIICode instance;
    return instance;
}

ASCIICode& ASCIICode::operator=(const ASCIICode& o)
{
    std::memcpy(entries, o.entries, sizeof(entries));
    return *this;
}

unsigned short ASCIICode::GetChar(KeyCode key, bool capsLock, bool shift, bool numLock) const
{
    const Entry& e = entries[key];
    switch (e.kind) {
    case CONTROL:
    case KEYPAD_OPERATOR: return e.normal;
    case LETTER: return capsLock != shift ? e.shifted : e.normal;
    case SYMBOL: return shift ? e.shifted : e.normal;
    case KEYPAD_DIGIT: return numLock ? e.normal : 0;
    default: return 0;
    }
}

bool ASCIICode::IsControl(KeyCode key) const { return entries[key].normal < 0x20; }

// ---- InputStrings
InputStrings::InputStrings()
{
    for (int i = 0; i < 0x100; ++i) SetText(names[KEYS + i], "");
    for (const NameInit& n : kNames) SetText(names[n.slot], n.text);
}

const InputStrings& InputStrings::Get()
{
    static InputStrings instance;
    return instance;
}

InputStrings::InputStrings(const InputStrings& o) = default;
InputStrings::~InputStrings() = default;
InputStrings& InputStrings::operator=(const InputStrings& o) = default;

PString InputStrings::KeyToString(KeyCode key) const { return names[KEYS + (key & 0xff)]; }

// mouse buttons are stored after the 32 joystick buttons
PString InputStrings::ButtonToString(ButtonCode button, bool mouse) const
{
    return names[BUTTONS + (mouse ? (button + 0x20) % 0x25 : button & 0x1f)];
}

PString InputStrings::AxisToString(AxisCode axis) const { return names[AXES + (axis & 7)]; }
PString InputStrings::PovToString(PovCode pov) const { return names[POVS + (pov & 3)]; }
PString InputStrings::PovDirToString(PovDir dir) const { return names[POV_DIRS + (dir & 7)]; }

// the reverse lookups compare the interned nodes; not found: 0 for keys and pov directions, -1 for the others
KeyCode InputStrings::StringToKey(const PString& s) const
{
    for (int i = 0; i < 0x100; ++i)
        if (names[KEYS + i].node == s.node) return static_cast<KeyCode>(i);
    return static_cast<KeyCode>(0);
}

ButtonCode InputStrings::StringToButton(const PString& s) const
{
    for (int i = 0; i < 0x25; ++i)
        if (names[BUTTONS + i].node == s.node) return static_cast<ButtonCode>(i >= 0x20 ? i - 0x20 : i);
    return static_cast<ButtonCode>(-1);
}

AxisCode InputStrings::StringToAxis(const PString& s) const
{
    for (int i = 0; i < 8; ++i)
        if (names[AXES + i].node == s.node) return static_cast<AxisCode>(i);
    return static_cast<AxisCode>(-1);
}

PovCode InputStrings::StringToPov(const PString& s) const
{
    for (int i = 0; i < 4; ++i)
        if (names[POVS + i].node == s.node) return static_cast<PovCode>(i);
    return static_cast<PovCode>(-1);
}

PovDir InputStrings::StringToPovDir(const PString& s) const
{
    for (int i = 0; i < 9; ++i)
        if (names[POV_DIRS + i].node == s.node) return static_cast<PovDir>(i);
    return static_cast<PovDir>(0);
}

}  // namespace Jet
