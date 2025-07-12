#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "Nodes.h"

#define MAX_LINKS_PER_PIN 8

typedef struct RuntimePin
{
    int id;
    PinType type;
    int nodeIndex;
    bool isInput;
    int valueIndex;

    int nextNodeIndex; //Flow
    int pickedOption; //Dropdown
    char textFieldValue[128]; //Field
} RuntimePin;

typedef struct RuntimeNode
{
    int index;
    NodeType type;

    RuntimePin *inputPins[MAX_NODE_PINS];
    int inputCount;

    RuntimePin *outputPins[MAX_NODE_PINS];
    int outputCount;
} RuntimeNode;

typedef struct RuntimeGraphContext
{
    RuntimeNode *nodes;
    int nodeCount;

    RuntimePin *pins;
    int pinCount;
} RuntimeGraphContext;

typedef enum {
    VAL_NULL,
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL,
    VAL_COLOR,
    VAL_SPRITE
} ValueType;

typedef struct {
    Texture2D texture;
    Vector2 position;
    int width;
    int height;
    float rotation;
    bool visible;
    int layer;
}Sprite;

typedef struct {
    char *name;
    ValueType type;
    bool isVariable;
    union {
        float number;
        bool boolean;
        char *string;
        Vector2 vector;
        Color color;
        Sprite sprite;
    };
} Value;

typedef struct {
    Value *values;
    int valueCount;

    int loopNodeIndex;

    bool isFirstFrame;

    bool newLogMessage;
    char logMessage[128];
    int logMessageLevel;
} InterpreterContext;

InterpreterContext InitInterpreterContext();

char *ValueTypeToString(ValueType type);

char *ValueToString(Value value);

void FreeInterpreterContext(InterpreterContext *interpreter);

RuntimeGraphContext ConvertToRuntimeGraph(GraphContext *graph, InterpreterContext *interpreter);

void AddToLogFromInterpreter(InterpreterContext *interpreter, Value message, int level);

void InterpretStringOfNodes(int eventNodeIndex, InterpreterContext *interpreter, RuntimeGraphContext *graph);

bool HandleGameScreen(InterpreterContext *interpreter, RuntimeGraphContext *graph);