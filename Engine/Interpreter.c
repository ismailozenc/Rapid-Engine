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

void FreeInterpreterContext(InterpreterContext *interpreter)
{
    free(interpreter->values);
    interpreter->values = NULL;
}

void AddToLogFromInterpreter(InterpreterContext *interpreter, Value message, int level)
{
    char str[128];

    if (message.type == VAL_NUMBER)
    {
        sprintf(str, "%f", message.number);
    }
    else if (message.type == VAL_BOOL){
        sprintf(str, "%s", message.boolean ? "true" : "false");
    }
    else
    {
        strcpy(str, message.string);
    }

    strncpy(interpreter->logMessage, str, 127);
    interpreter->logMessage[128] = '\0';
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

RuntimeGraphContext ConvertToRuntimeGraph(GraphContext *graph, InterpreterContext *interpreter)
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
        dst->pickedOption = src->pickedOption;
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

    int totalOutputPins = 0;
    for (int i = 0; i < graph->nodeCount; i++)
    {
        RuntimeNode *node = &runtime.nodes[i];
        for (int j = 0; j < node->outputCount; j++)
        {
            if (node->outputPins[j]->type != PIN_FLOW)
                totalOutputPins++;
        }
    }

    interpreter->values = malloc(sizeof(Value) * totalOutputPins);
    interpreter->valueCount = 0;

    for (int i = 0; i < graph->nodeCount; i++)
    {
        RuntimeNode *node = &runtime.nodes[i];
        Node *srcNode = &graph->nodes[i];

        for (int j = 0; j < node->outputCount; j++)
        {
            RuntimePin *pin = node->outputPins[j];
            if (pin->type == PIN_FLOW)
                continue;

            int idx = interpreter->valueCount;

            switch (pin->type)
            {
            case PIN_FLOAT:
                interpreter->values[idx].number = 0;
                interpreter->values[idx].type = VAL_NUMBER;
                interpreter->values[idx].name = srcNode->name;
                break;
            case PIN_STRING:
                interpreter->values[idx].string = "null";
                interpreter->values[idx].type = VAL_STRING;
                interpreter->values[idx].name = srcNode->name;
                break;
            case PIN_BOOL:
                interpreter->values[idx].boolean = false;
                interpreter->values[idx].type = VAL_BOOL;
                interpreter->values[idx].name = srcNode->name;
                break;
            case PIN_COLOR:
                interpreter->values[idx].color = (Color){255, 255, 255, 255};
                interpreter->values[idx].type = VAL_COLOR;
                interpreter->values[idx].name = srcNode->name;
                break;
            case PIN_SPRITE:
                interpreter->values[idx].sprite = (Sprite){0};
                interpreter->values[idx].type = VAL_SPRITE;
                interpreter->values[idx].name = srcNode->name;
                break;
            default:
                break;
            }

            pin->valueIndex = idx;
            interpreter->valueCount++;
        }
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
    case NODE_UNKNOWN:
    {
        AddToLogFromInterpreter(interpreter, (Value){.type = VAL_STRING, .string = "Unknown node"}, 2);
        break;
    }

    case NODE_NUM:
    {
        if (graph->nodes[currNodeIndex].inputPins[1]->linkCount > 0)
        {
            graph->nodes[currNodeIndex].outputPins[1]->valueIndex = graph->nodes[currNodeIndex].inputPins[1]->linkedPins[0]->valueIndex;
        }
        break;
    }

    case NODE_STRING:
    {
        if (graph->nodes[currNodeIndex].inputPins[1]->linkCount > 0)
        {
            graph->nodes[currNodeIndex].outputPins[1]->valueIndex = graph->nodes[currNodeIndex].inputPins[1]->linkedPins[0]->valueIndex;
        }
        break;
    }

    case NODE_SPRITE:
    {
        break;
    }

    case NODE_GET_VAR:
    {
        break;
    }

    case NODE_SET_VAR:
    {
        break;
    }

    case NODE_CALL_CUSTOM_EVENT:
    {
        break;
    }

    case NODE_SPAWN_SPRITE:
    {
        break;
    }

    case NODE_DESTROY_SPRITE:
    {
        break;
    }

    case NODE_MOVE_TO_SPRITE:
    {
        break;
    }

    case NODE_BRANCH:
    {
        break;
    }

    case NODE_LOOP:
    {
        break;
    }

    case NODE_COMPARISON:
    {
        switch(graph->nodes[currNodeIndex].inputPins[1]->pickedOption){
            case EQUAL_TO:
                interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].boolean = interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].number == interpreter->values[graph->nodes[currNodeIndex].inputPins[3]->valueIndex].number;
                break;
            case GREATER_THAN:
                interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].boolean = interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].number > interpreter->values[graph->nodes[currNodeIndex].inputPins[3]->valueIndex].number;
                break;
            case LESS_THAN:
                interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].boolean = interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].number < interpreter->values[graph->nodes[currNodeIndex].inputPins[3]->valueIndex].number;
                break;
            default:
                //Error
                break;
        }
        break;
    }

    case NODE_GATE:
    {
        switch(graph->nodes[currNodeIndex].inputPins[1]->pickedOption){
            case AND:
                interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].boolean = interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].boolean && interpreter->values[graph->nodes[currNodeIndex].inputPins[3]->valueIndex].boolean;
                break;
            case OR:
                interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].boolean = interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].boolean || interpreter->values[graph->nodes[currNodeIndex].inputPins[3]->valueIndex].boolean;
                break;
            case NOT:
                interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].boolean = !interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].boolean;
                break;
            case XOR:
                interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].boolean = interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].boolean != interpreter->values[graph->nodes[currNodeIndex].inputPins[3]->valueIndex].boolean;
                break;
            case NAND:
                interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].boolean = !(interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].boolean && interpreter->values[graph->nodes[currNodeIndex].inputPins[3]->valueIndex].boolean);
                break;
            case NOR:
                interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].boolean = !(interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].boolean || interpreter->values[graph->nodes[currNodeIndex].inputPins[3]->valueIndex].boolean);
                break;
            default:
                //Error
                break;
        }
        break;
    }

    case NODE_ARITHMETIC:
    {
        break;
    }

    case NODE_PRINT:
    {
        if (graph->nodes[currNodeIndex].inputPins[1]->linkCount > 0)
        {
            AddToLogFromInterpreter(interpreter, interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->linkedPins[0]->valueIndex], 0);
        }
        break;
    }

    case NODE_DRAW_LINE:
    {
        break;
    }

    case NODE_EX:
    {
        break;
    }

    case NODE_LITERAL_NUM:
    {
        break;
    }

    case NODE_LITERAL_STRING:
    {
        break;
    }

    case NODE_LITERAL_BOOL:
    {
        break;
    }

    case NODE_LITERAL_COLOR:
    {
        break;
    }
    }

    if (currNodeIndex != lastNodeIndex)
    {
        InterpretStringOfNodes(currNodeIndex, interpreter, graph);
    }
}

