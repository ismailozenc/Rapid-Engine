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
    EditorContext editor = {0};

    editor.CGFilePath = malloc(MAX_PATH_LENGTH);
    editor.CGFilePath[0] = '\0';

    editor.fileName = malloc(MAX_PATH_LENGTH);
    editor.fileName[0] = '\0';

    editor.lastClickedPin = INVALID_PIN;

    editor.scrollIndexNodeMenu = 0;
    editor.hoveredItem = -1;

    editor.mousePos = (Vector2){0, 0};

    editor.screenWidth = GetScreenWidth();
    editor.screenHeight = GetScreenHeight();

    editor.draggingNodeIndex = -1;
    editor.isDraggingScreen = false;

    editor.delayFrames = true;

    editor.menuOpen = false;
    Vector2 menuPosition = {0, 0};
    Vector2 submenuPosition = {0, 0};

    editor.font = LoadFontEx("fonts/arialbd.ttf", 128, NULL, 0);
    if (editor.font.texture.id == 0)
    {
        AddToEngineLog(&editor, "Couldn't load font", 1);
    }

    editor.newLogMessage = false;

    editor.cameraOffset = (Vector2){0, 0};

    return editor;
}

void FreeEditorContext(EditorContext *editor)
{
    if (editor->CGFilePath)
        free(editor->CGFilePath);

    if (editor->fileName)
        free(editor->fileName);

    UnloadFont(editor->font);
}

void AddToEngineLog(EditorContext *editor, char *message, int level)
{
    strncpy(editor->logMessage, message, 128 * sizeof(char));
    editor->logMessageLevel = level;
    editor->newLogMessage = true;
}

void SetProjectPaths(EditorContext *editor, const char *projectName)
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

    snprintf(editor->CGFilePath, MAX_PATH_LENGTH, "%s\\Projects\\%s\\%s.cg", cwd, projectName, projectName);
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

