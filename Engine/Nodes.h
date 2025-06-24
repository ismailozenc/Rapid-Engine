#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "InfoByType.h"

#define MAX_NODE_PINS 16

const char *InputsByNodeTypes[][5] = {
    {"name", "value"},
    {"name", "value"},
    {"a", "b", "c", "d", "e"},
    {"Sub 4-1"},
    {"Sub 5-1", "Sub 5-2"},
    {"Sub 6-1", "Sub 6-2", "Sub 6-3", "Sub 6-4"},
    {"Sub 7-1"},
    {"Sub 8-1", "Sub 8-2"},
    {"Sub 9-1", "Sub 9-2", "Sub 9-3"},
    {"Sub 10-1"}};

const char *OutputsByNodeTypes[][5] = {
    {NULL},
    {NULL},
    {"a", "b", "c", "d", "e"},
    {"Sub 4-1"},
    {"Sub 5-1", "Sub 5-2"},
    {"Sub 6-1", "Sub 6-2", "Sub 6-3", "Sub 6-4"},
    {"Sub 7-1"},
    {"Sub 8-1", "Sub 8-2"},
    {"Sub 9-1", "Sub 9-2", "Sub 9-3"},
    {"Sub 10-1"}};

typedef struct Node
{
    int id;
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
    int posInNode; //
    bool isFlow;
    bool isInput;
    Vector2 position;
} Pin;

typedef struct Link
{
    Pin inputPin;
    Pin outputPin;
} Link;

#define INVALID_PIN \
    (Pin) { -1, -1, -1, false, false }

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
} GraphContext;

void InitGraphContext(GraphContext *ctx)
{
    ctx->nodes = NULL;
    ctx->nodeCount = 0;
    ctx->nextNodeID = 1;

    ctx->pins = NULL;
    ctx->pinCount = 0;
    ctx->nextPinID = 1;

    ctx->links = NULL;
    ctx->linkCount = 0;
    ctx->nextLinkID = 1;
}

void FreeGraphContext(GraphContext *ctx)
{
    if (!ctx)
        return;

    // Free pins array
    if (ctx->pins)
    {
        free(ctx->pins);
        ctx->pins = NULL;
    }

    // Free links array
    if (ctx->links)
    {
        free(ctx->links);
        ctx->links = NULL;
    }

    // Free nodes array
    if (ctx->nodes)
    {
        free(ctx->nodes);
        ctx->nodes = NULL;
    }

    // Reset counters
    ctx->nodeCount = 0;
    ctx->nextNodeID = 0;
    ctx->pinCount = 0;
    ctx->nextPinID = 0;
    ctx->linkCount = 0;
    ctx->nextLinkID = 0;
}

int SaveGraphToFile(const char *filename, GraphContext *ctx)
{
    FILE *file = fopen(filename, "wb");
    if (!file)
    {
        printf("Failed to open file for writing: %s\n", filename);
        return 1;
    }

    fwrite(&ctx->nextNodeID, sizeof(int), 1, file);
    fwrite(&ctx->nextPinID, sizeof(int), 1, file);
    fwrite(&ctx->nextLinkID, sizeof(int), 1, file);

    fwrite(&ctx->nodeCount, sizeof(int), 1, file);
    fwrite(ctx->nodes, sizeof(Node), ctx->nodeCount, file);

    fwrite(&ctx->pinCount, sizeof(int), 1, file);
    fwrite(ctx->pins, sizeof(Pin), ctx->pinCount, file);

    fwrite(&ctx->linkCount, sizeof(int), 1, file);
    fwrite(ctx->links, sizeof(Link), ctx->linkCount, file);

    fclose(file);

    return 0;
}

