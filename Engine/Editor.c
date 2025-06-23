#include <stdio.h>
#include "raylib.h"
#include <direct.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include "shell_execute.h"
#include "Nodes.h"

#define MENU_WIDTH 250
#define MENU_ITEM_HEIGHT 40
#define MENU_VISIBLE_ITEMS 5.5
#define MENU_BORDER_THICKNESS 3
#define SUBMENU_WIDTH 200

#define MAX_PATH_LENGTH 1024
#define MAX_TYPE_LENGTH 16

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

typedef struct
{
    char *CGFilePath;

    int lastNodeID;

    bool isDraggingScreen;
    int draggingNodeIndex;
    Vector2 dragOffset;

    char **logMessages;
    int logCount;
    int logCapacity;

    bool menuOpen;
    bool submenuOpen;
    Vector2 menuPosition;
    Vector2 submenuPosition;

    int screenWidth;
    int screenHeight;
    int bottomBarHeight;
    int scrollIndex;

    Pin lastClickedPin;

    bool isConnecting;

    int hoveredItem;

    bool delayFrames;

    Vector2 mousePos;
    Vector2 prevMousePos;
    Vector2 mousePosAtStartOfDrag;
    Vector2 rightClickPos;

    Font font;

    // float zoom;
} EditorContext;

EditorContext InitEditorContext()
{
    EditorContext EC = {0};

    EC.CGFilePath = malloc(MAX_PATH_LENGTH);
    EC.CGFilePath[0] = '\0';

    EC.lastNodeID = 0;

    EC.lastClickedPin = INVALID_PIN;

    EC.scrollIndex = 0;
    EC.hoveredItem = -1;

    EC.isConnecting = false;

    EC.mousePos = (Vector2){0, 0};

    EC.screenWidth = GetScreenWidth();
    EC.screenHeight = GetScreenHeight();

    EC.draggingNodeIndex = -1;
    EC.dragOffset = (Vector2){0};

    EC.delayFrames = true;

    EC.menuOpen = false;
    EC.submenuOpen = false;
    Vector2 menuPosition = {0, 0};
    Vector2 submenuPosition = {0, 0};

    EC.logCapacity = 100;
    EC.logCount = 0;
    EC.logMessages = malloc(sizeof(char *) * EC.logCapacity);

    EC.font = LoadFontEx("fonts/arialbd.ttf", 128, NULL, 0);

    return EC;
}

void FreeEditorContext(EditorContext *EC)
{
    if (EC->CGFilePath)
        free(EC->CGFilePath);

    for (int i = 0; i < EC->logCount; i++)
    {
        free(EC->logMessages[i]);
    }
    free(EC->logMessages);
}

void SetProjectPaths(EditorContext *EC, const char *projectName)
{
    char cwd[MAX_PATH_LENGTH];

    if (!getcwd(cwd, sizeof(cwd)))
    {
        perror("Failed to get current working directory");
        exit(EXIT_FAILURE);
    }

    size_t len = strlen(cwd);

    if (len <= 7)
    {
        fprintf(stderr, "Current directory path too short to truncate 7 chars\n");
        exit(EXIT_FAILURE);
    }

    cwd[len - 7] = '\0';

    snprintf(EC->CGFilePath, MAX_PATH_LENGTH, "%s\\Projects\\%s\\%s.cg", cwd, projectName, projectName);
}

void AddToLog(EditorContext *EC, const char *newLine)
{
    if (EC->logCount >= EC->logCapacity)
    {
        EC->logCapacity += 100;
        EC->logMessages = realloc(EC->logMessages, sizeof(char *) * EC->logCapacity);
    }

    time_t timestamp = time(NULL);
    struct tm *tm_info = localtime(&timestamp);

    int hour = tm_info->tm_hour;
    int minute = tm_info->tm_min;

    size_t lineLength = strlen(newLine);
    char *message = malloc(16 + lineLength + 6);

    snprintf(message, 16 + lineLength + 6, "%02d:%02d %s", hour, minute, newLine);

    EC->logMessages[EC->logCount] = malloc(strlen(message) + 1);
    strcpy(EC->logMessages[EC->logCount], message);

    EC->logCount++;
    free(message);
}

