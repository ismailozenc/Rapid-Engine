#ifndef EDITOR_H
#define EDITOR_H

#include <stdio.h>
#include "raylib.h"
#include "Nodes.h"

#define MENU_WIDTH 250
#define MENU_ITEM_HEIGHT 40
#define MENU_VISIBLE_ITEMS 5.5
#define MENU_BORDER_THICKNESS 3
#define SUBMENU_WIDTH 200

#define MAX_TYPE_LENGTH 16

typedef struct {
    Rectangle bounds;
    bool editing;
    char text[256];
    int length;
} TextBox;

typedef struct
{
    int screenWidth;
    int screenHeight;

    bool delayFrames;
    bool isFirstFrame;
    bool engineDelayFrames;

    Vector2 mousePos;
    Vector2 rightClickPos;

    Texture2D gearTxt;

    bool isDraggingScreen;
    int draggingNodeIndex;

    bool menuOpen;
    Vector2 menuPosition;
    Vector2 submenuPosition;
    int scrollIndexNodeMenu;
    int hoveredItem;

    Pin lastClickedPin;

    Font font;

    int nodeDropdownFocused;
    int nodeFieldPinFocused;

    bool newLogMessage;
    char logMessage[128];
    int logMessageLevel;

    Vector2 cameraOffset;

    int editingNodeNameIndex;

    int cursor;

    bool hasChanged;
    bool hasChangedInLastFrame;

    int fps;

    float zoom;
} EditorContext;

EditorContext InitEditorContext(void);

void FreeEditorContext(EditorContext *editor);

void HandleEditor(EditorContext *editor, GraphContext *graph, RenderTexture2D *viewport, Vector2 mousePos, bool draggingDisabled);

#endif