void DrawBackgroundGrid(EditorContext *editor, int gridSpacing)
{

    if (editor->isDraggingScreen)
    {
        Vector2 delta = GetMouseDelta();
        editor->cameraOffset = Vector2Subtract(editor->cameraOffset, delta);
    }

    Vector2 offset = editor->cameraOffset;

    int startX = -((int)offset.x % gridSpacing) - gridSpacing;
    int startY = -((int)offset.y % gridSpacing) - gridSpacing;

    for (int y = startY; y < editor->screenHeight + gridSpacing; y += gridSpacing)
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

void DrawNodes(EditorContext *editor, GraphContext *graph)
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
            AddToEngineLog(editor, "Error drawing connection", 1);
        }
    }

    int hoveredNodeIndex = -1;

    for (int i = 0; i < graph->nodeCount; i++)
    {
        DrawRectangleRoundedLines((Rectangle){graph->nodes[i].position.x - 1, graph->nodes[i].position.y - 1, getNodeInfoByType(graph->nodes[i].type, "width") + 2, getNodeInfoByType(graph->nodes[i].type, "height") + 2}, 0.2f, 8, WHITE);
        DrawRectangleRounded((Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, "width"), getNodeInfoByType(graph->nodes[i].type, "height")}, 0.2f, 8, (Color){0, 0, 0, 120});
        char str[32];
        sprintf(str, "%d", i);
        DrawText(str, graph->nodes[i].position.x + 150, graph->nodes[i].position.y + 3, 30, RED);
        DrawTextEx(editor->font, NodeTypeToString(graph->nodes[i].type), (Vector2){graph->nodes[i].position.x + 10, graph->nodes[i].position.y + 3}, 30, 2, WHITE);
        if (CheckCollisionPointRec(editor->mousePos, (Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, "width"), getNodeInfoByType(graph->nodes[i].type, "height")}))
        {
            hoveredNodeIndex = i;
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
            {
                DeleteNode(graph, graph->nodes[i].id);
                editor->menuOpen = false;
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
            if (CheckCollisionPointRec(editor->mousePos, (Rectangle){nodePos.x + xOffset, nodePos.y + yOffset - 8, 15, 16}))
            {
                DrawTriangle((Vector2){nodePos.x + xOffset - 2, nodePos.y + yOffset - 10}, (Vector2){nodePos.x + xOffset - 2, nodePos.y + yOffset + 10}, (Vector2){nodePos.x + xOffset + 17, nodePos.y + yOffset}, WHITE);
                hoveredPinIndex = i;
            }
        }
        else
        {
            DrawCircle(nodePos.x + xOffset + 5, nodePos.y + yOffset, 5, WHITE);
            if (CheckCollisionPointCircle(editor->mousePos, (Vector2){nodePos.x + xOffset + 5, nodePos.y + yOffset}, 5))
            {
                DrawCircle(nodePos.x + xOffset + 5, nodePos.y + yOffset, 7, WHITE);
                hoveredPinIndex = i;
            }
        }
    }

    if (hoveredPinIndex != -1 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (editor->lastClickedPin.id == -1)
        {
            editor->lastClickedPin = graph->pins[hoveredPinIndex];
        }
        else
        {
            CreateLink(graph, editor->lastClickedPin, graph->pins[hoveredPinIndex]);
            editor->lastClickedPin = INVALID_PIN;
        }
    }

    if (hoveredPinIndex == -1 && hoveredNodeIndex != -1)
    {
        DrawRectangleRounded((Rectangle){graph->nodes[hoveredNodeIndex].position.x - 1, graph->nodes[hoveredNodeIndex].position.y - 1, getNodeInfoByType(graph->nodes[hoveredNodeIndex].type, "width") + 2, getNodeInfoByType(graph->nodes[hoveredNodeIndex].type, "height") + 2}, 0.2f, 8, (Color){255, 255, 255, 30});
        editor->delayFrames = true;
    }

    if (editor->lastClickedPin.id != -1)
    {
        if (editor->lastClickedPin.isInput)
        {
            DrawCurvedWire(editor->mousePos, editor->lastClickedPin.position, 2.0f, YELLOW);
        }
        else
        {
            DrawCurvedWire(editor->lastClickedPin.position, editor->mousePos, 2.0f, YELLOW);
        }
    }
}

bool CheckNodeCollisions(EditorContext *editor, GraphContext *graph)
{
    for (int i = 0; i < graph->nodeCount; i++)
    {
        if (CheckCollisionPointRec(editor->mousePos, (Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, "width"), getNodeInfoByType(graph->nodes[i].type, "height")}))
        {
            return true;
        }
    }

    return false;
}

