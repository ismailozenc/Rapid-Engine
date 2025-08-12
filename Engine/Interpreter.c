#include "Interpreter.h"

#define MAX_FORCES 99

InterpreterContext InitInterpreterContext()
{
    InterpreterContext interpreter = {0};

    interpreter.valueCount = 0;
    interpreter.loopNodeIndex = -1;

    interpreter.isFirstFrame = true;

    interpreter.newLogMessage = false;

    interpreter.buildFailed = false;

    interpreter.isInfiniteLoopProtectionOn = true;

    interpreter.backgroundColor = (Color){0, 0, 0, 255};

    return interpreter;
}

void FreeInterpreterContext(InterpreterContext *interpreter)
{
    free(interpreter->values);
    interpreter->values = NULL;
    interpreter->valueCount = 0;

    free(interpreter->onButtonNodeIndexes);
    interpreter->onButtonNodeIndexes = NULL;
    interpreter->onButtonNodeIndexesCount = 0;

    free(interpreter->forces);
    interpreter->forces = NULL;
    interpreter->forces = 0;

    interpreter->backgroundColor = (Color){0, 0, 0, 255};
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
        sprintf(temp, "%.2f", value.number);
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
        sprintf(temp, "%s, PosX: %.0f, PosY: %.0f, Rotation: %.2f", value.sprite.isVisible ? "Visible" : "Not visible", value.sprite.position.x, value.sprite.position.y, value.sprite.rotation);
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
        sprintf(str, "%.3f", message.number);
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
    case 5:
        sprintf(str, "Name: %s, Pos X: %.3f, Pos Y: %.3f, Width: %d, Height: %d, Rotation: %.3f, Layer: %d", message.name, message.sprite.position.x, message.sprite.position.y, message.sprite.width, message.sprite.height, message.sprite.rotation, message.sprite.layer);
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

void UpdateSpecialValues(InterpreterContext *interpreter, Vector2 mousePos){
    interpreter->values[SPECIAL_VALUE_MOUSE_X].number = mousePos.x;
    interpreter->values[SPECIAL_VALUE_MOUSE_Y].number = mousePos.y;
    interpreter->values[SPECIAL_VALUE_SCREEN_WIDTH].number = GetScreenWidth();
    interpreter->values[SPECIAL_VALUE_SCREEN_HEIGHT].number = GetScreenHeight();
    //
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
        dst->componentIndex = -1;
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

        if (node->type == NODE_DRAW_PROP_TEXTURE || node->type == NODE_DRAW_PROP_RECTANGLE || node->type == NODE_DRAW_PROP_CIRCLE || node->type == NODE_CREATE_SPRITE)
        {
            totalComponents++;
        }
    }

    interpreter->values = calloc(totalOutputPins + SPECIAL_VALUES_COUNT, sizeof(Value));
    interpreter->values[SPECIAL_VALUE_ERROR] = (Value){.type = VAL_STRING, .string = "Error value", .name = "Error value"};
    interpreter->values[SPECIAL_VALUE_MOUSE_X] = (Value){.type = VAL_NUMBER, .number = 0, .name = "Mouse X"};
    interpreter->values[SPECIAL_VALUE_MOUSE_Y] = (Value){.type = VAL_NUMBER, .number = 0, .name = "Mouse Y"};
    interpreter->values[SPECIAL_VALUE_SCREEN_WIDTH] = (Value){.type = VAL_NUMBER, .number = 0, .name = "Screen Width"};
    interpreter->values[SPECIAL_VALUE_SCREEN_HEIGHT] = (Value){.type = VAL_NUMBER, .number = 0, .name = "Screen Height"};
    interpreter->valueCount = SPECIAL_VALUES_COUNT;

    interpreter->components = malloc(sizeof(SceneComponent) * (totalComponents + 1));
    interpreter->componentCount = 0;

    interpreter->varIndexes = malloc(sizeof(int) * (totalOutputPins + 1));
    interpreter->varCount = 0;

    interpreter->forces = calloc(MAX_FORCES, sizeof(Force));
    interpreter->forcesCount = 0;

    for (int i = 0; i < graph->nodeCount; i++)
    {
        RuntimeNode *node = &runtime.nodes[i];
        Node *srcNode = &graph->nodes[i];

        switch (node->type)
        {
        case NODE_LITERAL_NUMBER:
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
            if (node->type == NODE_CREATE_NUMBER || node->type == NODE_CREATE_STRING || node->type == NODE_CREATE_SPRITE || node->type == NODE_CREATE_BOOL || node->type == NODE_CREATE_COLOR)
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

            interpreter->values[idx].componentIndex = -1;

            if (isVariable)
            {
                interpreter->varIndexes[interpreter->varCount] = idx;
                interpreter->varCount++;
            }

            pin->valueIndex = idx;
            interpreter->valueCount++;
        }
    }

    interpreter->varIndexes = realloc(interpreter->varIndexes, sizeof(int) * (interpreter->varCount + 1));

    for (int i = 0; i < graph->nodeCount; i++)
    {
        RuntimeNode *node = &runtime.nodes[i];
        Node *srcNode = &graph->nodes[i];

        bool valueFound = false;

        switch (graph->nodes[i].type)
        {
        case NODE_GET_VARIABLE:
            for (int j = 0; j < graph->nodeCount; j++)
            {
                if (j != i && strcmp(graph->variables[runtime.nodes[i].inputPins[0]->pickedOption], graph->nodes[j].name) == 0)
                {
                    node->outputPins[0]->valueIndex = runtime.nodes[j].outputPins[1]->valueIndex;
                    valueFound = true;
                    break;
                }
            }
            if (!valueFound)
            {
                node->outputPins[0]->valueIndex = 0;
            }
            continue;
        case NODE_SET_VARIABLE:
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
        case NODE_GET_SCREEN_WIDTH:
            node->outputPins[0]->valueIndex = SPECIAL_VALUE_SCREEN_WIDTH;
            continue;
        case NODE_GET_SCREEN_HEIGHT:
            node->outputPins[0]->valueIndex = SPECIAL_VALUE_SCREEN_HEIGHT;
            continue;
        case NODE_GET_MOUSE_X:
            node->outputPins[0]->valueIndex = SPECIAL_VALUE_MOUSE_X;
            continue;
        case NODE_GET_MOUSE_Y:
            node->outputPins[0]->valueIndex = SPECIAL_VALUE_MOUSE_Y;
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
        case NODE_CREATE_SPRITE:
            interpreter->components[interpreter->componentCount].isSprite = true;
            interpreter->components[interpreter->componentCount].isVisible = false;
            int fileIndex = node->inputPins[1]->valueIndex;
            if (fileIndex != -1 && interpreter->values[fileIndex].string && interpreter->values[fileIndex].string[0])
            {
                char path[512];
                snprintf(path, sizeof(path), "%s%c%s", interpreter->projectPath, PATH_SEPARATOR, interpreter->values[fileIndex].string);
                Texture2D tex = LoadTexture(path);
                if (tex.id == 0)
                {
                    AddToLogFromInterpreter(interpreter, (Value){.type = VAL_STRING, .string = "Failed to load texture"}, 2);
                }
                else
                {
                    interpreter->components[interpreter->componentCount].sprite.texture = tex;
                }
            }
            else
            {
                AddToLogFromInterpreter(interpreter, (Value){.type = VAL_STRING, .string = "Invalid texture input"}, 2);
            }
            interpreter->components[interpreter->componentCount].sprite.width = interpreter->values[node->inputPins[1]->valueIndex].number;
            interpreter->components[interpreter->componentCount].sprite.height = interpreter->values[node->inputPins[2]->valueIndex].number;
            interpreter->components[interpreter->componentCount].sprite.position.x = interpreter->values[node->inputPins[3]->valueIndex].number - interpreter->components[interpreter->componentCount].sprite.width / 2;
            interpreter->components[interpreter->componentCount].sprite.position.y = interpreter->values[node->inputPins[4]->valueIndex].number - interpreter->components[interpreter->componentCount].sprite.height / 2;
            interpreter->components[interpreter->componentCount].sprite.rotation = interpreter->values[node->inputPins[5]->valueIndex].number;
            interpreter->components[interpreter->componentCount].prop.layer = interpreter->values[node->inputPins[6]->valueIndex].number;
            node->outputPins[1]->componentIndex = interpreter->componentCount;
            for (int j = 0; j < runtime.nodeCount; j++)
            {
                for (int k = 0; k < runtime.nodes[j].inputCount; k++)
                {
                    if (runtime.nodes[j].inputPins[k]->type == PIN_SPRITE_VARIABLE && runtime.nodes[j].inputPins[k]->pickedOption != 0)
                    {
                        const char *varPtr = graph->nodes[i].name;
                        const char *valPtr = interpreter->values[interpreter->varIndexes[runtime.nodes[j].inputPins[1]->pickedOption - 1]].name;

                        if (varPtr && valPtr)
                        {
                            char varName[32];
                            char valName[32];
                            strncpy(varName, varPtr, sizeof(varName) - 1);
                            strncpy(valName, valPtr, sizeof(valName) - 1);
                            varName[31] = '\0';
                            valName[31] = '\0';

                            if (strcmp(varName, valName) == 0)
                            {
                                interpreter->values[interpreter->varIndexes[runtime.nodes[j].inputPins[k]->pickedOption - 1]].componentIndex = interpreter->componentCount;
                                runtime.nodes[j].inputPins[k]->valueIndex = interpreter->varIndexes[runtime.nodes[j].inputPins[k]->pickedOption - 1];
                            }
                        }
                    }
                }
            }
            interpreter->componentCount++;
            break;
        case NODE_DRAW_PROP_TEXTURE:
            continue;
        case NODE_DRAW_PROP_RECTANGLE:
            interpreter->components[interpreter->componentCount].isSprite = false;
            interpreter->components[interpreter->componentCount].isVisible = false;
            interpreter->components[interpreter->componentCount].prop.propType = PROP_RECTANGLE;
            interpreter->components[interpreter->componentCount].prop.width = interpreter->values[node->inputPins[3]->valueIndex].number;
            interpreter->components[interpreter->componentCount].prop.height = interpreter->values[node->inputPins[4]->valueIndex].number;
            interpreter->components[interpreter->componentCount].prop.position.x = interpreter->values[node->inputPins[1]->valueIndex].number - interpreter->components[interpreter->componentCount].prop.width / 2;
            interpreter->components[interpreter->componentCount].prop.position.y = interpreter->values[node->inputPins[2]->valueIndex].number - interpreter->components[interpreter->componentCount].prop.height / 2;
            interpreter->components[interpreter->componentCount].prop.color = interpreter->values[node->inputPins[5]->valueIndex].color;
            interpreter->components[interpreter->componentCount].prop.layer = interpreter->values[node->inputPins[6]->valueIndex].number;
            node->outputPins[1]->componentIndex = interpreter->componentCount;
            interpreter->componentCount++;
            continue;
        case NODE_DRAW_PROP_CIRCLE:
            interpreter->components[interpreter->componentCount].isSprite = false;
            interpreter->components[interpreter->componentCount].isVisible = false;
            interpreter->components[interpreter->componentCount].prop.propType = PROP_CIRCLE;
            interpreter->components[interpreter->componentCount].prop.position.x = interpreter->values[node->inputPins[1]->valueIndex].number;
            interpreter->components[interpreter->componentCount].prop.position.y = interpreter->values[node->inputPins[2]->valueIndex].number;
            interpreter->components[interpreter->componentCount].prop.radius = interpreter->values[node->inputPins[3]->valueIndex].number;
            interpreter->components[interpreter->componentCount].prop.color = interpreter->values[node->inputPins[4]->valueIndex].color;
            interpreter->components[interpreter->componentCount].prop.layer = interpreter->values[node->inputPins[5]->valueIndex].number;
            node->outputPins[1]->componentIndex = interpreter->componentCount;
            interpreter->componentCount++;
            continue;
        default:
            break;
        }
    }

    return runtime;
}

