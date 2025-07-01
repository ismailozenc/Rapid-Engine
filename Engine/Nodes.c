#include "Nodes.h"

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

GraphContext InitGraphContext()
{
    GraphContext graph;

    graph.nodes = NULL;
    graph.nodeCount = 0;
    graph.nextNodeID = 1;

    graph.pins = NULL;
    graph.pinCount = 0;
    graph.nextPinID = 1;

    graph.links = NULL;
    graph.linkCount = 0;
    graph.nextLinkID = 1;

    return graph;
}

void FreeGraphContext(GraphContext *graph)
{
    if (!graph)
        return;

    if (graph->pins)
    {
        free(graph->pins);
        graph->pins = NULL;
    }

    if (graph->links)
    {
        free(graph->links);
        graph->links = NULL;
    }

    if (graph->nodes)
    {
        free(graph->nodes);
        graph->nodes = NULL;
    }

    graph->nodeCount = 0;
    graph->nextNodeID = 0;
    graph->pinCount = 0;
    graph->nextPinID = 0;
    graph->linkCount = 0;
    graph->nextLinkID = 0;
}

int SaveGraphToFile(const char *filename, GraphContext *graph)
{
    FILE *file = fopen(filename, "wb");
    if (!file)
    {
        printf("Failed to open file for writing: %s\n", filename);
        return 1;
    }

    fwrite(&graph->nextNodeID, sizeof(int), 1, file);
    fwrite(&graph->nextPinID, sizeof(int), 1, file);
    fwrite(&graph->nextLinkID, sizeof(int), 1, file);

    fwrite(&graph->nodeCount, sizeof(int), 1, file);
    fwrite(graph->nodes, sizeof(Node), graph->nodeCount, file);

    fwrite(&graph->pinCount, sizeof(int), 1, file);
    fwrite(graph->pins, sizeof(Pin), graph->pinCount, file);

    fwrite(&graph->linkCount, sizeof(int), 1, file);
    fwrite(graph->links, sizeof(Link), graph->linkCount, file);

    fclose(file);

    return 0;
}

bool LoadGraphFromFile(const char *filename, GraphContext *graph)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        return false;
    }

    fread(&graph->nextNodeID, sizeof(int), 1, file);
    fread(&graph->nextPinID, sizeof(int), 1, file);
    fread(&graph->nextLinkID, sizeof(int), 1, file);

    fread(&graph->nodeCount, sizeof(int), 1, file);
    graph->nodes = malloc(sizeof(Node) * graph->nodeCount);
    fread(graph->nodes, sizeof(Node), graph->nodeCount, file);

    fread(&graph->pinCount, sizeof(int), 1, file);
    graph->pins = malloc(sizeof(Pin) * graph->pinCount);
    fread(graph->pins, sizeof(Pin), graph->pinCount, file);

    fread(&graph->linkCount, sizeof(int), 1, file);
    graph->links = malloc(sizeof(Link) * graph->linkCount);
    fread(graph->links, sizeof(Link), graph->linkCount, file);

    fclose(file);

    return true;
}

Pin CreatePin(GraphContext *graph, int nodeID, bool isInput, PinType type, int index, Vector2 pos)
{
    Pin pin = {0};
    pin.id = graph->nextPinID++;
    pin.nodeID = nodeID;
    pin.isInput = isInput;
    pin.type = type;
    pin.posInNode = index;
    pin.position = pos;
    return pin;
}

Node CreateNode(GraphContext *graph, NodeType type, Vector2 pos)
{
    Node node = {0};
    node.id = graph->nextNodeID++;
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

    int newPinCapacity = graph->pinCount + inputCount + outputCount;
    Pin *newPins = realloc(graph->pins, sizeof(Pin) * newPinCapacity);
    if (!newPins)
    {
        TraceLog(LOG_ERROR, "CreateNode: Failed to realloc pins array");
        return node;
    }
    graph->pins = newPins;

    for (int i = 0; i < inputCount; i++)
    {
        Pin pin = CreatePin(graph, node.id, true, getInputsByType(type)[i], i, (Vector2){0, 0});
        graph->pins[graph->pinCount] = pin;
        node.inputPins[node.inputCount++] = pin.id;
        graph->pinCount++;
    }

    for (int i = 0; i < outputCount; i++)
    {
        Pin pin = CreatePin(graph, node.id, false, getOutputsByType(type)[i], i, (Vector2){0, 0});
        graph->pins[graph->pinCount] = pin;
        node.outputPins[node.outputCount++] = pin.id;
        graph->pinCount++;
    }

    Node *newNodes = realloc(graph->nodes, sizeof(Node) * (graph->nodeCount + 1));
    if (!newNodes)
    {
        TraceLog(LOG_ERROR, "CreateNode: Failed to realloc nodes");
        return node;
    }
    graph->nodes = newNodes;
    graph->nodes[graph->nodeCount++] = node;

    return node;
}

