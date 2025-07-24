#include "Interpreter.h"

InterpreterContext InitInterpreterContext()
{
    InterpreterContext interpreter = {0};

    interpreter.valueCount = 0;
    interpreter.loopNodeIndex = -1;

    interpreter.isFirstFrame = true;

    interpreter.newLogMessage = false;

    interpreter.buildFailed = false;

    interpreter.isInfiniteLoopProtectionOn = true;

    return interpreter;
}

void FreeInterpreterContext(InterpreterContext *interpreter)
{
    free(interpreter->values);
    interpreter->values = NULL;
}

char *ValueTypeToString(ValueType type)
{
    switch (type)
    {
    case VAL_NULL:
        return "null(Error)";
    case VAL_NUMBER:
        return "number";
    case VAL_STRING:
        return "string";
    case VAL_BOOL:
        return "boolean";
    case VAL_COLOR:
        return "color";
    case VAL_SPRITE:
        return "sprite";
    default:
        return "Error";
    }
}

char *ValueToString(Value value)
{
    char *temp = malloc(256);
    if (!temp)
        return NULL;
    switch (value.type)
    {
    case VAL_NULL:
        sprintf(temp, "Error");
        break;
    case VAL_NUMBER:
        sprintf(temp, "%d", value.number);
        break;
    case VAL_STRING:
        sprintf(temp, "%s", value.string);
        break;
    case VAL_BOOL:
        sprintf(temp, "%s", value.boolean ? "true" : "false");
        break;
    case VAL_COLOR:
        sprintf(temp, "%d %d %d %d", value.color.r, value.color.g, value.color.b, value.color.a);
        break;
    case VAL_SPRITE:
        sprintf(temp, "");
        break;
    default:
        sprintf(temp, "Error");
    }
    return temp;
}

