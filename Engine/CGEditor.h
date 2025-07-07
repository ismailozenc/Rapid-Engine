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

typedef struct
{
    int screenWidth;
    int screenHeight;

    bool delayFrames;
    bool isFirstFrame;

    Vector2 mousePos;
    Vector2 rightClickPos;

    bool isDraggingScreen;
    int draggingNodeIndex;

    bool menuOpen;
    Vector2 menuPosition;
    Vector2 submenuPosition;
    int scrollIndexNodeMenu;
    int hoveredItem;

    Pin lastClickedPin;

    Font font;

    bool newLogMessage;
    char logMessage[128];
    int logMessageLevel;

    Vector2 cameraOffset;

    int cursor;

    int fps;

    float zoom;
} EditorContext;

EditorContext InitEditorContext(void);

void FreeEditorContext(EditorContext *editor);

void AddToEngineLog(EditorContext *editor, char *message, int level);

void OpenNewCGFile(EditorContext *editor, GraphContext *graph, char *openedFileName);

void DrawBackgroundGrid(EditorContext *editor, int gridSpacing, RenderTexture2D dot);

void DrawCurvedWire(Vector2 outputPos, Vector2 inputPos, float thickness, Color color);

void DrawNodes(EditorContext *editor, GraphContext *graph);

bool CheckNodeCollisions(EditorContext *editor, GraphContext *graph);

const char *DrawNodeMenu(EditorContext *editor);

void HandleDragging(EditorContext *editor, GraphContext *graph);

int DrawFullTexture(EditorContext *editor, GraphContext *graph, RenderTexture2D view, RenderTexture2D dot);

bool CheckAllCollisions(EditorContext *editor, GraphContext *graph);

void HandleEditor(EditorContext *editor, GraphContext *graph, RenderTexture2D *viewport, Vector2 mousePos, bool draggingDisabled);

#endif