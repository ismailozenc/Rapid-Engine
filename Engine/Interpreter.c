#include "Interpreter.h"

InterpreterContext InitInterpreterContext()
{
    InterpreterContext interpreter = {0};

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

int GetNodeIndexByID(int id, GraphContext *graph)
{
    for (int i = 0; i < graph->nodeCount; i++)
    {
        if (id == graph->nodes[i].id)
        {
            return i;
        }
    }

    return -1;
}

Link *GetPinLinks(int pinID, GraphContext *graph){
    Link *links = malloc(sizeof(Link) * graph->linkCount);
    if (!links) return NULL;

    int count = 0;

    links[0].inputPinID = -1;

    for(int i = 0; i < graph->linkCount; i++){
        if(graph->links[i].inputPinID == pinID || graph->links[i].outputPinID == pinID){
            links[count] = graph->links[i];
            count++;
            if(count == 100){ //
                return links;
            }
        }
    }

    return links;
}

void InterpretStringOfNodes(int currentNodeIndex, InterpreterContext *interpreter, GraphContext *graph)
{

    int nextNodeIndex = graph->pins[GetPinIndexByID(GetPinLinks(graph->pins[GetPinIndexByID(graph->nodes[currentNodeIndex].outputPins[0], graph)].id, graph)[0].inputPinID, graph)].nodeID;

    AddToLogFromInterpreter(interpreter, (Value){VAL_NUMBER, .number = nextNodeIndex}, 1);

    /*case NODE_NUM:
                if(setvalue){valueCount++; break;}
                values[valueCount].type = VAL_NUMBER;
                values[valueCount].number = 5;
                graph->pins[FindPinIndexByID(graph->nodes[i].outputPins[1], graph)].valueIndex = valueCount;
                valueCount++;
                setvalue = true;
                break;
            case NODE_PRINT:
                int pinID = graph->nodes[i].inputPins[1];
                int pinindex = GetPinIndexByID(pinID, graph);//
                AddToLogFromInterpreter(interpreter, values[graph->pins[pinindex].valueIndex], 0);
                break;*/

    /*if(1 != -1){
        InterpretStringOfNodes(nextNodeIndex, interpreter, graph);
    }*/
}

bool HandleGameScreen(InterpreterContext *interpreter, GraphContext *graph)
{

    static Value values[100];
    static int valueCount;
    valueCount = 0;
    static int loopNodeIndex = -1;

    static bool isFirstFrame;
    isFirstFrame = true;

    if (isFirstFrame)
    {
        for (int i = 0; i < graph->nodeCount; i++)
        {
            switch (graph->nodes[i].type)
            {
            case NODE_EVENT_START:
                InterpretStringOfNodes(i, interpreter, graph);
                break;
            case NODE_EVENT_LOOP:
                if (loopNodeIndex == -1)
                {
                    loopNodeIndex = i;
                }
                break;
            }
        }

        isFirstFrame = false;
    }

    if (loopNodeIndex == -1)
    {
        AddToLogFromInterpreter(interpreter, (Value){VAL_STRING, .string = "No loop node found"}, 2);
        return false;
    }

    return true;
}