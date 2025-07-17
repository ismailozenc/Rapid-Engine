#include "CGEditor.h"
#include <direct.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include "shell_execute.h"
#include "raymath.h"
#include "rlgl.h"

void AddToLogFromEditor(EditorContext *editor, char *message, int level);

EditorContext InitEditorContext()
{
    EditorContext editor = {0};

    editor.lastClickedPin = INVALID_PIN;

    editor.scrollIndexNodeMenu = 0;
    editor.hoveredItem = -1;

    editor.mousePos = (Vector2){0, 0};

    editor.draggingNodeIndex = -1;
    editor.isDraggingScreen = false;

    editor.delayFrames = true;
    editor.isFirstFrame = true;
    editor.engineDelayFrames = false;

    editor.menuOpen = false;
    Vector2 menuPosition = {0, 0};
    Vector2 submenuPosition = {0, 0};

    editor.gearTxt = LoadTexture("textures//gear.png");
    if (editor.gearTxt.id == 0)
    {
        // Error
    }

    editor.nodeDropdownFocused = -1;
    editor.nodeFieldPinFocused = -1;

    editor.font = LoadFontEx("fonts//arialbd.ttf", 128, NULL, 0);
    if (editor.font.texture.id == 0)
    {
        AddToLogFromEditor(&editor, "Couldn't load font", 1);
    }

    editor.newLogMessage = false;

    editor.cameraOffset = (Vector2){0, 0};

    editor.editingNodeNameIndex = -1;

    editor.hasChanged = false;
    editor.hasChangedInLastFrame = false;

    return editor;
}

void FreeEditorContext(EditorContext *editor)
{
    UnloadTexture(editor->gearTxt);

    UnloadFont(editor->font);
}

void AddToLogFromEditor(EditorContext *editor, char *message, int level)
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

    DrawLineEx(outputPos, (Vector2){outputPos.x + 17, outputPos.y}, thickness, color);
    outputPos.x += 17;
    inputPos.x -= 12;

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

    DrawLineEx(inputPos, (Vector2){inputPos.x + 12, inputPos.y}, thickness, color);
}

