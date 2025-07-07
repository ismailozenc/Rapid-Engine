#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TYPE_LENGTH 16
#define typesCount (sizeof(NodeInfoByType) / sizeof(NodeInfoByType[0]))

typedef enum
{
    NODE_UNKNOWN = 0,
    NODE_NUM,
    NODE_STRING,
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
    NODE_LITERAL
} NodeType;

typedef enum
{
    PIN_NONE,
    PIN_FLOW,
    PIN_INT,
    PIN_FLOAT,
    PIN_STRING,
    PIN_FIELD,
    PIN_DROPDOWN
    //
} PinType;

typedef struct InfoByType{
    NodeType type;

    int inputCount;
    int outputCount;

    int width;
    int height;

    Color color;

    PinType inputs[16];
    PinType outputs[16];

    char *inputNames[16];
    char *outputNames[16];
}InfoByType;

static InfoByType NodeInfoByType[] = {
    {NODE_NUM, 2, 2, 120, 100, {60, 100, 159, 200}, {PIN_FLOW, PIN_INT}, {PIN_FLOW, PIN_INT}, {"", "Set var"}, {"", "Get var"}},
    {NODE_STRING, 2, 2, 120, 100, {60, 100, 159, 200}, {PIN_FLOW, PIN_STRING}, {PIN_FLOW, PIN_STRING}, {"", "Set var"}, {"", "Get var"}},
    {NODE_SPRITE, 3, 3, 240, 200, {60, 100, 159, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_GET_VAR, 3, 3, 240, 200, {60, 100, 159, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_SET_VAR, 3, 3, 240, 200, {60, 100, 159, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_EVENT_START, 0, 1, 150, 120, {148, 0, 0, 200}, {0}, {PIN_FLOW}, {""}, {""}},
    {NODE_EVENT_LOOP, 0, 1, 150, 120, {148, 0, 0, 200}, {0}, {PIN_FLOW}, {""}, {""}},
    {NODE_EVENT_ON_BUTTON, 3, 3, 240, 200, {148, 0, 0, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_CREATE_CUSTOM_EVENT, 3, 3, 240, 200, {148, 0, 0, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_CALL_CUSTOM_EVENT, 3, 3, 240, 200, {148, 0, 0, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_SPAWN_SPRITE, 3, 3, 240, 200, {60, 100, 159, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_DESTROY_SPRITE, 3, 3, 240, 200, {60, 100, 159, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_MOVE_TO_SPRITE, 3, 3, 240, 200, {60, 100, 159, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_BRANCH, 3, 3, 240, 200, {60, 100, 159, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_LOOP, 3, 3, 240, 200, {60, 100, 159, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_COMPARISON, 3, 3, 240, 200, {60, 100, 159, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_GATE, 3, 3, 240, 200, {60, 100, 159, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_ARITHMETIC, 3, 3, 240, 200, {60, 100, 159, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_PRINT, 2, 1, 140, 100, {60, 100, 159, 200}, {PIN_FLOW, PIN_STRING}, {PIN_FLOW}, {""}, {""}},
    {NODE_DRAW_LINE, 3, 3, 240, 200, {60, 100, 159, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}},
    {NODE_EX, 5, 5, 240, 200, {60, 100, 159, 200}, {PIN_FLOW, PIN_INT, PIN_INT, PIN_INT, PIN_INT}, {PIN_FLOW, PIN_FLOW, PIN_INT, PIN_INT, PIN_INT}, {""}, {""}},
    {NODE_LITERAL, 3, 3, 240, 200, {60, 100, 159, 200}, {PIN_FLOW}, {PIN_FLOW}, {""}, {""}}
};

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
        case NODE_LITERAL: return "Literal";
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
    if (strcmp(strType, "Literal") == 0) return NODE_LITERAL;
    return NODE_UNKNOWN;
}