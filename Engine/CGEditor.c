#include "CGEditor.h"
#include <direct.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include "shell_execute.h"
#include "raymath.h"

void AddToEngineLog(EditorContext *editor, char *message, int level);

EditorContext InitEditorContext()
{
    EditorContext EC = {0};

    EC.editorOpen = false;

    EC.CGFilePath = malloc(MAX_PATH_LENGTH);
    EC.CGFilePath[0] = '\0';

    EC.fileName = malloc(MAX_PATH_LENGTH);
    EC.fileName[0] = '\0';

    EC.lastClickedPin = INVALID_PIN;

    EC.scrollIndex = 0;
    EC.hoveredItem = -1;

    EC.mousePos = (Vector2){0, 0};

    EC.screenWidth = GetScreenWidth();
    EC.screenHeight = GetScreenHeight();

    EC.draggingNodeIndex = -1;
    EC.dragOffset = (Vector2){0};
    EC.isDraggingScreen = false;

    EC.delayFrames = true;

    EC.menuOpen = false;
    EC.submenuOpen = false;
    Vector2 menuPosition = {0, 0};
    Vector2 submenuPosition = {0, 0};

    EC.font = LoadFontEx("fonts/arialbd.ttf", 128, NULL, 0);
    if (EC.font.texture.id == 0)
    {
        AddToEngineLog(&EC, "Couldn't load font", 1);
    }

    EC.newLogMessage = false;

    EC.cameraOffset = (Vector2){0, 0};

    return EC;
}

void FreeEditorContext(EditorContext *EC)
{
    if (EC->CGFilePath)
        free(EC->CGFilePath);

    if (EC->fileName)
        free(EC->fileName);
}

