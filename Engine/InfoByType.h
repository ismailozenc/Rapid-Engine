#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TYPE_SIZE 16
#define typesCount 3

typedef struct InfoByType{
    char *type;

    int inputCount;
    int outputCount;

    int width;
    int height;
}InfoByType;

static InfoByType NodeInfoByType[] = {
    {"num", 1, 0, 120, 100},
    {"string", 1, 0, 120, 100},
    {"ex", 5, 5, 240, 200}
};

int getNodeInfoByType(char *type, char *info){
    for(int i = 0; i < typesCount; i++){
        if(strcmp(type, NodeInfoByType[i].type) == 0){
            if(strcmp(info, "inputCount") == 0){
                return NodeInfoByType[i].inputCount;
            }
            else if(strcmp(info, "outputCount") == 0){
                return NodeInfoByType[i].outputCount;
            }
            else if(strcmp(info, "width") == 0){
                return NodeInfoByType[i].width;
            }
            else if(strcmp(info, "height") == 0){
                return NodeInfoByType[i].height;
            }
        }
    }

    return -1;
}