const char *DrawNodeMenu(EditorContext *editor)
{
    Color MenuColor = {50, 50, 50, 255};
    Color ScrollIndicatorColor = {150, 150, 150, 255};
    Color BorderColor = {200, 200, 200, 255};
    Color HighlightColor = {80, 80, 80, 255};

    const char *menuItems[] = {"Variable", "Event", "Sprite", "Flow", "Logical", "Debug", "More"};
    const char *subMenuItems[][5] = {
        {"num", "string", "sprite", "Get var", "Set var"},
        {"Start", "On Loop", "On Button", "Create custom", "Call custom"},
        {"Spawn", "Destroy", "Move To"},
        {"Branch", "Loop"},
        {"Comparison", "Gate", "Arithmetic"},
        {"Print", "Draw Line"},
        {"ex", "Literal"}};
    int menuItemCount = sizeof(menuItems) / sizeof(menuItems[0]);
    int subMenuCounts[] = {5, 5, 3, 2, 3, 2, 2};

    float menuHeight = MENU_ITEM_HEIGHT * MENU_VISIBLE_ITEMS;

    if (GetMouseWheelMove() < 0 && editor->scrollIndexNodeMenu < menuItemCount - 5)
        editor->scrollIndexNodeMenu++;
    if (GetMouseWheelMove() > 0 && editor->scrollIndexNodeMenu > 0)
        editor->scrollIndexNodeMenu--;

    editor->menuPosition.x = editor->rightClickPos.x;
    editor->menuPosition.y = editor->rightClickPos.y;

    if (editor->menuPosition.x + MENU_WIDTH > editor->screenWidth)
    {
        editor->menuPosition.x -= MENU_WIDTH;
    }

    DrawRectangleRounded((Rectangle){editor->menuPosition.x, editor->menuPosition.y, MENU_WIDTH, menuHeight}, 0.2f, 8, MenuColor);
    DrawRectangleRoundedLinesEx((Rectangle){editor->menuPosition.x, editor->menuPosition.y, MENU_WIDTH, menuHeight}, 0.2, 8, MENU_BORDER_THICKNESS, BorderColor);

    for (int i = 0; i < (int)MENU_VISIBLE_ITEMS; i++)
    {
        int itemIndex = i + editor->scrollIndexNodeMenu;
        if (itemIndex >= menuItemCount)
            break;

        Rectangle itemRect = {editor->menuPosition.x, editor->menuPosition.y + i * MENU_ITEM_HEIGHT, MENU_WIDTH, MENU_ITEM_HEIGHT};
        if (CheckCollisionPointRec(editor->mousePos, itemRect))
        {
            DrawRectangleRec(itemRect, HighlightColor);
            editor->hoveredItem = itemIndex;
            editor->submenuPosition.x = (editor->menuPosition.x + MENU_WIDTH + SUBMENU_WIDTH > editor->screenWidth) ? (editor->menuPosition.x - SUBMENU_WIDTH) : (editor->menuPosition.x + MENU_WIDTH);
            editor->submenuPosition.y = itemRect.y;
        }
        DrawText(menuItems[itemIndex], editor->menuPosition.x + 10, editor->menuPosition.y + i * MENU_ITEM_HEIGHT + 10, 25, WHITE);
    }

    // Scrollbar
    float scrollBarHeight = (menuHeight / (menuItemCount * 2)) * (int)MENU_VISIBLE_ITEMS;
    float scrollBarY = editor->menuPosition.y + 20 + (editor->scrollIndexNodeMenu * (menuHeight / menuItemCount));
    DrawRectangle(editor->menuPosition.x + MENU_WIDTH - 10, scrollBarY, 8, scrollBarHeight, ScrollIndicatorColor);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        editor->menuOpen = false;
    }

    if (editor->hoveredItem >= 0 && editor->hoveredItem < menuItemCount)
    {
        int subCount = subMenuCounts[editor->hoveredItem];
        float submenuHeight = subCount * MENU_ITEM_HEIGHT;
        DrawRectangle(editor->submenuPosition.x, editor->submenuPosition.y, SUBMENU_WIDTH, submenuHeight, MenuColor);
        DrawRectangleLinesEx((Rectangle){editor->submenuPosition.x, editor->submenuPosition.y, SUBMENU_WIDTH, submenuHeight}, MENU_BORDER_THICKNESS, BorderColor);
        for (int j = 0; j < subCount; j++)
        {
            Rectangle subItemRect = {editor->submenuPosition.x, editor->submenuPosition.y + j * MENU_ITEM_HEIGHT, SUBMENU_WIDTH, MENU_ITEM_HEIGHT};
            if (CheckCollisionPointRec(editor->mousePos, subItemRect))
            {
                DrawRectangleRec(subItemRect, HighlightColor);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    editor->delayFrames = true;
                    // Handle click event for submenu item
                    return subMenuItems[editor->hoveredItem][j];
                }
            }
            DrawText(subMenuItems[editor->hoveredItem][j], editor->submenuPosition.x + 10, editor->submenuPosition.y + j * MENU_ITEM_HEIGHT + 10, 25, WHITE);
        }
    }

    return "NULL";
}