void AddToEngineLog(EditorContext *editor, char *message, int level)
{
    strncpy(editor->logMessage, message, 128 * sizeof(char));
    editor->logMessageLevel = level;
    editor->newLogMessage = true;
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

void OpenNewCGFile(EditorContext *editor, GraphContext *graph, char *openedFileName)
{
    FreeEditorContext(editor);
    FreeGraphContext(graph);

    *editor = InitEditorContext();
    *graph = InitGraphContext();

    SetProjectPaths(editor, openedFileName);

    LoadGraphFromFile(editor->CGFilePath, graph);

    strcpy(editor->fileName, openedFileName);
}

void DrawBackgroundGrid(EditorContext *EC, int gridSpacing)
{

    if (EC->isDraggingScreen)
    {
        Vector2 delta = GetMouseDelta();
        EC->cameraOffset = Vector2Subtract(EC->cameraOffset, delta);
    }

    Vector2 offset = EC->cameraOffset;

    int startX = -((int)offset.x % gridSpacing) - gridSpacing;
    int startY = -((int)offset.y % gridSpacing) - gridSpacing;

    for (int y = startY; y < EC->screenHeight + gridSpacing; y += gridSpacing)
    {
        int row = (y + (int)offset.y) / gridSpacing;

        for (int x = startX; x < 2560 + gridSpacing; x += gridSpacing)
        {
            float drawX = (float)(x + (row % 2) * (gridSpacing / 2));
            float drawY = (float)(y);

            DrawRectangleRounded(
                (Rectangle){drawX, drawY, 15, 15},
                1.0f, 8,
                (Color){128, 128, 128, 10});
        }
    }
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

void DrawNodes(EditorContext *EC, GraphContext *graph)
{
    if (graph->nodeCount == 0)
    {
        return;
    }

    for (int i = 0; i < graph->linkCount; i++)
    {
        Vector2 inputPinPosition = (Vector2){-1};
        Vector2 outputPinPosition = (Vector2){-1};
        for (int j = 0; j < graph->pinCount; j++)
        {
            if (graph->links[i].inputPinID == graph->pins[j].id)
            {
                inputPinPosition = graph->pins[j].position;
            }
            else if (graph->links[i].outputPinID == graph->pins[j].id)
            {
                outputPinPosition = graph->pins[j].position;
            }
        }
        if (inputPinPosition.x != -1 && outputPinPosition.x != -1)
        {
            DrawCurvedWire(outputPinPosition, inputPinPosition, 2.0f, (Color){180, 100, 200, 255});
        }
        else
        {
            AddToEngineLog(EC, "Error drawing connection", 1);
        }
    }

    int hoveredNodeIndex = -1;

    for (int i = 0; i < graph->nodeCount; i++)
    {
        DrawRectangleRoundedLines((Rectangle){graph->nodes[i].position.x - 1, graph->nodes[i].position.y - 1, getNodeInfoByType(graph->nodes[i].type, "width") + 2, getNodeInfoByType(graph->nodes[i].type, "height") + 2}, 0.2f, 8, WHITE);
        DrawRectangleRounded((Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, "width"), getNodeInfoByType(graph->nodes[i].type, "height")}, 0.2f, 8, (Color){0, 0, 0, 120});
        DrawTextEx(EC->font, NodeTypeToString(graph->nodes[i].type), (Vector2){graph->nodes[i].position.x + 10, graph->nodes[i].position.y + 3}, 30, 2, WHITE);
        if (CheckCollisionPointRec(EC->mousePos, (Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, "width"), getNodeInfoByType(graph->nodes[i].type, "height")}))
        {
            hoveredNodeIndex = i;
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
            {
                DeleteNode(graph, graph->nodes[i].id);
                EC->menuOpen = false;
                return;
            }
        }
    }

    int hoveredPinIndex = -1;

    for (int i = 0; i < graph->pinCount; i++)
    {
        int currNodeIndex = -1;

        for (int j = 0; j < graph->nodeCount; j++)
        {
            if (graph->nodes[j].id == graph->pins[i].nodeID)
            {
                currNodeIndex = j;
                break;
            }
        }

        if (currNodeIndex == -1)
        {
            TraceLog(LOG_WARNING, "Pin %d has no matching node (ID %d)", i, graph->pins[i].nodeID);
            continue;
        }

        Vector2 nodePos = graph->nodes[currNodeIndex].position;
        int xOffset = graph->pins[i].isInput ? 5 : (getNodeInfoByType(graph->nodes[currNodeIndex].type, "width") - 20);
        int yOffset = 50 + graph->pins[i].posInNode * 30;

        graph->pins[i].position = (Vector2){nodePos.x + xOffset + 5, nodePos.y + yOffset};

        if (graph->pins[i].type == PIN_FLOW)
        {
            DrawTriangle((Vector2){nodePos.x + xOffset, nodePos.y + yOffset - 8}, (Vector2){nodePos.x + xOffset, nodePos.y + yOffset + 8}, (Vector2){nodePos.x + xOffset + 15, nodePos.y + yOffset}, WHITE);
            if (CheckCollisionPointRec(EC->mousePos, (Rectangle){nodePos.x + xOffset, nodePos.y + yOffset - 8, 15, 16}))
            {
                DrawTriangle((Vector2){nodePos.x + xOffset - 2, nodePos.y + yOffset - 10}, (Vector2){nodePos.x + xOffset - 2, nodePos.y + yOffset + 10}, (Vector2){nodePos.x + xOffset + 17, nodePos.y + yOffset}, WHITE);
                hoveredPinIndex = i;
            }
        }
        else
        {
            DrawCircle(nodePos.x + xOffset + 5, nodePos.y + yOffset, 5, WHITE);
            if (CheckCollisionPointCircle(EC->mousePos, (Vector2){nodePos.x + xOffset + 5, nodePos.y + yOffset}, 5))
            {
                DrawCircle(nodePos.x + xOffset + 5, nodePos.y + yOffset, 7, WHITE);
                hoveredPinIndex = i;
            }
        }
    }

    if (hoveredPinIndex != -1 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (EC->lastClickedPin.id == -1)
        {
            EC->lastClickedPin = graph->pins[hoveredPinIndex];
        }
        else
        {
            CreateLink(graph, EC->lastClickedPin, graph->pins[hoveredPinIndex]);
            EC->lastClickedPin = INVALID_PIN;
        }
    }

    if (hoveredPinIndex == -1 && hoveredNodeIndex != -1)
    {
        DrawRectangleRounded((Rectangle){graph->nodes[hoveredNodeIndex].position.x - 1, graph->nodes[hoveredNodeIndex].position.y - 1, getNodeInfoByType(graph->nodes[hoveredNodeIndex].type, "width") + 2, getNodeInfoByType(graph->nodes[hoveredNodeIndex].type, "height") + 2}, 0.2f, 8, (Color){255, 255, 255, 30});
        EC->delayFrames = true;
    }

    if (EC->lastClickedPin.id != -1)
    {
        if (EC->lastClickedPin.isInput)
        {
            DrawCurvedWire(EC->mousePos, EC->lastClickedPin.position, 2.0f, YELLOW);
        }
        else
        {
            DrawCurvedWire(EC->lastClickedPin.position, EC->mousePos, 2.0f, YELLOW);
        }
    }
}

