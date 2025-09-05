#include "CGEditor.h"
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include "raymath.h"
#include "rlgl.h"

#define MENU_WIDTH 270
#define MENU_ITEM_HEIGHT 40
#define MENU_VISIBLE_ITEMS 5.5
#define MENU_BORDER_THICKNESS 3
#define SUBMENU_WIDTH 250

typedef struct {
    Rectangle bounds;
    bool editing;
    char text[256];
    int length;
} TextBox;

void AddToLogFromEditor(EditorContext *editor, char *message, int level);

EditorContext InitEditorContext()
{
    EditorContext editor = {0};

    editor.lastClickedPin = INVALID_PIN;

    editor.scrollIndexNodeMenu = 0;
    editor.hoveredItem = 0;

    editor.mousePos = (Vector2){0, 0};

    editor.draggingNodeIndex = -1;
    editor.isDraggingScreen = false;

    editor.delayFrames = true;
    editor.isFirstFrame = true;
    editor.engineDelayFrames = false;

    editor.menuOpen = false;
    Vector2 menuPosition = {0, 0};
    Vector2 submenuPosition = {0, 0};

    Image tempImg;
    tempImg = LoadImageFromMemory(".png", node_gear_png, node_gear_png_len);
    editor.gearTxt = LoadTextureFromImage(tempImg);
    UnloadImage(tempImg);
    if (editor.gearTxt.id == 0)
    {
        AddToLogFromEditor(&editor, "Failed to load texture", LOG_LEVEL_ERROR);
    }

    editor.nodeDropdownFocused = -1;
    editor.nodeFieldPinFocused = -1;

    editor.font = LoadFontFromMemory(".ttf", arialbd_ttf, arialbd_ttf_len, 256, NULL, 0);
    if (editor.font.texture.id == 0)
    {
        AddToLogFromEditor(&editor, "Failed to load font", LOG_LEVEL_ERROR);
    }

    editor.newLogMessage = false;

    editor.cameraOffset = (Vector2){0, 0};

    editor.editingNodeNameIndex = -1;

    editor.hasChanged = false;
    editor.hasChangedInLastFrame = false;

    editor.createNodeMenuFirstFrame = true;

    editor.zoom = 1.0f;

    return editor;
}

void FreeEditorContext(EditorContext *editor)
{
    UnloadTexture(editor->gearTxt);

    UnloadFont(editor->font);
}