void LoadGraphFromFile(const char *filename, GraphContext *ctx)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        printf("Failed to open file for reading: %s\n", filename);
        return;
    }

    fread(&ctx->nextNodeID, sizeof(int), 1, file);
    fread(&ctx->nextPinID, sizeof(int), 1, file);
    fread(&ctx->nextLinkID, sizeof(int), 1, file);

    fread(&ctx->nodeCount, sizeof(int), 1, file);
    ctx->nodes = malloc(sizeof(Node) * ctx->nodeCount);
    fread(ctx->nodes, sizeof(Node), ctx->nodeCount, file);

    fread(&ctx->pinCount, sizeof(int), 1, file);
    ctx->pins = malloc(sizeof(Pin) * ctx->pinCount);
    fread(ctx->pins, sizeof(Pin), ctx->pinCount, file);

    fread(&ctx->linkCount, sizeof(int), 1, file);
    ctx->links = malloc(sizeof(Link) * ctx->linkCount);
    fread(ctx->links, sizeof(Link), ctx->linkCount, file);

    fclose(file);
}

Pin CreatePin(GraphContext *ctx, int nodeID, bool isInput, PinType type, int index, Vector2 pos)
{
    Pin pin = {0};
    pin.id = ctx->nextPinID++;
    pin.nodeID = nodeID;
    pin.isInput = isInput;
    pin.type = type;
    pin.posInNode = index;
    pin.position = pos;
    return pin;
}

Node CreateNode(GraphContext *ctx, NodeType type, Vector2 pos)
{
    Node node = {0};
    node.id = ctx->nextNodeID++;
    node.type = type;
    node.position = pos;

    if (type == NODE_UNKNOWN)
    {
        return node;
    }

    int inputCount = getNodeInfoByType(type, "inputCount");
    int outputCount = getNodeInfoByType(type, "outputCount");

    if (inputCount > MAX_NODE_PINS || outputCount > MAX_NODE_PINS)
    {
        TraceLog(LOG_ERROR, "CreateNode: input/output count exceeds MAX_NODE_PINS");
        return node;
    }

    // Preallocate room for all pins at once
    int newPinCapacity = ctx->pinCount + inputCount + outputCount;
    Pin *newPins = realloc(ctx->pins, sizeof(Pin) * newPinCapacity);
    if (!newPins)
    {
        TraceLog(LOG_ERROR, "CreateNode: Failed to realloc pins array");
        return node;
    }
    ctx->pins = newPins;

    // Create input pins
    for (int i = 0; i < inputCount; i++)
    {
        Pin pin = CreatePin(ctx, node.id, true, getInputsByType(type)[i], i, (Vector2){0, 0});
        ctx->pins[ctx->pinCount] = pin;
        node.inputPins[node.inputCount++] = pin.id;
        ctx->pinCount++;
    }

    // Create output pins
    for (int i = 0; i < outputCount; i++)
    {
        Pin pin = CreatePin(ctx, node.id, false, getOutputsByType(type)[i], i, (Vector2){0, 0});
        ctx->pins[ctx->pinCount] = pin;
        node.outputPins[node.outputCount++] = pin.id;
        ctx->pinCount++;
    }

    Node *newNodes = realloc(ctx->nodes, sizeof(Node) * (ctx->nodeCount + 1));
    if (!newNodes)
    {
        TraceLog(LOG_ERROR, "CreateNode: Failed to realloc nodes");
        return node;
    }
    ctx->nodes = newNodes;
    ctx->nodes[ctx->nodeCount++] = node;

    return node;
}

Link CreateLink(GraphContext *ctx, Pin inputPin, Pin outputPin)
{
    Link link = {0};
    link.inputPin = inputPin;
    link.outputPin = outputPin;
    return link;
}