bool CheckNodeCollisions(EditorContext *EC, GraphContext *graph)
{
    for (int i = 0; i < graph->nodeCount; i++)
    {
        if (CheckCollisionPointRec(EC->mousePos, (Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, "width"), getNodeInfoByType(graph->nodes[i].type, "height")}))
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

    if (GetMouseWheelMove() < 0 && EC->scrollIndex < menuItemCount - 5)
        EC->scrollIndex++;
    if (GetMouseWheelMove() > 0 && EC->scrollIndex > 0)
        EC->scrollIndex--;

    EC->menuPosition.x = EC->rightClickPos.x;
    EC->menuPosition.y = EC->rightClickPos.y;

    if (EC->menuPosition.x + MENU_WIDTH > EC->screenWidth)
    {
        EC->menuPosition.x -= MENU_WIDTH;
    }

    DrawRectangleRounded((Rectangle){EC->menuPosition.x, EC->menuPosition.y, MENU_WIDTH, menuHeight}, 0.2f, 8, MenuColor);
    DrawRectangleRoundedLinesEx((Rectangle){EC->menuPosition.x, EC->menuPosition.y, MENU_WIDTH, menuHeight}, 0.2, 8, MENU_BORDER_THICKNESS, BorderColor);

    for (int i = 0; i < (int)MENU_VISIBLE_ITEMS; i++)
    {
        int itemIndex = i + EC->scrollIndex;
        if (itemIndex >= menuItemCount)
            break;

        Rectangle itemRect = {EC->menuPosition.x, EC->menuPosition.y + i * MENU_ITEM_HEIGHT, MENU_WIDTH, MENU_ITEM_HEIGHT};
        if (CheckCollisionPointRec(EC->mousePos, itemRect))
        {
            DrawRectangleRec(itemRect, HighlightColor);
            EC->hoveredItem = itemIndex;
            EC->submenuOpen = true;
            EC->submenuPosition.x = (EC->menuPosition.x + MENU_WIDTH + SUBMENU_WIDTH > EC->screenWidth) ? (EC->menuPosition.x - SUBMENU_WIDTH) : (EC->menuPosition.x + MENU_WIDTH);
            EC->submenuPosition.y = itemRect.y;
        }
        DrawText(menuItems[itemIndex], EC->menuPosition.x + 10, EC->menuPosition.y + i * MENU_ITEM_HEIGHT + 10, 25, WHITE);
    }

    // Scrollbar
    float scrollBarHeight = (menuHeight / (menuItemCount * 2)) * (int)MENU_VISIBLE_ITEMS;
    float scrollBarY = EC->menuPosition.y + 20 + (EC->scrollIndex * (menuHeight / menuItemCount));
    DrawRectangle(EC->menuPosition.x + MENU_WIDTH - 10, scrollBarY, 8, scrollBarHeight, ScrollIndicatorColor);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        EC->menuOpen = false;
    }

    if (EC->submenuOpen && EC->hoveredItem >= 0 && EC->hoveredItem < menuItemCount)
    {
        int subCount = subMenuCounts[EC->hoveredItem];
        float submenuHeight = subCount * MENU_ITEM_HEIGHT;
        DrawRectangle(EC->submenuPosition.x, EC->submenuPosition.y, SUBMENU_WIDTH, submenuHeight, MenuColor);
        DrawRectangleLinesEx((Rectangle){EC->submenuPosition.x, EC->submenuPosition.y, SUBMENU_WIDTH, submenuHeight}, MENU_BORDER_THICKNESS, BorderColor);
        for (int j = 0; j < subCount; j++)
        {
            Rectangle subItemRect = {EC->submenuPosition.x, EC->submenuPosition.y + j * MENU_ITEM_HEIGHT, SUBMENU_WIDTH, MENU_ITEM_HEIGHT};
            if (CheckCollisionPointRec(EC->mousePos, subItemRect))
            {
                DrawRectangleRec(subItemRect, HighlightColor);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    EC->delayFrames = true;
                    // Handle click event for submenu item
                    return subMenuItems[EC->hoveredItem][j];
                }
            }
            DrawText(subMenuItems[EC->hoveredItem][j], EC->submenuPosition.x + 10, EC->submenuPosition.y + j * MENU_ITEM_HEIGHT + 10, 25, WHITE);
        }
    }

    return "NULL";
}

