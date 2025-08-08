#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TYPE_LENGTH 16
#define typesCount (sizeof(NodeInfoByType) / sizeof(NodeInfoByType[0]))
#define pinTypesCount (sizeof(PinDropdownOptionsByType) / sizeof(PinDropdownOptionsByType[0])) // not named properly

typedef enum
{
    NODE_UNKNOWN = 0,
    NODE_NUM,
    NODE_STRING,
    NODE_BOOL,
    NODE_COLOR,
    NODE_SPRITE,
    NODE_GET_VAR,
    NODE_SET_VAR,
    NODE_EVENT_START,
    NODE_EVENT_LOOP_TICK,
    NODE_EVENT_ON_BUTTON,
    NODE_CREATE_CUSTOM_EVENT,
    NODE_CALL_CUSTOM_EVENT,
    NODE_SPAWN_SPRITE,
    NODE_DESTROY_SPRITE,
    NODE_MOVE_TO_SPRITE,
    NODE_FORCE_SPRITE,
    NODE_BRANCH,
    NODE_LOOP,
    NODE_COMPARISON,
    NODE_GATE,
    NODE_ARITHMETIC,
    NODE_PROP_TEXTURE,
    NODE_PROP_RECTANGLE,
    NODE_PROP_CIRCLE,
    NODE_PRINT,
    NODE_DRAW_LINE,
    NODE_LITERAL_NUM,
    NODE_LITERAL_STRING,
    NODE_LITERAL_BOOL,
    NODE_LITERAL_COLOR
} NodeType;

typedef enum
{
    PIN_NONE,
    PIN_FLOW,
    PIN_NUM,
    PIN_STRING,
    PIN_BOOL,
    PIN_COLOR,
    PIN_SPRITE,
    PIN_FIELD_NUM,
    PIN_FIELD_STRING,
    PIN_FIELD_BOOL,
    PIN_FIELD_COLOR,
    PIN_FIELD_KEY,
    PIN_DROPDOWN_COMPARISON_OPERATOR,
    PIN_DROPDOWN_GATE,
    PIN_DROPDOWN_ARITHMETIC,
    PIN_DROPDOWN_KEY_ACTION,
    PIN_VARIABLE,
    PIN_SPRITE_VARIABLE,
    PIN_ANY_VALUE,
    PIN_UNKNOWN_VALUE
} PinType;

typedef enum
{
    EQUAL_TO,
    GREATER_THAN,
    LESS_THAN
} Comparison;

typedef enum
{
    AND,
    OR,
    NOT,
    XOR,
    NAND,
    NOR
} Gate;

typedef enum
{
    ADD,
    SUBTRACT,
    MULTIPLY,
    DIVIDE,
    MODULO
} Arithmetic;

typedef enum
{
    KEY_ACTION_PRESSED,
    KEY_ACTION_RELEASED,
    KEY_ACTION_DOWN,
    KEY_ACTION_NOT_DOWN
} KeyAction;

typedef struct InfoByType
{
    NodeType type;

    int inputCount;
    int outputCount;

    int width;
    int height;

    Color color;

    bool isEditable;

    PinType inputs[16];
    PinType outputs[16];

    char *inputNames[16];
    char *outputNames[16];
} InfoByType;

