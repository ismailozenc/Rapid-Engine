#include <stdio.h>
#include "raylib.h"
#include <direct.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include "shell_execute.h"
#include "RewriteNodes.h"

#define MENU_WIDTH 250
#define MENU_ITEM_HEIGHT 40
#define MENU_VISIBLE_ITEMS 5.5
#define MENU_BORDER_THICKNESS 3
#define SUBMENU_WIDTH 200

#define MAX_PATH_LENGTH 1024
#define MAX_TYPE_LENGTH 16

#define PREV_SUB_ID 0
#define NEXT_SUB_ID 1

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

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

const char *InputsByNodeTypes[][5] = {
    {"name", "value"},
    {"name", "value"},
    {"a", "b", "c", "d", "e"},
    {"Sub 4-1"},
    {"Sub 5-1", "Sub 5-2"},
    {"Sub 6-1", "Sub 6-2", "Sub 6-3", "Sub 6-4"},
    {"Sub 7-1"},
    {"Sub 8-1", "Sub 8-2"},
    {"Sub 9-1", "Sub 9-2", "Sub 9-3"},
    {"Sub 10-1"}};

const char *OutputsByNodeTypes[][5] = {
    {NULL},
    {NULL},
    {"a", "b", "c", "d", "e"},
    {"Sub 4-1"},
    {"Sub 5-1", "Sub 5-2"},
    {"Sub 6-1", "Sub 6-2", "Sub 6-3", "Sub 6-4"},
    {"Sub 7-1"},
    {"Sub 8-1", "Sub 8-2"},
    {"Sub 9-1", "Sub 9-2", "Sub 9-3"},
    {"Sub 10-1"}};

typedef struct
{
    char *CGFilePath;

    Node *nodes;
    size_t nodeCount;
    int lastNodeID;
    int draggingNodeIndex;
    Vector2 dragOffset;
    char **logMessages;
    int logCount;
    int logCapacity;
    bool menuOpen;
    bool submenuOpen;
    int screenWidth;
    int screenHeight;
    int bottomBarHeight;
    int scrollIndex;
    Pin clickedPin;
    Pin lastClickedPin;
    int hoveredItem;
    char *clickedType;
    Vector2 menuPosition;
    Vector2 submenuPosition;

    Vector2 rightClickPos;
    Vector2 mousePos;

} EditorContext;

EditorContext InitEditorContext()
{
    EditorContext EC = {0}; // Zero all fields

    EC.CGFilePath = malloc(MAX_PATH_LENGTH);
    EC.CGFilePath[0] = '\0';

    EC.nodes = NULL;
    EC.nodeCount = 0;
    EC.lastNodeID = 0;

    EC.clickedPin = INVALID_PIN;
    EC.lastClickedPin = INVALID_PIN;

    EC.scrollIndex = 0;
    EC.hoveredItem = -1;

    EC.mousePos = (Vector2){0, 0};

    EC.screenWidth = GetScreenWidth();
    EC.screenHeight = GetScreenHeight();

    EC.draggingNodeIndex = -1;
    EC.dragOffset = (Vector2){0};

    EC.menuOpen = false;
    EC.submenuOpen = false;
    Vector2 menuPosition = {0, 0};
    Vector2 submenuPosition = {0, 0};

    EC.logCapacity = 100;
    EC.logCount = 0;
    EC.logMessages = malloc(sizeof(char *) * EC.logCapacity);

    EC.clickedType = malloc(MAX_TYPE_LENGTH);
    EC.clickedType[0] = '\0';

    return EC;
}