void DrawCurvedWire(Vector2 outputPos, Vector2 inputPos, float thickness, Color color)
{
    float distance = fabsf(inputPos.x - outputPos.x);
    float controlOffset = distance * 0.5f;

    Vector2 p0 = outputPos;
    Vector2 p1 = {outputPos.x + controlOffset, outputPos.y};
    Vector2 p2 = {inputPos.x - controlOffset, inputPos.y};
    Vector2 p3 = inputPos;

    Vector2 prev = p0;
    int segments = 40;

    for (int i = 1; i <= segments; i++)
    {
        float t = (float)i / segments;
        float u = 1.0f - t;

        Vector2 point = {
            u * u * u * p0.x + 3 * u * u * t * p1.x + 3 * u * t * t * p2.x + t * t * t * p3.x,
            u * u * u * p0.y + 3 * u * u * t * p1.y + 3 * u * t * t * p2.y + t * t * t * p3.y};

        DrawLineEx(prev, point, thickness, color);
        prev = point;
    }
}

void DrawBottomBar(EditorContext *EC)
{
    Color BottomBarColor = BLACK;

    DrawRectangle(0, (*EC).screenHeight - (*EC).bottomBarHeight, (*EC).screenWidth, (*EC).bottomBarHeight, BottomBarColor);
    DrawLineEx((Vector2){0, (*EC).screenHeight - (*EC).bottomBarHeight}, (Vector2){(*EC).screenWidth, (*EC).screenHeight - (*EC).bottomBarHeight}, 2, WHITE);

    DrawRectangleRounded((Rectangle){85, (*EC).screenHeight - (*EC).bottomBarHeight + 25, 60, 30}, 0.2f, 8, CLITERAL(Color){255, 255, 255, 50});
    // DrawText("Save", 90, (*EC).screenHeight - (*EC).bottomBarHeight + 30, 20, WHITE);
    DrawTextEx((*EC).font, "Save", (Vector2){90, (*EC).screenHeight - (*EC).bottomBarHeight + 30}, 20, 2, WHITE);

    DrawRectangleRounded((Rectangle){10, (*EC).screenHeight - (*EC).bottomBarHeight + 25, 60, 30}, 0.2f, 8, CLITERAL(Color){255, 255, 255, 50});
    // DrawText("Run", 20, (*EC).screenHeight - (*EC).bottomBarHeight + 30, 20, WHITE);
    DrawTextEx((*EC).font, "Run", (Vector2){20, (*EC).screenHeight - (*EC).bottomBarHeight + 30}, 20, 2, WHITE);

    int y = (*EC).screenHeight - 60;

    for (int i = (*EC).logCount - 1; i >= 0 && y >= (*EC).screenHeight - (*EC).bottomBarHeight + 50; i--)
    {
        // DrawText((*EC).logMessages[i], 20, y, 20, WHITE);
        DrawTextEx((*EC).font, (*EC).logMessages[i], (Vector2){20, y}, 20, 2, WHITE);
        y -= 25;
    }
}

bool CheckBottomBarCollisions(EditorContext *EC, GraphContext *graph)
{

    // Save button collisions
    if (CheckCollisionPointRec((*EC).mousePos, (Rectangle){85, (*EC).screenHeight - (*EC).bottomBarHeight + 25, 60, 30}))
    {
        DrawRectangleRounded((Rectangle){85, (*EC).screenHeight - (*EC).bottomBarHeight + 25, 60, 30}, 0.2f, 8, Fade(WHITE, 0.6f));
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (SaveGraphToFile(EC->CGFilePath, graph) == 0)
            {
                AddToLog(EC, "Saved successfully!");
            }
            else
            {
                AddToLog(EC, "ERROR SAVING CHANGES!");
            }
        }

        return true;
    }

    // Run button collisions
    if (CheckCollisionPointRec((*EC).mousePos, (Rectangle){10, (*EC).screenHeight - (*EC).bottomBarHeight + 25, 60, 30}))
    {
        DrawRectangleRounded((Rectangle){10, (*EC).screenHeight - (*EC).bottomBarHeight + 25, 60, 30}, 0.2f, 8, Fade(WHITE, 0.6f));
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            // Compile and run CoreGraph
            AddToLog(EC, "Compiler not ready"); ////
        }

        return true;
    }

    return false;
}