static InfoByType NodeInfoByType[] = {
    {NODE_UNKNOWN, 0, 0, 50, 50},
    {NODE_NUM, 2, 2, 120, 100, {100, 60, 120, 200}, true, {PIN_FLOW, PIN_NUM}, {PIN_FLOW, PIN_NUM}, {"Prev", "Set value"}, {"Next", "Get value"}},
    {NODE_STRING, 2, 2, 120, 100, {100, 60, 120, 200}, true, {PIN_FLOW, PIN_STRING}, {PIN_FLOW, PIN_STRING}, {"Prev", "Set value"}, {"Next", "Get value"}},
    {NODE_BOOL, 2, 2, 120, 100, {100, 60, 120, 200}, true, {PIN_FLOW, PIN_BOOL}, {PIN_FLOW, PIN_BOOL}, {"Prev", "Set value"}, {"Next", "Get value"}},
    {NODE_COLOR, 2, 2, 120, 100, {100, 60, 120, 200}, true, {PIN_FLOW, PIN_COLOR}, {PIN_FLOW, PIN_COLOR}, {"Prev", "Set value"}, {"Next", "Get value"}},
    {NODE_SPRITE, 8, 2, 200, 285, {70, 100, 70, 200}, true, {PIN_FLOW, PIN_STRING, PIN_NUM, PIN_NUM, PIN_NUM, PIN_NUM, PIN_NUM, PIN_NUM}, {PIN_FLOW, PIN_SPRITE}, {"Prev", "Texture file name", "Pos X", "Pos Y", "Width", "Height", "Rotation", "Layer"}, {"Next", "Sprite"}},
    {NODE_GET_VAR, 1, 1, 140, 70, {60, 100, 159, 200}, false, {PIN_VARIABLE}, {PIN_UNKNOWN_VALUE}, {"Variable"}, {"Get value"}},
    {NODE_SET_VAR, 3, 2, 140, 130, {60, 100, 159, 200}, false, {PIN_FLOW, PIN_VARIABLE, PIN_UNKNOWN_VALUE}, {PIN_FLOW, PIN_NONE}, {"Prev", "Variable", "Set value"}, {"Next", ""}}, // shouldn't have PIN_NONE
    {NODE_EVENT_START, 0, 1, 150, 120, {148, 0, 0, 200}, false, {0}, {PIN_FLOW}, {0}, {"Next"}},
    {NODE_EVENT_LOOP_TICK, 0, 1, 150, 120, {148, 0, 0, 200}, false, {0}, {PIN_FLOW}, {0}, {"Next"}},
    {NODE_EVENT_ON_BUTTON, 2, 1, 160, 120, {148, 0, 0, 200}, false, {PIN_FIELD_KEY, PIN_DROPDOWN_KEY_ACTION}, {PIN_FLOW}, {"Key", "Action"}, {"Next"}},
    {NODE_CREATE_CUSTOM_EVENT, 0, 1, 240, 200, {148, 0, 0, 200}, false, {0}, {PIN_FLOW}, {"Prev"}, {"Next"}}, // not implemented
    {NODE_CALL_CUSTOM_EVENT, 0, 1, 240, 200, {148, 0, 0, 200}, false, {PIN_FLOW}, {PIN_FLOW}, {"Prev"}, {"Next"}}, // not implemented
    {NODE_SPAWN_SPRITE, 2, 1, 120, 100, {40, 110, 70, 200}, false, {PIN_FLOW, PIN_SPRITE_VARIABLE}, {PIN_FLOW}, {"Prev", "Sprite"}, {"Next"}},
    {NODE_DESTROY_SPRITE, 2, 1, 120, 100, {40, 110, 70, 200}, false, {PIN_FLOW, PIN_SPRITE_VARIABLE}, {PIN_FLOW}, {"Prev", "Sprite"}, {"Next"}},
    {NODE_MOVE_TO_SPRITE, 3, 3, 240, 200, {40, 110, 70, 200}, false, {PIN_FLOW}, {PIN_FLOW}, {"Prev"}, {"Next"}}, // not implemented
    {NODE_FORCE_SPRITE, 5, 1, 160, 190, {40, 110, 70, 200}, false, {PIN_FLOW, PIN_SPRITE_VARIABLE, PIN_NUM, PIN_NUM, PIN_NUM}, {PIN_FLOW}, {"Prev","Sprite", "Pixels / second", "Angle", "Time"}, {"Next"}},
    {NODE_BRANCH, 2, 2, 130, 100, {90, 90, 90, 200}, false, {PIN_FLOW, PIN_BOOL}, {PIN_FLOW, PIN_FLOW}, {"Prev", "Condition"}, {"True", "False"}},
    {NODE_LOOP, 2, 2, 130, 100, {90, 90, 90, 200}, false, {PIN_FLOW, PIN_BOOL}, {PIN_FLOW, PIN_FLOW}, {"Prev", "Condition"}, {"Next", "Loop body"}},
    {NODE_COMPARISON, 4, 2, 210, 160, {60, 100, 159, 200}, false, {PIN_FLOW, PIN_DROPDOWN_COMPARISON_OPERATOR, PIN_NUM, PIN_NUM}, {PIN_FLOW, PIN_BOOL}, {"Prev", "Operator", "Value A", "Value B"}, {"Next", "Result"}},
    {NODE_GATE, 4, 2, 180, 160, {60, 100, 159, 200}, false, {PIN_FLOW, PIN_DROPDOWN_GATE, PIN_BOOL, PIN_BOOL}, {PIN_FLOW, PIN_BOOL}, {"Prev", "Gate", "Condition A", "Condition B"}, {"Next", "Result"}},
    {NODE_ARITHMETIC, 4, 2, 240, 200, {60, 100, 159, 200}, false, {PIN_FLOW, PIN_DROPDOWN_ARITHMETIC, PIN_NUM, PIN_NUM}, {PIN_FLOW, PIN_NUM}, {"Prev", "Arithmetic", "Number A", "Number B"}, {"Next", "Result"}},
    {NODE_PROP_TEXTURE, 0, 0, 240, 200, {40, 110, 70, 200}, false, {PIN_FLOW}, {PIN_FLOW}, {"Prev"}, {"Next"}}, // not implemented
    {NODE_PROP_RECTANGLE, 7, 2, 230, 250, {40, 110, 70, 200}, false, {PIN_FLOW, PIN_NUM, PIN_NUM, PIN_NUM, PIN_NUM, PIN_COLOR, PIN_NUM}, {PIN_FLOW, PIN_NONE}, {"Prev", "Pos X", "Pos Y", "Width", "Height", "Color", "Layer"}, {"Next"}},
    {NODE_PROP_CIRCLE, 6, 2, 230, 230, {40, 110, 70, 200}, false, {PIN_FLOW, PIN_NUM, PIN_NUM, PIN_NUM, PIN_COLOR, PIN_NUM}, {PIN_FLOW, PIN_NONE}, {"Prev", "Pos X", "Pos Y", "Radius", "Color", "Layer"}, {"Next"}},
    {NODE_PRINT, 2, 1, 140, 100, {200, 170, 50, 200}, false, {PIN_FLOW, PIN_ANY_VALUE}, {PIN_FLOW}, {"Prev", "Print value"}, {"Next"}},
    {NODE_DRAW_LINE, 6, 1, 240, 200, {200, 170, 50, 200}, false, {PIN_FLOW, PIN_NUM, PIN_NUM, PIN_NUM, PIN_NUM, PIN_COLOR}, {PIN_FLOW}, {"Prev", "Start X", "Start Y", "End X", "End Y", "Color"}, {"Next"}}, // not implemented
    {NODE_LITERAL_NUM, 1, 1, 200, 70, {110, 85, 40, 200}, false, {PIN_FIELD_NUM}, {PIN_NUM}, {""}, {"number"}},
    {NODE_LITERAL_STRING, 1, 1, 200, 70, {110, 85, 40, 200}, false, {PIN_FIELD_STRING}, {PIN_STRING}, {""}, {"string"}},
    {NODE_LITERAL_BOOL, 1, 1, 180, 70, {110, 85, 40, 200}, false, {PIN_FIELD_BOOL}, {PIN_BOOL}, {""}, {"bool"}},
    {NODE_LITERAL_COLOR, 1, 1, 200, 70, {110, 85, 40, 200}, false, {PIN_FIELD_COLOR}, {PIN_COLOR}, {""}, {"color"}}};