void AddToLogFromEditor(EditorContext *editor, char *message, int level)
{
    if (editor->logMessageCount >= MAX_LOG_MESSAGES){return;}

    strmac(editor->logMessages[editor->logMessageCount], MAX_LOG_MESSAGE_SIZE, "%s", message);
    editor->logMessageLevels[editor->logMessageCount] = level;
    editor->logMessageCount++;
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

            DrawTextureRec(dot.texture, (Rectangle){0, 0, (float)dot.texture.width, (float)-dot.texture.height}, (Vector2){screenX, screenY}, (Color){255, 255, 255, 15});
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

void HandleVarTextBox(EditorContext *editor, Rectangle bounds, char *text, int index, GraphContext *graph)
{
    bounds.width = MeasureTextEx(editor->font, text, 16, 2).x + 25;

    DrawRectangleRounded((Rectangle){bounds.x, bounds.y - 20, 56, bounds.height}, 0.4f, 4, DARKGRAY);
    DrawTextEx(editor->font, "Name:", (Vector2){bounds.x + 4, bounds.y - 16}, 14, 2, WHITE);

    DrawRectangleRounded(bounds, 0.6f, 4, GRAY);
    DrawRectangleRoundedLinesEx(bounds, 0.6f, 4, 2, DARKGRAY);

    bool showCursor = ((int)(GetTime() * 2) % 2) == 0;
    char buffer[130];
    strmac(buffer, sizeof(buffer), "%s%s", text, showCursor ? "_" : " ");
    DrawTextEx(editor->font, buffer, (Vector2){bounds.x + 5, bounds.y + 8}, 16, 2, BLACK);

    int key = GetCharPressed();

    bool hasNameChanged = false;

    if (key > 0)
    {
        int len = strlen(text);
        if (len < 127 && key >= 32 && key <= 125)
        {
            text[len] = (char)key;
            text[len + 1] = '\0';
            hasNameChanged = true;
            editor->hasChanged = true;
            editor->hasChangedInLastFrame = true;
        }
    }

    if (IsKeyPressed(KEY_BACKSPACE))
    {
        int len = strlen(text);
        if (len > 0)
        {
            text[len - 1] = '\0';
            hasNameChanged = true;
            editor->hasChanged = true;
            editor->hasChangedInLastFrame = true;
        }
    }

    if (hasNameChanged)
    {
        graph->variables = NULL;

        graph->variables = malloc(sizeof(char *) * 1);
        graph->variableTypes = malloc(sizeof(NodeType) * 1);
        graph->variables[0] = strmac(NULL, 4, "NONE");
        graph->variableTypes[0] = NODE_UNKNOWN;
        graph->variablesCount = 1;

        for (int i = 0; i < graph->nodeCount; i++)
        {
            if (graph->nodes[i].type == NODE_CREATE_NUMBER || graph->nodes[i].type == NODE_CREATE_STRING || graph->nodes[i].type == NODE_CREATE_BOOL || graph->nodes[i].type == NODE_CREATE_COLOR || graph->nodes[i].type == NODE_CREATE_SPRITE)
            {
                graph->variables = realloc(graph->variables, sizeof(char *) * (graph->variablesCount + 1));
                graph->variables[graph->variablesCount] = strmac(NULL, MAX_VARIABLE_NAME_SIZE, "%s", graph->nodes[i].name);

                graph->variableTypes = realloc(graph->variableTypes, sizeof(int) * (graph->variablesCount + 1));
                graph->variableTypes[graph->variablesCount] = graph->nodes[i].type;

                graph->variablesCount++;
            }
        }
    }
}

const char *AddEllipsis(Font font, const char *text, float fontSize, float maxWidth, bool showEnd)
{
    static char result[256];
    float fullWidth = MeasureTextEx(font, text, fontSize, 0).x;
    if (fullWidth <= maxWidth)
        return text;

    int len = strlen(text);
    int maxChars = 0;
    char temp[256];

    for (int c = 1; c <= len; c++)
    {
        if (showEnd){
            strmac(temp, c, "%s", text + len - c);
        }
        else{
            strmac(temp, c, "%s", text);
        }

        temp[c] = '\0';

        float width = MeasureTextEx(font, temp, fontSize, 0).x +
                      MeasureTextEx(font, "...", fontSize, 0).x;

        if (width > maxWidth)
            break;

        maxChars = c;
    }

    if (showEnd)
    {
        strmac(temp, maxChars, "%s", text + len - maxChars);
        temp[maxChars] = '\0';
        strmac(result, sizeof(result), "...%s", temp);
    }
    else
    {
        strmac(result, maxChars, "%s", text);
        result[maxChars] = '\0';
        strmac(result, maxChars, "%s...", result);
    }

    return result;
}

void HandleLiteralNodeField(EditorContext *editor, GraphContext *graph, int currPinIndex)
{
    PinType type = graph->pins[currPinIndex].type;
    int limit = 0;
    switch (type)
    {
    case PIN_FIELD_NUM:
        limit = 90;
        break;
    case PIN_FIELD_STRING:
        limit = 150;
        break;
    case PIN_FIELD_BOOL:
        limit = 100;
        break;
    case PIN_FIELD_COLOR:
        limit = 120;
        break;
    default:
        break;
    }

    Rectangle textbox = {
        graph->pins[currPinIndex].position.x - 6,
        graph->pins[currPinIndex].position.y - 10,
        limit,
        24};

    float textWidth = MeasureTextEx(editor->font, graph->pins[currPinIndex].textFieldValue, 20, 0).x;
    if (editor->nodeFieldPinFocused == currPinIndex)
        textWidth += MeasureTextEx(editor->font, "_", 20, 0).x;
    float boxWidth = (textWidth + 10 > limit) ? limit : textWidth + 10;
    if (boxWidth < 25)
    {
        boxWidth = 25;
    }
    textbox.width = boxWidth;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (CheckCollisionPointRec(editor->mousePos, textbox))
        {
            if (type == PIN_FIELD_BOOL)
            {
                if (strcmp(graph->pins[currPinIndex].textFieldValue, "false") == 0 || strcmp(graph->pins[currPinIndex].textFieldValue, "") == 0){
                    strmac(graph->pins[currPinIndex].textFieldValue, 4, "true");
                }
                else{
                    strmac(graph->pins[currPinIndex].textFieldValue, 5, "false");
                }
            }
            else
            {
                editor->nodeFieldPinFocused = currPinIndex;
            }
        }
        else if (editor->nodeFieldPinFocused == currPinIndex)
        {
            switch (type)
            {
            case PIN_FIELD_NUM:
                if (graph->pins[currPinIndex].textFieldValue[0] == '\0')
                {
                    strmac(graph->pins[currPinIndex].textFieldValue, 1, "0");
                }
                break;
            case PIN_FIELD_STRING:
                if (graph->pins[currPinIndex].textFieldValue[0] == '\0')
                {
                    strmac(graph->pins[currPinIndex].textFieldValue, 1, "");
                }
                break;
            case PIN_FIELD_COLOR:
                if (strlen(graph->pins[currPinIndex].textFieldValue) != 8)
                {
                    strmac(graph->pins[currPinIndex].textFieldValue, 8, "00000000");
                }
                break;
            default:
                break;
            }
            editor->nodeFieldPinFocused = -1;
        }
    }

    DrawRectangleRec(textbox, (editor->nodeFieldPinFocused == currPinIndex) ? LIGHTGRAY : GRAY);
    DrawRectangleLinesEx(textbox, 1, WHITE);

    const char *originalText = graph->pins[currPinIndex].textFieldValue;
    const char *text = originalText;

    static char truncated[256];

    if (boxWidth == limit)
    {
        text = AddEllipsis(editor->font, originalText, 20, limit - 10, editor->nodeFieldPinFocused == currPinIndex);
    }

    if (editor->nodeFieldPinFocused == currPinIndex)
    {
        static float blinkTime = 0;
        blinkTime += GetFrameTime();
        if (fmodf(blinkTime, 1.0f) < 0.5f)
        {
            char blinking[256];
            strmac(blinking, sizeof(blinking), "%s_", text);
            DrawTextEx(editor->font, blinking, (Vector2){textbox.x + 5, textbox.y + 4}, 20, 0, BLACK);
        }
        else
        {
            DrawTextEx(editor->font, text, (Vector2){textbox.x + 5, textbox.y + 4}, 20, 0, BLACK);
        }
    }
    else
    {
        DrawTextEx(editor->font, text, (Vector2){textbox.x + 5, textbox.y + 4}, 20, 0, BLACK);
    }

    if (editor->nodeFieldPinFocused == currPinIndex)
    {
        int key = GetCharPressed();
        while (key > 0)
        {
            int len = strlen(graph->pins[currPinIndex].textFieldValue);

            bool validKey =
                (type == PIN_FIELD_NUM && key >= '0' && key <= '9') ||
                (type == PIN_FIELD_STRING && key >= 32 && key <= 126) ||
                (type == PIN_FIELD_COLOR &&
                 ((key >= '0' && key <= '9') || (key >= 'a' && key <= 'f') || (key >= 'A' && key <= 'F')));

            if (validKey)
            {
                if (type == PIN_FIELD_COLOR && len >= 8)
                {
                    key = GetCharPressed();
                    continue;
                }

                graph->pins[currPinIndex].textFieldValue[len] = (char)key;
                graph->pins[currPinIndex].textFieldValue[len + 1] = '\0';
            }
            else if (
                key == 46 &&
                type == PIN_FIELD_NUM &&
                !graph->pins[currPinIndex].isFloat)
            {
                graph->pins[currPinIndex].textFieldValue[len] = (char)key;
                graph->pins[currPinIndex].textFieldValue[len + 1] = '\0';
                graph->pins[currPinIndex].isFloat = true;
            }

            key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE))
        {
            size_t len = strlen(graph->pins[currPinIndex].textFieldValue);
            if (len > 0)
            {
                if (type == PIN_FIELD_NUM &&
                    graph->pins[currPinIndex].textFieldValue[len - 1] == 46)
                {
                    graph->pins[currPinIndex].isFloat = false;
                }
                graph->pins[currPinIndex].textFieldValue[len - 1] = '\0';
            }
        }

        if (IsKeyPressed(KEY_ENTER))
        {
            switch (type)
            {
            case PIN_FIELD_NUM:
                if (graph->pins[currPinIndex].textFieldValue[0] == '\0')
                {
                    strmac(graph->pins[currPinIndex].textFieldValue, 1, "0");
                }
                break;
            case PIN_FIELD_STRING:
                if (graph->pins[currPinIndex].textFieldValue[0] == '\0')
                {
                    strmac(graph->pins[currPinIndex].textFieldValue, 1, "");
                }
                break;
            case PIN_FIELD_COLOR:
                if (strlen(graph->pins[currPinIndex].textFieldValue) != 8)
                {
                    strmac(graph->pins[currPinIndex].textFieldValue, 8, "00000000");
                }
                break;
            default:
                break;
            }
            editor->nodeFieldPinFocused = -1;
        }
    }
}