void HandleVarTextBox(EditorContext *editor, Rectangle bounds, char *text, int index)
{
    bounds.width = MeasureTextEx(editor->font, text, 16, 2).x + 25;

    DrawRectangleRounded((Rectangle){bounds.x, bounds.y - 20, 56, bounds.height}, 0.4f, 4, DARKGRAY);
    DrawTextEx(editor->font, "Name:", (Vector2){bounds.x + 4, bounds.y - 16}, 14, 2, WHITE);

    DrawRectangleRounded(bounds, 0.6f, 4, GRAY);
    DrawRectangleRoundedLinesEx(bounds, 0.6f, 4, 2, DARKGRAY);

    bool showCursor = ((int)(GetTime() * 2) % 2) == 0;
    char buffer[130];
    snprintf(buffer, sizeof(buffer), "%s%s", text, showCursor ? "_" : " ");
    DrawTextEx(editor->font, buffer, (Vector2){bounds.x + 5, bounds.y + 8}, 16, 2, BLACK);

    int key = GetCharPressed();

    if (key > 0)
    {
        int len = strlen(text);
        if (len < 127 && key >= 32 && key <= 125)
        {
            text[len] = (char)key;
            text[len + 1] = '\0';
        }
        editor->hasChanged = true;
        editor->hasChangedInLastFrame = true;
    }

    if (IsKeyPressed(KEY_BACKSPACE))
    {
        int len = strlen(text);
        if (len > 0)
            text[len - 1] = '\0';
        editor->hasChanged = true;
        editor->hasChangedInLastFrame = true;
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
            AddToLogFromEditor(editor, "Error drawing connection", 1);
        }
    }

    int hoveredNodeIndex = -1;
    int nodeToDelete = -1;
    static Rectangle textBoxRect = {0};

    for (int i = 0; i < graph->nodeCount; i++)
    {
        float x = graph->nodes[i].position.x;
        float y = graph->nodes[i].position.y;
        float width = getNodeInfoByType(graph->nodes[i].type, WIDTH);
        float height = getNodeInfoByType(graph->nodes[i].type, HEIGHT);
        float roundness = 0.2f;
        float segments = 8;
        Color nodeLeftGradientColor = getNodeColorByType(graph->nodes[i].type);
        nodeLeftGradientColor.r += 30;
        nodeLeftGradientColor.g += 30;
        nodeLeftGradientColor.b += 30;

        float fullRadius = roundness * fminf(width, height) / 2.0f;

        Color nodeRightGradientColor = (Color){(unsigned char)(nodeLeftGradientColor.r > 80 ? nodeLeftGradientColor.r - 80 : 0), (unsigned char)(nodeLeftGradientColor.g > 80 ? nodeLeftGradientColor.g - 80 : 0), (unsigned char)(nodeLeftGradientColor.b > 80 ? nodeLeftGradientColor.b - 80 : 0), nodeLeftGradientColor.a};

        DrawRectangleRounded(
            (Rectangle){x, y, width, height},
            roundness, segments, (Color){0, 0, 0, 120});

        DrawRectangleGradientH(x + fullRadius - 2, y - 2, width - 2 * fullRadius + 4, fullRadius, nodeLeftGradientColor, nodeRightGradientColor);

        DrawRectangleGradientH(x - 2, y + fullRadius - 2, width + 4, 35 - fullRadius, nodeLeftGradientColor, nodeRightGradientColor);

        DrawCircleSector(
            (Vector2){x + fullRadius - 2, y + fullRadius - 2},
            fullRadius, 180, 270, segments, nodeLeftGradientColor);

        DrawCircleSector(
            (Vector2){x + width - fullRadius + 2, y + fullRadius - 2},
            fullRadius, 270, 360, segments, nodeRightGradientColor);

        DrawRectangleRoundedLinesEx(
            (Rectangle){x - 1, y - 1, width + 2, height + 2},
            roundness, segments, 2.0f / editor->zoom, WHITE);

        DrawTextEx(editor->font, NodeTypeToString(graph->nodes[i].type),
                   (Vector2){x + 10, y + 3}, 30, 1, WHITE);

        if (getIsEditableByType(graph->nodes[i].type))
        {
            Rectangle gearRect = {graph->nodes[i].position.x + getNodeInfoByType(graph->nodes[i].type, WIDTH) - 18 - fullRadius/5, graph->nodes[i].position.y + 5 + fullRadius/5, 16, 16};
            DrawTexture(editor->gearTxt, gearRect.x, gearRect.y, WHITE);

            if (CheckCollisionPointRec(editor->mousePos, gearRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                editor->editingNodeNameIndex = i;
                textBoxRect = (Rectangle){graph->nodes[i].position.x + getNodeInfoByType(graph->nodes[i].type, WIDTH) + 10, graph->nodes[i].position.y, MeasureTextEx(editor->font, graph->nodes[i].name, 16, 2).x + 25, 30};
            }
            else if (editor->editingNodeNameIndex == i)
            {
                HandleVarTextBox(editor, textBoxRect, graph->nodes[editor->editingNodeNameIndex].name, editor->editingNodeNameIndex);
                editor->delayFrames = true;

                if (CheckCollisionPointRec(editor->mousePos, textBoxRect))
                    editor->isDraggingScreen = false;

                if (IsKeyPressed(KEY_ENTER) || (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(editor->mousePos, textBoxRect)))
                {
                    editor->editingNodeNameIndex = -1;
                    editor->engineDelayFrames = true;
                }
            }
        }

        if (CheckCollisionPointRec(editor->mousePos, (Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, WIDTH), getNodeInfoByType(graph->nodes[i].type, HEIGHT)}))
        {
            hoveredNodeIndex = i;
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
            {
                nodeToDelete = graph->nodes[i].id;
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
        int xOffset = graph->pins[i].isInput ? 5 : (getNodeInfoByType(graph->nodes[currNodeIndex].type, WIDTH) - 20);
        int yOffset = 50 + graph->pins[i].posInNode * 30;

        graph->pins[i].position = (Vector2){nodePos.x + xOffset + 5, nodePos.y + yOffset};

        if (graph->pins[i].type == PIN_NONE)
        {
            continue;
        }
        else if (graph->pins[i].type == PIN_FLOW)
        {
            DrawTriangle((Vector2){nodePos.x + xOffset, nodePos.y + yOffset - 8}, (Vector2){nodePos.x + xOffset, nodePos.y + yOffset + 8}, (Vector2){nodePos.x + xOffset + 15, nodePos.y + yOffset}, WHITE);
            if (CheckCollisionPointRec(editor->mousePos, (Rectangle){nodePos.x + xOffset - 5, nodePos.y + yOffset - 15, 25, 31}))
            {
                if (graph->pins[i].isInput)
                {
                    DrawTextEx(editor->font, getNodeInputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], (Vector2){(2 * nodePos.x + getNodeInfoByType(graph->nodes[currNodeIndex].type, WIDTH)) / 2 - MeasureTextEx(editor->font, getNodeInputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], 18, 0).x / 2, nodePos.y + yOffset - 8}, 18, 0, WHITE);
                    DrawLine(graph->pins[i].position.x, graph->pins[i].position.y, (2 * nodePos.x + getNodeInfoByType(graph->nodes[currNodeIndex].type, WIDTH)) / 2 - MeasureTextEx(editor->font, getNodeInputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], 18, 0).x / 2 - 5, graph->pins[i].position.y, WHITE);
                }
                else
                {
                    DrawTextEx(editor->font, getNodeOutputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], (Vector2){(2 * nodePos.x + getNodeInfoByType(graph->nodes[currNodeIndex].type, WIDTH)) / 2 - MeasureTextEx(editor->font, getNodeOutputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], 18, 0).x / 2 - 5, nodePos.y + yOffset - 8}, 18, 0, WHITE);
                    DrawLine(graph->pins[i].position.x, graph->pins[i].position.y, (2 * nodePos.x + getNodeInfoByType(graph->nodes[currNodeIndex].type, WIDTH)) / 2 + MeasureTextEx(editor->font, getNodeOutputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], 18, 0).x / 2, graph->pins[i].position.y, WHITE);
                }
                DrawTriangle((Vector2){nodePos.x + xOffset - 2, nodePos.y + yOffset - 10}, (Vector2){nodePos.x + xOffset - 2, nodePos.y + yOffset + 10}, (Vector2){nodePos.x + xOffset + 17, nodePos.y + yOffset}, WHITE);
                hoveredPinIndex = i;
            }
        }
        else if (graph->pins[i].type == PIN_COMPARISON_OPERATOR || graph->pins[i].type == PIN_GATE || graph->pins[i].type == PIN_ARITHMETIC)
        {
            DropdownOptionsByPinType options = getPinDropdownOptionsByType(graph->pins[i].type);

            Rectangle dropdown = {graph->pins[i].position.x - 6, graph->pins[i].position.y - 12, options.boxWidth, 24};

            DrawRectangleRec(dropdown, GRAY);
            DrawTextEx(editor->font, options.options[graph->pins[i].pickedOption], (Vector2){dropdown.x + 3, dropdown.y + 3}, 20, 0, BLACK);
            DrawRectangleLinesEx(dropdown, 1, WHITE);

            bool mouseOnDropdown = CheckCollisionPointRec(editor->mousePos, dropdown);
            bool mouseOnOptions = false;
            if (editor->nodeDropdownFocused == i)
            {
                for (int j = 0; j < options.optionsCount; j++)
                {
                    Rectangle option = {dropdown.x, dropdown.y - (j + 1) * 30, dropdown.width, 30};
                    if (CheckCollisionPointRec(editor->mousePos, option))
                    {
                        mouseOnOptions = true;
                        break;
                    }
                }
            }

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (editor->nodeDropdownFocused == i)
                {
                    if (mouseOnDropdown)
                        editor->nodeDropdownFocused = -1;
                    else if (!mouseOnOptions)
                        editor->nodeDropdownFocused = -1;
                }
                else if (mouseOnDropdown)
                {
                    editor->nodeDropdownFocused = i;
                }
            }

            if (editor->nodeDropdownFocused == i)
            {
                editor->delayFrames = true;
                hoveredNodeIndex = -1;

                for (int j = 0; j < options.optionsCount; j++)
                {
                    Rectangle option = {dropdown.x, dropdown.y - (j + 1) * 30, dropdown.width, 30};
                    DrawRectangleRec(option, RAYWHITE);
                    DrawTextEx(editor->font, options.options[j], (Vector2){option.x + 3, option.y + 3}, 20, 0, BLACK);
                    DrawRectangleLinesEx(option, 1, DARKGRAY);

                    if (CheckCollisionPointRec(editor->mousePos, option) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                    {
                        graph->pins[i].pickedOption = j;
                        editor->nodeDropdownFocused = -1;
                        editor->hasChanged = true;
                        editor->hasChangedInLastFrame = true;
                    }
                }
            }
        }
        else if (graph->pins[i].type == PIN_FIELD_NUM)
        {
            Rectangle textbox = {graph->pins[i].position.x - 6, graph->pins[i].position.y - 12, 150, 24};

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (CheckCollisionPointRec(editor->mousePos, textbox))
                {
                    editor->nodeFieldPinFocused = i;
                }
                else if (editor->nodeFieldPinFocused == i)
                {
                    editor->nodeFieldPinFocused = -1;
                }
            }

            DrawRectangleRec(textbox, (editor->nodeFieldPinFocused == i) ? LIGHTGRAY : GRAY);
            DrawRectangleLinesEx(textbox, 1, WHITE);

            DrawTextEx(editor->font, graph->pins[i].textFieldValue, (Vector2){textbox.x + 5, textbox.y + 4}, 20, 0, BLACK);

            if (editor->nodeFieldPinFocused == i)
            {
                int key = GetCharPressed();
                while (key > 0)
                {
                    if ((key >= 48) && (key <= 57) && strlen(graph->pins[i].textFieldValue) < 255)
                    {
                        int len = strlen(graph->pins[i].textFieldValue);
                        graph->pins[i].textFieldValue[len] = (char)key;
                        graph->pins[i].textFieldValue[len + 1] = '\0';
                    }
                    else if (key == 46 && !graph->pins[i].isFloat)
                    {
                        int len = strlen(graph->pins[i].textFieldValue);
                        graph->pins[i].textFieldValue[len] = (char)key;
                        graph->pins[i].textFieldValue[len + 1] = '\0';
                        graph->pins[i].isFloat = true;
                    }
                    key = GetCharPressed();
                }

                if (IsKeyPressed(KEY_BACKSPACE) && strlen(graph->pins[i].textFieldValue) > 0)
                {
                    size_t len = strlen(graph->pins[i].textFieldValue);
                    if (graph->pins[i].textFieldValue[len - 1] == 46)
                    {
                        graph->pins[i].isFloat = false;
                    }
                    graph->pins[i].textFieldValue[len - 1] = '\0';
                }

                if (IsKeyPressed(KEY_ENTER))
                {
                    editor->nodeFieldPinFocused = -1;
                }
            }
        }
        else if (graph->pins[i].type == PIN_FIELD_STRING)
        {
            Rectangle textbox = {graph->pins[i].position.x - 6, graph->pins[i].position.y - 12, 150, 24};

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (CheckCollisionPointRec(editor->mousePos, textbox))
                {
                    editor->nodeFieldPinFocused = i;
                }
                else if (editor->nodeFieldPinFocused == i)
                {
                    editor->nodeFieldPinFocused = -1;
                }
            }

            DrawRectangleRec(textbox, (editor->nodeFieldPinFocused == i) ? LIGHTGRAY : GRAY);
            DrawRectangleLinesEx(textbox, 1, WHITE);

            DrawTextEx(editor->font, graph->pins[i].textFieldValue, (Vector2){textbox.x + 5, textbox.y + 4}, 20, 0, BLACK);

            if (editor->nodeFieldPinFocused == i)
            {
                int key = GetCharPressed();
                while (key > 0)
                {
                    if ((key >= 32) && (key <= 126) && strlen(graph->pins[i].textFieldValue) < 255)
                    {
                        int len = strlen(graph->pins[i].textFieldValue);
                        graph->pins[i].textFieldValue[len] = (char)key;
                        graph->pins[i].textFieldValue[len + 1] = '\0';
                    }
                    key = GetCharPressed();
                }

                if (IsKeyPressed(KEY_BACKSPACE) && strlen(graph->pins[i].textFieldValue) > 0)
                {
                    size_t len = strlen(graph->pins[i].textFieldValue);
                    graph->pins[i].textFieldValue[len - 1] = '\0';
                }

                if (IsKeyPressed(KEY_ENTER))
                {
                    editor->nodeFieldPinFocused = -1;
                }
            }
        }
        else if (graph->pins[i].type == PIN_FIELD_BOOL)
        {
            Rectangle box = {graph->pins[i].position.x - 6, graph->pins[i].position.y - 12, 24, 24};

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(editor->mousePos, box))
            {
                if (strcmp(graph->pins[i].textFieldValue, "0") == 0 || strcmp(graph->pins[i].textFieldValue, "") == 0)
                    strcpy(graph->pins[i].textFieldValue, "1");
                else
                    strcpy(graph->pins[i].textFieldValue, "0");
            }

            DrawRectangleRec(box, GRAY);
            DrawRectangleLinesEx(box, 1, WHITE);

            if (strcmp(graph->pins[i].textFieldValue, "1") == 0)
                DrawText("1", box.x + 6, box.y + 4, 20, BLACK);
            else
            {
                DrawText("0", box.x + 6, box.y + 4, 20, BLACK);
            }
        }
        else
        {
            DrawCircle(nodePos.x + xOffset + 5, nodePos.y + yOffset, 5, WHITE);
            if (CheckCollisionPointCircle(editor->mousePos, (Vector2){nodePos.x + xOffset + 5, nodePos.y + yOffset}, 12))
            {
                if (graph->pins[i].isInput)
                {
                    DrawTextEx(editor->font, getNodeInputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], (Vector2){(2 * nodePos.x + getNodeInfoByType(graph->nodes[currNodeIndex].type, WIDTH)) / 2 - MeasureTextEx(editor->font, getNodeInputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], 18, 0).x / 2, nodePos.y + yOffset - 8}, 18, 0, WHITE);
                    DrawLine(graph->pins[i].position.x, graph->pins[i].position.y, (2 * nodePos.x + getNodeInfoByType(graph->nodes[currNodeIndex].type, WIDTH)) / 2 - MeasureTextEx(editor->font, getNodeInputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], 18, 0).x / 2 - 5, graph->pins[i].position.y, WHITE);
                }
                else if (graph->nodes[currNodeIndex].type == NODE_COMPARISON || graph->nodes[currNodeIndex].type == NODE_GATE || graph->nodes[currNodeIndex].type == NODE_LITERAL_NUM || graph->nodes[currNodeIndex].type == NODE_LITERAL_STRING || graph->nodes[currNodeIndex].type == NODE_LITERAL_BOOL || graph->nodes[currNodeIndex].type == NODE_LITERAL_COLOR)
                {
                    const char *label = getNodeOutputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode];
                    Vector2 textSize = MeasureTextEx(editor->font, label, 18, 0);
                    DrawTextEx(editor->font, label,
                               (Vector2){
                                   graph->pins[i].position.x - textSize.x - 12,
                                   graph->pins[i].position.y - textSize.y / 2},
                               18, 0, WHITE);
                    DrawLine(graph->pins[i].position.x, graph->pins[i].position.y, graph->pins[i].position.x - 10, graph->pins[i].position.y, WHITE);
                }
                else
                {
                    DrawTextEx(editor->font, getNodeOutputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], (Vector2){(2 * nodePos.x + getNodeInfoByType(graph->nodes[currNodeIndex].type, WIDTH)) / 2 - MeasureTextEx(editor->font, getNodeOutputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], 18, 0).x / 2 - 5, nodePos.y + yOffset - 8}, 18, 0, WHITE);
                    DrawLine(graph->pins[i].position.x, graph->pins[i].position.y, (2 * nodePos.x + getNodeInfoByType(graph->nodes[currNodeIndex].type, WIDTH)) / 2 + MeasureTextEx(editor->font, getNodeOutputNamesByType(graph->nodes[currNodeIndex].type)[graph->pins[i].posInNode], 18, 0).x / 2, graph->pins[i].position.y, WHITE);
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
            editor->hasChanged = true;
            editor->hasChangedInLastFrame = true;
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
        editor->hasChanged = true;
        editor->hasChangedInLastFrame = true;
    }

    if (hoveredPinIndex == -1 && hoveredNodeIndex != -1)
    {
        DrawRectangleRounded((Rectangle){graph->nodes[hoveredNodeIndex].position.x - 1, graph->nodes[hoveredNodeIndex].position.y - 1, getNodeInfoByType(graph->nodes[hoveredNodeIndex].type, WIDTH) + 2, getNodeInfoByType(graph->nodes[hoveredNodeIndex].type, HEIGHT) + 2}, 0.2f, 8, (Color){255, 255, 255, 30});
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
        editor->hasChanged = true;
        editor->hasChangedInLastFrame = true;
        return;
    }
}

