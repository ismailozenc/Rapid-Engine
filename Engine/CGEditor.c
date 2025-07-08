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

    editor.lastClickedPin = INVALID_PIN;

    editor.scrollIndexNodeMenu = 0;
    editor.hoveredItem = -1;

    editor.mousePos = (Vector2){0, 0};

    editor.screenWidth = GetScreenWidth();
    editor.screenHeight = GetScreenHeight();

    editor.draggingNodeIndex = -1;
    editor.isDraggingScreen = false;

    editor.delayFrames = true;
    editor.isFirstFrame = true;

    editor.menuOpen = false;
    Vector2 menuPosition = {0, 0};
    Vector2 submenuPosition = {0, 0};

    editor.labelClicked = -1;
    editor.nodeTextBox = (TextBox){0};
    editor.nodeTextBox.bounds = (Rectangle){0, 0, 200, 30}; // dummy init
    editor.nodeTextBox.length = 0;
    editor.nodeTextBox.text[0] = '\0';

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
    UnloadFont(editor->font);
}

void AddToEngineLog(EditorContext *editor, char *message, int level)
{
    strncpy(editor->logMessage, message, 128 * sizeof(char));
    editor->logMessageLevel = level;
    editor->newLogMessage = true;
}

void DrawBackgroundGrid(EditorContext *editor, int gridSpacing, RenderTexture2D dot)
{
    if (editor->isDraggingScreen)
    {
        Vector2 delta = Vector2Scale(GetMouseDelta(), 1.0f / editor->zoom);
        editor->cameraOffset = Vector2Subtract(editor->cameraOffset, Vector2Scale(delta, 1.0f / editor->zoom));
    }
    gridSpacing /= editor->zoom;

    const float maxOffset = 100000.0f;
    editor->cameraOffset.x = Clamp(editor->cameraOffset.x, -maxOffset, maxOffset);
    editor->cameraOffset.y = Clamp(editor->cameraOffset.y, -maxOffset, maxOffset);

    Vector2 offset = editor->cameraOffset;

    float worldLeft = offset.x;
    float worldTop = offset.y;
    float worldRight = offset.x + editor->screenWidth / editor->zoom;
    float worldBottom = offset.y + editor->screenHeight / editor->zoom;

    int startX = ((int)worldLeft / gridSpacing) * gridSpacing - gridSpacing;
    int startY = ((int)worldTop / gridSpacing) * gridSpacing - gridSpacing;
    int endX = ((int)worldRight / gridSpacing) * gridSpacing + gridSpacing;
    int endY = ((int)worldBottom / gridSpacing) * gridSpacing + gridSpacing;

    for (int y = startY; y <= endY; y += gridSpacing)
    {
        int row = y / gridSpacing;
        for (int x = startX; x <= endX; x += gridSpacing)
        {
            float drawX = x + (row % 2) * (gridSpacing / 2);
            float drawY = (float)y;

            float screenX = (drawX - offset.x) * editor->zoom;
            float screenY = (drawY - offset.y) * editor->zoom;

            DrawTextureRec(dot.texture, (Rectangle){0, 0, (float)dot.texture.width, (float)-dot.texture.height}, (Vector2){screenX, screenY}, (Color){255, 255, 255, 10});
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

bool CheckCollisionRoundedTopRect(Rectangle rect, float radius, Vector2 point)
{
    Rectangle body = {
        rect.x,
        rect.y + radius,
        rect.width,
        rect.height - radius};

    if (CheckCollisionPointRec(point, body))
        return true;

    Vector2 topLeft = {rect.x + radius, rect.y + radius};
    if (CheckCollisionPointCircle(point, topLeft, radius))
        return true;

    Vector2 topRight = {rect.x + rect.width - radius, rect.y + radius};
    if (CheckCollisionPointCircle(point, topRight, radius))
        return true;

    Rectangle topMid = {
        rect.x + radius,
        rect.y,
        rect.width - 2 * radius,
        radius};
    return CheckCollisionPointRec(point, topMid);
}

void DrawArcOutline(Vector2 center, float radius, float startAngle, float endAngle, int segments, float thickness, Color color)
{
    float step = (endAngle - startAngle) / segments;
    Vector2 prev = {
        center.x + cosf(DEG2RAD * startAngle) * radius,
        center.y + sinf(DEG2RAD * startAngle) * radius};

    for (int i = 1; i <= segments; i++)
    {
        float angle = startAngle + step * i;
        Vector2 next = {
            center.x + cosf(DEG2RAD * angle) * radius,
            center.y + sinf(DEG2RAD * angle) * radius};
        DrawLineEx(prev, next, thickness, color);
        prev = next;
    }
}

void DrawRoundedTopRectOutline(Rectangle rect, float radius, float thickness, Color color)
{
    float half = thickness * 0.5f;
    float r = radius - half;

    DrawLineEx((Vector2){rect.x + r, rect.y}, (Vector2){rect.x + rect.width - r, rect.y}, thickness, color);

    DrawArcOutline((Vector2){rect.x + r, rect.y + r}, r, 180, 270, 16, thickness, color);
    DrawArcOutline((Vector2){rect.x + rect.width - r, rect.y + r}, r, 270, 360, 16, thickness, color);

    DrawLineEx(
        (Vector2){rect.x, rect.y + r},
        (Vector2){rect.x, rect.y + rect.height}, thickness, color);
    DrawLineEx(
        (Vector2){rect.x + rect.width, rect.y + r},
        (Vector2){rect.x + rect.width, rect.y + rect.height}, thickness, color);

    DrawLineEx(
        (Vector2){rect.x, rect.y + rect.height},
        (Vector2){rect.x + rect.width, rect.y + rect.height}, thickness, color);
}

void DrawTextBox(TextBox *box)
{
    DrawRectangleRounded(box->bounds, 0.2f, 8, box->editing ? GRAY : DARKGRAY);
    DrawRectangleLinesEx(box->bounds, 2, WHITE);
    DrawText(box->text, box->bounds.x + 4, box->bounds.y + 6, 20, WHITE);
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
        bool isFlowConnection = false;
        for (int j = 0; j < graph->pinCount; j++)
        {
            if (graph->links[i].inputPinID == graph->pins[j].id)
            {
                inputPinPosition = graph->pins[j].position;
            }
            else if (graph->links[i].outputPinID == graph->pins[j].id)
            {
                outputPinPosition = graph->pins[j].position;
                isFlowConnection = (graph->pins[j].type == PIN_FLOW);
            }
        }
        if (inputPinPosition.x != -1 && outputPinPosition.x != -1)
        {
            DrawCurvedWire(outputPinPosition, inputPinPosition, 2.0f + 2.0f / editor->zoom, isFlowConnection ? (Color){180, 100, 200, 255} : (Color){0, 255, 255, 255});
        }
        else
        {
            AddToEngineLog(editor, "Error drawing connection", 1);
        }
    }

    int hoveredNodeIndex = -1;
    int nodeToDelete = -1;

    for (int i = 0; i < graph->nodeCount; i++)
    {
        float x = graph->nodes[i].position.x;
        float y = graph->nodes[i].position.y;
        float width = getNodeInfoByType(graph->nodes[i].type, "width");
        float height = getNodeInfoByType(graph->nodes[i].type, "height");
        float roundness = 0.2f;
        float segments = 8;
        Color nodeColor = getNodeColorByType(graph->nodes[i].type);

        float fullRadius = roundness * fminf(width, height) / 2.0f;

        DrawRectangleRounded(
            (Rectangle){x, y, width, height},
            roundness, segments, (Color){0, 0, 0, 120});

        DrawRectangleRec(
            (Rectangle){x + fullRadius - 2, y - 2, width - 2 * fullRadius + 4, fullRadius},
            nodeColor);

        DrawRectangleRec(
            (Rectangle){x - 2, y + fullRadius - 2, width + 4, 35 - fullRadius},
            nodeColor);

        DrawCircleSector(
            (Vector2){x + fullRadius - 2, y + fullRadius - 2},
            fullRadius, 180, 270, segments, nodeColor);

        DrawCircleSector(
            (Vector2){x + width - fullRadius + 2, y + fullRadius - 2},
            fullRadius, 270, 360, segments, nodeColor);

        DrawRectangleRoundedLinesEx(
            (Rectangle){x - 1, y - 1, width + 2, height + 2},
            roundness, segments, 2.0f / editor->zoom, WHITE);

        if (CheckCollisionRoundedTopRect((Rectangle){x - 1, y, width + 1, 35}, fullRadius, editor->mousePos) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && getIsEditableByType(graph->nodes[i].type))
        {
            editor->labelClicked = i;
        }
        if (editor->labelClicked == i)
        {
            DrawRoundedTopRectOutline((Rectangle){x - 2, y - 2, width + 5, 37}, fullRadius, 3.0f, WHITE);
            editor->nodeTextBox.bounds = (Rectangle){x, y - 40, width, 30};
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                editor->nodeTextBox.editing = CheckCollisionPointRec(editor->mousePos, editor->nodeTextBox.bounds);
            }

            if (editor->nodeTextBox.editing)
            {
                int key = GetCharPressed();
                while (key > 0)
                {
                    if (editor->nodeTextBox.length < 255)
                    {
                        editor->nodeTextBox.text[editor->nodeTextBox.length++] = (char)key;
                        editor->nodeTextBox.text[editor->nodeTextBox.length] = '\0';
                    }
                    key = GetCharPressed();
                }

                if (IsKeyPressed(KEY_BACKSPACE) && editor->nodeTextBox.length > 0)
                {
                    editor->nodeTextBox.text[--editor->nodeTextBox.length] = '\0';
                }

                if (IsKeyPressed(KEY_ENTER))
                {
                    editor->nodeTextBox.editing = false;
                }
            }
            DrawTextBox(&editor->nodeTextBox);
        }

        DrawTextEx(editor->font, NodeTypeToString(graph->nodes[i].type),
                   (Vector2){x + 10, y + 3}, 30, 2, WHITE);

        if (CheckCollisionPointRec(editor->mousePos, (Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, "width"), getNodeInfoByType(graph->nodes[i].type, "height")}))
        {
            hoveredNodeIndex = i;
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
            {
                nodeToDelete = graph->nodes[i].id;
            }
        }
    }

    if (hoveredNodeIndex == -1 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(editor->mousePos, editor->nodeTextBox.bounds))
    {
        editor->labelClicked = -1;
        editor->nodeTextBox.editing = false;
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
            if (CheckCollisionPointRec(editor->mousePos, (Rectangle){nodePos.x + xOffset - 5, nodePos.y + yOffset - 15, 25, 31}))
            {
                DrawTriangle((Vector2){nodePos.x + xOffset - 2, nodePos.y + yOffset - 10}, (Vector2){nodePos.x + xOffset - 2, nodePos.y + yOffset + 10}, (Vector2){nodePos.x + xOffset + 17, nodePos.y + yOffset}, WHITE);
                hoveredPinIndex = i;
            }
        }
        else if (graph->pins[i].type == PIN_FIELD)
        {
            editor->nodeTextBox.bounds = (Rectangle){graph->pins[i].position.x, graph->pins[i].position.y, 30, 30};
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                editor->nodeTextBox.editing = CheckCollisionPointRec(editor->mousePos, editor->nodeTextBox.bounds);
            }

            if (editor->nodeTextBox.editing)
            {
                int key = GetCharPressed();
                while (key > 0)
                {
                    if (editor->nodeTextBox.length < 255)
                    {
                        editor->nodeTextBox.text[editor->nodeTextBox.length++] = (char)key;
                        editor->nodeTextBox.text[editor->nodeTextBox.length] = '\0';
                    }
                    key = GetCharPressed();
                }

                if (IsKeyPressed(KEY_BACKSPACE) && editor->nodeTextBox.length > 0)
                {
                    editor->nodeTextBox.text[--editor->nodeTextBox.length] = '\0';
                }

                if (IsKeyPressed(KEY_ENTER))
                {
                    editor->nodeTextBox.editing = false;
                    //graph->nodes[i].
                }
            }
            DrawTextBox(&editor->nodeTextBox);
        }
        else
        {
            DrawCircle(nodePos.x + xOffset + 5, nodePos.y + yOffset, 5, WHITE);
            if (CheckCollisionPointCircle(editor->mousePos, (Vector2){nodePos.x + xOffset + 5, nodePos.y + yOffset}, 12))
            {
                if (graph->pins[i].isInput)
                {
                    DrawTextEx(editor->font, getNodeInputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], (Vector2){(2 * nodePos.x + getNodeInfoByType(graph->nodes[currNodeIndex].type, "width")) / 2 - MeasureTextEx(editor->font, getNodeInputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], 18, 0).x / 2, nodePos.y + yOffset - 8}, 18, 0, WHITE);
                    DrawLine(graph->pins[i].position.x, graph->pins[i].position.y, (2 * nodePos.x + getNodeInfoByType(graph->nodes[currNodeIndex].type, "width")) / 2 - MeasureTextEx(editor->font, getNodeInputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], 18, 0).x / 2 - 5, graph->pins[i].position.y, WHITE);
                }
                else
                {
                    DrawTextEx(editor->font, getNodeOutputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], (Vector2){(2 * nodePos.x + getNodeInfoByType(graph->nodes[currNodeIndex].type, "width")) / 2 - MeasureTextEx(editor->font, getNodeOutputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], 18, 0).x / 2, nodePos.y + yOffset - 8}, 18, 0, WHITE);
                    DrawLine(graph->pins[i].position.x, graph->pins[i].position.y, (2 * nodePos.x + getNodeInfoByType(graph->nodes[currNodeIndex].type, "width")) / 2 + MeasureTextEx(editor->font, getNodeOutputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], 18, 0).x / 2 + 5, graph->pins[i].position.y, WHITE);
                }
                DrawCircle(nodePos.x + xOffset + 5, nodePos.y + yOffset, 7, WHITE);
                hoveredPinIndex = i;
            }
        }
    }

    if (hoveredPinIndex != -1 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        editor->draggingNodeIndex = -1;
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
    else if (hoveredPinIndex == -1 && (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)))
    {
        editor->lastClickedPin = INVALID_PIN;
    }
    else if (hoveredPinIndex != -1 && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        RemoveConnections(graph, graph->pins[hoveredPinIndex].id);
        editor->menuOpen = false;
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
            DrawCurvedWire(editor->mousePos, editor->lastClickedPin.position, 2.0f + 2.0f / editor->zoom, YELLOW);
        }
        else
        {
            DrawCurvedWire(editor->lastClickedPin.position, editor->mousePos, 2.0f + 2.0f / editor->zoom, YELLOW);
        }
    }

    if (nodeToDelete != -1 && hoveredPinIndex == -1)
    {
        DeleteNode(graph, nodeToDelete);
        editor->menuOpen = false;
        return;
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
            Vector2 delta = Vector2Scale(GetMouseDelta(), 1.0f / editor->zoom);
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

int DrawFullTexture(EditorContext *editor, GraphContext *graph, RenderTexture2D view, RenderTexture2D dot)
{
    BeginTextureMode(view);
    ClearBackground((Color){40, 42, 54, 255});

    HandleDragging(editor, graph);

    DrawBackgroundGrid(editor, 40, dot);

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

    return CheckNodeCollisions(editor, graph) || editor->draggingNodeIndex != -1 || IsMouseButtonDown(MOUSE_LEFT_BUTTON) || editor->lastClickedPin.id != -1 || editor->menuOpen || editor->labelClicked != -1;
}

void HandleEditor(EditorContext *editor, GraphContext *graph, RenderTexture2D *viewport, Vector2 mousePos, bool draggingDisabled)
{
    editor->newLogMessage = false;
    editor->cursor = MOUSE_CURSOR_ARROW;

    editor->screenWidth = viewport->texture.width;
    editor->screenHeight = viewport->texture.height;
    editor->mousePos = mousePos;

    static RenderTexture2D dot;

    if (editor->isFirstFrame)
    {
        dot = LoadRenderTexture(15, 15);
        SetTextureFilter(dot.texture, TEXTURE_FILTER_BILINEAR);
        BeginTextureMode(dot);
        ClearBackground(BLANK);
        DrawRectangleRounded((Rectangle){0, 0, 15, 15}, 1.0f, 8, (Color){128, 128, 128, 255});
        EndTextureMode();

        editor->isFirstFrame = false;
    }

    if (draggingDisabled)
    {
        return;
    }

    if (CheckAllCollisions(editor, graph))
    {
        DrawFullTexture(editor, graph, *viewport, dot);
        editor->delayFrames = true;
        editor->cursor = MOUSE_CURSOR_POINTING_HAND;
    }
    else if (editor->delayFrames == true)
    {
        DrawFullTexture(editor, graph, *viewport, dot);
        editor->delayFrames = false;
    }
}