void FreeEditorContext(EditorContext *EC)
{
    if (EC->CGFilePath)
        free(EC->CGFilePath);

    if (EC->clickedType)
        free(EC->clickedType);

    for (int i = 0; i < EC->logCount; i++)
    {
        free(EC->logMessages[i]);
    }
    free(EC->logMessages);

    if (EC->nodes)
        free(EC->nodes);
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

void DrawBlueprintWire(Vector2 outputPos, Vector2 inputPos, float thickness, Color color)
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

const char *DrawBlockMenu(EditorContext *EC)
{

    Color MenuColor = {50, 50, 50, 255};
    Color ScrollIndicatorColor = {150, 150, 150, 255};
    Color BorderColor = {200, 200, 200, 255};
    Color HighlightColor = {80, 80, 80, 255};

    int maxVisibleItems = (int)MENU_VISIBLE_ITEMS;
    float menuHeight = MENU_ITEM_HEIGHT * MENU_VISIBLE_ITEMS;
    bool canScroll = menuItemCount > maxVisibleItems;

    if (menuItemCount > 5)
    {
        if (GetMouseWheelMove() < 0 && (*EC).scrollIndex < menuItemCount - 5)
            (*EC).scrollIndex++;
        if (GetMouseWheelMove() > 0 && (*EC).scrollIndex > 0)
            (*EC).scrollIndex--;
    }

    (*EC).menuPosition = (*EC).rightClickPos;
    if ((*EC).menuPosition.x + MENU_WIDTH > (*EC).screenWidth)
    {
        (*EC).menuPosition.x -= MENU_WIDTH;
    }

    DrawRectangle((*EC).menuPosition.x, (*EC).menuPosition.y, MENU_WIDTH, menuHeight, MenuColor);
    DrawRectangleLinesEx((Rectangle){(*EC).menuPosition.x, (*EC).menuPosition.y, MENU_WIDTH, menuHeight}, MENU_BORDER_THICKNESS, BorderColor);

    for (int i = 0; i < maxVisibleItems; i++)
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

    if (canScroll) // Scrollbar
    {
        float scrollBarHeight = (menuHeight / menuItemCount) * maxVisibleItems;
        float scrollBarY = (*EC).menuPosition.y + ((*EC).scrollIndex * (menuHeight / menuItemCount));
        DrawRectangle((*EC).menuPosition.x + MENU_WIDTH - 10, scrollBarY, 8, scrollBarHeight, ScrollIndicatorColor);
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
                    // Handle click event for submenu item

                    return subMenuItems[(*EC).hoveredItem][j];
                }
            }
            DrawText(subMenuItems[(*EC).hoveredItem][j], (*EC).submenuPosition.x + 10, (*EC).submenuPosition.y + j * MENU_ITEM_HEIGHT + 10, 25, WHITE);
        }
    }

    return "NULL";
}