/*
if ((*EC).lastClickedPin.nodeID == -1)
        {
            (*EC).lastClickedPin = clickedPin;
            (*EC).isConnecting = true;
        }
        else
        {
            MakeConnection(clickedPin, (*EC).lastClickedPin, graph);//
            (*EC).isConnecting = false;
            (*EC).lastClickedPin = INVALID_PIN;
        }
*/

void DrawNodes(EditorContext *EC, GraphContext *graph){
    if(graph->nodeCount == 0){
        return;
    }

    for(int i = 0; i < graph->nodeCount; i++){
        DrawRectangleRoundedLines((Rectangle){graph->nodes[i].position.x - 1, graph->nodes[i].position.y - 1, getNodeInfoByType(graph->nodes[i].type, "width") + 2, getNodeInfoByType(graph->nodes[i].type, "height") + 2}, 0.2f, 8, WHITE);
        DrawRectangleRounded((Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, "width"), getNodeInfoByType(graph->nodes[i].type, "height")}, 0.2f, 8, (Color){0, 0, 0, 120});
        //DrawTextEx(EC->font, graph->nodes[i].type, (Vector2){graph->nodes[i].position.x + 10, graph->nodes[i].position.y + 3}, 30, 2, WHITE);
    }
}

/*Pin DDrawNodes(EditorContext *EC, Node **NodeList, size_t *nodeCount)
{
    if (nodeCount == 0 || NodeList == NULL)
        return INVALID_PIN;

    int linkedNodeIndex = -1;

    for (int i = 0; i < *nodeCount; i++)
    {
        DrawRectangleRoundedLines((Rectangle){(*NodeList)[i].position.x - 1, (*NodeList)[i].position.y - 1, getNodeInfoByType((*NodeList)[i].type, "width") + 2, getNodeInfoByType((*NodeList)[i].type, "height") + 2}, 0.2f, 8, WHITE);
        DrawRectangleRounded((Rectangle){(*NodeList)[i].position.x, (*NodeList)[i].position.y, getNodeInfoByType((*NodeList)[i].type, "width"), getNodeInfoByType((*NodeList)[i].type, "height")}, 0.2f, 8, (Color){0, 0, 0, 120});
        DrawTextEx((*EC).font, (*NodeList)[i].type, (Vector2){(*NodeList)[i].position.x + 10, (*NodeList)[i].position.y + 3}, 30, 2, WHITE);

        DrawTriangle(
            (Vector2){(*NodeList)[i].position.x + 10, (*NodeList)[i].position.y + 40},
            (Vector2){(*NodeList)[i].position.x + 10, (*NodeList)[i].position.y + 56},
            (Vector2){(*NodeList)[i].position.x + 25, (*NodeList)[i].position.y + 48},
            WHITE);

        DrawTriangle(
            (Vector2){(*NodeList)[i].position.x + getNodeInfoByType((*NodeList)[i].type, "width") - 20, (*NodeList)[i].position.y + 40},
            (Vector2){(*NodeList)[i].position.x + getNodeInfoByType((*NodeList)[i].type, "width") - 20, (*NodeList)[i].position.y + 56},
            (Vector2){(*NodeList)[i].position.x + getNodeInfoByType((*NodeList)[i].type, "width") - 5, (*NodeList)[i].position.y + 48},
            WHITE);

        if ((*NodeList)[i].next != -1)
        {
            linkedNodeIndex = GetLinkedNodeIndex(*NodeList, *nodeCount, (*NodeList)[i].next);
            DrawCurvedWire((Vector2){(*NodeList)[i].position.x + getNodeInfoByType((*NodeList)[i].type, "width") - 5, (*NodeList)[i].position.y + 48}, (Vector2){(*NodeList)[linkedNodeIndex].position.x + 10, (*NodeList)[linkedNodeIndex].position.y + 48}, 2.0f, (Color){180, 100, 200, 255});
        }

        for (int j = 0; j < getNodeInfoByType((*NodeList)[i].type, "inputCount"); j++)
        {
            DrawCircle((*NodeList)[i].position.x + 15, (*NodeList)[i].position.y + 80 + j * 25, 5, WHITE);

            if ((*NodeList)[i].input[j] != -1)
            {
                linkedNodeIndex = GetLinkedNodeIndex(*NodeList, *nodeCount, (*NodeList)[i].input[j]);

                DrawCurvedWire((Vector2){(*NodeList)[linkedNodeIndex].position.x + getNodeInfoByType((*NodeList)[linkedNodeIndex].type, "width") - 14, (*NodeList)[linkedNodeIndex].position.y + 80 + ((*NodeList)[i].input[j] % 100) * 25}, (Vector2){(*NodeList)[i].position.x + 15, (*NodeList)[i].position.y + 80 + j * 25}, 2.0f, (Color){139, 233, 253, 255});
            }
        }

        for (int j = 0; j < getNodeInfoByType((*NodeList)[i].type, "outputCount"); j++)
        {
            DrawCircle((*NodeList)[i].position.x + getNodeInfoByType((*NodeList)[i].type, "width") - 14, (*NodeList)[i].position.y + 80 + j * 25, 5, WHITE);
        }
    }

    // Collisions and pins part

    if ((*EC).isConnecting == true)
    {
        if ((*EC).lastClickedPin.isInput)
        {
            DrawCurvedWire((*EC).mousePos, (*EC).lastClickedPin.position, 2.0f, YELLOW);
        }
        else
        {
            DrawCurvedWire((*EC).lastClickedPin.position, (*EC).mousePos, 2.0f, YELLOW);
        }
    }

    for (int i = 0; i < *nodeCount; i++)
    {
        if (CheckCollisionPointRec((*EC).mousePos, (Rectangle){(*NodeList)[i].position.x, (*NodeList)[i].position.y, getNodeInfoByType((*NodeList)[i].type, "width"), getNodeInfoByType((*NodeList)[i].type, "height")}))
        {
            if (CheckCollisionPointRec((*EC).mousePos, (Rectangle){(*NodeList)[i].position.x + 10, (*NodeList)[i].position.y + 40, 15, 16}))
            {
                DrawTriangle(
                    (Vector2){(*NodeList)[i].position.x + 8, (*NodeList)[i].position.y + 38},
                    (Vector2){(*NodeList)[i].position.x + 8, (*NodeList)[i].position.y + 58},
                    (Vector2){(*NodeList)[i].position.x + 27, (*NodeList)[i].position.y + 48},
                    WHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    return (Pin){(*NodeList)[i].id, i, 0, true, true, (Vector2){(*NodeList)[i].position.x + 10, (*NodeList)[i].position.y + 48}};
                }
                else
                {
                    return INVALID_PIN;
                }
            }
            else if (CheckCollisionPointRec((*EC).mousePos, (Rectangle){(*NodeList)[i].position.x + getNodeInfoByType((*NodeList)[i].type, "width") - 20, (*NodeList)[i].position.y + 40, 15, 16}))
            {
                DrawTriangle(
                    (Vector2){(*NodeList)[i].position.x + getNodeInfoByType((*NodeList)[i].type, "width") - 22, (*NodeList)[i].position.y + 38},
                    (Vector2){(*NodeList)[i].position.x + getNodeInfoByType((*NodeList)[i].type, "width") - 22, (*NodeList)[i].position.y + 58},
                    (Vector2){(*NodeList)[i].position.x + getNodeInfoByType((*NodeList)[i].type, "width") - 3, (*NodeList)[i].position.y + 48},
                    WHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    return (Pin){(*NodeList)[i].id, i, 1, true, false, (Vector2){(*NodeList)[i].position.x + getNodeInfoByType((*NodeList)[i].type, "width") - 7, (*NodeList)[i].position.y + 48}};
                }
                else
                {
                    return INVALID_PIN;
                }
            }
            else
            {
                for (int j = 0; j < getNodeInfoByType((*NodeList)[i].type, "inputCount"); j++)
                {
                    if (CheckCollisionPointCircle((*EC).mousePos, (Vector2){(*NodeList)[i].position.x + 15, (*NodeList)[i].position.y + 80 + j * 25}, 7))
                    {
                        DrawCircle((*NodeList)[i].position.x + 15, (*NodeList)[i].position.y + 80 + j * 25, 7, WHITE);
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                        {
                            return (Pin){(*NodeList)[i].id, i, j, false, true, (Vector2){(*NodeList)[i].position.x + 15, (*NodeList)[i].position.y + 80 + j * 25}};
                        }
                        else
                        {
                            return INVALID_PIN;
                        }
                    }
                }

                for (int j = 0; j < getNodeInfoByType((*NodeList)[i].type, "outputCount"); j++)
                {
                    if (CheckCollisionPointCircle((*EC).mousePos, (Vector2){(*NodeList)[i].position.x + getNodeInfoByType((*NodeList)[i].type, "width") - 14, (*NodeList)[i].position.y + 80 + j * 25}, 7))
                    {
                        DrawCircle((*NodeList)[i].position.x + getNodeInfoByType((*NodeList)[i].type, "width") - 14, (*NodeList)[i].position.y + 80 + j * 25, 7, WHITE);
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                        {
                            return (Pin){(*NodeList)[i].id, i, j, false, false, (Vector2){(*NodeList)[i].position.x + getNodeInfoByType((*NodeList)[i].type, "width") - 14, (*NodeList)[i].position.y + 80 + j * 25}};
                        }
                        else
                        {
                            return INVALID_PIN;
                        }
                    }
                }

                DrawRectangleRounded((Rectangle){(*NodeList)[i].position.x, (*NodeList)[i].position.y, getNodeInfoByType((*NodeList)[i].type, "width"), getNodeInfoByType((*NodeList)[i].type, "height")}, 0.2f, 8, CLITERAL(Color){255, 255, 255, 50});
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            {
                // show delete button
                RemoveNode(NodeList, nodeCount, i);
                (*EC).menuOpen = false;

                return INVALID_PIN;
            }
        }
    }

    return INVALID_PIN;
}*/

