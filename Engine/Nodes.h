#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "InfoByType.h"

#define TYPE_SIZE 16

typedef struct Node
{
    int id;
    char type[TYPE_SIZE];
    Vector2 position;

    int next;

    int input[16];
} Node;

typedef struct Pin
{
    int nodeID;
    int nodeIndexInList;
    int number;
    bool isFlow;
    bool isInput;
    Vector2 position;
} Pin;

#define INVALID_PIN \
    (Pin) { -1, -1, -1, false, false }

int SaveNode(char CGFilePath[], Node *node)
{
    FILE *file = fopen(CGFilePath, "ab");
    if (!file)
    {
        perror("Failed to open file");
        return 1;
    }

    fwrite(node->type, sizeof(char), 16, file);
    fwrite(&node->id, sizeof(int), 1, file);
    fwrite(&node->position, sizeof(Vector2), 1, file);

    fwrite(&node->next, sizeof(int), 1, file);

    for (int i = 0; i < getNodeInfoByType(node->type, "inputCount"); i++)
    {
        fwrite(&node->input[i], sizeof(int), 1, file);
    }

    fclose(file);

    return 0;
}

int OverrideFileWithList(char CGFilePath[], Node *NodeList, size_t nodeCount, int lastNodeID)
{
    FILE *file = fopen(CGFilePath, "wb");
    if (!file)
    {
        perror("Failed to open file");
        return 1;
    }

    fwrite(&lastNodeID, sizeof(int), 1, file);

    fclose(file);

    for (size_t i = 0; i < nodeCount; i++)
    {
        if (SaveNode(CGFilePath, &NodeList[i]) == 1)
        {
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

    node.next = -1;

    for (int i = 0; i < getNodeInfoByType(node.type, "inputCount"); i++)
    {
        node.input[i] = -1;
    }

    *NodeList = (Node *)realloc(*NodeList, (*nodeCount + 1) * sizeof(Node));
    if (!*NodeList)
    {
        perror("Failed to allocate memory for NodeList");
        exit(1);
    }

    (*NodeList)[*nodeCount] = node;
    (*nodeCount)++;
}

int GetLinkedNodeIndex(Node *NodeList, int nodeCount, int ID)
{
    for (int i = 0; i < nodeCount; i++)
    {
        if (ID > 10000 && NodeList[i].id == ID / 100)
        {
            return i;
        }
        else if (ID < 10000 && NodeList[i].id == ID)
        {
            return i;
        }
    }

    return -1;
}

void RemoveNode(Node **NodeList, size_t *nodeCount, int index)
{
    if (!NodeList || !*NodeList || !nodeCount || index < 0 || index >= (int)(*nodeCount))
        return;

    for (int i = 0; i < *nodeCount; i++)
    {
        if ((*NodeList)[i].next == (*NodeList)[index].id)
        {
            (*NodeList)[i].next = -1;
        }

        for (int inputPin = 0; inputPin < getNodeInfoByType((*NodeList)[i].type, "inputCount"); inputPin++)
        {
            if ((*NodeList)[i].input[inputPin] / 100 == (*NodeList)[index].id)
            {
                (*NodeList)[i].input[inputPin] = -1;
            }
        }
    }

    for (int i = index; i < *nodeCount - 1; i++)
    {
        (*NodeList)[i] = (*NodeList)[i + 1];
    }

    (*nodeCount)--;
    if (*nodeCount == 0)
    {
        free(*NodeList);
        *NodeList = NULL;
    }
    else
    {
        Node *newList = realloc(*NodeList, (*nodeCount) * sizeof(Node));
        if (newList || *nodeCount == 0)
        {
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

        fread(&tempNode.next, sizeof(int), 1, file);

        if (getNodeInfoByType(tempNode.type, "inputCount") == -1) //should be CheckIfTypeExistsFunction
        {
            printf("\nCorrupted file");
            return 2;
        }

        for (int i = 0; i < getNodeInfoByType(tempNode.type, "inputCount"); i++)
        {
            fread(&tempNode.input[i], sizeof(int), 1, file);
        }

        *NodeList = (Node *)realloc(*NodeList, (*nodeCount + 1) * sizeof(Node));
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

bool MakeConnection(Pin Pin1, Pin Pin2, Node *NodeList, size_t nodeCount)
{
    if (Pin1.isFlow != Pin2.isFlow || Pin1.isInput == Pin2.isInput || Pin1.nodeID == Pin2.nodeID)
    {
        return false;
    }

    if (Pin1.isFlow && Pin2.isFlow)
    {
        if (Pin1.isInput == false)
        {
            NodeList[Pin1.nodeIndexInList].next = Pin2.nodeID;
            return true;
        }
        else if (Pin2.isInput == false)
        {
            NodeList[Pin2.nodeIndexInList].next = Pin1.nodeID;
            return true;
        }
        return false;
    }
    else
    {
        if (Pin1.isInput)
        {
            NodeList[Pin1.nodeIndexInList].input[Pin1.number] = Pin2.nodeID * 100 + Pin2.number;
            return true;
        }
        else if (Pin2.isInput)
        {
            NodeList[Pin2.nodeIndexInList].input[Pin2.number] = Pin1.nodeID * 100 + Pin1.number;
            return true;
        }
        return false;
    }
}

void TestClearFile(char CGFilePath[])
{
    FILE *file = fopen(CGFilePath, "wb");
    if (!file)
    {
        perror("Failed to open file");
    }

    int value = 1000;
    fwrite(&value, sizeof(int), 1, file);

    fclose(file);
}

// CheckIfTypeIsKnown()