void DrawNodes(Node *NodeList, size_t nodeCount)
{
    if (nodeCount == 0 || NodeList == NULL)
        return;

    int linkedNodeIndex = -1;

    for (int i = 0; i < nodeCount; i++)
    {

        DrawRectangleRounded((Rectangle){NodeList[i].position.x, NodeList[i].position.y, GetWidthByType(NodeList[i].type), GetHeightByType(NodeList[i].type)}, 0.2f, 8, (Color){0, 0, 0, 120});
        DrawText(NodeList[i].type, NodeList[i].position.x + 10, NodeList[i].position.y + 10, 24, WHITE);

    } /// shorten from two for loops

    for (int i = 0; i < nodeCount; i++)
    {

        DrawTriangle(
            (Vector2){NodeList[i].position.x + 10, NodeList[i].position.y + 40},
            (Vector2){NodeList[i].position.x + 10, NodeList[i].position.y + 56},
            (Vector2){NodeList[i].position.x + 25, NodeList[i].position.y + 48},
            WHITE);

        DrawTriangle(
            (Vector2){NodeList[i].position.x + GetWidthByType(NodeList[i].type) - 20, NodeList[i].position.y + 40},
            (Vector2){NodeList[i].position.x + GetWidthByType(NodeList[i].type) - 20, NodeList[i].position.y + 56},
            (Vector2){NodeList[i].position.x + GetWidthByType(NodeList[i].type) - 5, NodeList[i].position.y + 48},
            WHITE);

        if (NodeList[i].next != -1)
        {

            linkedNodeIndex = GetLinkedNodeIndex(NodeList, nodeCount, NodeList[i].next);

            if (linkedNodeIndex != -1)
            {
                int linkedNodeAttributeNumber = NodeList[i].next + 4;

                DrawBlueprintWire((Vector2){NodeList[i].position.x + GetWidthByType(NodeList[i].type) - 5, NodeList[i].position.y + 48}, (Vector2){NodeList[linkedNodeIndex].position.x + 10, NodeList[linkedNodeIndex].position.y + 48}, 2.0f, (Color){180, 100, 200, 255});
            }
        }

        for (int j = 0; j < GetInputCountByType(NodeList[i].type); j++)
        {
            DrawCircle(NodeList[i].position.x + 15, NodeList[i].position.y + 80 + j * 25, 5, WHITE);
            //DrawText(NodeList[i].input[j], NodeList[i].position.x + 25, NodeList[i].position.y + 80 + j * 25, 20, RED); //temp

            if (NodeList[i].input[j] == -1)
            {
                linkedNodeIndex = GetLinkedNodeIndex(NodeList, nodeCount, NodeList[i].input[j]);

                if (linkedNodeIndex != -1)
                {
                    int linkedNodeAttributeNumber = NodeList[i].input[j] + 6;

                    DrawBlueprintWire((Vector2){NodeList[i].position.x + GetWidthByType(NodeList[i].type) - 14, NodeList[i].position.y + 80 + j * 25}, (Vector2){NodeList[linkedNodeIndex].position.x + 15, NodeList[linkedNodeIndex].position.y + 80 + (linkedNodeAttributeNumber - 2) * 25}, 2.0f, (Color){139, 233, 253, 255});
                }
            }
        }

        for (int j = 0; j < GetOutputCountByType(NodeList[i].type); j++)
        {
            DrawCircle(NodeList[i].position.x + GetWidthByType(NodeList[i].type) - 14, NodeList[i].position.y + 80 + j * 25, 5, WHITE);
            DrawText("-1", NodeList[i].position.x + GetWidthByType(NodeList[i].type) + 1, NodeList[i].position.y + 80 + j * 25, 20, RED); //temp
        }
    }
}