bool CheckNodeCollisions(EditorContext *EC, GraphContext *graph)
{
    for (int i = 0; i < graph->nodeCount; i++)
    {
        if (CheckCollisionPointRec((*EC).mousePos, (Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, "width"), getNodeInfoByType(graph->nodes[i].type, "height")}))
        {
            return true;
        }
    }

    return false;
}

const char *DrawNodeMenu(EditorContext *EC)
{
    Color MenuColor = {50, 50, 50, 255};
    Color ScrollIndicatorColor = {150, 150, 150, 255};
    Color BorderColor = {200, 200, 200, 255};
    Color HighlightColor = {80, 80, 80, 255};

    const char *menuItems[] = {"Variable", "Option 2", "Option 3", "Option 4", "Option 5", "Option 6", "Option 7", "Option 8", "Option 9", "Option 10"};
    const char *subMenuItems[][4] = {
        {"num", "string"},
        {"ex", "Sub 2-2", "Sub 2-3"},
        {"Sub 3-1", "Sub 3-2"},
        {"Sub 4-1"},
        {"Sub 5-1", "Sub 5-2"},
        {"Sub 6-1", "Sub 6-2", "Sub 6-3", "Sub 6-4"},
        {"Sub 7-1"},
        {"Sub 8-1", "Sub 8-2"},
        {"Sub 9-1", "Sub 9-2", "Sub 9-3"},
        {"Sub 10-1"}};
    int menuItemCount = sizeof(menuItems) / sizeof(menuItems[0]);
    int subMenuCounts[] = {2, 3, 2, 1, 2, 4, 1, 2, 3, 1};

    float menuHeight = MENU_ITEM_HEIGHT * MENU_VISIBLE_ITEMS;

    if (GetMouseWheelMove() < 0 && (*EC).scrollIndex < menuItemCount - 5)
        (*EC).scrollIndex++;
    if (GetMouseWheelMove() > 0 && (*EC).scrollIndex > 0)
        (*EC).scrollIndex--;

    (*EC).menuPosition = (*EC).rightClickPos;
    if ((*EC).menuPosition.x + MENU_WIDTH > (*EC).screenWidth)
    {
        (*EC).menuPosition.x -= MENU_WIDTH;
    }

    DrawRectangle((*EC).menuPosition.x, (*EC).menuPosition.y, MENU_WIDTH, menuHeight, MenuColor);
    DrawRectangleLinesEx((Rectangle){(*EC).menuPosition.x, (*EC).menuPosition.y, MENU_WIDTH, menuHeight}, MENU_BORDER_THICKNESS, BorderColor);

    for (int i = 0; i < (int)MENU_VISIBLE_ITEMS; i++)
    {
        int itemIndex = i + (*EC).scrollIndex;
        if (itemIndex >= menuItemCount)
            break;

        Rectangle itemRect = {(*EC).menuPosition.x, (*EC).menuPosition.y + i * MENU_ITEM_HEIGHT, MENU_WIDTH, MENU_ITEM_HEIGHT};
        if (CheckCollisionPointRec((*EC).mousePos, itemRect))
        {
            DrawRectangleRec(itemRect, HighlightColor);
            (*EC).hoveredItem = itemIndex;
            (*EC).submenuOpen = true;
            (*EC).submenuPosition.x = ((*EC).menuPosition.x + MENU_WIDTH + SUBMENU_WIDTH > (*EC).screenWidth) ? ((*EC).menuPosition.x - SUBMENU_WIDTH) : ((*EC).menuPosition.x + MENU_WIDTH);
            (*EC).submenuPosition.y = itemRect.y;
        }
        Rectangle subMenuRect = {(*EC).submenuPosition.x, (*EC).submenuPosition.y, SUBMENU_WIDTH, subMenuCounts[(*EC).hoveredItem] * MENU_ITEM_HEIGHT};
        if (CheckCollisionPointRec((*EC).mousePos, subMenuRect))
        {
            (*EC).submenuOpen = true;
        }
        DrawText(menuItems[itemIndex], (*EC).menuPosition.x + 10, (*EC).menuPosition.y + i * MENU_ITEM_HEIGHT + 10, 25, WHITE);
    }

    // Scrollbar
    float scrollBarHeight = (menuHeight / menuItemCount) * (int)MENU_VISIBLE_ITEMS;
    float scrollBarY = (*EC).menuPosition.y + ((*EC).scrollIndex * (menuHeight / menuItemCount));
    DrawRectangle((*EC).menuPosition.x + MENU_WIDTH - 10, scrollBarY, 8, scrollBarHeight, ScrollIndicatorColor);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        (*EC).menuOpen = false;
    }

    if ((*EC).submenuOpen && (*EC).hoveredItem >= 0 && (*EC).hoveredItem < menuItemCount)
    {
        int subCount = subMenuCounts[(*EC).hoveredItem];
        float submenuHeight = subCount * MENU_ITEM_HEIGHT;
        DrawRectangle((*EC).submenuPosition.x, (*EC).submenuPosition.y, SUBMENU_WIDTH, submenuHeight, MenuColor);
        DrawRectangleLinesEx((Rectangle){(*EC).submenuPosition.x, (*EC).submenuPosition.y, SUBMENU_WIDTH, submenuHeight}, MENU_BORDER_THICKNESS, BorderColor);
        for (int j = 0; j < subCount; j++)
        {
            Rectangle subItemRect = {(*EC).submenuPosition.x, (*EC).submenuPosition.y + j * MENU_ITEM_HEIGHT, SUBMENU_WIDTH, MENU_ITEM_HEIGHT};
            if (CheckCollisionPointRec((*EC).mousePos, subItemRect))
            {
                DrawRectangleRec(subItemRect, HighlightColor);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    (*EC).delayFrames = true;
                    // Handle click event for submenu item
                    return subMenuItems[(*EC).hoveredItem][j];
                }
            }
            DrawText(subMenuItems[(*EC).hoveredItem][j], (*EC).submenuPosition.x + 10, (*EC).submenuPosition.y + j * MENU_ITEM_HEIGHT + 10, 25, WHITE);
        }
    }

    return "NULL";
}

