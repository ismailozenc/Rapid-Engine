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
    NODE_EVENT_LOOP,
    NODE_EVENT_ON_BUTTON,
    NODE_CREATE_CUSTOM_EVENT,
    NODE_CALL_CUSTOM_EVENT,
    NODE_SPAWN_SPRITE,
    NODE_DESTROY_SPRITE,
    NODE_MOVE_TO_SPRITE,
    NODE_BRANCH,
    NODE_LOOP,
    NODE_COMPARISON,
    NODE_GATE,
    NODE_ARITHMETIC,
    NODE_PRINT,
    NODE_DRAW_LINE,
    NODE_EX,
    NODE_LITERAL_NUM,
    NODE_LITERAL_STRING,
    NODE_LITERAL_BOOL,
    NODE_LITERAL_COLOR
} NodeType;

typedef enum
{
    PIN_NONE,
    PIN_FLOW,
    PIN_FLOAT,
    PIN_STRING,
    PIN_BOOL,
    PIN_COLOR,
    PIN_SPRITE,
    PIN_FIELD,
    PIN_COMPARISON_OPERATOR,
    PIN_GATE,
    PIN_ARITHMETIC
} PinType;

typedef enum
{
    EQUAL_TO,
    GREATER_THAN,
    LESS_THAN
}Comparison;

typedef enum
{
    AND,
    OR,
    NOT,
    XOR,
    NAND,
    NOR
}Gate;

typedef struct InfoByType{
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
}InfoByType;

static InfoByType NodeInfoByType[] = {
    {NODE_NUM, 2, 2, 120, 100, {60, 100, 159, 200}, true, {PIN_FLOW, PIN_FLOAT}, {PIN_FLOW, PIN_FLOAT}, {"", "Set value"}, {"", "Get value"}},
    {NODE_STRING, 2, 2, 120, 100, {60, 100, 159, 200}, true, {PIN_FLOW, PIN_STRING}, {PIN_FLOW, PIN_STRING}, {"", "Set value"}, {"", "Get value"}},
    {NODE_SPRITE, 3, 3, 240, 200, {60, 100, 159, 200}, true, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_GET_VAR, 3, 3, 240, 200, {60, 100, 159, 200}, true, {PIN_FLOW}, {PIN_FLOW, PIN_FLOAT}, {""}, {"", "Get value"}},
    {NODE_SET_VAR, 3, 3, 240, 200, {60, 100, 159, 200}, true, {PIN_FLOW, PIN_FLOAT}, {PIN_FLOW}, {"", "Set value"}, {""}},
    {NODE_EVENT_START, 0, 1, 150, 120, {148, 0, 0, 200}, false, {0}, {PIN_FLOW}, {""}, {""}},
    {NODE_EVENT_LOOP, 0, 1, 150, 120, {148, 0, 0, 200}, false, {0}, {PIN_FLOW}, {""}, {""}},
    {NODE_EVENT_ON_BUTTON, 0, 1, 240, 200, {148, 0, 0, 200}, false, {0}, {PIN_FLOW}, {""}, {""}},
    {NODE_CREATE_CUSTOM_EVENT, 0, 1, 240, 200, {148, 0, 0, 200}, false, {0}, {PIN_FLOW}, {""}, {""}},
    {NODE_CALL_CUSTOM_EVENT, 99, 1, 240, 200, {148, 0, 0, 200}, true, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_SPAWN_SPRITE, 1, 2, 240, 200, {60, 100, 159, 200}, true, {PIN_FLOW, PIN_SPRITE}, {PIN_FLOW}, {"", "Sprite"}, {""}},
    {NODE_DESTROY_SPRITE, 2, 1, 240, 200, {60, 100, 159, 200}, true, {PIN_FLOW, PIN_SPRITE}, {PIN_FLOW}, {"", "Sprite"}, {""}},
    {NODE_MOVE_TO_SPRITE, 3, 3, 240, 200, {60, 100, 159, 200}, true, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_BRANCH, 2, 2, 240, 200, {60, 100, 159, 200}, true, {PIN_FLOW, PIN_BOOL}, {PIN_FLOW, PIN_FLOW}, {"", "Condition"}, {""}},
    {NODE_LOOP, 2, 2, 240, 200, {60, 100, 159, 200}, true, {PIN_FLOW, PIN_BOOL}, {PIN_FLOW, PIN_FLOW}, {"", "Condition"}, {"", "Loop body"}},
    {NODE_COMPARISON, 4, 2, 200, 160, {60, 100, 159, 200}, true, {PIN_FLOW, PIN_COMPARISON_OPERATOR, PIN_FLOAT, PIN_FLOAT}, {PIN_FLOW, PIN_BOOL}, {"", "Value A", "Value B", "Operator"}, {"", "Result"}},
    {NODE_GATE, 4, 2, 240, 200, {60, 100, 159, 200}, true, {PIN_FLOW, PIN_GATE, PIN_BOOL, PIN_BOOL}, {PIN_FLOW, PIN_BOOL}, {"", "Condition A", "Condition B", "Gate"}, {"", "Result"}},
    {NODE_ARITHMETIC, 4, 2, 240, 200, {60, 100, 159, 200}, true, {PIN_FLOW, PIN_FLOAT, PIN_FLOAT, PIN_ARITHMETIC}, {PIN_FLOW, PIN_FLOAT}, {"", "Number A", "Number B", "Arithmetic"}, {"", "Result"}},
    {NODE_PRINT, 2, 1, 140, 100, {60, 100, 159, 200}, true, {PIN_FLOW, PIN_STRING}, {PIN_FLOW}, {"", "Print value"}, {""}},
    {NODE_DRAW_LINE, 6, 1, 240, 200, {60, 100, 159, 200}, true, {PIN_FLOW, PIN_FLOAT, PIN_FLOAT, PIN_FLOAT, PIN_FLOAT, PIN_COLOR}, {PIN_FLOW}, {"", "Start X", "Start Y", "End X", "End Y", "Color"}, {""}},
    {NODE_EX, 5, 5, 240, 200, {60, 100, 159, 200}, true, {PIN_FLOW, PIN_FLOAT, PIN_FLOAT, PIN_FLOAT, PIN_FLOAT}, {PIN_FLOW, PIN_FLOW, PIN_FLOAT, PIN_FLOAT, PIN_FLOAT}, {""}, {""}},
    {NODE_LITERAL_NUM, 1, 1, 240, 200, {60, 100, 159, 200}, true, {PIN_FIELD}, {PIN_FLOAT}, {""}, {""}},
    {NODE_LITERAL_STRING, 1, 1, 240, 200, {60, 100, 159, 200}, true, {PIN_FIELD}, {PIN_STRING}, {""}, {""}},
    {NODE_LITERAL_BOOL, 1, 1, 240, 200, {60, 100, 159, 200}, true, {PIN_FIELD}, {PIN_BOOL}, {""}, {""}},
    {NODE_LITERAL_COLOR, 1, 1, 240, 200, {60, 100, 159, 200}, true, {PIN_FIELD}, {PIN_COLOR}, {""}, {""}}
};