bool CheckNodeCollisions(EditorContext *editor, GraphContext *graph)
{
    for (int i = 0; i < graph->nodeCount; i++)
    {
        if (CheckCollisionPointRec(editor->mousePos, (Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, WIDTH), getNodeInfoByType(graph->nodes[i].type, HEIGHT)}))
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
    const char *subMenuItems[][7] = {
        {"num", "string", "bool", "color", "sprite", "Get var", "Set var"},
        {"Start", "On Loop", "On Button", "Create custom", "Call custom"},
        {"Spawn", "Destroy", "Move To"},
        {"Branch", "Loop"},
        {"Comparison", "Gate", "Arithmetic"},
        {"Print", "Draw Line"},
        {"ex", "Literal num", "Literal string", "Literal bool", "Literal color"}};
    int menuItemCount = sizeof(menuItems) / sizeof(menuItems[0]);
    int subMenuCounts[] = {7, 5, 3, 2, 3, 2, 5};

    float menuHeight = MENU_ITEM_HEIGHT * MENU_VISIBLE_ITEMS;

    if (GetMouseWheelMove() < 0 && editor->scrollIndexNodeMenu < menuItemCount - 5)
        editor->scrollIndexNodeMenu++;
    if (GetMouseWheelMove() > 0 && editor->scrollIndexNodeMenu > 0)
        editor->scrollIndexNodeMenu--;

    if (editor->menuPosition.x != editor->rightClickPos.x || editor->menuPosition.y != editor->rightClickPos.y)
    {
        editor->hoveredItem = editor->scrollIndexNodeMenu;
        editor->submenuPosition.x = (editor->rightClickPos.x + MENU_WIDTH + SUBMENU_WIDTH > editor->screenWidth)
                                        ? (editor->rightClickPos.x - SUBMENU_WIDTH)
                                        : (editor->rightClickPos.x + MENU_WIDTH - 15);
        editor->submenuPosition.y = editor->rightClickPos.y + 2;
        editor->hoveredItem = 0;
    }

    editor->menuPosition.x = editor->rightClickPos.x;
    editor->menuPosition.y = editor->rightClickPos.y;

    if (editor->menuPosition.x + MENU_WIDTH > editor->screenWidth)
    {
        editor->menuPosition.x -= MENU_WIDTH;
    }

    DrawRectangleRounded((Rectangle){editor->menuPosition.x, editor->menuPosition.y, MENU_WIDTH, menuHeight}, 0.1f, 8, MenuColor);
    DrawRectangleRoundedLinesEx((Rectangle){editor->menuPosition.x, editor->menuPosition.y, MENU_WIDTH, menuHeight}, 0.1f, 8, MENU_BORDER_THICKNESS, BorderColor);

    for (int i = 0; i < (int)MENU_VISIBLE_ITEMS; i++)
    {
        int itemIndex = i + editor->scrollIndexNodeMenu;
        if (itemIndex >= menuItemCount)
            break;

        Rectangle itemRect = {editor->menuPosition.x, editor->menuPosition.y + i * MENU_ITEM_HEIGHT + 5, MENU_WIDTH, MENU_ITEM_HEIGHT};
        if (CheckCollisionPointRec(editor->mousePos, itemRect))
        {
            editor->hoveredItem = itemIndex;
            editor->submenuPosition.x = (editor->menuPosition.x + MENU_WIDTH + SUBMENU_WIDTH > editor->screenWidth) ? (editor->menuPosition.x - SUBMENU_WIDTH) : (editor->menuPosition.x + MENU_WIDTH - 15);
            editor->submenuPosition.y = itemRect.y - 3;
        }

        if (itemIndex == editor->hoveredItem)
        {
            DrawRectangleRec(itemRect, HighlightColor);
        }
        DrawTextEx(editor->font, menuItems[itemIndex], (Vector2){editor->menuPosition.x + 20, editor->menuPosition.y + i * MENU_ITEM_HEIGHT + 12}, 25, 1, WHITE);
        DrawLine(editor->menuPosition.x, itemRect.y + MENU_ITEM_HEIGHT - 1, editor->menuPosition.x + MENU_WIDTH, itemRect.y + MENU_ITEM_HEIGHT - 1, DARKGRAY);
    }

    // Scrollbar
    float padding = 6.0f;
    float scrollTrackY = editor->menuPosition.y + padding;
    float scrollTrackHeight = menuHeight - 2 * padding - 40;

    int maxScroll = menuItemCount - MENU_VISIBLE_ITEMS;
    if (maxScroll < 1)
        maxScroll = 1;

    float scrollBarHeight = scrollTrackHeight * ((float)MENU_VISIBLE_ITEMS / menuItemCount);
    if (scrollBarHeight < 20.0f)
        scrollBarHeight = 20.0f;
    if (scrollBarHeight > scrollTrackHeight)
        scrollBarHeight = scrollTrackHeight;

    float scrollStep = (scrollTrackHeight - scrollBarHeight) / maxScroll;
    float scrollBarY = scrollTrackY + editor->scrollIndexNodeMenu * scrollStep;

    DrawRectangleRounded((Rectangle){editor->menuPosition.x + 2, scrollBarY, 8, scrollBarHeight}, 0.8f, 4, ScrollIndicatorColor);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        editor->menuOpen = false;
        editor->hoveredItem = 0;
    }

    if (editor->hoveredItem >= 0 && editor->hoveredItem < menuItemCount)
    {
        int subCount = subMenuCounts[editor->hoveredItem];
        float submenuHeight = subCount * MENU_ITEM_HEIGHT;
        DrawRectangleRounded((Rectangle){editor->submenuPosition.x, editor->submenuPosition.y, SUBMENU_WIDTH, submenuHeight}, 0.1f, 2, MenuColor);
        DrawRectangleRoundedLinesEx((Rectangle){editor->submenuPosition.x, editor->submenuPosition.y, SUBMENU_WIDTH, submenuHeight}, 0.1f, 2, MENU_BORDER_THICKNESS, BorderColor);
        DrawRectangleGradientH(editor->submenuPosition.x - 5, editor->submenuPosition.y + 3, 20, MENU_ITEM_HEIGHT, HighlightColor, MenuColor);
        for (int j = 0; j < subCount; j++)
        {
            Rectangle subItemRect = {editor->submenuPosition.x, editor->submenuPosition.y + j * MENU_ITEM_HEIGHT, SUBMENU_WIDTH, MENU_ITEM_HEIGHT};
            if (CheckCollisionPointRec(editor->mousePos, subItemRect))
            {
                DrawRectangleRounded(subItemRect, 0.2f, 2, HighlightColor);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    editor->delayFrames = true;
                    editor->hasChanged = true;
                    editor->hasChangedInLastFrame = true;
                    return subMenuItems[editor->hoveredItem][j];
                }
            }
            DrawTextEx(editor->font, subMenuItems[editor->hoveredItem][j], (Vector2){editor->submenuPosition.x + 20, editor->submenuPosition.y + j * MENU_ITEM_HEIGHT + 10}, 25, 0, WHITE);
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
            if (CheckCollisionPointRec(editor->mousePos, (Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, WIDTH), getNodeInfoByType(graph->nodes[i].type, HEIGHT)}))
            {
                editor->draggingNodeIndex = i;
                dragOffset = (Vector2){editor->mousePos.x - graph->nodes[i].position.x, editor->mousePos.y - graph->nodes[i].position.y};
                editor->hasChanged = true;
                editor->hasChangedInLastFrame = true;
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
        DrawRectangleRounded((Rectangle){graph->nodes[editor->draggingNodeIndex].position.x, graph->nodes[editor->draggingNodeIndex].position.y, getNodeInfoByType(graph->nodes[editor->draggingNodeIndex].type, WIDTH), getNodeInfoByType(graph->nodes[editor->draggingNodeIndex].type, HEIGHT)}, 0.2f, 8, CLITERAL(Color){255, 255, 255, 50});
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

    return CheckNodeCollisions(editor, graph) || IsMouseButtonDown(MOUSE_LEFT_BUTTON);
}

bool CheckOpenMenus(EditorContext *editor)
{
    return editor->draggingNodeIndex != -1 || editor->lastClickedPin.id != -1 || editor->menuOpen || editor->nodeDropdownFocused != -1 || editor->nodeFieldPinFocused != -1 || editor->editingNodeNameIndex != -1;
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
    else if (CheckOpenMenus(editor))
    {
        DrawFullTexture(editor, graph, *viewport, dot);
        editor->delayFrames = true;
    }
    else if (editor->delayFrames == true)
    {
        DrawFullTexture(editor, graph, *viewport, dot);
        editor->delayFrames = false;
    }
}