void HandleDragging(EditorContext *EC, GraphContext *graph)
{
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && (*EC).draggingNodeIndex == -1)
    {
        SetTargetFPS(140);
        for (int i = 0; i < graph->nodeCount; i++)
        {
            if (CheckCollisionPointRec((*EC).mousePos, (Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, "width"), getNodeInfoByType(graph->nodes[i].type, "height")}))
            {
                (*EC).draggingNodeIndex = i;
                (*EC).dragOffset = (Vector2){(*EC).mousePos.x - graph->nodes[i].position.x, (*EC).mousePos.y - graph->nodes[i].position.y};
                return;
            }
        }

        (*EC).isDraggingScreen = true;
        (*EC).prevMousePos = (*EC).mousePos;
        (*EC).mousePosAtStartOfDrag = (*EC).mousePos;
    }

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && (*EC).draggingNodeIndex != -1)
    {
        graph->nodes[(*EC).draggingNodeIndex].position.x = (*EC).mousePos.x - (*EC).dragOffset.x;
        graph->nodes[(*EC).draggingNodeIndex].position.y = (*EC).mousePos.y - (*EC).dragOffset.y;
        DrawRectangleRounded((Rectangle){graph->nodes[(*EC).draggingNodeIndex].position.x, graph->nodes[(*EC).draggingNodeIndex].position.y, getNodeInfoByType(graph->nodes[(*EC).draggingNodeIndex].type, "width"), getNodeInfoByType(graph->nodes[(*EC).draggingNodeIndex].type, "height")}, 0.2f, 8, CLITERAL(Color){255, 255, 255, 50});
    }
    else if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && (*EC).isDraggingScreen)
    {
        for (int i = 0; i < graph->nodeCount; i++)
        {
            graph->nodes[i].position.x = graph->nodes[i].position.x + EC->mousePos.x - EC->prevMousePos.x;
            graph->nodes[i].position.y = graph->nodes[i].position.y + EC->mousePos.y - EC->prevMousePos.y;
        }
        (*EC).prevMousePos = (*EC).mousePos;
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
    {
        SetTargetFPS(60);
        (*EC).draggingNodeIndex = -1;
        (*EC).isDraggingScreen = false;
    }
}