typedef struct DropdownOptionsByPinType
{
    PinType type;
    int optionsCount;
    char **options;
    int boxWidth;
} DropdownOptionsByPinType;

static char *comparisonOps[] = {"Equal To", "Greater Than", "Less Than"};
static char *gateOps[] = {"AND", "OR", "NOT", "XOR", "NAND", "NOR"};
static char *arithmeticOps[] = {"ADD", "SUBTRACT", "MULTIPLY", "DIVIDE", "MODULO"};
static char *keyActionOps[] = {"Pressed", "Released", "Down", "Not down"};

static DropdownOptionsByPinType PinDropdownOptionsByType[] = {
    {PIN_DROPDOWN_COMPARISON_OPERATOR, 3, comparisonOps, 120},
    {PIN_DROPDOWN_GATE, 6, gateOps, 60},
    {PIN_DROPDOWN_ARITHMETIC, 5, arithmeticOps, 110},
    {PIN_DROPDOWN_KEY_ACTION, 4, keyActionOps, 110}
};

static inline DropdownOptionsByPinType getPinDropdownOptionsByType(PinType type)
{
    for (int i = 0; i < pinTypesCount; i++)
    {
        if (type == PinDropdownOptionsByType[i].type)
        {
            return PinDropdownOptionsByType[i];
        }
    }

    return (DropdownOptionsByPinType){0};
}

