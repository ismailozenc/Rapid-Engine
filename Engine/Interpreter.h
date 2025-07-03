#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "Nodes.h"

typedef struct {
    bool newLogMessage;
    char logMessage[128];
    int logMessageLevel;
} InterpreterContext;

typedef enum {
    VAL_NULL,
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL,
    VAL_VECTOR2,
    // add more types as needed
} ValueType;

typedef struct {
    ValueType type;
    union {
        float number;
        bool boolean;
        char *string;
        Vector2 vector;
        // Add more fields as needed
    };
} Value;

InterpreterContext InitInterpreterContext();

void AddToLogFromInterpreter(InterpreterContext *interpreter, Value message, int level);

void HandleGameScreen(InterpreterContext *interpreter, GraphContext *graph);