bool DoesForceExist(InterpreterContext *interpreter, int id)
{
    for (int i = 0; i < interpreter->forcesCount; i++)
    {
        if (interpreter->forces[i].id == id)
            return true;
    }
    return false;
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

    case NODE_CREATE_NUMBER:
    {
        if (graph->nodes[currNodeIndex].inputPins[1]->valueIndex != -1)
        {
            interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].number = interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].number;
        }
        break;
    }

    case NODE_CREATE_STRING:
    {
        if (graph->nodes[currNodeIndex].inputPins[1]->valueIndex != -1)
        {
            interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].string = interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].string;
        }
        break;
    }

    case NODE_CREATE_BOOL:
    {
        if (graph->nodes[currNodeIndex].inputPins[1]->valueIndex != -1)
        {
            interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].boolean = interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].boolean;
        }
        break;
    }

    case NODE_CREATE_COLOR:
    {
        if (graph->nodes[currNodeIndex].inputPins[1]->valueIndex != -1)
        {
            interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].color = interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].color;
        }
        break;
    }

    case NODE_EVENT_START:
    {
        break;
    }

    case NODE_EVENT_TICK:
    {
        break;
    }

    case NODE_EVENT_ON_BUTTON:
    {
        break;
    }

    case NODE_CREATE_CUSTOM_EVENT:
    {
        break;
    }

    case NODE_CALL_CUSTOM_EVENT:
    {
        break;
    }

    case NODE_GET_VARIABLE:
    {
        break;
    }

    case NODE_GET_SCREEN_WIDTH:
    {
        break;
    }

    case NODE_GET_SCREEN_HEIGHT:
    {
        break;
    }

    case NODE_GET_MOUSE_X:
    {
        interpreter->values[graph->nodes[currNodeIndex].outputPins[0]->valueIndex].number = GetMousePosition().x;
        printf("%d\n", GetMousePosition().x);
        break;
    }

    case NODE_GET_MOUSE_Y:
    {
        break;
    }

    case NODE_GET_RANDOM_NUMBER:
    {
        break;
    }

    case NODE_SET_VARIABLE:
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

    case NODE_SET_BACKGROUND:
    {
        if (graph->nodes[currNodeIndex].inputPins[1]->valueIndex != -1)
        {
            interpreter->backgroundColor = interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].color;
        }
        break;
    }

    case NODE_SET_FPS:
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

    case NODE_CREATE_SPRITE:
    {
        Sprite *sprite = &interpreter->values[graph->nodes[currNodeIndex].outputPins[1]->valueIndex].sprite;
        if (graph->nodes[currNodeIndex].inputPins[2]->valueIndex != -1)
        {
            sprite->position.x = interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].number;
        }
        if (graph->nodes[currNodeIndex].inputPins[3]->valueIndex != -1)
        {
            sprite->position.y = interpreter->values[graph->nodes[currNodeIndex].inputPins[3]->valueIndex].number;
        }
        if (graph->nodes[currNodeIndex].inputPins[4]->valueIndex != -1)
        {
            sprite->width = interpreter->values[graph->nodes[currNodeIndex].inputPins[4]->valueIndex].number;
        }
        if (graph->nodes[currNodeIndex].inputPins[5]->valueIndex != -1)
        {
            sprite->height = interpreter->values[graph->nodes[currNodeIndex].inputPins[5]->valueIndex].number;
        }
        if (graph->nodes[currNodeIndex].inputPins[6]->valueIndex != -1)
        {
            sprite->rotation = interpreter->values[graph->nodes[currNodeIndex].inputPins[6]->valueIndex].number;
        }
        if (graph->nodes[currNodeIndex].inputPins[7]->valueIndex != -1)
        {
            sprite->layer = interpreter->values[graph->nodes[currNodeIndex].inputPins[7]->valueIndex].number;
        }
        Texture2D temp = interpreter->components[graph->nodes[currNodeIndex].outputPins[1]->componentIndex].sprite.texture;
        interpreter->components[graph->nodes[currNodeIndex].outputPins[1]->componentIndex].sprite = *sprite;
        interpreter->components[graph->nodes[currNodeIndex].outputPins[1]->componentIndex].sprite.texture = temp;
        break;
    }

    case NODE_SET_SPRITE_POSITION:
    {
        break;
    }

    case NODE_SET_SPRITE_ROTATION:
    {
        break;
    }

    case NODE_SET_SPRITE_TEXTURE:
    {
        break;
    }

    case NODE_SET_SPRITE_SIZE:
    {
        break;
    }

    case NODE_SPAWN_SPRITE:
    {
        if (interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].componentIndex >= 0 && interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].componentIndex < interpreter->componentCount)
        {
            interpreter->components[interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].componentIndex].isVisible = true;
        }
        break;
    }

    case NODE_DESTROY_SPRITE:
    {
        if (interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].componentIndex >= 0 && interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].componentIndex < interpreter->componentCount)
        {
            interpreter->components[interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].componentIndex].isVisible = false;
        }
        break;
    }

    case NODE_MOVE_TO_SPRITE:
    {
        break;
    }

    case NODE_FORCE_SPRITE:
    {
        SceneComponent *component = &interpreter->components[interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].componentIndex];
        if (interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].componentIndex >= 0 && interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].componentIndex < interpreter->componentCount && !DoesForceExist(interpreter, graph->nodes[currNodeIndex].index))
        {
            interpreter->forces[interpreter->forcesCount].id = graph->nodes[currNodeIndex].index;
            interpreter->forces[interpreter->forcesCount].componentIndex = interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex].componentIndex;
            interpreter->forces[interpreter->forcesCount].pixelsPerSecond = interpreter->values[graph->nodes[currNodeIndex].inputPins[2]->valueIndex].number;
            interpreter->forces[interpreter->forcesCount].angle = interpreter->values[graph->nodes[currNodeIndex].inputPins[3]->valueIndex].number;
            interpreter->forces[interpreter->forcesCount].duration = interpreter->values[graph->nodes[currNodeIndex].inputPins[4]->valueIndex].number;
            interpreter->forcesCount++;
        }
        break;
    }

    case NODE_DRAW_PROP_TEXTURE:
    {
        break;
    }

    case NODE_DRAW_PROP_RECTANGLE:
    {
        interpreter->components[graph->nodes[currNodeIndex].outputPins[1]->componentIndex].isVisible = true;
        break;
    }

    case NODE_DRAW_PROP_CIRCLE:
    {
        interpreter->components[graph->nodes[currNodeIndex].outputPins[1]->componentIndex].isVisible = true;
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
            break;
        }
        break;
    }

    case NODE_PRINT_TO_LOG:
    {
        if (graph->nodes[currNodeIndex].inputPins[1]->valueIndex != -1)
        {
            AddToLogFromInterpreter(interpreter, interpreter->values[graph->nodes[currNodeIndex].inputPins[1]->valueIndex], 4);
        }
        break;
    }

    case NODE_DRAW_DEBUG_LINE:
    {
        break;
    }

    case NODE_LITERAL_NUMBER:
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
        InterpretStringOfNodes(currNodeIndex, interpreter, graph, 0);
    }
}