typedef enum
{
    INPUT_COUNT,
    OUTPUT_COUNT,
    WIDTH,
    HEIGHT
} RequestedInfo;

static inline int getNodeInfoByType(NodeType type, RequestedInfo info)
{
    if (type < 0 || type >= typesCount)
        return -1;

    switch (info)
    {
    case INPUT_COUNT:
        return NodeInfoByType[type].inputCount;
    case OUTPUT_COUNT:
        return NodeInfoByType[type].outputCount;
    case WIDTH:
        return NodeInfoByType[type].width;
    case HEIGHT:
        return NodeInfoByType[type].height;
    default:
        return -1;
    }

    return -1;
}

static inline bool getIsEditableByType(NodeType type)
{
    for (int i = 0; i < typesCount; i++)
    {
        if (type == NodeInfoByType[i].type)
        {
            return NodeInfoByType[i].isEditable;
        }
    }
}

static inline char **getNodeInputNamesByType(NodeType type)
{
    for (int i = 0; i < typesCount; i++)
    {
        if (type == NodeInfoByType[i].type)
        {
            return NodeInfoByType[i].inputNames;
        }
    }
}

static inline char **getNodeOutputNamesByType(NodeType type)
{
    for (int i = 0; i < typesCount; i++)
    {
        if (type == NodeInfoByType[i].type)
        {
            return NodeInfoByType[i].outputNames;
        }
    }
}

static inline Color getNodeColorByType(NodeType type)
{
    for (int i = 0; i < typesCount; i++)
    {
        if (type == NodeInfoByType[i].type)
        {
            return NodeInfoByType[i].color;
        }
    }
}

static inline PinType *getInputsByType(NodeType type)
{
    if (!NodeInfoByType)
        return NULL;

    for (int i = 0; i < typesCount; i++)
    {
        if (NodeInfoByType[i].type == type)
        {
            return NodeInfoByType[i].inputs ? NodeInfoByType[i].inputs : NULL;
        }
    }
    return NULL;
}

static inline PinType *getOutputsByType(NodeType type)
{
    if (!NodeInfoByType)
        return NULL;

    for (int i = 0; i < typesCount; i++)
    {
        if (NodeInfoByType[i].type == type)
        {
            return NodeInfoByType[i].outputs ? NodeInfoByType[i].outputs : NULL;
        }
    }
    return NULL;
}

static inline const char *NodeTypeToString(NodeType type)
{
    switch (type)
    {
    case NODE_UNKNOWN:
        return "unknown";
    case NODE_NUM:
        return "num";
    case NODE_STRING:
        return "string";
    case NODE_BOOL:
        return "bool";
    case NODE_COLOR:
        return "color";
    case NODE_SPRITE:
        return "sprite";
    case NODE_GET_VAR:
        return "Get var";
    case NODE_SET_VAR:
        return "Set var";
    case NODE_EVENT_START:
        return "Start";
    case NODE_EVENT_LOOP_TICK:
        return "Loop Tick";
    case NODE_EVENT_ON_BUTTON:
        return "On Button";
    case NODE_CREATE_CUSTOM_EVENT:
        return "Create custom";
    case NODE_CALL_CUSTOM_EVENT:
        return "Call custom";
    case NODE_SPAWN_SPRITE:
        return "Spawn";
    case NODE_DESTROY_SPRITE:
        return "Destroy";
    case NODE_MOVE_TO_SPRITE:
        return "Move To";
    case NODE_FORCE_SPRITE:
        return "Force";
    case NODE_BRANCH:
        return "Branch";
    case NODE_LOOP:
        return "Loop";
    case NODE_COMPARISON:
        return "Comparison";
    case NODE_GATE:
        return "Gate";
    case NODE_ARITHMETIC:
        return "Arithmetic";
    case NODE_PROP_TEXTURE:
        return "Prop Texture";
    case NODE_PROP_RECTANGLE:
        return "Prop Rectangle";
    case NODE_PROP_CIRCLE:
        return "Prop Circle";
    case NODE_PRINT:
        return "Print";
    case NODE_DRAW_LINE:
        return "Draw Line";
    case NODE_LITERAL_NUM:
        return "Literal num";
    case NODE_LITERAL_STRING:
        return "Literal string";
    case NODE_LITERAL_BOOL:
        return "Literal bool";
    case NODE_LITERAL_COLOR:
        return "Literal color";
    default:
        return "invalid";
    }
}

