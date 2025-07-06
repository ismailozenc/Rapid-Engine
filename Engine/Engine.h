#include "raylib.h"

#define MAX_UI_ELEMENTS 100

const double doubleClickThreshold = 0.3;

typedef enum
{
    NO_COLLISION_ACTION,
    SAVE_CG,
    RUN_GAME,
    BACK_FILEPATH,
    REFRESH_FILES,
    CLOSE_WINDOW,
    MINIMIZE_WINDOW,
    RESIZE_BOTTOM_BAR,
    RESIZE_SIDE_BAR,
    RESIZE_SIDE_BAR_MIDDLE,
    OPEN_FILE,
    SHOW_VAR_INFO,
    VAR_TOOLTIP
} UIElementCollisionType;

typedef enum
{
    UIRectangle,
    UICircle,
    UILine,
    UIText
} UIElementShape;

typedef struct LogEntry
{
    char message[256];
    int level; // 0 = Info, 1 = Warning, 2 = Error
} LogEntry;

typedef struct Logs
{
    LogEntry *entries;
    int count;
    int capacity;
} Logs;

typedef struct UIElement
{
    char name[256];
    UIElementShape shape;
    UIElementCollisionType type;
    union
    {
        struct
        {
            Vector2 pos;
            Vector2 recSize;
            float roundness;
            int roundSegments;
            Color hoverColor;
        } rect;

        struct
        {
            Vector2 center;
            int radius;
        } circle;

        struct
        {
            Vector2 startPos;
            Vector2 engPos;
            int thickness;
        } line;
    };
    Color color;
    int layer;
    struct
    {
        char string[256];
        Vector2 textPos;
        int textSize;
        int textSpacing;
        Color textColor;
    } text;
} UIElement;

typedef struct EngineContext
{
    int screenWidth, screenHeight, bottomBarHeight, sideBarWidth, sideBarMiddleY;
    int prevScreenWidth;
    int prevScreenHeight;
    int viewportWidth;
    int viewportHeight;
    Vector2 mousePos;
    RenderTexture2D viewport, UI;
    Texture2D resizeButton;
    char *projectPath;
    char *currentPath;
    Font font;
    Logs logs;
    bool delayFrames;
    FilePathList files;
    int draggingResizeButtonID;
    bool hasResizedBar;
    int cursor;
    UIElement uiElements[MAX_UI_ELEMENTS]; // temporary hardcoded size
    int uiElementCount;
    int hoveredUIElementIndex;
    bool isEditorOpened;
    bool isViewportFocused;
    Sound save;
    bool isSoundOn;
    int fps;
    bool isGameRunning;
    bool sideBarHalfSnap;
    bool sideBarFullSnap;
} EngineContext;