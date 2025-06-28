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

#define MAX_PATH_LENGTH 1024
#define MAX_TYPE_LENGTH 16

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

typedef struct
{
    bool editorOpen;

    char *CGFilePath;
    char *fileName;

    int screenWidth;
    int screenHeight;

    bool delayFrames;

    Vector2 mousePos;
    Vector2 prevMousePos;
    Vector2 mousePosAtStartOfDrag;
    Vector2 rightClickPos;

    bool isDraggingScreen;
    int draggingNodeIndex;
    Vector2 dragOffset;

    bool menuOpen;
    bool submenuOpen;
    Vector2 menuPosition;
    Vector2 submenuPosition;
    int scrollIndex;
    int hoveredItem;

    Pin lastClickedPin;

    Font font;

    bool newLogMessage;
    char logMessage[128];
    int logMessageLevel;

    // float zoom;
} EditorContext;

EditorContext InitEditorContext(void);

void FreeEditorContext(EditorContext *EC);

void AddToEngineLog(EditorContext *editor, char *message, int level);

void SetProjectPaths(EditorContext *EC, const char *projectName);

void OpenNewCGFile(EditorContext *editor, GraphContext *graph, char *openedFileName);

void DrawBackgroundGrid(EditorContext *EC, int gridSpacing);

void DrawCurvedWire(Vector2 outputPos, Vector2 inputPos, float thickness, Color color);

void DrawNodes(EditorContext *EC, GraphContext *graph);

bool CheckNodeCollisions(EditorContext *EC, GraphContext *graph);

const char *DrawNodeMenu(EditorContext *EC);

void HandleDragging(EditorContext *EC, GraphContext *graph);

int DrawFullTexture(EditorContext *EC, GraphContext *graph, RenderTexture2D view);

bool CheckAllCollisions(EditorContext *EC, GraphContext *graph);

void handleEditor(EditorContext *EC, GraphContext *graph, RenderTexture2D *viewport, Vector2 mousePos, int screenWidth, int screenHeight);

#endif