static inline NodeType StringToNodeType(char strType[MAX_TYPE_LENGTH])
{
    if (strcmp(strType, "unknown") == 0)
        return NODE_UNKNOWN;
    if (strcmp(strType, "num") == 0)
        return NODE_NUM;
    if (strcmp(strType, "string") == 0)
        return NODE_STRING;
    if (strcmp(strType, "bool") == 0)
        return NODE_BOOL;
    if (strcmp(strType, "color") == 0)
        return NODE_COLOR;
    if (strcmp(strType, "sprite") == 0)
        return NODE_SPRITE;
    if (strcmp(strType, "Get var") == 0)
        return NODE_GET_VAR;
    if (strcmp(strType, "Set var") == 0)
        return NODE_SET_VAR;
    if (strcmp(strType, "Start") == 0)
        return NODE_EVENT_START;
    if (strcmp(strType, "Loop Tick") == 0)
        return NODE_EVENT_LOOP_TICK;
    if (strcmp(strType, "On Button") == 0)
        return NODE_EVENT_ON_BUTTON;
    if (strcmp(strType, "Create custom") == 0)
        return NODE_CREATE_CUSTOM_EVENT;
    if (strcmp(strType, "Call custom") == 0)
        return NODE_CALL_CUSTOM_EVENT;
    if (strcmp(strType, "Spawn") == 0)
        return NODE_SPAWN_SPRITE;
    if (strcmp(strType, "Destroy") == 0)
        return NODE_DESTROY_SPRITE;
    if (strcmp(strType, "Move To") == 0)
        return NODE_MOVE_TO_SPRITE;
    if (strcmp(strType, "Force") == 0)
        return NODE_FORCE_SPRITE;
    if (strcmp(strType, "Branch") == 0)
        return NODE_BRANCH;
    if (strcmp(strType, "Loop") == 0)
        return NODE_LOOP;
    if (strcmp(strType, "Comparison") == 0)
        return NODE_COMPARISON;
    if (strcmp(strType, "Gate") == 0)
        return NODE_GATE;
    if (strcmp(strType, "Arithmetic") == 0)
        return NODE_ARITHMETIC;
    if (strcmp(strType, "Prop Texture") == 0)
        return NODE_PROP_TEXTURE;
    if (strcmp(strType, "Prop Rectangle") == 0)
        return NODE_PROP_RECTANGLE;
    if (strcmp(strType, "Prop Circle") == 0)
        return NODE_PROP_CIRCLE;
    if (strcmp(strType, "Print") == 0)
        return NODE_PRINT;
    if (strcmp(strType, "Draw Line") == 0)
        return NODE_DRAW_LINE;
    if (strcmp(strType, "Literal num") == 0)
        return NODE_LITERAL_NUM;
    if (strcmp(strType, "Literal string") == 0)
        return NODE_LITERAL_STRING;
    if (strcmp(strType, "Literal bool") == 0)
        return NODE_LITERAL_BOOL;
    if (strcmp(strType, "Literal color") == 0)
        return NODE_LITERAL_COLOR;
    return NODE_UNKNOWN;
}