void DrawBackgroundGrid(EditorContext *EC, int gridSpacing)
{
    int offsetX = EC->mousePosAtStartOfDrag.x - EC->mousePos.x;
    int offsetY = EC->mousePosAtStartOfDrag.y - EC->mousePos.y;

    if ((*EC).isDraggingScreen == false)
    {
        offsetX = 0;
        offsetY = 0;
    }

    int startX = (offsetX / gridSpacing) * gridSpacing - gridSpacing;
    int startY = (offsetY / gridSpacing) * gridSpacing - gridSpacing;

    for (int y = startY; y < EC->screenHeight + offsetY; y += gridSpacing - 5)
    {
        int worldY = y;
        int row = worldY / (gridSpacing - 5); // row anchored to world space

        for (int x = startX; x < EC->screenWidth + offsetX; x += gridSpacing)
        {
            float drawX = (float)(x - offsetX + (row % 2) * 25); // stagger every other row
            float drawY = (float)(y - offsetY);

            DrawRectangleRounded(
                (Rectangle){drawX, drawY, 20 - (row % 2) * 5, 10 + (row % 2) * 5},
                1.0f, 8,
                (Color){128, 128, 128, 10});
        }
    }
}

int DrawViewTexture(EditorContext *EC, RenderTexture2D view, GraphContext *graph)
{
    BeginTextureMode(view);
    ClearBackground((Color){40, 42, 54, 255});

    HandleDragging(EC, graph);

    DrawBackgroundGrid(EC, 40);

    DrawNodes(EC, graph);

    DrawBottomBar(EC);

    DrawTextEx(GetFontDefault(), "CoreGraph", (Vector2){20, 20}, 40, 4, Fade(WHITE, 0.2f));
    DrawTextEx(GetFontDefault(), "TM", (Vector2){230, 10}, 15, 1, Fade(WHITE, 0.2f));

    EndTextureMode();

    return 0;
}