void DeleteNode(GraphContext *ctx, int nodeID)
{
    // 1. Find and remove node
    int nodeIndex = -1;
    for (int i = 0; i < ctx->nodeCount; i++)
    {
        if (ctx->nodes[i].id == nodeID)
        {
            nodeIndex = i;
            break;
        }
    }
    if (nodeIndex == -1)
        return; // Node not found

    // Remove node by swapping with last and shrinking array
    ctx->nodes[nodeIndex] = ctx->nodes[ctx->nodeCount - 1];
    ctx->nodeCount--;
    ctx->nodes = realloc(ctx->nodes, sizeof(Node) * ctx->nodeCount);

    // 2. Remove pins belonging to node and collect their IDs
    int *pinsToDelete = malloc(sizeof(int) * ctx->pinCount);
    int pinsToDeleteCount = 0;

    for (int i = 0; i < ctx->pinCount; /*increment inside*/)
    {
        if (ctx->pins[i].nodeID == nodeID)
        {
            pinsToDelete[pinsToDeleteCount++] = ctx->pins[i].id;

            // Remove pin by swapping with last and shrinking
            ctx->pins[i] = ctx->pins[ctx->pinCount - 1];
            ctx->pinCount--;
            ctx->pins = realloc(ctx->pins, sizeof(Pin) * ctx->pinCount);
            // Don't increment i because we swapped a new pin into i
        }
        else
        {
            i++;
        }
    }

    // 3. Remove all links connected to deleted pins
    for (int i = 0; i < ctx->linkCount; /* increment inside */)
    {
        int outputPinID = ctx->links[i].outputPin.id;
        int inputPinID = ctx->links[i].inputPin.id;

        bool outputDeleted = false;
        bool inputDeleted = false;

        for (int j = 0; j < pinsToDeleteCount; j++)
        {
            if (outputPinID == pinsToDelete[j])
                outputDeleted = true;
            if (inputPinID == pinsToDelete[j])
                inputDeleted = true;
            if (outputDeleted && inputDeleted)
                break;
        }

        if (outputDeleted || inputDeleted)
        {
            // Remove link by swapping with last and shrinking
            ctx->links[i] = ctx->links[ctx->linkCount - 1];
            ctx->linkCount--;
            ctx->links = realloc(ctx->links, sizeof(Link) * ctx->linkCount);
            // Don't increment i because we swapped a new element into i
        }
        else
        {
            i++;
        }
    }

    free(pinsToDelete);
}

void RemoveConnection(GraphContext *ctx, int fromPinID, int toPinID)
{
    for (int i = 0; i < ctx->linkCount; i++)
    {
        if (ctx->links[i].outputPin.id == fromPinID && ctx->links[i].inputPin.id == toPinID)
        {
            // Swap with last link and shrink
            ctx->links[i] = ctx->links[ctx->linkCount - 1];
            ctx->linkCount--;
            ctx->links = realloc(ctx->links, sizeof(Link) * ctx->linkCount);
            return; // Assuming only one connection between those pins
        }
    }
}

// makeConnection