static inline char *GetKeyName(KeyboardKey key)
{
    if (key >= 'a' && key <= 'z') key -= 32;
    switch (key)
    {
    case KEY_APOSTROPHE: return "'";
    case KEY_COMMA: return ",";
    case KEY_MINUS: return "-";
    case KEY_PERIOD: return ".";
    case KEY_SLASH: return "/";
    case KEY_ZERO: return "0";
    case KEY_ONE: return "1";
    case KEY_TWO: return "2";
    case KEY_THREE: return "3";
    case KEY_FOUR: return "4";
    case KEY_FIVE: return "5";
    case KEY_SIX: return "6";
    case KEY_SEVEN: return "7";
    case KEY_EIGHT: return "8";
    case KEY_NINE: return "9";
    case KEY_SEMICOLON: return ";";
    case KEY_EQUAL: return "=";
    case KEY_A: return "A";
    case KEY_B: return "B";
    case KEY_C: return "C";
    case KEY_D: return "D";
    case KEY_E: return "E";
    case KEY_F: return "F";
    case KEY_G: return "G";
    case KEY_H: return "H";
    case KEY_I: return "I";
    case KEY_J: return "J";
    case KEY_K: return "K";
    case KEY_L: return "L";
    case KEY_M: return "M";
    case KEY_N: return "N";
    case KEY_O: return "O";
    case KEY_P: return "P";
    case KEY_Q: return "Q";
    case KEY_R: return "R";
    case KEY_S: return "S";
    case KEY_T: return "T";
    case KEY_U: return "U";
    case KEY_V: return "V";
    case KEY_W: return "W";
    case KEY_X: return "X";
    case KEY_Y: return "Y";
    case KEY_Z: return "Z";
    case KEY_LEFT_BRACKET: return "[";
    case KEY_BACKSLASH: return "\\";
    case KEY_RIGHT_BRACKET: return "]";
    case KEY_GRAVE: return "`";
    case KEY_SPACE: return "Space";
    case KEY_ESCAPE: return "Escape";
    case KEY_ENTER: return "Enter";
    case KEY_TAB: return "Tab";
    case KEY_BACKSPACE: return "Backspace";
    case KEY_INSERT: return "Insert";
    case KEY_DELETE: return "Delete";
    case KEY_RIGHT: return "Right";
    case KEY_LEFT: return "Left";
    case KEY_DOWN: return "Down";
    case KEY_UP: return "Up";
    case KEY_PAGE_UP: return "PageUp";
    case KEY_PAGE_DOWN: return "PageDown";
    case KEY_HOME: return "Home";
    case KEY_END: return "End";
    case KEY_CAPS_LOCK: return "CapsLock";
    case KEY_SCROLL_LOCK: return "ScrollLock";
    case KEY_NUM_LOCK: return "NumLock";
    case KEY_PRINT_SCREEN: return "PrintScreen";
    case KEY_PAUSE: return "Pause";
    case KEY_F1: return "F1";
    case KEY_F2: return "F2";
    case KEY_F3: return "F3";
    case KEY_F4: return "F4";
    case KEY_F5: return "F5";
    case KEY_F6: return "F6";
    case KEY_F7: return "F7";
    case KEY_F8: return "F8";
    case KEY_F9: return "F9";
    case KEY_F10: return "F10";
    case KEY_F11: return "F11";
    case KEY_F12: return "F12";
    case KEY_LEFT_SHIFT: return "Shift";
    case KEY_LEFT_CONTROL: return "Ctrl";
    case KEY_LEFT_ALT: return "Alt";
    case KEY_LEFT_SUPER: return "Super";
    case KEY_RIGHT_SHIFT: return "RightShift";
    case KEY_RIGHT_CONTROL: return "RightCtrl";
    case KEY_RIGHT_ALT: return "RightAlt";
    case KEY_RIGHT_SUPER: return "RightSuper";
    case KEY_KB_MENU: return "Menu";
    case KEY_KP_0: return "Keypad 0";
    case KEY_KP_1: return "Keypad 1";
    case KEY_KP_2: return "Keypad 2";
    case KEY_KP_3: return "Keypad 3";
    case KEY_KP_4: return "Keypad 4";
    case KEY_KP_5: return "Keypad 5";
    case KEY_KP_6: return "Keypad 6";
    case KEY_KP_7: return "Keypad 7";
    case KEY_KP_8: return "Keypad 8";
    case KEY_KP_9: return "Keypad 9";
    case KEY_KP_DECIMAL: return "Keypad .";
    case KEY_KP_DIVIDE: return "Keypad /";
    case KEY_KP_MULTIPLY: return "Keypad *";
    case KEY_KP_SUBTRACT: return "Keypad -";
    case KEY_KP_ADD: return "Keypad +";
    case KEY_KP_ENTER: return "Keypad Enter";
    case KEY_KP_EQUAL: return "Keypad =";
    default: return "Unknown";
    }
}