bool CheckAllCollisions(EditorContext *EC, GraphContext *graph)
{
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && (*EC).mousePos.y < (*EC).screenHeight - (*EC).bottomBarHeight)
    {
        (*EC).menuOpen = true;
        (*EC).rightClickPos = (*EC).mousePos;
        (*EC).scrollIndex = 0;
    }

    return CheckNodeCollisions(EC, graph) || CheckBottomBarCollisions(EC, graph) || (*EC).draggingNodeIndex != -1 || IsMouseButtonDown(MOUSE_LEFT_BUTTON);
}

NodeType MatchChosenType(char strType[MAX_TYPE_LENGTH]){
    if(strcmp(strType, "num")){
        return NODE_NUM;
    }
}

int main(int argc, char *argv[])
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1600, 1000, "CG Editor");
    MaximizeWindow();
    SetTargetFPS(40);

    EditorContext EC = InitEditorContext();

    if (EC.font.texture.id == 0)
    {
        AddToLog(&EC, "Couldn't load font");
    }
    else
    {
        AddToLog(&EC, "Loaded font");
    }

    AddToLog(&EC, "Initialized window");

    if (argc < 2)
    {
        AddToLog(&EC, "Error: No project file specified.");
        ShellExecuteA(NULL, "open", "ProjectManager.exe", NULL, NULL, SW_SHOWNORMAL);
        exit(1);
    }
    AddToLog(&EC, "Recieved project file");

    SetProjectPaths(&EC, argv[1]);

    GraphContext graph;
    InitGraphContext(&graph);

    /*switch (LoadNodesFromFile(EC.CGFilePath, &NodeList, &nodeCount, &EC.lastNodeID))
    {
    case 0:
        AddToLog(&EC, "Loaded CoreGraph file");
        break;
    case 1:
        AddToLog(&EC, "Failed to open file");
        break;
    case 2:
        AddToLog(&EC, "Corrupted CoreGraph file");
        break;
    case 3:
        AddToLog(&EC, "Failed memory allocation");
        break;
    default:
        AddToLog(&EC, "Error while loading nodes from file");
        break;
    }*/

    LoadGraphFromFile(EC.CGFilePath, &graph);

    AddToLog(&EC, "Welcome!");

    RenderTexture2D view = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

    while (!WindowShouldClose())
    {
        EC.screenWidth = GetScreenWidth();
        EC.screenHeight = GetScreenHeight();
        EC.bottomBarHeight = EC.screenHeight * 0.25;
        EC.mousePos = GetMousePosition();

        BeginDrawing();
        ClearBackground((Color){40, 42, 54, 255});

        if (CheckAllCollisions(&EC, &graph) || EC.isConnecting)
        {
            DrawViewTexture(&EC, view, &graph);
            EC.delayFrames = true;
        }
        else if (EC.delayFrames)
        {
            DrawViewTexture(&EC, view, &graph);
            EC.delayFrames = false;
        }

        DrawTextureRec(view.texture, (Rectangle){0, 0, view.texture.width, -view.texture.height}, (Vector2){0, 0}, WHITE);

        if (EC.menuOpen)
        {
            char createdNode[MAX_TYPE_LENGTH];
            strcpy(createdNode, DrawNodeMenu(&EC));
            if (createdNode != "NULL")
            {
                if (EC.lastNodeID == 9999)
                {
                    AddToLog(&EC, "CG file too big to process");
                }
                else
                {
                    CreateNode(&graph, MatchChosenType(createdNode), EC.rightClickPos);
                }
            }
        }

        char str[30];
        sprintf(str, "%d", graph.nodes[0].type);
        DrawText(str, 50, 50, 50, WHITE);

        DrawFPS(10, 10);

        EndDrawing();
    }

    CloseWindow();

    FreeEditorContext(&EC);

    UnloadRenderTexture(view);

    UnloadFont(EC.font);

    /*if (NodeList)
    {
        free(NodeList);
    }*/

    return 0;
}