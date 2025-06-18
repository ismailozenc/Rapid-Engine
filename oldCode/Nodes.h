#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"

#define TYPE_SIZE 16

#define ATTRIBUTES_SUB_ID_STARTING_POS 2
#define PREV_SUB_ID 0
#define NEXT_SUB_ID 1

typedef struct Node{
    int id;
    char type[TYPE_SIZE];
    Vector2 position;

    int prev;
    int next;

    char *input[16];
    char *output[16];
}Node;

int GetInputCountByType(char type[TYPE_SIZE]){
    if(strcmp(type, "num") == 0){
        return 1;
    }
    if(strcmp(type, "string") == 0){
        return 1;
    }
    if(strcmp(type, "ex") == 0){
        return 5;
    }////////////
    else{
        return -1;
    }
}

int GetOutputCountByType(char type[TYPE_SIZE]){
    if(strcmp(type, "num") == 0){
        return 0;
    }
    if(strcmp(type, "string") == 0){
        return 0;
    }
    if(strcmp(type, "ex") == 0){
        return 5;
    }////////////
    else{
        return -1;
    }
}

int GetWidthByType(char type[TYPE_SIZE]){
    if(strcmp(type, "num") == 0){
        return 120;
    }
    if(strcmp(type, "string") == 0){
        return 120;
    }
    if(strcmp(type, "ex") == 0){
        return 240;
    }////////////
    else{
        return -1;
    }
}

int GetHeightByType(char type[TYPE_SIZE]){
    if(strcmp(type, "num") == 0){
        return 100;
    }
    if(strcmp(type, "string") == 0){
        return 100;
    }
    if(strcmp(type, "ex") == 0){
        return 200;
    }////////////
    else{
        return -1;
    }
}

//CheckIfTypeIsKnown()

int SaveNode(char CGFilePath[], Node *node){
    FILE *file = fopen(CGFilePath, "ab");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    fwrite(node->type, sizeof(char), 16, file);
    fwrite(&node->id, sizeof(int), 1, file);
    fwrite(&node->position, sizeof(Vector2), 1, file);

    fwrite(&node->prev, sizeof(int), 1, file);
    fwrite(&node->next, sizeof(int), 1, file);

    for(int i = 0; i < GetInputCountByType(node->type); i++){
        fwrite(node->input[i], sizeof(char), 16, file);
    }

    fwrite("in", sizeof(char), 2, file);

    for(int i = 0; i < GetOutputCountByType(node->type); i++){
        fwrite(node->output[i], sizeof(char), 16, file);
    }

    fwrite("out", sizeof(char), 3, file);

    fclose(file);

    return 0;
}