Pin CheckOverlaps(EditorContext *EC, Node *NodeList, size_t *nodeCount)
{
    if (NodeList == NULL || nodeCount == 0)
    {
        return INVALID_PIN;
    }

    for (int i = 0; i < *nodeCount; i++)
    {
        if (CheckCollisionPointRec((*EC).mousePos, (Rectangle){NodeList[i].position.x, NodeList[i].position.y, GetWidthByType(NodeList[i].type), GetHeightByType(NodeList[i].type)}))
        {
            if (CheckCollisionPointRec((*EC).mousePos, (Rectangle){NodeList[i].position.x + 10, NodeList[i].position.y + 40, 15, 16}))
            {
                DrawTriangle(
                    (Vector2){NodeList[i].position.x + 8, NodeList[i].position.y + 38},
                    (Vector2){NodeList[i].position.x + 8, NodeList[i].position.y + 58},
                    (Vector2){NodeList[i].position.x + 27, NodeList[i].position.y + 48},
                    WHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    //return NodeList[i].id * 100 + PREV_SUB_ID;
                    return (Pin){NodeList[i].id, PREV_SUB_ID, true, true};
                }
                else
                {
                    return INVALID_PIN;
                }
            }
            else if (CheckCollisionPointRec((*EC).mousePos, (Rectangle){NodeList[i].position.x + GetWidthByType(NodeList[i].type) - 20, NodeList[i].position.y + 40, 15, 16}))
            {
                DrawTriangle(
                    (Vector2){NodeList[i].position.x + GetWidthByType(NodeList[i].type) - 22, NodeList[i].position.y + 38},
                    (Vector2){NodeList[i].position.x + GetWidthByType(NodeList[i].type) - 22, NodeList[i].position.y + 58},
                    (Vector2){NodeList[i].position.x + GetWidthByType(NodeList[i].type) - 3, NodeList[i].position.y + 48},
                    WHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    //return NodeList[i].id * 100 + NEXT_SUB_ID;
                    return (Pin){NodeList[i].id, NEXT_SUB_ID, true, false};
                }
                else
                {
                    return INVALID_PIN;
                }
            }
            else
            {
                for (int j = 0; j < GetInputCountByType(NodeList[i].type); j++)
                {
                    if (CheckCollisionPointCircle((*EC).mousePos, (Vector2){NodeList[i].position.x + 15, NodeList[i].position.y + 80 + j * 25}, 7))
                    {
                        DrawCircle(NodeList[i].position.x + 15, NodeList[i].position.y + 80 + j * 25, 7, WHITE);
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                        {
                            //return NodeList[i].id * 100 + j + ATTRIBUTES_SUB_ID_STARTING_POS;
                            return (Pin){NodeList[i].id, j, false, true};
                        }
                        else
                        {
                            return INVALID_PIN;
                        }
                    }
                }

                for (int j = 0; j < GetOutputCountByType(NodeList[i].type); j++)
                {
                    if (CheckCollisionPointCircle((*EC).mousePos, (Vector2){NodeList[i].position.x + GetWidthByType(NodeList[i].type) - 14, NodeList[i].position.y + 80 + j * 25}, 7))
                    {
                        DrawCircle(NodeList[i].position.x + GetWidthByType(NodeList[i].type) - 14, NodeList[i].position.y + 80 + j * 25, 7, WHITE);
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                        {
                            //return NodeList[i].id * 100 + GetInputCountByType(NodeList[i].type) + j + ATTRIBUTES_SUB_ID_STARTING_POS;
                            return (Pin){NodeList[i].id, j, false, false};
                        }
                        else
                        {
                            return INVALID_PIN;
                        }
                    }
                }

                DrawRectangleRounded((Rectangle){NodeList[i].position.x, NodeList[i].position.y, GetWidthByType(NodeList[i].type), GetHeightByType(NodeList[i].type)}, 0.2f, 8, CLITERAL(Color){255, 255, 255, 50});
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            {
                // show delete button
                RemoveNode(&NodeList, nodeCount, i);

                //return 1;
                return INVALID_PIN;
            }
        }
    }

    return INVALID_PIN;
}

void DrawBottomBar(EditorContext *EC, Node *NodeList, size_t nodeCount)
{
    Color BottomBarColor = BLACK;

    DrawRectangle(0, (*EC).screenHeight - (*EC).bottomBarHeight, (*EC).screenWidth, (*EC).bottomBarHeight, BottomBarColor);
    DrawLineEx((Vector2){0, (*EC).screenHeight - (*EC).bottomBarHeight}, (Vector2){(*EC).screenWidth, (*EC).screenHeight - (*EC).bottomBarHeight}, 2, WHITE);

    DrawRectangleRounded((Rectangle){85, (*EC).screenHeight - (*EC).bottomBarHeight + 25, 60, 30}, 0.2f, 8, CLITERAL(Color){255, 255, 255, 50});
    DrawText("Save", 90, (*EC).screenHeight - (*EC).bottomBarHeight + 30, 20, WHITE);

    if (CheckCollisionPointRec((*EC).mousePos, (Rectangle){85, (*EC).screenHeight - (*EC).bottomBarHeight + 25, 60, 30}))
    {
        DrawRectangleRounded((Rectangle){85, (*EC).screenHeight - (*EC).bottomBarHeight + 25, 60, 30}, 0.2f, 8, Fade(WHITE, 0.2f));
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (OverrideFileWithList((*EC).CGFilePath, NodeList, nodeCount, (*EC).lastNodeID) == 0)
            {
                AddToLog(EC, "Saved successfully!");
            }
            else
            {
                AddToLog(EC, "ERROR SAVING CHANGES!");
            }
        }
    }

    DrawRectangleRounded((Rectangle){10, (*EC).screenHeight - (*EC).bottomBarHeight + 25, 60, 30}, 0.2f, 8, CLITERAL(Color){255, 255, 255, 50});
    DrawText("Run", 20, (*EC).screenHeight - (*EC).bottomBarHeight + 30, 20, WHITE);

    if (CheckCollisionPointRec((*EC).mousePos, (Rectangle){10, (*EC).screenHeight - (*EC).bottomBarHeight + 25, 60, 30}))
    {
        DrawRectangleRounded((Rectangle){10, (*EC).screenHeight - (*EC).bottomBarHeight + 25, 60, 30}, 0.2f, 8, Fade(WHITE, 0.2f));
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            // Interpret and run CoreGraph
        }
    }

    int y = (*EC).screenHeight - 60;

    for (int i = (*EC).logCount - 1; i >= 0 && y >= (*EC).screenHeight - (*EC).bottomBarHeight + 50; i--)
    {
        DrawText((*EC).logMessages[i], 20, y, 20, WHITE);
        y -= 25;
    }
}