void HandleKeyNodeField(EditorContext *editor, GraphContext *graph, int currPinIndex)
{
    Rectangle textbox = {
        graph->pins[currPinIndex].position.x - 6,
        graph->pins[currPinIndex].position.y - 10,
        editor->nodeFieldPinFocused == currPinIndex ? 110 : MeasureTextEx(editor->font, graph->pins[currPinIndex].textFieldValue, 20, 0).x + 10,
        24};

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (CheckCollisionPointRec(editor->mousePos, textbox))
        {
            editor->nodeFieldPinFocused = currPinIndex;
        }
        else if (editor->nodeFieldPinFocused == currPinIndex)
        {
            if (graph->pins[currPinIndex].textFieldValue[0] == '\0')
            {
                strmac(graph->pins[currPinIndex].textFieldValue, 1, "NONE");
                graph->pins[currPinIndex].pickedOption = -1;
            }
            editor->nodeFieldPinFocused = -1;
        }
    }

    DrawRectangleRec(textbox, (editor->nodeFieldPinFocused == currPinIndex) ? LIGHTGRAY : GRAY);
    DrawRectangleLinesEx(textbox, 1, WHITE);

    const char *originalText = graph->pins[currPinIndex].textFieldValue;
    const char *text = originalText;

    static char truncated[256];

    if (editor->nodeFieldPinFocused == currPinIndex)
    {
        static float blinkTime = 0;
        blinkTime += GetFrameTime();
        if (fmodf(blinkTime, 1.0f) < 0.6f)
        {
            char blinking[256];
            strmac(blinking, sizeof(blinking), "Press a key");
            DrawTextEx(editor->font, blinking, (Vector2){textbox.x + 5, textbox.y + 4}, 20, 0, BLACK);
        }
    }
    else
    {
        DrawTextEx(editor->font, text, (Vector2){textbox.x + 5, textbox.y + 4}, 20, 0, BLACK);
    }

    if (editor->nodeFieldPinFocused == currPinIndex)
    {
        for (int key = 0; key <= KEY_KB_MENU; key++)
        {
            if (IsKeyPressed(key))
            {
                strmac(graph->pins[currPinIndex].textFieldValue, MAX_KEY_NAME_SIZE, GetKeyboardKeyName(key));
                graph->pins[currPinIndex].pickedOption = key;
                editor->nodeFieldPinFocused = -1;
                break;
            }
        }

        if (IsKeyPressed(KEY_ENTER))
        {
            if (graph->pins[currPinIndex].textFieldValue[0] == '\0')
            {
                strmac(graph->pins[currPinIndex].textFieldValue, 4, "NONE");
                graph->pins[currPinIndex].pickedOption = -1;
            }
            editor->nodeFieldPinFocused = -1;
        }
    }
}