int OverrideFileWithList(char CGFilePath[], Node *NodeList, size_t nodeCount, int lastNodeID){
    FILE *file = fopen(CGFilePath, "wb");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    fwrite(&lastNodeID, sizeof(int), 1, file);

    fclose(file);

    for (size_t i = 0; i < nodeCount; i++) {
        if(SaveNode(CGFilePath, &NodeList[i]) == 1){
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

    node.prev = -1;
    node.next = -1;

    for (int i = 0; i < GetInputCountByType(node.type); i++)
    {
        node.input[i] = strdup("-1");
    }

    for (int i = 0; i < GetOutputCountByType(node.type); i++)
    {
        node.output[i] = strdup("-1");
    }

    *NodeList = realloc(*NodeList, (*nodeCount + 1) * sizeof(Node));
    if (!*NodeList)
    {
        perror("Failed to allocate memory for NodeList");
        exit(1);
    }

    (*NodeList)[*nodeCount] = node;
    (*nodeCount)++;
}

int GetLinkedNodeIndex(Node *NodeList, int nodeCount, char *ID)
{

    char temp[5];
    int result;

    for (int i = 0; i < nodeCount; i++)
    {

        strncpy(temp, ID, 4);
        temp[4] = '\0';

        result = atoi(temp);

        if (NodeList[i].id == result)
        {
            return i;
        }
    }

    return -1;
}

void RemoveNode(Node **NodeList, size_t *nodeCount, int index) {
    if (!NodeList || !*NodeList || !nodeCount || index < 0 || index >= (int)(*nodeCount)) return;

    for(int i = 0; i < *nodeCount; i++){
        if((*NodeList)[index].next != -1 && (*NodeList)[i].id == (*NodeList)[index].next / 100){
            (*NodeList)[i].prev = -1;
        }

        if((*NodeList)[index].prev != -1 && (*NodeList)[i].id == (*NodeList)[index].prev / 100){
            (*NodeList)[i].next = -1;
        }
    }

    int inputCount = GetInputCountByType((*NodeList)[index].type);

    for(int i = 0; i < inputCount; i++){
        for(int j = 0; j < *nodeCount; j++){
            if((*NodeList)[i].id == atoi((*NodeList)[index].input[j]) / 100){
                if(atoi((*NodeList)[index].input[j]) % 100 + ATTRIBUTES_SUB_ID_STARTING_POS < inputCount){
                    free((*NodeList)[i].input[atoi((*NodeList)[index].input[j]) % 100 - ATTRIBUTES_SUB_ID_STARTING_POS]);
                    (*NodeList)[i].input[atoi((*NodeList)[index].input[j]) % 100 - ATTRIBUTES_SUB_ID_STARTING_POS] = strdup("-1");
                }
                else if(atoi((*NodeList)[index].input[j]) % 100 + ATTRIBUTES_SUB_ID_STARTING_POS >= inputCount){
                    free((*NodeList)[i].output[atoi((*NodeList)[index].input[j]) % 100 - inputCount - ATTRIBUTES_SUB_ID_STARTING_POS]);
                    (*NodeList)[i].output[atoi((*NodeList)[index].input[j]) % 100 - inputCount - ATTRIBUTES_SUB_ID_STARTING_POS] = strdup("-1");
                }
            }
        }
    }

    for (int i = index; i < *nodeCount - 1; i++) {
        (*NodeList)[i] = (*NodeList)[i + 1];
    }

    (*nodeCount)--;
    if (*nodeCount == 0) {
        free(*NodeList);
        *NodeList = NULL;
    } else {
        Node *newList = realloc(*NodeList, (*nodeCount) * sizeof(Node));
        if (newList) {
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

        fread(&tempNode.prev, sizeof(int), 1, file);
        fread(&tempNode.next, sizeof(int), 1, file);

        if (GetInputCountByType(tempNode.type) == -1 || GetOutputCountByType(tempNode.type) == -1)
        {
            printf("\nGraverror %s, %d, %d, %f", tempNode.type, GetInputCountByType(tempNode.type), GetOutputCountByType(tempNode.type), tempNode.position.x);
            return 2;
        }

        char inputEnd[3];
        char outputEnd[4];

        for (int i = 0; i < GetInputCountByType(tempNode.type); i++)
        {
            tempNode.input[i] = malloc(16);
            fread(tempNode.input[i], sizeof(char), 16, file);
        }

        fread(inputEnd, sizeof(char), 2, file);
        inputEnd[2] = '\0';
        if (strcmp(inputEnd, "in") != 0)
        {
            return 2;
        }

        for (int i = 0; i < GetOutputCountByType(tempNode.type); i++)
        {
            tempNode.output[i] = malloc(16);
            fread(tempNode.output[i], sizeof(char), 16, file);
        }

        fread(outputEnd, sizeof(char), 3, file);
        outputEnd[3] = '\0';
        if (strcmp(outputEnd, "out") != 0)
        {
            return 2;
        }

        *NodeList = realloc(*NodeList, (*nodeCount + 1) * sizeof(Node));
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

int MakeConnection(int Attribute1ID, int Attribute2ID, Node *NodeList, size_t nodeCount)
{

    int Node1Index = -1;
    int Node2Index = -1;

    for (int i = 0; i < nodeCount; i++)
    {
        if (NodeList[i].id == Attribute1ID / 100)
        {
            Node1Index = i;
        }
        if (NodeList[i].id == Attribute2ID / 100)
        {
            Node2Index = i;
        }
    }

    int successfullyChangedAttribute1 = 1;
    int successfullyChangedAttribute2 = 1;

    if(Attribute1ID % 100 == PREV_SUB_ID && Attribute2ID % 100 == NEXT_SUB_ID){
        NodeList[Node1Index].prev = Attribute2ID;
        successfullyChangedAttribute1 = 0;
    }
    else if(Attribute1ID % 100 == NEXT_SUB_ID && Attribute2ID % 100 == PREV_SUB_ID){
        NodeList[Node1Index].next = Attribute2ID;
        successfullyChangedAttribute1 = 0;
    }

    if(Attribute2ID % 100 == PREV_SUB_ID && Attribute1ID % 100 == NEXT_SUB_ID){
        NodeList[Node2Index].prev = Attribute1ID;
        successfullyChangedAttribute2 = 0;
    }
    else if(Attribute2ID % 100 == NEXT_SUB_ID && Attribute1ID % 100 == PREV_SUB_ID){
        NodeList[Node2Index].next = Attribute1ID;
        successfullyChangedAttribute2 = 0;
    }

    if(successfullyChangedAttribute1 + successfullyChangedAttribute2 == 0){
        return 0;
    }

    if (Node1Index == Node2Index)
    {
        return 1;
    }
    else if (Attribute1ID % 100 - ATTRIBUTES_SUB_ID_STARTING_POS < GetInputCountByType(NodeList[Node1Index].type) && Attribute2ID % 100 - ATTRIBUTES_SUB_ID_STARTING_POS < GetInputCountByType(NodeList[Node2Index].type))
    {
        return 1;
    }
    else if (Attribute1ID % 100 - ATTRIBUTES_SUB_ID_STARTING_POS >= GetInputCountByType(NodeList[Node1Index].type) && Attribute2ID % 100 - ATTRIBUTES_SUB_ID_STARTING_POS >= GetInputCountByType(NodeList[Node2Index].type))
    {
        return 1;
    }

    char str[20];

    if (Attribute1ID % 100 - ATTRIBUTES_SUB_ID_STARTING_POS < GetInputCountByType(NodeList[Node1Index].type))
    {
        sprintf(str, "%d", Attribute2ID);
        NodeList[Node1Index].input[Attribute1ID % 100 - ATTRIBUTES_SUB_ID_STARTING_POS] = strdup(str);
        successfullyChangedAttribute1 = 0;
    }
    else if (Attribute1ID % 100 - ATTRIBUTES_SUB_ID_STARTING_POS >= GetInputCountByType(NodeList[Node1Index].type))
    {
        sprintf(str, "%d", Attribute2ID);
        NodeList[Node1Index].output[Attribute1ID % 100 - GetInputCountByType(NodeList[Node1Index].type) - ATTRIBUTES_SUB_ID_STARTING_POS] = strdup(str);
        successfullyChangedAttribute1 = 0;
    }

    if (Attribute2ID % 100 - ATTRIBUTES_SUB_ID_STARTING_POS < GetInputCountByType(NodeList[Node2Index].type))
    {
        sprintf(str, "%d", Attribute1ID);
        NodeList[Node2Index].input[Attribute2ID % 100 - ATTRIBUTES_SUB_ID_STARTING_POS] = strdup(str);
        successfullyChangedAttribute2 = 0;
    }
    else if (Attribute2ID % 100 - ATTRIBUTES_SUB_ID_STARTING_POS >= GetInputCountByType(NodeList[Node2Index].type))
    {
        sprintf(str, "%d", Attribute1ID);
        NodeList[Node2Index].output[Attribute2ID % 100 - GetInputCountByType(NodeList[Node2Index].type) - ATTRIBUTES_SUB_ID_STARTING_POS] = strdup(str);
        successfullyChangedAttribute2 = 0;
    }

    return successfullyChangedAttribute1 + successfullyChangedAttribute2;
}

void TestClearFile(char CGFilePath[]){
    FILE *file = fopen(CGFilePath, "wb");
    if (!file) {
        perror("Failed to open file");
    }

    int value = 1000;
    fwrite(&value, sizeof(int), 1, file);

    fclose(file);
}