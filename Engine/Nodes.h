#ifndef NODES_H
#define NODES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "InfoByType.h"

#define MAX_NODE_PINS 16

#define MAX_HITBOX_VERTICES 64

typedef struct {
    Vector2 vertices[MAX_HITBOX_VERTICES];
    int count;
    bool closed;
} Polygon;

extern const char *InputsByNodeTypes[][5];

extern const char *OutputsByNodeTypes[][5];

typedef struct Node
{
    int id;
    char name[128];
    NodeType type;
    Vector2 position;

    int inputPins[MAX_NODE_PINS];
    int inputCount;

    int outputPins[MAX_NODE_PINS];
    int outputCount;
} Node;

typedef struct Pin
{
    int id;
    PinType type;
    int nodeID;
    int posInNode;
    bool isInput;
    Vector2 position;
    int pickedOption;
    char textFieldValue[128];
    bool isFloat;
} Pin;

typedef struct Link
{
    int inputPinID;
    int outputPinID;
} Link;

#define INVALID_PIN (Pin) {-1}

typedef struct GraphContext
{
    Node *nodes;
    int nodeCount;
    int nextNodeID;

    Pin *pins;
    int pinCount;
    int nextPinID;

    Link *links;
    int linkCount;
    int nextLinkID;

    char **variables;
    NodeType *variableTypes;
    int variablesCount;
} GraphContext;

GraphContext InitGraphContext();

void FreeGraphContext(GraphContext *graph);

int SaveGraphToFile(const char *filename, GraphContext *graph);

bool LoadGraphFromFile(const char *filename, GraphContext *graph);

Pin CreatePin(GraphContext *graph, int nodeID, bool isInput, PinType type, int index, Vector2 pos);

Node CreateNode(GraphContext *graph, NodeType type, Vector2 pos);

void CreateLink(GraphContext *graph, Pin Pin1, Pin Pin2);

void DeleteNode(GraphContext *graph, int nodeID);

int FindPinIndexByID(GraphContext *graph, int id);

void RemoveConnections(GraphContext *graph, int pinID);

#endif