void DrawComponents(InterpreterContext *interpreter)
{
    ClearBackground(interpreter->backgroundColor);

    for (int i = 0; i < interpreter->componentCount; i++)
    {
        SceneComponent component = interpreter->components[i];
        if (!component.isVisible)
        {
            continue;
        }
        if (component.isSprite)
        {
            DrawTexturePro(
                component.sprite.texture,
                (Rectangle){0, 0, (float)component.sprite.texture.width, (float)component.sprite.texture.height},
                (Rectangle){
                    component.sprite.position.x,
                    component.sprite.position.y,
                    (float)component.sprite.width,
                    (float)component.sprite.height},
                (Vector2){component.sprite.width / 2.0f, component.sprite.height / 2.0f},
                component.sprite.rotation,
                WHITE);
            continue;
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

void HandleForces(InterpreterContext *interpreter)
{
    for (int i = 0; i < interpreter->forcesCount; i++)
    {
        float speed = interpreter->forces[i].pixelsPerSecond;
        float angle = interpreter->forces[i].angle * (PI / 180.0f);
        float vx = cosf(angle) * speed;
        float vy = -sinf(angle) * speed;
        float deltaTime = GetFrameTime();
        interpreter->forces[i].duration -= deltaTime;
        interpreter->components[interpreter->forces[i].componentIndex].sprite.position.x += vx * deltaTime;
        interpreter->components[interpreter->forces[i].componentIndex].sprite.position.y += vy * deltaTime;
        if (interpreter->forces[i].duration <= 0)
        {
            for (int j = i; j < interpreter->forcesCount - 1; j++)
            {
                interpreter->forces[j] = interpreter->forces[j + 1];
            }
            interpreter->forcesCount--;
            i--;
        }
    }
}

bool HandleGameScreen(InterpreterContext *interpreter, RuntimeGraphContext *graph, Vector2 mousePos, Rectangle screenBoundary)
{
    UpdateSpecialValues(interpreter, mousePos);

    if (interpreter->isFirstFrame)
    {
        interpreter->onButtonNodeIndexes = malloc(sizeof(int) * graph->nodeCount);
        for (int i = 0; i < graph->nodeCount; i++)
        {
            switch (graph->nodes[i].type)
            {
            case NODE_EVENT_START:
                InterpretStringOfNodes(i, interpreter, graph, 0);
                break;
            case NODE_EVENT_TICK:
                if (interpreter->loopNodeIndex == -1)
                {
                    interpreter->loopNodeIndex = i;
                }
                break;
            case NODE_EVENT_ON_BUTTON:
                interpreter->onButtonNodeIndexes[interpreter->onButtonNodeIndexesCount++] = i;
                break;
            }
        }
        interpreter->onButtonNodeIndexes = realloc(interpreter->onButtonNodeIndexes, sizeof(int) * interpreter->onButtonNodeIndexesCount);

        interpreter->isFirstFrame = false;
    }
    else
    {
        interpreter->newLogMessage = false;
    }

    for (int i = 0; i < interpreter->onButtonNodeIndexesCount; i++)
    {
        int nodeIndex = interpreter->onButtonNodeIndexes[i];
        KeyboardKey key = graph->nodes[nodeIndex].inputPins[0]->pickedOption;
        KeyAction action = graph->nodes[nodeIndex].inputPins[1]->pickedOption; // You need to store this

        bool triggered = false;
        switch (action)
        {
        case KEY_ACTION_PRESSED:
            triggered = IsKeyPressed(key);
            break;
        case KEY_ACTION_RELEASED:
            triggered = IsKeyReleased(key);
            break;
        case KEY_ACTION_DOWN:
            triggered = IsKeyDown(key);
            break;
        case KEY_ACTION_NOT_DOWN:
            triggered = IsKeyUp(key);
            break;
        }

        if (triggered)
        {
            InterpretStringOfNodes(nodeIndex, interpreter, graph, 0);
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

    HandleForces(interpreter);

    DrawComponents(interpreter);

    for (int i = 0; i < interpreter->valueCount; i++)
    {
        if (interpreter->values[i].type == VAL_SPRITE)
        {
            interpreter->components[interpreter->values[i].componentIndex].sprite.isVisible = interpreter->components[interpreter->values[i].componentIndex].isVisible;
            interpreter->values[i].sprite = interpreter->components[interpreter->values[i].componentIndex].sprite;
        }
    }

    return true;
}