/*int SaveNode(char CGFilePath[], Node *node)
{
    FILE *file = fopen(CGFilePath, "ab");
    if (!file)
    {
        perror("Failed to open file");
        return 1;
    }

    fwrite(node->type, sizeof(char), 16, file);
    fwrite(&node->id, sizeof(int), 1, file);
    fwrite(&node->position, sizeof(Vector2), 1, file);

    fwrite(&node->next, sizeof(int), 1, file);

    for (int i = 0; i < getNodeInfoByType(node->type, "inputCount"); i++)
    {
        fwrite(&node->input[i], sizeof(int), 1, file);
    }

    fclose(file);

    return 0;
}

int OverrideFileWithList(char CGFilePath[], Node *NodeList, size_t nodeCount, int lastNodeID)
{
    FILE *file = fopen(CGFilePath, "wb");
    if (!file)
    {
        perror("Failed to open file");
        return 1;
    }

    fwrite(&lastNodeID, sizeof(int), 1, file);

    fclose(file);

    for (size_t i = 0; i < nodeCount; i++)
    {
        if (SaveNode(CGFilePath, &NodeList[i]) == 1)
        {
            return 1;
        }
    }

    return 0;
}

void MakeNode(char *nodeType, char *projectName, char *CGFilePath, Node **NodeList, size_t *nodeCount, Vector2 position, int *lastID)
{
    Node node;
    strcpy(node.type, nodeType);
    *lastID += 1;
    node.id = *lastID;
    node.position = position;

    node.next = -1;

    for (int i = 0; i < getNodeInfoByType(node.type, "inputCount"); i++)
    {
        node.input[i] = -1;
    }

    *NodeList = (Node *)realloc(*NodeList, (*nodeCount + 1) * sizeof(Node));
    if (!*NodeList)
    {
        perror("Failed to allocate memory for NodeList");
        exit(1);
    }

    (*NodeList)[*nodeCount] = node;
    (*nodeCount)++;
}

int GetLinkedNodeIndex(Node *NodeList, int nodeCount, int ID)
{
    for (int i = 0; i < nodeCount; i++)
    {
        if (ID > 10000 && NodeList[i].id == ID / 100)
        {
            return i;
        }
        else if (ID < 10000 && NodeList[i].id == ID)
        {
            return i;
        }
    }

    return -1;
}

void RemoveNode(Node **NodeList, size_t *nodeCount, int index)
{
    if (!NodeList || !*NodeList || !nodeCount || index < 0 || index >= (int)(*nodeCount))
        return;

    for (int i = 0; i < *nodeCount; i++)
    {
        if ((*NodeList)[i].next == (*NodeList)[index].id)
        {
            (*NodeList)[i].next = -1;
        }

        for (int inputPin = 0; inputPin < getNodeInfoByType((*NodeList)[i].type, "inputCount"); inputPin++)
        {
            if ((*NodeList)[i].input[inputPin] / 100 == (*NodeList)[index].id)
            {
                (*NodeList)[i].input[inputPin] = -1;
            }
        }
    }

    for (int i = index; i < *nodeCount - 1; i++)
    {
        (*NodeList)[i] = (*NodeList)[i + 1];
    }

    (*nodeCount)--;
    if (*nodeCount == 0)
    {
        free(*NodeList);
        *NodeList = NULL;
    }
    else
    {
        Node *newList = realloc(*NodeList, (*nodeCount) * sizeof(Node));
        if (newList || *nodeCount == 0)
        {
            *NodeList = newList;
        }
    }
}

int LoadNodesFromFile(char *CGFilePath, Node **NodeList, size_t *nodeCount, int *lastID)
{
    FILE *file = fopen(CGFilePath, "rb");
    if (!file)
    {
        perror("Failed to open file");
        return 1;
    }

    Node tempNode;
    *nodeCount = 0;
    *NodeList = NULL;

    fread(lastID, sizeof(int), 1, file);

    while (fread(tempNode.type, sizeof(char), 16, file) == 16)
    {
        tempNode.type[15] = '\0';

        fread(&tempNode.id, sizeof(int), 1, file);
        fread(&tempNode.position, sizeof(Vector2), 1, file);

        fread(&tempNode.next, sizeof(int), 1, file);

        if (getNodeInfoByType(tempNode.type, "inputCount") == -1) // should be CheckIfTypeExistsFunction
        {
            printf("\nCorrupted file");
            return 2;
        }

        for (int i = 0; i < getNodeInfoByType(tempNode.type, "inputCount"); i++)
        {
            fread(&tempNode.input[i], sizeof(int), 1, file);
        }

        *NodeList = (Node *)realloc(*NodeList, (*nodeCount + 1) * sizeof(Node));
        if (!*NodeList)
        {
            perror("Memory allocation failed");
            fclose(file);
            return 3;
        }

        (*NodeList)[*nodeCount] = tempNode;
        (*nodeCount)++;
    }

    fclose(file);

    return 0;
}

bool MakeConnection(Pin Pin1, Pin Pin2, Node *NodeList, size_t nodeCount)
{
    if (Pin1.isFlow != Pin2.isFlow || Pin1.isInput == Pin2.isInput || Pin1.nodeID == Pin2.nodeID)
    {
        return false;
    }

    if (Pin1.isFlow && Pin2.isFlow)
    {
        if (Pin1.isInput == false)
        {
            NodeList[Pin1.nodeIndexInList].next = Pin2.nodeID;
            return true;
        }
        else if (Pin2.isInput == false)
        {
            NodeList[Pin2.nodeIndexInList].next = Pin1.nodeID;
            return true;
        }
        return false;
    }
    else
    {
        if (Pin1.isInput)
        {
            NodeList[Pin1.nodeIndexInList].input[Pin1.number] = Pin2.nodeID * 100 + Pin2.number;
            return true;
        }
        else if (Pin2.isInput)
        {
            NodeList[Pin2.nodeIndexInList].input[Pin2.number] = Pin1.nodeID * 100 + Pin1.number;
            return true;
        }
        return false;
    }
}*/