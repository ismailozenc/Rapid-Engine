#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "Nodes.h"

#define MAX_LINKS_PER_PIN 8

#define MAX_LOG_MESSAGES 32

typedef struct RuntimePin
{
    int id;
    PinType type;
    int nodeIndex;
    bool isInput;
    int valueIndex;

    int nextNodeIndex;           // Flow
    int pickedOption;            // Dropdown
    char textFieldValue[128];    // Field
    int componentIndex;          // Scene Component
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

typedef enum
{
    VAL_NULL,
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL,
    VAL_COLOR,
    VAL_SPRITE
} ValueType;

typedef struct
{
    Texture2D texture;
    Vector2 position;
    int width;
    int height;
    float rotation;
    bool isVisible;
    int layer;
} Sprite;

typedef enum
{
    PROP_TEXTURE,
    PROP_RECTANGLE,
    PROP_CIRCLE
} PropType;

typedef struct
{
    Texture2D texture;
    int width;
    int height;
    int radius;
    Vector2 position;
    PropType propType;
    Color color;
    float rotation;
    int layer;
} Prop;

typedef struct
{
    bool isVisible;
    bool isSprite;
    union
    {
        Sprite sprite;
        Prop prop;
    };
} SceneComponent;

typedef struct
{
    char *name;
    ValueType type;
    bool isVariable;
    union
    {
        float number;
        bool boolean;
        char *string;
        Vector2 vector;
        Color color;
        Sprite sprite;
    };
    int componentIndex;
} Value;

typedef struct
{
    Value *values;
    int valueCount;

    SceneComponent *components;
    int componentCount;

    int loopNodeIndex;

    bool isFirstFrame;

    bool newLogMessage;
    char logMessages[MAX_LOG_MESSAGES][128];
    int logMessageLevels[MAX_LOG_MESSAGES];
    int logMessageCount;

    int *onButtonNodeIndexes;
    int onButtonNodeIndexesCount;

    bool isInfiniteLoopProtectionOn;

    bool buildFailed;
} InterpreterContext;

InterpreterContext InitInterpreterContext();

void FreeInterpreterContext(InterpreterContext *interpreter);

char *ValueTypeToString(ValueType type);

char *ValueToString(Value value);

RuntimeGraphContext ConvertToRuntimeGraph(GraphContext *graph, InterpreterContext *interpreter);

bool HandleGameScreen(InterpreterContext *interpreter, RuntimeGraphContext *graph);