void HandleDragging(EditorContext *editor, GraphContext *graph)
{
    static Vector2 dragOffset;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && editor->draggingNodeIndex == -1)
    {
        editor->fps = 140;
        for (int i = 0; i < graph->nodeCount; i++)
        {
            if (CheckCollisionPointRec(editor->mousePos, (Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, "width"), getNodeInfoByType(graph->nodes[i].type, "height")}))
            {
                editor->draggingNodeIndex = i;
                dragOffset = (Vector2){editor->mousePos.x - graph->nodes[i].position.x, editor->mousePos.y - graph->nodes[i].position.y};
                return;
            }
        }

        editor->isDraggingScreen = true;
        return;
    }
    else if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && editor->draggingNodeIndex != -1)
    {
        graph->nodes[editor->draggingNodeIndex].position.x = editor->mousePos.x - dragOffset.x;
        graph->nodes[editor->draggingNodeIndex].position.y = editor->mousePos.y - dragOffset.y;
        DrawRectangleRounded((Rectangle){graph->nodes[editor->draggingNodeIndex].position.x, graph->nodes[editor->draggingNodeIndex].position.y, getNodeInfoByType(graph->nodes[editor->draggingNodeIndex].type, "width"), getNodeInfoByType(graph->nodes[editor->draggingNodeIndex].type, "height")}, 0.2f, 8, CLITERAL(Color){255, 255, 255, 50});
    }
    else if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && editor->isDraggingScreen)
    {
        for (int i = 0; i < graph->nodeCount; i++)
        {
            Vector2 delta = GetMouseDelta();
            graph->nodes[i].position.x += delta.x;
            graph->nodes[i].position.y += delta.y;
        }
    }
    else if (IsMouseButtonUp(MOUSE_LEFT_BUTTON))
    {
        editor->fps = 60;
        editor->draggingNodeIndex = -1;
        editor->isDraggingScreen = false;
    }
}

int DrawFullTexture(EditorContext *editor, GraphContext *graph, RenderTexture2D view)
{
    BeginTextureMode(view);
    ClearBackground((Color){40, 42, 54, 255});

    HandleDragging(editor, graph);

    DrawBackgroundGrid(editor, 40);

    DrawNodes(editor, graph);

    if (editor->menuOpen)
    {
        char createdNode[MAX_TYPE_LENGTH];
        strcpy(createdNode, DrawNodeMenu(editor));
        if (strcmp(createdNode, "NULL") != 0)
        {
            CreateNode(graph, StringToNodeType(createdNode), editor->rightClickPos);
        }
    }

    EndTextureMode();

    return 0;
}

bool CheckAllCollisions(EditorContext *editor, GraphContext *graph)
{
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        editor->menuOpen = true;
        editor->rightClickPos = editor->mousePos;
        editor->scrollIndexNodeMenu = 0;
        editor->delayFrames = true;
    }

    return CheckNodeCollisions(editor, graph) || editor->draggingNodeIndex != -1 || IsMouseButtonDown(MOUSE_LEFT_BUTTON) || editor->lastClickedPin.id != -1 || editor->menuOpen;
}

void HandleEditor(EditorContext *editor, GraphContext *graph, RenderTexture2D *viewport, Vector2 mousePos, int screenWidth, int screenHeight, bool draggingDisabled)
{
    editor->newLogMessage = false;
    editor->cursor = MOUSE_CURSOR_ARROW;

    editor->screenWidth = screenWidth;
    editor->screenHeight = screenHeight;
    editor->mousePos = mousePos;

    if(draggingDisabled){
        return;
    }

    if (CheckAllCollisions(editor, graph))
    {
        DrawFullTexture(editor, graph, *viewport);
        editor->delayFrames = true;
        editor->cursor = MOUSE_CURSOR_POINTING_HAND;
    }
    else if (editor->delayFrames == true)
    {
        DrawFullTexture(editor, graph, *viewport);
        editor->delayFrames = false;
    }
}