void HandleDropdownMenu(GraphContext *graph, int currPinIndex, int hoveredNodeIndex, int currNodeIndex, EditorContext *editor)
{
    DropdownOptionsByPinType options;
    if (graph->pins[currPinIndex].type == PIN_VARIABLE || graph->pins[currPinIndex].type == PIN_SPRITE_VARIABLE)
    {
        options.boxWidth = 100;
        options.optionsCount = graph->variablesCount;
        options.options = graph->variables;

        if (options.optionsCount == 0)
        {
            static char *noVars[] = {"No variables"};
            options.options = noVars;
            options.optionsCount = 1;
        }
    }
    else
    {
        options = getPinDropdownOptionsByType(graph->pins[currPinIndex].type);
    }

    if (graph->pins[currPinIndex].pickedOption >= options.optionsCount)
    {
        graph->pins[currPinIndex].pickedOption = 0;
    }

    Rectangle dropdown = {graph->pins[currPinIndex].position.x - 6, graph->pins[currPinIndex].position.y - 10, options.boxWidth, 24};

    DrawRectangleRec(dropdown, GRAY);
    const char *text = AddEllipsis(editor->font, options.options[graph->pins[currPinIndex].pickedOption], 20, options.boxWidth - 20, false);
    DrawTextEx(editor->font, text, (Vector2){(graph->pins[currPinIndex].type == PIN_VARIABLE || graph->pins[currPinIndex].type == PIN_SPRITE_VARIABLE) ? dropdown.x + 20 : dropdown.x + 3, dropdown.y + 3}, 20, 0, BLACK);
    DrawRectangleLinesEx(dropdown, 1, WHITE);
    if (graph->pins[currPinIndex].type == PIN_VARIABLE || graph->pins[currPinIndex].type == PIN_SPRITE_VARIABLE)
    {
        Color varTypeColor;
        PinType varType = PIN_UNKNOWN_VALUE;
        switch (graph->variableTypes[graph->pins[currPinIndex].pickedOption])
        {
        case NODE_CREATE_NUMBER:
            varTypeColor = (Color){24, 119, 149, 255};
            varType = PIN_NUM;
            break;
        case NODE_CREATE_STRING:
            varTypeColor = (Color){180, 178, 40, 255};
            varType = PIN_STRING;
            break;
        case NODE_CREATE_BOOL:
            varTypeColor = (Color){27, 64, 121, 255};
            varType = PIN_BOOL;
            break;
        case NODE_CREATE_COLOR:
            varTypeColor = (Color){217, 3, 104, 255};
            varType = PIN_COLOR;
            break;
        case NODE_CREATE_SPRITE:
            varTypeColor = (Color){3, 206, 164, 255};
            varType = PIN_SPRITE;
            break;
        default:
            varTypeColor = LIGHTGRAY;
        }
        if (graph->nodes[currNodeIndex].type == NODE_GET_VARIABLE)
        {
            graph->pins[FindPinIndexByID(graph, graph->nodes[currNodeIndex].outputPins[0])].type = varType;
        }
        else if (graph->nodes[currNodeIndex].type == NODE_SET_VARIABLE)
        {
            graph->pins[FindPinIndexByID(graph, graph->nodes[currNodeIndex].inputPins[2])].type = varType;
        }
        DrawCircle(graph->pins[currPinIndex].position.x + 4, graph->pins[currPinIndex].position.y, 6, varTypeColor);
    }

    bool mouseOnDropdown = CheckCollisionPointRec(editor->mousePos, dropdown);
    bool mouseOnOptions = false;
    if (editor->nodeDropdownFocused == currPinIndex)
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
        if (editor->nodeDropdownFocused == currPinIndex)
        {
            if (mouseOnDropdown)
                editor->nodeDropdownFocused = -1;
            else if (!mouseOnOptions)
                editor->nodeDropdownFocused = -1;
        }
        else if (mouseOnDropdown)
        {
            editor->nodeDropdownFocused = currPinIndex;
        }
    }

    if (editor->nodeDropdownFocused == currPinIndex)
    {
        editor->delayFrames = true;
        hoveredNodeIndex = -1;
        int displayedVarsCounter = 0;
        for (int j = 0; j < options.optionsCount; j++)
        {
            displayedVarsCounter++;
            if ((graph->pins[currPinIndex].type == PIN_SPRITE_VARIABLE && graph->variableTypes[j] != NODE_CREATE_SPRITE) && j != 0)
            {
                displayedVarsCounter--;
                continue;
            }
            Rectangle option = {dropdown.x, dropdown.y - displayedVarsCounter * 30, dropdown.width, 30};
            DrawRectangleRec(option, RAYWHITE);
            const char *text = AddEllipsis(editor->font, options.options[j], 20, options.boxWidth - 20, false);
            DrawTextEx(editor->font, text, (Vector2){(graph->pins[currPinIndex].type == PIN_VARIABLE || graph->pins[currPinIndex].type == PIN_SPRITE_VARIABLE) ? option.x + 20 : option.x + 3, option.y + 3}, 20, 0, BLACK);
            DrawRectangleLinesEx(option, 1, DARKGRAY);

            if (graph->pins[currPinIndex].type == PIN_VARIABLE || graph->pins[currPinIndex].type == PIN_SPRITE_VARIABLE)
            {
                Color varTypeColor;
                switch (graph->variableTypes[j])
                {
                case NODE_CREATE_NUMBER:
                    varTypeColor = (Color){24, 119, 149, 255};
                    break;
                case NODE_CREATE_STRING:
                    varTypeColor = (Color){180, 178, 40, 255};
                    break;
                case NODE_CREATE_BOOL:
                    varTypeColor = (Color){27, 64, 121, 255};
                    break;
                case NODE_CREATE_COLOR:
                    varTypeColor = (Color){217, 3, 104, 255};
                    break;
                case NODE_CREATE_SPRITE:
                    varTypeColor = (Color){3, 206, 164, 255};
                    break;
                default:
                    varTypeColor = LIGHTGRAY;
                }
                DrawCircle(option.x + 10, option.y + 12, 6, varTypeColor);
            }

            if (CheckCollisionPointRec(editor->mousePos, option) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                graph->pins[currPinIndex].pickedOption = j;
                editor->nodeDropdownFocused = -1;
                editor->hasChanged = true;
                editor->hasChangedInLastFrame = true;
            }
        }
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
            AddToLogFromEditor(editor, "Error drawing connection", LOG_LEVEL_WARNING);
        }
    }

    int hoveredNodeIndex = -1;
    int nodeToDelete = -1;
    static Rectangle textBoxRect = {0};
    static float glareTime = 0;

    for (int i = 0; i < graph->nodeCount; i++)
    {
        float x = graph->nodes[i].position.x;
        float y = graph->nodes[i].position.y;
        float width = getNodeInfoByType(graph->nodes[i].type, WIDTH);
        float height = getNodeInfoByType(graph->nodes[i].type, HEIGHT);
        float roundness = 0.2f;
        float segments = 8;
        int glareOffset = 0;

        if (CheckCollisionPointRec(editor->mousePos, (Rectangle){graph->nodes[i].position.x, graph->nodes[i].position.y, getNodeInfoByType(graph->nodes[i].type, WIDTH), getNodeInfoByType(graph->nodes[i].type, HEIGHT)}))
        {
            hoveredNodeIndex = i;
            glareTime += GetFrameTime();
            glareOffset = (int)(sinf(glareTime * 6.0f) * 30);

            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
            {
                nodeToDelete = graph->nodes[i].id;
            }
        }

        Color nodeColor = getNodeColorByType(graph->nodes[i].type);

        Color nodeLeftGradientColor = {
            (unsigned char)Clamp((int)nodeColor.r + 40 + glareOffset, 0, 255),
            (unsigned char)Clamp((int)nodeColor.g + 40 + glareOffset, 0, 255),
            (unsigned char)Clamp((int)nodeColor.b + 40 + glareOffset, 0, 255),
            nodeColor.a};

        Color nodeRightGradientColor = {
            (unsigned char)Clamp((int)nodeColor.r - 60 + glareOffset, 0, 255),
            (unsigned char)Clamp((int)nodeColor.g - 60 + glareOffset, 0, 255),
            (unsigned char)Clamp((int)nodeColor.b - 60 + glareOffset, 0, 255),
            nodeColor.a};

        float fullRadius = roundness * fminf(width, height) / 2.0f;

        Color nodeBackgroundColor = {
            (unsigned char)Clamp((int)glareOffset + 5, 0, 255),
            (unsigned char)Clamp((int)glareOffset + 5, 0, 255),
            (unsigned char)Clamp((int)glareOffset + 5, 0, 255),
            120};

        DrawRectangleRounded(
            (Rectangle){x, y, width, height},
            roundness, segments, nodeBackgroundColor);

        DrawCircleSector(
            (Vector2){x + fullRadius - 2, y + fullRadius - 2},
            fullRadius, 180, 270, segments, nodeLeftGradientColor);

        DrawCircleSector(
            (Vector2){x + width - fullRadius + 2, y + fullRadius - 2},
            fullRadius, 270, 360, segments, nodeRightGradientColor);

        DrawRectangleGradientH(x + fullRadius - 2, y - 2, width - 2 * fullRadius + 4, fullRadius, nodeLeftGradientColor, nodeRightGradientColor);

        DrawRectangleGradientH(x - 2, y + fullRadius - 2, width + 4, 38 - fullRadius, nodeLeftGradientColor, nodeRightGradientColor);

        DrawRectangleRoundedLinesEx(
            (Rectangle){x - 1, y - 1, width + 2, height + 2},
            roundness, segments, 2.0f / editor->zoom, WHITE);

        DrawTextEx(editor->font, NodeTypeToString(graph->nodes[i].type),
                   (Vector2){x + 8, y + 6}, 28, 1, WHITE);

        if (getIsEditableByType(graph->nodes[i].type))
        {
            Rectangle gearRect = {graph->nodes[i].position.x + getNodeInfoByType(graph->nodes[i].type, WIDTH) - 18 - fullRadius / 5, graph->nodes[i].position.y + 5 + fullRadius / 5, 16, 16};

            Rectangle src = {0, 0, editor->gearTxt.width, editor->gearTxt.height};
            Rectangle dst = {gearRect.x, gearRect.y, 15, 15};
            Vector2 origin = {0, 0};
            DrawTexturePro(editor->gearTxt, src, dst, origin, 0.0f, WHITE);

            if (CheckCollisionPointRec(editor->mousePos, gearRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                editor->editingNodeNameIndex = i;
                textBoxRect = (Rectangle){graph->nodes[i].position.x + getNodeInfoByType(graph->nodes[i].type, WIDTH) + 10, graph->nodes[i].position.y, MeasureTextEx(editor->font, graph->nodes[i].name, 16, 2).x + 25, 30};
            }
            else if (editor->editingNodeNameIndex == i)
            {
                HandleVarTextBox(editor, textBoxRect, graph->nodes[editor->editingNodeNameIndex].name, editor->editingNodeNameIndex, graph);
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
        int yOffset = 52 + graph->pins[i].posInNode * 30;

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
        else if (graph->pins[i].type == PIN_DROPDOWN_COMPARISON_OPERATOR || graph->pins[i].type == PIN_DROPDOWN_GATE || graph->pins[i].type == PIN_DROPDOWN_ARITHMETIC || graph->pins[i].type == PIN_DROPDOWN_KEY_ACTION || graph->pins[i].type == PIN_VARIABLE || graph->pins[i].type == PIN_SPRITE_VARIABLE)
        {
            HandleDropdownMenu(graph, i, hoveredNodeIndex, currNodeIndex, editor);
        }
        else if (graph->pins[i].type == PIN_FIELD_NUM || graph->pins[i].type == PIN_FIELD_STRING || graph->pins[i].type == PIN_FIELD_BOOL || graph->pins[i].type == PIN_FIELD_COLOR)
        {
            HandleLiteralNodeField(editor, graph, i);
        }
        else if (graph->pins[i].type == PIN_FIELD_KEY)
        {
            HandleKeyNodeField(editor, graph, i);
        }
        else if (graph->pins[i].type == PIN_EDIT_HITBOX)
        {
            DrawRectangleRounded((Rectangle){graph->pins[i].position.x - 6, graph->pins[i].position.y - 10, 96, 24}, 0.4f, 4, DARKPURPLE);
            if (CheckCollisionPointRec(editor->mousePos, (Rectangle){graph->pins[i].position.x - 6, graph->pins[i].position.y - 10, 96, 24}))
            {
                DrawRectangleRounded((Rectangle){graph->pins[i].position.x - 6, graph->pins[i].position.y - 10, 96, 24}, 0.4f, 4, (Color){255, 255, 255, 100});
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    // Find texture file name from linked literal node(shouldn't only work with literal nodes)
                    for (int b = 0; b < graph->nodeCount; b++)
                    {
                        if (graph->nodes[b].id == graph->pins[i].nodeID)
                        {
                            for (int c = 0; c < graph->pinCount; c++)
                            {
                                if (graph->nodes[b].inputPins[1] == graph->pins[c].id)
                                {
                                    for (int k = 0; k < graph->linkCount; k++)
                                    {
                                        if (graph->links[k].inputPinID == graph->pins[c].id)
                                        {
                                            for (int d = 0; d < graph->nodeCount; d++)
                                            {
                                                if (graph->nodes[d].outputPins[0] == graph->links[k].outputPinID)
                                                {
                                                    for (int e = 0; e < graph->pinCount; e++)
                                                    {
                                                        if (graph->pins[e].id == graph->nodes[d].inputPins[0])
                                                        {
                                                            editor->shouldOpenHitboxEditor = true;
                                                            strmac(editor->hitboxEditorFileName, MAX_FILE_NAME, "%s", graph->pins[e].textFieldValue);
                                                            editor->hitboxEditingPinID = graph->pins[i].id;
                                                            editor->hasChanged = false;
                                                            editor->hasChangedInLastFrame = false;
                                                            return;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            DrawTextEx(editor->font, "Edit Hitbox", (Vector2){graph->pins[i].position.x - 2, graph->pins[i].position.y - 6}, 18, 0.2f, WHITE);
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
                else if (graph->nodes[currNodeIndex].type == NODE_COMPARISON || graph->nodes[currNodeIndex].type == NODE_GATE || graph->nodes[currNodeIndex].type == NODE_LITERAL_NUMBER || graph->nodes[currNodeIndex].type == NODE_LITERAL_STRING || graph->nodes[currNodeIndex].type == NODE_LITERAL_BOOL || graph->nodes[currNodeIndex].type == NODE_LITERAL_COLOR)
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
        DrawRectangleRounded((Rectangle){graph->nodes[hoveredNodeIndex].position.x - 1, graph->nodes[hoveredNodeIndex].position.y - 1, getNodeInfoByType(graph->nodes[hoveredNodeIndex].type, WIDTH) + 2, getNodeInfoByType(graph->nodes[hoveredNodeIndex].type, HEIGHT) + 2}, 0.2f, 8, (Color){255, 255, 255, 5});
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

const char *Search(const char *haystack, const char *needle)
{
    if (!*needle)
        return haystack;
    for (; *haystack; haystack++)
    {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle))
        {
            const char *h = haystack + 1;
            const char *n = needle + 1;
            while (*n && tolower((unsigned char)*h) == tolower((unsigned char)*n))
            {
                h++;
                n++;
            }
            if (!*n)
                return haystack;
        }
    }
    return NULL;
}

const char *DrawNodeMenu(EditorContext *editor, RenderTexture2D view)
{
    Color MenuColor = {50, 50, 50, 255};
    Color ScrollIndicatorColor = {150, 150, 150, 255};
    Color BorderColor = {200, 200, 200, 255};
    Color HighlightColor = {80, 80, 80, 255};

    const char *menuItems[] = {"Variable", "Event", "Get", "Set", "Flow", "Sprite", "Draw Prop", "Logical", "Debug", "Literal"};
    const char *subMenuItems[][9] = {
        {"Create number", "Create string", "Create bool", "Create color", "", "", "", "", ""},
        {"Event Start", "Event Tick", "Event On Button", "Create Custom Event", "Call Custom Event", "", "", "", ""},
        {"Get variable", "Get Screen Width", "Get Screen Height", "Get Mouse X", "Get Mouse Y", "Get Random Number", "", "", ""},
        {"Set variable", "Set Background", "Set FPS", "", "", "", "", "", ""},
        {"Branch", "Loop", "Delay", "Flip Flop", "Break", "Return", "", "", ""},
        {"Create sprite", "Spawn sprite", "Destroy sprite", "Set Sprite Position", "Set Sprite Rotation", "Set Sprite Texture", "Set Sprite Size", "Move To", "Force"},
        {"Draw Prop Texture", "Draw Prop Rectangle", "Draw Prop Circle", "", "", "", "", "", ""},
        {"Comparison", "Gate", "Arithmetic", "", "", "", "", "", ""},
        {"Print To Log", "Draw Debug Line", "", "", "", "", "", "", ""},
        {"Literal number", "Literal string", "Literal bool", "Literal color", "", "", "", "", ""}};
    int menuItemCount = sizeof(menuItems) / sizeof(menuItems[0]);
    int subMenuCounts[] = {4, 5, 6, 3, 6, 9, 3, 3, 2, 4};

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        editor->createNodeMenuFirstFrame = true;
    }

    float searchBarHeight = 30.0f;
    float menuHeight = MENU_ITEM_HEIGHT * MENU_VISIBLE_ITEMS + searchBarHeight + 10;

    int key = GetCharPressed();
    while (key > 0)
    {
        int len = strlen(editor->nodeMenuSearch);
        if (len < 63 && key >= 32 && key <= 125)
        {
            editor->nodeMenuSearch[len] = (char)key;
            editor->nodeMenuSearch[len + 1] = '\0';
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE))
    {
        int len = strlen(editor->nodeMenuSearch);
        if (len > 0)
            editor->nodeMenuSearch[len - 1] = '\0';
    }

    typedef struct
    {
        int parentIndex;
        int subIndex;
    } SearchResult;
    SearchResult filtered[128];
    int filteredCount = 0;

    for (int i = 0; i < menuItemCount; i++)
    {
        for (int j = 0; j < subMenuCounts[i]; j++)
        {
            if (strlen(editor->nodeMenuSearch) == 0 ||
                (subMenuItems[i][j][0] != '\0' && Search(subMenuItems[i][j], editor->nodeMenuSearch)))
            {
                if (filteredCount < menuItemCount)
                    filtered[filteredCount++] = (SearchResult){i, j};
            }
        }
    }

    if (strlen(editor->nodeMenuSearch) == 0)
        filteredCount = menuItemCount;

    if (GetMouseWheelMove() < 0 && editor->scrollIndexNodeMenu < filteredCount - MENU_VISIBLE_ITEMS)
    {
        editor->scrollIndexNodeMenu++;
    }
    if (GetMouseWheelMove() > 0 && editor->scrollIndexNodeMenu > 0)
    {
        editor->scrollIndexNodeMenu--;
    }

    if (editor->createNodeMenuFirstFrame)
    {
        editor->menuPosition.x = editor->rightClickPos.x;
        editor->menuPosition.y = editor->rightClickPos.y;
        if (editor->menuPosition.y + menuHeight > editor->viewportBoundary.y + editor->viewportBoundary.height)
        {
            editor->menuPosition.y -= menuHeight;
        }
        if (editor->menuPosition.x + MENU_WIDTH > editor->viewportBoundary.x + editor->viewportBoundary.width)
        {
            editor->menuPosition.x -= MENU_WIDTH;
        }

        Rectangle menuRect = {editor->menuPosition.x, editor->menuPosition.y + searchBarHeight + 10, MENU_WIDTH, menuHeight - searchBarHeight - 10};
        editor->submenuPosition.x = (editor->menuPosition.x + MENU_WIDTH + SUBMENU_WIDTH > editor->screenWidth)
                                        ? (editor->menuPosition.x - SUBMENU_WIDTH)
                                        : (editor->menuPosition.x + MENU_WIDTH - 15);
        editor->submenuPosition.y = editor->menuPosition.y + searchBarHeight + 7;
        editor->hoveredItem = 0;
        editor->createNodeMenuFirstFrame = false;
    }

    DrawRectangleRounded((Rectangle){editor->menuPosition.x, editor->menuPosition.y, MENU_WIDTH, menuHeight}, 0.1f, 8, MenuColor);
    DrawRectangleRoundedLinesEx((Rectangle){editor->menuPosition.x, editor->menuPosition.y, MENU_WIDTH, menuHeight}, 0.1f, 8, MENU_BORDER_THICKNESS, BorderColor);

    if (strlen(editor->nodeMenuSearch) > 0)
    {
        for (int i = 0; i < (int)MENU_VISIBLE_ITEMS; i++)
        {
            int listIndex = i + editor->scrollIndexNodeMenu;
            if (listIndex >= filteredCount)
                break;

            SearchResult res = filtered[listIndex];
            Rectangle itemRect = {editor->menuPosition.x, editor->menuPosition.y + searchBarHeight + 10 + i * MENU_ITEM_HEIGHT, MENU_WIDTH, MENU_ITEM_HEIGHT};

            if (CheckCollisionPointRec(editor->mousePos, itemRect))
            {
                DrawRectangleRec(itemRect, HighlightColor);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    editor->delayFrames = true;
                    editor->hasChanged = true;
                    editor->hasChangedInLastFrame = true;
                    editor->menuOpen = false;
                    editor->nodeMenuSearch[0] = '\0';
                    return subMenuItems[res.parentIndex][res.subIndex];
                }
            }
            DrawTextEx(editor->font, subMenuItems[res.parentIndex][res.subIndex], (Vector2){itemRect.x + 20, itemRect.y + 12}, 25, 1, WHITE);
            DrawLine(itemRect.x, itemRect.y + MENU_ITEM_HEIGHT - 1, itemRect.x + MENU_WIDTH, itemRect.y + MENU_ITEM_HEIGHT - 1, DARKGRAY);
        }
    }
    else
    {
        for (int i = 0; i < (int)MENU_VISIBLE_ITEMS; i++)
        {
            int listIndex = i + editor->scrollIndexNodeMenu;
            if (listIndex >= menuItemCount)
                break;

            Rectangle itemRect = {editor->menuPosition.x, editor->menuPosition.y + searchBarHeight + 10 + i * MENU_ITEM_HEIGHT, MENU_WIDTH, MENU_ITEM_HEIGHT};

            if (CheckCollisionPointRec(editor->mousePos, itemRect))
            {
                editor->hoveredItem = listIndex;
                editor->submenuPosition.x = (editor->menuPosition.x + MENU_WIDTH + SUBMENU_WIDTH > editor->screenWidth)
                                                ? (editor->menuPosition.x - SUBMENU_WIDTH)
                                                : (editor->menuPosition.x + MENU_WIDTH - 15);
                editor->submenuPosition.y = itemRect.y - 3;
            }

            if (listIndex == editor->hoveredItem)
                DrawRectangleRec(itemRect, HighlightColor);

            DrawTextEx(editor->font, menuItems[listIndex], (Vector2){itemRect.x + 20, itemRect.y + 12}, 25, 1, WHITE);
            DrawLine(itemRect.x, itemRect.y + MENU_ITEM_HEIGHT - 1, itemRect.x + MENU_WIDTH, itemRect.y + MENU_ITEM_HEIGHT - 1, DARKGRAY);
        }
    }

    float scrollTrackY = editor->menuPosition.y + searchBarHeight + 10 + 6;
    float scrollTrackHeight = MENU_ITEM_HEIGHT * MENU_VISIBLE_ITEMS - 16;

    int maxScroll = filteredCount - MENU_VISIBLE_ITEMS;
    if (maxScroll < 1)
        maxScroll = 1;

    float scrollBarHeight = scrollTrackHeight * ((float)MENU_VISIBLE_ITEMS / ((filteredCount > 5) ? filteredCount : 1));
    if (scrollBarHeight < 20.0f)
        scrollBarHeight = 20.0f;
    if (scrollBarHeight > scrollTrackHeight)
        scrollBarHeight = scrollTrackHeight;

    float scrollStep = (scrollTrackHeight - scrollBarHeight) / (maxScroll + 1);
    float scrollBarY = scrollTrackY + editor->scrollIndexNodeMenu * scrollStep;

    DrawRectangleRounded((Rectangle){editor->menuPosition.x + 2, scrollBarY, 8, scrollBarHeight}, 0.8f, 4, ScrollIndicatorColor);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        editor->nodeMenuSearch[0] = '\0';
        editor->menuOpen = false;
    }

    if (strlen(editor->nodeMenuSearch) == 0 && editor->hoveredItem >= 0 && editor->hoveredItem < menuItemCount)
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
                    editor->menuOpen = false;
                    return subMenuItems[editor->hoveredItem][j];
                }
            }
            DrawTextEx(editor->font, subMenuItems[editor->hoveredItem][j], (Vector2){editor->submenuPosition.x + 20, editor->submenuPosition.y + j * MENU_ITEM_HEIGHT + 10}, 25, 0, WHITE);
        }
    }

    Rectangle searchRect = {editor->menuPosition.x + 5, editor->menuPosition.y + 5, MENU_WIDTH - 10, searchBarHeight};
    DrawRectangleRec(searchRect, DARKGRAY);
    char buff[MAX_SEARCH_BAR_FIELD_SIZE];
    if (editor->nodeMenuSearch[0] == '\0'){
        strmac(buff, 6, "Search");
    }
    else{
        strmac(buff, MAX_SEARCH_BAR_FIELD_SIZE, editor->nodeMenuSearch);
    }
    DrawTextEx(editor->font, buff, (Vector2){searchRect.x + 5, searchRect.y + 5}, 20, 0, WHITE);

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
        const char *createdNode = DrawNodeMenu(editor, view);

        if (strcmp(createdNode, "NULL") != 0)
        {
            NodeType newNodeType = StringToNodeType(createdNode);
            CreateNode(graph, newNodeType, editor->rightClickPos);
            editor->rightClickPos = (Vector2){0, 0};
            if (newNodeType == NODE_CREATE_NUMBER || newNodeType == NODE_CREATE_STRING || newNodeType == NODE_CREATE_BOOL || newNodeType == NODE_CREATE_COLOR || newNodeType == NODE_CREATE_SPRITE)
            {
                graph->variables = realloc(graph->variables, sizeof(char *) * (graph->variablesCount + 1));
                graph->variables[graph->variablesCount] = strmac(NULL, MAX_VARIABLE_NAME_SIZE, "%s", graph->nodes[graph->nodeCount - 1].name);

                graph->variableTypes = realloc(graph->variableTypes, sizeof(int) * (graph->variablesCount + 1));
                graph->variableTypes[graph->variablesCount] = graph->nodes[graph->nodeCount - 1].type;

                graph->variablesCount++;
            }
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

void HandleEditor(EditorContext *editor, GraphContext *graph, RenderTexture2D *viewport, Vector2 mousePos, bool draggingDisabled, bool isSecondFrame)
{
    editor->newLogMessage = false;
    editor->cursor = MOUSE_CURSOR_ARROW;

    editor->screenWidth = viewport->texture.width;
    editor->screenHeight = viewport->texture.height;
    editor->mousePos = mousePos;

    static RenderTexture2D dot;

    if (isSecondFrame)
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