bool HandleGameScreen(InterpreterContext *interpreter, RuntimeGraphContext *graph)
{
    interpreter->newLogMessage = false;

    if (interpreter->isFirstFrame)
    {
        for (int i = 0; i < graph->nodeCount; i++)
        {
            switch (graph->nodes[i].type)
            {
            case NODE_EVENT_START:
                InterpretStringOfNodes(i, interpreter, graph);
                break;
            case NODE_EVENT_LOOP:
                if (interpreter->loopNodeIndex == -1)
                {
                    interpreter->loopNodeIndex = i;
                }
                break;
            }

            interpreter->isFirstFrame = false;
        }

        for (int i = 0; i < graph->nodeCount; i++)
        {
            switch (graph->nodes[i].type)
            {
            case NODE_EVENT_ON_BUTTON:
                /*if(IsKeyPressed(KEY_K)){
                    InterpretStringOfNodes(i, interpreter, &graph);
                    printf("a\n\n");
                }*/
                break;
            }
        }
    }

    if (interpreter->loopNodeIndex == -1)
    {
        AddToLogFromInterpreter(interpreter, (Value){.type = VAL_STRING, .string = "No loop node found"}, 2);
        return false;
    }
    else
    {
        //InterpretStringOfNodes(interpreter->loopNodeIndex, interpreter, graph);
    }

    return true;
}