typedef struct DropdownOptionsByPinType{
    PinType type;
    int optionsCount;
    char *options[32];
    int boxWidth;
}DropdownOptionsByPinType;

static DropdownOptionsByPinType PinDropdownOptionsByType[] = {
    {PIN_COMPARISON_OPERATOR, 3, {"Equal To", "Greater Than", "Less Than"}, 120},
    {PIN_GATE, 6, {"AND", "OR", "NOT", "XOR", "NAND", "NOR"}, 60}
};

static inline DropdownOptionsByPinType getPinDropdownOptionsByType(PinType type){
    for(int i = 0; i < pinTypesCount; i++){
        if(type == PinDropdownOptionsByType[i].type){
            return PinDropdownOptionsByType[i];
        }
    }

    return (DropdownOptionsByPinType){0};
}

static inline int getNodeInfoByType(NodeType type, char *info){
    for(int i = 0; i < typesCount; i++){
        if(type == NodeInfoByType[i].type){
            if(strcmp(info, "inputCount") == 0){
                return NodeInfoByType[i].inputCount;
            }
            else if(strcmp(info, "outputCount") == 0){
                return NodeInfoByType[i].outputCount;
            }
            else if(strcmp(info, "width") == 0){
                return NodeInfoByType[i].width;
            }
            else if(strcmp(info, "height") == 0){
                return NodeInfoByType[i].height;
            }
        }
    }

    return -1;
}

static inline bool getIsEditableByType(NodeType type){
    for(int i = 0; i < typesCount; i++){
        if(type == NodeInfoByType[i].type){
            return NodeInfoByType[i].isEditable;
        }
     }
}

static inline char **getNodeInputNamesByType(NodeType type){
     for(int i = 0; i < typesCount; i++){
        if(type == NodeInfoByType[i].type){
            return NodeInfoByType[i].inputNames;
        }
     }
}

static inline char **getNodeOutputNamesByType(NodeType type){
     for(int i = 0; i < typesCount; i++){
        if(type == NodeInfoByType[i].type){
            return NodeInfoByType[i].outputNames;
        }
     }
}

static inline Color getNodeColorByType(NodeType type){
     for(int i = 0; i < typesCount; i++){
        if(type == NodeInfoByType[i].type){
            return NodeInfoByType[i].color;
        }
     }
}

