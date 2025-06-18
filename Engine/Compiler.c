#include <stdio.h>

#include "Nodes.h"

int main() {

    Node *NodeList = NULL;
    size_t nodeCount = 0;

    int lastID;

    LoadNodesFromFile(&NodeList, &nodeCount, "C:\\Users\\user\\Desktop\\GameEngine\\Projects\\Snake\\Snake.cg", &lastID);

    for(int i = 0; i < nodeCount; i++){
        if(NodeList[i].type == "start"){
            while(1){
                
            }
        }
    }

    return 0;
}