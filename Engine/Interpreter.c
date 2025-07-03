#include "Interpreter.h"

InterpreterContext InitInterpreterContext(){
    InterpreterContext interpreter = {0};

    interpreter.newLogMessage = false;

    return interpreter;
}

void AddToLogFromInterpreter(InterpreterContext *interpreter, Value message, int level)
{
    char str[32];

    if(message.type == VAL_NUMBER){
        sprintf(str, "%f", message.number);
    }
    else{
        strcpy(str, message.string);
    }

    strncpy(interpreter->logMessage, str, 128 * sizeof(char));
    interpreter->logMessageLevel = level;
    interpreter->newLogMessage = true;
}

int FindPinIndexByID(int id, GraphContext *graph){

    for(int i = 0; i < graph->pinCount; i++){
        if(id == graph->pins[i].id){
            return i;
        }
    }

    return -1;
}

void HandleGameScreen(InterpreterContext *interpreter, GraphContext *graph){
    
    static Value values[100];
    static int valueCount;
    valueCount = 0;
    static int loopNodeIndex = -1;

    static bool setvalue = false;

    for(int i = 0; i < graph->nodeCount; i++){
        switch(graph->nodes[i].type){
            /*case NODE_START:
                printf("a");
                break;
            case NODE_LOOP:
                if(loopNodeIndex == -1){loopNodeIndex = i;}
                break;*/
            case NODE_NUM:
                if(setvalue){valueCount++; break;}
                values[valueCount].type = VAL_NUMBER;
                values[valueCount].number = 5;
                graph->pins[FindPinIndexByID(graph->nodes[i].outputPins[1], graph)].valueIndex = valueCount;
                valueCount++;
                setvalue = true;
                break;
            case NODE_PRINT:
                int pinID = graph->nodes[i].inputPins[1];
                int pinindex = FindPinIndexByID(pinID, graph);//
                AddToLogFromInterpreter(interpreter, values[graph->pins[pinindex].valueIndex], 0);
                break;
        }
    }
}