static inline PinType *getInputsByType(NodeType type) {
    if (!NodeInfoByType) return NULL;

    for (int i = 0; i < typesCount; i++) {
        if (NodeInfoByType[i].type == type) {
            return NodeInfoByType[i].inputs ? NodeInfoByType[i].inputs : NULL;
        }
    }
    return NULL;
}

static inline PinType *getOutputsByType(NodeType type) {
    if (!NodeInfoByType) return NULL;

    for (int i = 0; i < typesCount; i++) {
        if (NodeInfoByType[i].type == type) {
            return NodeInfoByType[i].outputs ? NodeInfoByType[i].outputs : NULL;
        }
    }
    return NULL;
}

static inline const char* NodeTypeToString(NodeType type) {
    switch (type) {
        case NODE_UNKNOWN: return "unknown";
        case NODE_NUM: return "num";
        case NODE_STRING: return "string";
        case NODE_SPRITE: return "sprite";
        case NODE_GET_VAR: return "Get var";
        case NODE_SET_VAR: return "Set var";
        case NODE_EVENT_START: return "Start";
        case NODE_EVENT_LOOP: return "On Loop";
        case NODE_EVENT_ON_BUTTON: return "On Button";
        case NODE_CREATE_CUSTOM_EVENT: return "Create custom";
        case NODE_CALL_CUSTOM_EVENT: return "Call custom";
        case NODE_SPAWN_SPRITE: return "Spawn";
        case NODE_DESTROY_SPRITE: return "Destroy";
        case NODE_MOVE_TO_SPRITE: return "Move To";
        case NODE_BRANCH: return "Branch";
        case NODE_LOOP: return "Loop";
        case NODE_COMPARISON: return "Comparison";
        case NODE_GATE: return "Gate";
        case NODE_ARITHMETIC: return "Arithmetic";
        case NODE_PRINT: return "Print";
        case NODE_DRAW_LINE: return "Draw Line";
        case NODE_EX: return "ex";
        case NODE_LITERAL_NUM: return "Literal num";
        case NODE_LITERAL_STRING: return "Literal string";
        case NODE_LITERAL_BOOL: return "Literal bool";
        case NODE_LITERAL_COLOR: return "Literal color";
        default: return "invalid";
    }
}

static inline NodeType StringToNodeType(char strType[MAX_TYPE_LENGTH]) {
    if (strcmp(strType, "num") == 0) return NODE_NUM;
    if (strcmp(strType, "string") == 0) return NODE_STRING;
    if (strcmp(strType, "sprite") == 0) return NODE_SPRITE;
    if (strcmp(strType, "Get var") == 0) return NODE_GET_VAR;
    if (strcmp(strType, "Set var") == 0) return NODE_SET_VAR;
    if (strcmp(strType, "Start") == 0) return NODE_EVENT_START;
    if (strcmp(strType, "On Loop") == 0) return NODE_EVENT_LOOP;
    if (strcmp(strType, "On Button") == 0) return NODE_EVENT_ON_BUTTON;
    if (strcmp(strType, "Create custom") == 0) return NODE_CREATE_CUSTOM_EVENT;
    if (strcmp(strType, "Call custom") == 0) return NODE_CALL_CUSTOM_EVENT;
    if (strcmp(strType, "Spawn") == 0) return NODE_SPAWN_SPRITE;
    if (strcmp(strType, "Destroy") == 0) return NODE_DESTROY_SPRITE;
    if (strcmp(strType, "Move To") == 0) return NODE_MOVE_TO_SPRITE;
    if (strcmp(strType, "Branch") == 0) return NODE_BRANCH;
    if (strcmp(strType, "Loop") == 0) return NODE_LOOP;
    if (strcmp(strType, "Comparison") == 0) return NODE_COMPARISON;
    if (strcmp(strType, "Gate") == 0) return NODE_GATE;
    if (strcmp(strType, "Arithmetic") == 0) return NODE_ARITHMETIC;
    if (strcmp(strType, "Print") == 0) return NODE_PRINT;
    if (strcmp(strType, "Draw Line") == 0) return NODE_DRAW_LINE;
    if (strcmp(strType, "ex") == 0) return NODE_EX;
    if (strcmp(strType, "Literal num") == 0) return NODE_LITERAL_NUM;
    if (strcmp(strType, "Literal string") == 0) return NODE_LITERAL_STRING;
    if (strcmp(strType, "Literal bool") == 0) return NODE_LITERAL_BOOL;
    if (strcmp(strType, "Literal color") == 0) return NODE_LITERAL_COLOR;
    return NODE_UNKNOWN;
}