void HandleDragging(EditorContext *EC, GraphContext *graph)
{
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && EC->draggingNodeIndex == -1)
    {
        SetTargetFPS(140);
        for (int i = 0; i < graph->nodeCount; i++)
        {
            if (CheckCollisionPointRec(EC->mousePos, (Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, "width"), getNodeInfoByType(graph->nodes[i].type, "height")}))
            {
                EC->draggingNodeIndex = i;
                EC->dragOffset = (Vector2){EC->mousePos.x - graph->nodes[i].position.x, EC->mousePos.y - graph->nodes[i].position.y};
                return;
            }
        }

        EC->isDraggingScreen = true;
        EC->prevMousePos = EC->mousePos;
        EC->mousePosAtStartOfDrag = EC->mousePos;
        return;
    }
    else if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && EC->draggingNodeIndex != -1)
    {
        graph->nodes[EC->draggingNodeIndex].position.x = EC->mousePos.x - EC->dragOffset.x;
        graph->nodes[EC->draggingNodeIndex].position.y = EC->mousePos.y - EC->dragOffset.y;
        DrawRectangleRounded((Rectangle){graph->nodes[EC->draggingNodeIndex].position.x, graph->nodes[EC->draggingNodeIndex].position.y, getNodeInfoByType(graph->nodes[EC->draggingNodeIndex].type, "width"), getNodeInfoByType(graph->nodes[EC->draggingNodeIndex].type, "height")}, 0.2f, 8, CLITERAL(Color){255, 255, 255, 50});
    }
    else if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && EC->isDraggingScreen)
    {
        for (int i = 0; i < graph->nodeCount; i++)
        {
            graph->nodes[i].position.x = graph->nodes[i].position.x + EC->mousePos.x - EC->prevMousePos.x;
            graph->nodes[i].position.y = graph->nodes[i].position.y + EC->mousePos.y - EC->prevMousePos.y;
        }
        EC->prevMousePos = EC->mousePos;
    }
    else if (IsMouseButtonUp(MOUSE_LEFT_BUTTON))
    {
        SetTargetFPS(60);
        EC->draggingNodeIndex = -1;
        EC->isDraggingScreen = false;
    }
}

int DrawFullTexture(EditorContext *EC, GraphContext *graph, RenderTexture2D view)
{
    BeginTextureMode(view);
    ClearBackground((Color){40, 42, 54, 255});

    HandleDragging(EC, graph);

    DrawBackgroundGrid(EC, 40);

    DrawNodes(EC, graph);

    if (EC->menuOpen)
    {
        char createdNode[MAX_TYPE_LENGTH];
        strcpy(createdNode, DrawNodeMenu(EC));
        if (strcmp(createdNode, "NULL") != 0)
        {
            CreateNode(graph, StringToNodeType(createdNode), EC->rightClickPos);
        }
    }

    EndTextureMode();

    return 0;
}

bool CheckAllCollisions(EditorContext *EC, GraphContext *graph)
{
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        EC->menuOpen = true;
        EC->rightClickPos = EC->mousePos;
        EC->scrollIndex = 0;
        EC->delayFrames = true;
    }

    return CheckNodeCollisions(EC, graph) || EC->draggingNodeIndex != -1 || IsMouseButtonDown(MOUSE_LEFT_BUTTON) || EC->lastClickedPin.id != -1 || EC->menuOpen;
}

void handleEditor(EditorContext *EC, GraphContext *graph, RenderTexture2D *viewport, Vector2 mousePos, int screenWidth, int screenHeight)
{
    EC->newLogMessage = false;

    EC->screenWidth = screenWidth;
    EC->screenHeight = screenHeight;
    EC->mousePos = mousePos;

    if (CheckAllCollisions(EC, graph))
    {
        DrawFullTexture(EC, graph, *viewport);
        EC->delayFrames = true;
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    }
    else if (EC->delayFrames == true)
    {
        DrawFullTexture(EC, graph, *viewport);
        EC->delayFrames = false;
        SetMouseCursor(MOUSE_CURSOR_ARROW);
    }
}