void CreateLink(GraphContext *graph, Pin Pin1, Pin Pin2)
{
    if(Pin1.isInput == Pin2.isInput){return;}
    else if((Pin1.type == PIN_FLOW && Pin2.type != PIN_FLOW) || (Pin1.type != PIN_FLOW && Pin2.type == PIN_FLOW)){return;}

    Link link = {0};

    if(Pin1.isInput){
        link.inputPinID = Pin1.id;
        link.outputPinID = Pin2.id;
    }
    else if(Pin2.isInput){
        link.inputPinID = Pin2.id;
        link.outputPinID = Pin1.id;
    }

    graph->links = realloc(graph->links, sizeof(Link) * (graph->linkCount + 1));
    graph->links[graph->linkCount++] = link;

    return;
}

void DeleteNode(GraphContext *graph, int nodeID)
{
    if (graph->nodeCount == 0) return;

    int nodeIndex = -1;
    for (int i = 0; i < graph->nodeCount; i++) {
        if (graph->nodes[i].id == nodeID) {
            nodeIndex = i;
            break;
        }
    }
    if (nodeIndex == -1) return;

    graph->nodes[nodeIndex] = graph->nodes[graph->nodeCount - 1];
    graph->nodeCount--;

    if (graph->nodeCount == 0) {
        free(graph->nodes);
        graph->nodes = NULL;
    } else {
        Node *resized = realloc(graph->nodes, graph->nodeCount * sizeof(Node));
        if (resized) graph->nodes = resized;
    }

    int *pinsToDelete = malloc(graph->pinCount * sizeof(int));
    int pinsToDeleteCount = 0;

    for (int i = 0; i < graph->pinCount;) {
        if (graph->pins[i].nodeID == nodeID) {
            pinsToDelete[pinsToDeleteCount++] = graph->pins[i].id;
            graph->pins[i] = graph->pins[graph->pinCount - 1];
            graph->pinCount--;

            if (graph->pinCount == 0) {
                free(graph->pins);
                graph->pins = NULL;
                break;
            } else {
                Pin *resized = realloc(graph->pins, graph->pinCount * sizeof(Pin));
                if (resized) graph->pins = resized;
            }
        } else {
            i++;
        }
    }

    for (int i = 0; i < graph->linkCount;) {
        bool remove = false;
        for (int j = 0; j < pinsToDeleteCount; j++) {
            if (graph->links[i].inputPinID == pinsToDelete[j] ||
                graph->links[i].outputPinID == pinsToDelete[j]) {
                remove = true;
                break;
            }
        }

        if (remove) {
            graph->links[i] = graph->links[graph->linkCount - 1];
            graph->linkCount--;

            if (graph->linkCount == 0) {
                free(graph->links);
                graph->links = NULL;
                break;
            } else {
                Link *resized = realloc(graph->links, graph->linkCount * sizeof(Link));
                if (resized) graph->links = resized;
            }
        } else {
            i++;
        }
    }

    free(pinsToDelete);
}

void RemoveConnection(GraphContext *graph, int fromPinID, int toPinID)
{
    for (int i = 0; i < graph->linkCount; i++)
    {
        if (graph->links[i].outputPinID == fromPinID && graph->links[i].inputPinID == toPinID)
        {
            graph->links[i] = graph->links[graph->linkCount - 1];
            graph->linkCount--;
            graph->links = realloc(graph->links, sizeof(Link) * graph->linkCount);
            return;
        }
    }
}