int main(int argc, char *argv[])
{
    EditorContext EC = InitEditorContext();

    if (argc < 2)
    {
        AddToLog(&EC, "Error: No project file specified.");
        ShellExecuteA(NULL, "open", "ProjectManager.exe", NULL, NULL, SW_SHOWNORMAL);
        exit(1);
    }
    AddToLog(&EC, "Recieved project file");

    SetProjectPaths(&EC, argv[1]);

    // TestClearFile(CGFilePath);

    Node *NodeList = NULL;
    size_t nodeCount = 0;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1600, 1000, "CG Editor");
    MaximizeWindow();
    SetTargetFPS(40);

    AddToLog(&EC, "Initialized window");

    switch (LoadNodesFromFile(EC.CGFilePath, &NodeList, &nodeCount, &EC.lastNodeID))
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
    }

    AddToLog(&EC, "Welcome!");

    while (!WindowShouldClose())
    {
        EC.screenWidth = GetScreenWidth();
        EC.screenHeight = GetScreenHeight();
        EC.bottomBarHeight = EC.screenHeight * 0.25;
        EC.mousePos = GetMousePosition();

        BeginDrawing();
        ClearBackground((Color){40, 42, 54, 255});

        DrawTextEx(GetFontDefault(), "CoreGraph", (Vector2){20, 20}, 40, 2, Fade(WHITE, 0.2f));
        DrawTextEx(GetFontDefault(), "TM", (Vector2){230, 10}, 15, 1, Fade(WHITE, 0.2f));

        DrawBottomBar(&EC, NodeList, nodeCount);

        if (EC.menuOpen == true)
        {
            strcpy(EC.clickedType, DrawBlockMenu(&EC));

            if (strcmp(EC.clickedType, "NULL") != 0)
            {
                if (EC.lastNodeID == 9999)
                {
                    AddToLog(&EC, "CG file too big to process");
                }
                else
                {
                    MakeNode(EC.clickedType, argv[1], EC.CGFilePath, &NodeList, &nodeCount, EC.rightClickPos, &EC.lastNodeID);
                }
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            EC.menuOpen = false;
        }

        DrawNodes(NodeList, nodeCount);

        EC.clickedPin = CheckOverlaps(&EC, NodeList, &nodeCount);

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && EC.clickedPin.nodeID == -1)
        {
            if (EC.mousePos.y < EC.screenHeight - EC.bottomBarHeight)
            {
                EC.menuOpen = true;
                EC.rightClickPos = EC.mousePos;
                EC.scrollIndex = 0;
            }
        }

        if (EC.clickedPin.nodeID == -1)
        {
            EC.clickedPin = INVALID_PIN;
            EC.lastClickedPin = INVALID_PIN;
            EC.clickedPin.nodeIndexInList = -1;
        }
        else if (EC.lastClickedPin.nodeID != -1)
        {
            for (int i = 0; i < nodeCount; i++)
            {
                if (NodeList[i].id == EC.lastClickedPin.nodeID)
                {
                    EC.clickedPin.nodeIndexInList = i;
                    break;
                }
            }

            if (EC.lastClickedPin.isFlow == true && EC.lastClickedPin.isInput == true)
            {
                DrawBlueprintWire((Vector2){EC.mousePos.x, EC.mousePos.y}, (Vector2){NodeList[EC.clickedPin.nodeIndexInList].position.x + 10, NodeList[EC.clickedPin.nodeIndexInList].position.y + 48}, 2.0f, (Color){209, 255, 5, 255});
            }
            else if (EC.lastClickedPin.isFlow == true && EC.lastClickedPin.isInput == false)
            {
                DrawBlueprintWire((Vector2){NodeList[EC.clickedPin.nodeIndexInList].position.x + GetWidthByType(NodeList[EC.clickedPin.nodeIndexInList].type) - 5, NodeList[EC.clickedPin.nodeIndexInList].position.y + 48}, (Vector2){EC.mousePos.x, EC.mousePos.y}, 2.0f, (Color){209, 255, 5, 255});
            }
            else if (EC.lastClickedPin.isFlow == false && EC.lastClickedPin.isInput == true)
            {
                DrawBlueprintWire((Vector2){EC.mousePos.x, EC.mousePos.y}, (Vector2){NodeList[EC.clickedPin.nodeIndexInList].position.x + 15, NodeList[EC.clickedPin.nodeIndexInList].position.y + 80 + EC.clickedPin.number * 25}, 2.0f, (Color){209, 255, 5, 255});
            }
            else
            {
                DrawBlueprintWire((Vector2){NodeList[EC.clickedPin.nodeIndexInList].position.x + GetWidthByType(NodeList[EC.clickedPin.nodeIndexInList].type) - 14, NodeList[EC.clickedPin.nodeIndexInList].position.y + 80 + EC.clickedPin.number * 25}, (Vector2){EC.mousePos.x, EC.mousePos.y}, 2.0f, (Color){209, 255, 5, 255});
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if (EC.clickedPin.nodeID == -1)
                {
                    EC.lastClickedPin = INVALID_PIN;
                }
                else
                {
                    if (MakeConnection(EC.lastClickedPin, EC.clickedPin, NodeList, nodeCount) == 1)
                    {
                        AddToLog(&EC, "Can not connect");
                    }
                    EC.lastClickedPin = INVALID_PIN;
                    EC.clickedPin = INVALID_PIN;
                }
            }
        }

        if (EC.clickedPin.nodeID!= -1)
        {
            EC.lastClickedPin = EC.clickedPin;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && EC.draggingNodeIndex == -1)
        {
            SetTargetFPS(140);
            for (int i = 0; i < nodeCount; i++)
            {
                Rectangle nodeRect = (Rectangle){NodeList[i].position.x, NodeList[i].position.y, GetWidthByType(NodeList[i].type), GetHeightByType(NodeList[i].type)};
                if (CheckCollisionPointRec(EC.mousePos, nodeRect))
                {
                    EC.draggingNodeIndex = i;
                    EC.dragOffset = (Vector2){EC.mousePos.x - NodeList[i].position.x, EC.mousePos.y - NodeList[i].position.y};
                    break;
                }
            }
        }

        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && EC.draggingNodeIndex != -1)
        {
            NodeList[EC.draggingNodeIndex].position.x = EC.mousePos.x - EC.dragOffset.x;
            NodeList[EC.draggingNodeIndex].position.y = EC.mousePos.y - EC.dragOffset.y;
            DrawRectangleRounded((Rectangle){NodeList[EC.draggingNodeIndex].position.x, NodeList[EC.draggingNodeIndex].position.y, GetWidthByType(NodeList[EC.draggingNodeIndex].type), GetHeightByType(NodeList[EC.draggingNodeIndex].type)}, 0.2f, 8, CLITERAL(Color){255, 255, 255, 50});
        }

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
        {
            SetTargetFPS(60);
            EC.draggingNodeIndex = -1;
        }

        EndDrawing();
    }

    // Free the NodeList itself
    if (NodeList)
    {
        free(NodeList);
    }

    FreeEditorContext(&EC);

    CloseWindow();

    return 0;
}