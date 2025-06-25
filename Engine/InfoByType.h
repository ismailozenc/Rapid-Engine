#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TYPE_LENGTH 16
#define typesCount 4

typedef enum
{
    NODE_UNKNOWN = 0,
    NODE_NUM,
    NODE_STRING,
    NODE_EX,
    NODE_BEGIN,
    NODE_PRINT,
    NODE_ADD,
    NODE_DELAY,
    NODE_BRANCH,
    //
} NodeType;

typedef enum
{
    PIN_FLOW,
    PIN_INT,
    PIN_FLOAT,
    PIN_STRING,
    //
} PinType;

typedef struct InfoByType{
    NodeType type;

    int inputCount;
    int outputCount;

    int width;
    int height;

    PinType inputs[16];
    PinType outputs[16];
}InfoByType;

static InfoByType NodeInfoByType[] = {
    {NODE_NUM, 2, 1, 120, 100, {PIN_FLOW, PIN_INT}, {PIN_FLOW}},
    {NODE_STRING, 2, 1, 120, 100, {PIN_FLOW, PIN_INT}, {PIN_FLOW}},
    {NODE_EX, 5, 5, 240, 200, {PIN_FLOW, PIN_INT, PIN_INT, PIN_INT, PIN_INT}, {PIN_FLOW, PIN_FLOW, PIN_INT, PIN_INT, PIN_INT}}
};

int getNodeInfoByType(NodeType type, char *info){
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

PinType *getInputsByType(NodeType type) {
    if (!NodeInfoByType) return NULL;

    for (int i = 0; i < typesCount; i++) {
        if (NodeInfoByType[i].type == type) {
            return NodeInfoByType[i].inputs ? NodeInfoByType[i].inputs : NULL;
        }
    }
    return NULL;
}
PinType *getOutputsByType(NodeType type) {
    if (!NodeInfoByType) return NULL;

    for (int i = 0; i < typesCount; i++) {
        if (NodeInfoByType[i].type == type) {
            return NodeInfoByType[i].outputs ? NodeInfoByType[i].outputs : NULL;
        }
    }
    return NULL;
}

const char* NodeTypeToString(NodeType type) {
    switch (type) {
        case NODE_UNKNOWN: return "unknown";
        case NODE_NUM:     return "num";
        case NODE_STRING:  return "string";
        case NODE_EX:      return "ex";
        case NODE_BEGIN:   return "begin";
        case NODE_PRINT:   return "print";
        case NODE_ADD:     return "add";
        case NODE_DELAY:   return "delay";
        case NODE_BRANCH:  return "branch";
        default:           return "invalid";
    }
}

NodeType StringToNodeType(char strType[MAX_TYPE_LENGTH])
{
    if (strcmp(strType, "num") == 0)
        return NODE_NUM;
    if (strcmp(strType, "string") == 0)
        return NODE_STRING;
    if (strcmp(strType, "ex") == 0)
        return NODE_EX;
    if (strcmp(strType, "print") == 0)
        return NODE_PRINT;
    if (strcmp(strType, "add") == 0)
        return NODE_ADD;
    if (strcmp(strType, "delay") == 0)
        return NODE_DELAY;
    if (strcmp(strType, "branch") == 0)
        return NODE_BRANCH;
    return NODE_UNKNOWN;
}