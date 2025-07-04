#include "Interpreter.h"

InterpreterContext InitInterpreterContext()
{
    InterpreterContext interpreter = {0};

    interpreter.valueCount = 0;
    interpreter.loopNodeIndex = -1;

    interpreter.isFirstFrame = true;

    interpreter.newLogMessage = false;

    return interpreter;
}

void AddToLogFromInterpreter(InterpreterContext *interpreter, Value message, int level)
{
    char str[32];

    if (message.type == VAL_NUMBER)
    {
        sprintf(str, "%f", message.number);
    }
    else
    {
        strcpy(str, message.string);
    }

    strncpy(interpreter->logMessage, str, 128 * sizeof(char));
    interpreter->logMessageLevel = level;
    interpreter->newLogMessage = true;
}

int GetPinIndexByID(int id, GraphContext *graph)
{
    for (int i = 0; i < graph->pinCount; i++)
    {
        if (id == graph->pins[i].id)
        {
            return i;
        }
    }

    return -1;
}

RuntimeGraphContext ConvertToRuntimeGraph(GraphContext *graph)
{
    RuntimeGraphContext runtime = {0};

    runtime.nodeCount = graph->nodeCount;
    runtime.nodes = malloc(sizeof(RuntimeNode) * graph->nodeCount);

    runtime.pinCount = graph->pinCount;
    runtime.pins = malloc(sizeof(RuntimePin) * graph->pinCount);

    for (int i = 0; i < graph->pinCount; i++)
    {
        Pin *src = &graph->pins[i];
        RuntimePin *dst = &runtime.pins[i];

        dst->id = src->id;
        dst->type = src->type;
        dst->nodeIndex = -1;
        dst->isInput = src->isInput;
        dst->valueIndex = -1;
        dst->linkCount = 0;
    }

    for (int i = 0; i < graph->nodeCount; i++)
    {
        Node *srcNode = &graph->nodes[i];
        RuntimeNode *dstNode = &runtime.nodes[i];

        dstNode->index = i;
        dstNode->type = srcNode->type;
        dstNode->inputCount = srcNode->inputCount;
        dstNode->outputCount = srcNode->outputCount;

        for (int j = 0; j < srcNode->inputCount; j++)
        {
            int pinIndex = -1;
            for (int k = 0; k < graph->pinCount; k++)
            {
                if (graph->pins[k].id == srcNode->inputPins[j])
                {
                    pinIndex = k;
                    break;
                }
            }
            dstNode->inputPins[j] = &runtime.pins[pinIndex];
            runtime.pins[pinIndex].nodeIndex = i;
        }

        for (int j = 0; j < srcNode->outputCount; j++)
        {
            int pinIndex = -1;
            for (int k = 0; k < graph->pinCount; k++)
            {
                if (graph->pins[k].id == srcNode->outputPins[j])
                {
                    pinIndex = k;
                    break;
                }
            }
            dstNode->outputPins[j] = &runtime.pins[pinIndex];
            runtime.pins[pinIndex].nodeIndex = i;
        }
    }

    for (int i = 0; i < graph->linkCount; i++)
    {
        int inputIndex = -1;
        int outputIndex = -1;

        for (int j = 0; j < graph->pinCount; j++)
        {
            if (graph->pins[j].id == graph->links[i].inputPinID)
                inputIndex = j;
            if (graph->pins[j].id == graph->links[i].outputPinID)
                outputIndex = j;
        }

        if (inputIndex == -1 || outputIndex == -1)
            continue;

        RuntimePin *inputPin = &runtime.pins[inputIndex];
        RuntimePin *outputPin = &runtime.pins[outputIndex];

        if (inputPin->linkCount < MAX_LINKS_PER_PIN)
            inputPin->linkedPins[inputPin->linkCount++] = outputPin;

        if (outputPin->linkCount < MAX_LINKS_PER_PIN)
            outputPin->linkedPins[outputPin->linkCount++] = inputPin;
    }

    return runtime;
}

void InterpretStringOfNodes(int lastNodeIndex, InterpreterContext *interpreter, RuntimeGraphContext *graph)
{
    if (lastNodeIndex < 0 || lastNodeIndex >= graph->nodeCount)
        return;

    RuntimeNode *node = &graph->nodes[lastNodeIndex];
    if (node->outputCount == 0)
        return;
    RuntimePin *outPin = node->outputPins[0];
    if (outPin->linkCount == 0)
        return;
    int currNodeIndex = outPin->linkedPins[0]->nodeIndex;
    if (currNodeIndex < 0 || currNodeIndex >= graph->nodeCount)
        return;

    switch (graph->nodes[currNodeIndex].type)
    {
    case NODE_NUM:
    {
        interpreter->values[interpreter->valueCount].type = VAL_NUMBER;
        interpreter->values[interpreter->valueCount].number = 0;

        RuntimePin *outPin = graph->nodes[currNodeIndex].outputPins[1];
        outPin->valueIndex = interpreter->valueCount;

        interpreter->valueCount++;
        break;
    }
    case NODE_PRINT:
    {
        RuntimePin *input = graph->nodes[currNodeIndex].inputPins[1];

        if (input->linkCount > 0)
        {
            RuntimePin *linkedOutput = input->linkedPins[0];
            int index = linkedOutput->valueIndex;

            if (index >= 0 && index < interpreter->valueCount)
            {
                AddToLogFromInterpreter(interpreter, interpreter->values[index], 0);
            }
            else
            {
                AddToLogFromInterpreter(interpreter, (Value){VAL_STRING, .string = "Invalid value index"}, 2);
            }
        }
    }
    }

    if (currNodeIndex != lastNodeIndex)
    {
        InterpretStringOfNodes(currNodeIndex, interpreter, graph);
    }
}

bool HandleGameScreen(InterpreterContext *interpreter, GraphContext *initialGraph)
{
    static RuntimeGraphContext graph;

    if (interpreter->isFirstFrame)
    {
        graph = ConvertToRuntimeGraph(initialGraph);
        for (int i = 0; i < graph.nodeCount; i++)
        {
            switch (graph.nodes[i].type)
            {
            case NODE_EVENT_START:
                InterpretStringOfNodes(i, interpreter, &graph);
                break;
            case NODE_EVENT_LOOP:
                if (interpreter->loopNodeIndex == -1)
                {
                    interpreter->loopNodeIndex = i;
                }
                break;
            }
        }

        interpreter->isFirstFrame = false;
    }

    if (interpreter->loopNodeIndex == -1)
    {
        AddToLogFromInterpreter(interpreter, (Value){VAL_STRING, .string = "No loop node found"}, 2);
        return false;
    }
    else
    {
        InterpretStringOfNodes(interpreter->loopNodeIndex, interpreter, &graph);
    }

    return true;
}