void AddToLogFromInterpreter(InterpreterContext *interpreter, Value message, int level)
{
    if (interpreter->logMessageCount >= MAX_LOG_MESSAGES)
        return;

    char str[128];

    switch (message.type)
    {
    case VAL_NUMBER:
        sprintf(str, "%f", message.number);
        break;
    case VAL_STRING:
        strncpy(str, message.string, 127);
        break;
    case VAL_BOOL:
        strcpy(str, message.boolean ? "true" : "false");
        break;
    case VAL_COLOR:
        sprintf(str, "%08X", (message.color.r << 24) | (message.color.g << 16) | (message.color.b << 8) | message.color.a);
        break;
    default:
        strcpy(str, "Error");
    }

    str[127] = '\0';

    strncpy(interpreter->logMessages[interpreter->logMessageCount], str, 127);
    interpreter->logMessageLevels[interpreter->logMessageCount] = level;
    interpreter->logMessageCount++;
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
        dst->pickedOption = src->pickedOption;
        dst->nextNodeIndex = -1;
        strcpy(dst->textFieldValue, src->textFieldValue);
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

    int totalOutputPins = 0;
    int totalComponents = 0;
    for (int i = 0; i < graph->nodeCount; i++)
    {
        RuntimeNode *node = &runtime.nodes[i];
        for (int j = 0; j < node->outputCount; j++)
        {
            if (node->outputPins[j]->type != PIN_FLOW)
                totalOutputPins++;
        }

        if (node->type == NODE_PROP_TEXTURE || node->type == NODE_PROP_RECTANGLE || node->type == NODE_PROP_CIRCLE || node->type == NODE_SPRITE)
        {
            totalComponents++;
        }
    }

    interpreter->values = malloc(sizeof(Value) * (totalOutputPins + 1));
    interpreter->values[0] = (Value){.type = VAL_STRING, .string = "Error value"};
    interpreter->valueCount = 1;

    interpreter->components = malloc(sizeof(SceneComponent) * (totalComponents + 1));
    interpreter->componentCount = 0;

    for (int i = 0; i < graph->nodeCount; i++)
    {
        RuntimeNode *node = &runtime.nodes[i];
        Node *srcNode = &graph->nodes[i];

        switch (node->type)
        {
        case NODE_LITERAL_NUM:
            interpreter->values[interpreter->valueCount].number = strtof(node->inputPins[0]->textFieldValue, NULL);
            interpreter->values[interpreter->valueCount].type = VAL_NUMBER;
            interpreter->values[interpreter->valueCount].isVariable = false;
            interpreter->values[interpreter->valueCount].name = srcNode->name;
            node->outputPins[0]->valueIndex = interpreter->valueCount;
            interpreter->valueCount++;
            continue;
        case NODE_LITERAL_STRING:
            interpreter->values[interpreter->valueCount].string = strdup(node->inputPins[0]->textFieldValue);
            interpreter->values[interpreter->valueCount].type = VAL_STRING;
            interpreter->values[interpreter->valueCount].isVariable = false;
            interpreter->values[interpreter->valueCount].name = srcNode->name;
            node->outputPins[0]->valueIndex = interpreter->valueCount;
            interpreter->valueCount++;
            continue;
        case NODE_LITERAL_BOOL:
            if (strcmp(node->inputPins[0]->textFieldValue, "true") == 0)
            {
                interpreter->values[interpreter->valueCount].boolean = true;
            }
            else
            {
                interpreter->values[interpreter->valueCount].boolean = false;
            }
            interpreter->values[interpreter->valueCount].type = VAL_BOOL;
            interpreter->values[interpreter->valueCount].isVariable = false;
            interpreter->values[interpreter->valueCount].name = srcNode->name;
            node->outputPins[0]->valueIndex = interpreter->valueCount;
            interpreter->valueCount++;
            continue;
        case NODE_LITERAL_COLOR:
            unsigned int hexValue;
            if (sscanf(node->inputPins[0]->textFieldValue, "%x", &hexValue) == 1)
            {
                Color color = {(hexValue >> 24) & 0xFF, (hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF};
                interpreter->values[interpreter->valueCount].color = color;
                interpreter->values[interpreter->valueCount].type = VAL_COLOR;
                interpreter->values[interpreter->valueCount].isVariable = false;
                interpreter->values[interpreter->valueCount].name = srcNode->name;
                node->outputPins[0]->valueIndex = interpreter->valueCount;
                interpreter->valueCount++;
            }
            else
            {
                AddToLogFromInterpreter(interpreter, (Value){.type = VAL_STRING, .string = "Error: Invalid color"}, 2);
            }
            continue;
        default:
            break;
        }

        for (int j = 0; j < node->outputCount; j++)
        {
            RuntimePin *pin = node->outputPins[j];
            if (pin->type == PIN_FLOW)
                continue;

            int idx = interpreter->valueCount;

            bool isVariable = false;
            if (node->type == NODE_NUM || node->type == NODE_STRING || node->type == NODE_SPRITE || node->type == NODE_BOOL || node->type == NODE_COLOR)
            {
                isVariable = true;
                node->inputPins[1]->valueIndex = idx;
            }

            switch (pin->type)
            {
            case PIN_NUM:
                interpreter->values[idx].number = 0;
                interpreter->values[idx].type = VAL_NUMBER;
                interpreter->values[idx].isVariable = isVariable;
                interpreter->values[idx].name = srcNode->name;
                break;
            case PIN_STRING:
                interpreter->values[idx].string = "null";
                interpreter->values[idx].type = VAL_STRING;
                interpreter->values[idx].isVariable = isVariable;
                interpreter->values[idx].name = srcNode->name;
                break;
            case PIN_BOOL:
                interpreter->values[idx].boolean = false;
                interpreter->values[idx].type = VAL_BOOL;
                interpreter->values[idx].isVariable = isVariable;
                interpreter->values[idx].name = srcNode->name;
                break;
            case PIN_COLOR:
                interpreter->values[idx].color = (Color){255, 255, 255, 255};
                interpreter->values[idx].type = VAL_COLOR;
                interpreter->values[idx].isVariable = isVariable;
                interpreter->values[idx].name = srcNode->name;
                break;
            case PIN_SPRITE:
                interpreter->values[idx].sprite = (Sprite){0};
                interpreter->values[idx].type = VAL_SPRITE;
                interpreter->values[idx].isVariable = isVariable;
                interpreter->values[idx].name = srcNode->name;
                break;
            default:
                break;
            }

            pin->valueIndex = idx;
            interpreter->valueCount++;
        }
    }

    for (int i = 0; i < graph->nodeCount; i++)
    {
        RuntimeNode *node = &runtime.nodes[i];
        Node *srcNode = &graph->nodes[i];

        bool valueFound = false;

        switch (graph->nodes[i].type)
        {
        case NODE_GET_VAR:
            for (int j = 0; j < graph->nodeCount; j++)
            {
                if (j != i && strcmp(graph->variables[runtime.nodes[i].inputPins[1]->pickedOption], graph->nodes[j].name) == 0)
                {
                    node->outputPins[1]->valueIndex = runtime.nodes[j].outputPins[1]->valueIndex;
                    valueFound = true;
                    break;
                }
            }
            if (!valueFound)
            {
                node->outputPins[1]->valueIndex = 0;
            }
            continue;
        case NODE_SET_VAR:
            for (int j = 0; j < graph->nodeCount; j++)
            {
                if (j != i && strcmp(graph->variables[runtime.nodes[i].inputPins[1]->pickedOption], graph->nodes[j].name) == 0)
                {
                    node->outputPins[1]->valueIndex = runtime.nodes[j].outputPins[1]->valueIndex;
                    valueFound = true;
                    break;
                }
            }
            if (!valueFound)
            {
                node->outputPins[1]->valueIndex = 0;
            }
            continue;
        default:
            continue;
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

        inputPin->valueIndex = outputPin->valueIndex;

        if (inputPin->type == PIN_FLOW && outputPin->type == PIN_FLOW)
        {
            outputPin->nextNodeIndex = inputPin->nodeIndex;
        }
    }

    for (int i = 0; i < graph->nodeCount; i++)
    {
        RuntimeNode *node = &runtime.nodes[i];

        switch (node->type)
        {
        case NODE_PROP_TEXTURE:
            continue;
        case NODE_PROP_RECTANGLE:
            interpreter->components[interpreter->componentCount].isSprite = false;
            interpreter->components[interpreter->componentCount].prop.propType = PROP_RECTANGLE;
            interpreter->components[interpreter->componentCount].prop.width = interpreter->values[node->inputPins[3]->valueIndex].number;
            interpreter->components[interpreter->componentCount].prop.height = interpreter->values[node->inputPins[4]->valueIndex].number;
            interpreter->components[interpreter->componentCount].prop.position.x = interpreter->values[node->inputPins[1]->valueIndex].number - interpreter->components[interpreter->componentCount].prop.width / 2;
            interpreter->components[interpreter->componentCount].prop.position.y = interpreter->values[node->inputPins[2]->valueIndex].number - interpreter->components[interpreter->componentCount].prop.height / 2;
            interpreter->components[interpreter->componentCount].prop.color = interpreter->values[node->inputPins[5]->valueIndex].color;
            interpreter->components[interpreter->componentCount].prop.layer = interpreter->values[node->inputPins[6]->valueIndex].number;
            interpreter->componentCount++;
            continue;
        case NODE_PROP_CIRCLE:
            interpreter->components[interpreter->componentCount].isSprite = false;
            interpreter->components[interpreter->componentCount].prop.propType = PROP_CIRCLE;
            interpreter->components[interpreter->componentCount].prop.position.x = interpreter->values[node->inputPins[1]->valueIndex].number;
            interpreter->components[interpreter->componentCount].prop.position.y = interpreter->values[node->inputPins[2]->valueIndex].number;
            interpreter->components[interpreter->componentCount].prop.radius = interpreter->values[node->inputPins[3]->valueIndex].number;
            interpreter->components[interpreter->componentCount].prop.color = interpreter->values[node->inputPins[4]->valueIndex].color;
            interpreter->components[interpreter->componentCount].prop.layer = interpreter->values[node->inputPins[5]->valueIndex].number;
            interpreter->componentCount++;
            continue;
        default:
            break;
        }
    }

    return runtime;
}

void InterpretStringOfNodes(int lastNodeIndex, InterpreterContext *interpreter, RuntimeGraphContext *graph, int outFlowPinIndexInNode)
{
    if (lastNodeIndex < 0 || lastNodeIndex >= graph->nodeCount)
        return;

    if (graph->nodes[lastNodeIndex].outputCount == 0)
        return;

    if (graph->nodes[lastNodeIndex].outputPins[outFlowPinIndexInNode]->nextNodeIndex == -1)
        return;
    int currNodeIndex = graph->nodes[lastNodeIndex].outputPins[outFlowPinIndexInNode]->nextNodeIndex;
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
        if (graph->nodes[currNodeIndex].inputPins[1]->valueIndex != -1)
        {
            interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].number = interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].number;
        }
        break;
    }

    case NODE_STRING:
    {
        if (graph->nodes[currNodeIndex].inputPins[1]->valueIndex != -1)
        {
            interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].string = interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].string;
        }
        break;
    }

    case NODE_BOOL:
    {
        if (graph->nodes[currNodeIndex].inputPins[1]->valueIndex != -1)
        {
            interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].boolean = interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].boolean;
        }
        break;
    }

    case NODE_COLOR:
    {
        if (graph->nodes[currNodeIndex].inputPins[1]->valueIndex != -1)
        {
            interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].color = interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].color;
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
        if (graph->nodes[currNodeIndex].outputPins[1]->valueIndex == -1)
        {
            break;
        }
        Value *valToSet = &interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex];
        Value newValue = interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex];
        switch (valToSet->type)
        {
        case VAL_NUMBER:
            valToSet->number = newValue.number;
            break;
        case VAL_STRING:
            valToSet->string = newValue.string;
            break;
        case VAL_BOOL:
            valToSet->boolean = newValue.boolean;
            break;
        case VAL_COLOR:
            valToSet->color = newValue.color;
            break;
        case VAL_SPRITE:
            valToSet->sprite = newValue.sprite;
            break;
        default:
            break;
        }
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
        if (interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].boolean)
        {
            InterpretStringOfNodes(currNodeIndex, interpreter, graph, 0);
        }
        else
        {
            InterpretStringOfNodes(currNodeIndex, interpreter, graph, 1);
        }
        return;
        break;
    }

    case NODE_LOOP:
    {
        int steps = 1000;
        while (interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].boolean)
        {
            if (steps == 0)
            {
                if (interpreter->isInfiniteLoopProtectionOn)
                {
                    AddToLogFromInterpreter(interpreter, (Value){.type = VAL_STRING, .string = "Possible infinite loop detected and exited! To turn off infinite loop protection [BLANK]"}, 2);
                    break;
                }
                else
                {
                    AddToLogFromInterpreter(interpreter, (Value){.type = VAL_STRING, .string = "Possible infinite loop detected! Infinite loop protection is off!"}, 2);
                }
            }
            else
            {
                steps--;
            }
            InterpretStringOfNodes(currNodeIndex, interpreter, graph, 1);
        }
        break;
    }

    case NODE_COMPARISON:
    {
        float numA = interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].number;
        float numB = interpreter->values[graph->nodes[currNodeIndex].inputPins[3]->valueIndex].number;
        bool *result = &interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].boolean;
        switch (graph->nodes[currNodeIndex].inputPins[1]->pickedOption)
        {
        case EQUAL_TO:
            *result = numA == numB;
            break;
        case GREATER_THAN:
            *result = numA > numB;
            break;
        case LESS_THAN:
            *result = numA < numB;
            break;
        default:
            // Error
            break;
        }
        break;
    }

    case NODE_GATE:
    {
        bool boolA = interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].boolean;
        bool boolB = interpreter->values[graph->nodes[currNodeIndex].inputPins[3]->valueIndex].boolean;
        bool *result = &interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].boolean;
        switch (graph->nodes[currNodeIndex].inputPins[1]->pickedOption)
        {
        case AND:
            *result = boolA && boolB;
            break;
        case OR:
            *result = boolA || boolB;
            break;
        case NOT:
            *result = !boolA;
            break;
        case XOR:
            *result = boolA != boolB;
            break;
        case NAND:
            *result = !(boolA && boolB);
            break;
        case NOR:
            *result = !(boolA || boolB);
            break;
        default:
            // Error
            break;
        }
        break;
    }

    case NODE_ARITHMETIC:
    {
        float numA = interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].number;
        float numB = interpreter->values[graph->nodes[currNodeIndex].inputPins[3]->valueIndex].number;
        float *result = &interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].number;
        switch (graph->nodes[currNodeIndex].inputPins[1]->pickedOption)
        {
        case ADD:
            *result = numA + numB;
            break;
        case SUBTRACT:
            *result = numA - numB;
            break;
        case MULTIPLY:
            *result = numA * numB;
            break;
        case DIVIDE:
            *result = numA / numB;
            break;
        case MODULO:
            *result = (int)numA % (int)numB;
            break;
        default:
            // Error
            break;
        }
        break;
    }

    case NODE_PROP_TEXTURE:
    {
        break;
    }

    case NODE_PROP_RECTANGLE:
    {
        break;
    }

    case NODE_PROP_CIRCLE:
    {
        break;
    }

    case NODE_PRINT:
    {
        if (graph->nodes[currNodeIndex].inputPins[1]->valueIndex != -1)
        {
            AddToLogFromInterpreter(interpreter, interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex], 4);
        }
        break;
    }

    case NODE_DRAW_LINE:
    {
        break;
    }
    }

    if (currNodeIndex != lastNodeIndex)
    {
        InterpretStringOfNodes(currNodeIndex, interpreter, graph, 0);
    }
}

void DrawComponents(InterpreterContext *interpreter)
{
    for (int i = 0; i < interpreter->componentCount; i++)
    {
        SceneComponent component = interpreter->components[i];
        if (component.isSprite)
        {
            continue; //
        }
        else
        {
            switch (component.prop.propType)
            {
            case PROP_TEXTURE:
                break; //
            case PROP_RECTANGLE:
                DrawRectangle(component.prop.position.x, component.prop.position.y, component.prop.width, component.prop.height, component.prop.color);
                break;
            case PROP_CIRCLE:
                DrawCircle(component.prop.position.x, component.prop.position.y, component.prop.radius, component.prop.color);
                break;
            default:
                AddToLogFromInterpreter(interpreter, (Value){.type = VAL_STRING, .string = "Component error!"}, 2);
            }
        }
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
                InterpretStringOfNodes(i, interpreter, graph, 0);
                break;
            case NODE_EVENT_LOOP_TICK:
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
        InterpretStringOfNodes(interpreter->loopNodeIndex, interpreter, graph, 0);
    }

    DrawComponents(interpreter);

    return true;
}