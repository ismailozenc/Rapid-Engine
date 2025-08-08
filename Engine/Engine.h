#include "raylib.h"
#include "raymath.h"

#define MAX_UI_ELEMENTS 100

#define MAX_PATH_LENGTH 1024

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

const double doubleClickThreshold = 0.3;

#define MAX_LAYER_COUNT 100

typedef enum
{
    NO_COLLISION_ACTION,
    SAVE_CG,
    STOP_GAME,
    RUN_GAME,
    BUILD_GRAPH,
    BACK_FILEPATH,
    REFRESH_FILES,
    CLOSE_WINDOW,
    MINIMIZE_WINDOW,
    RESIZE_BOTTOM_BAR,
    RESIZE_SIDE_BAR,
    RESIZE_SIDE_BAR_MIDDLE,
    OPEN_FILE,
    CHANGE_VARS_FILTER,
    VAR_TOOLTIP_RUNTIME,
    VIEWPORT_FULLSCREEN
} UIElementCollisionType;

typedef enum
{
    UIRectangle,
    UICircle,
    UILine,
    UIText
} UIElementShape;

typedef enum
{
    VAR_FILTER_ALL,
    VAR_FILTER_NUMBERS,
    VAR_FILTER_STRINGS,
    VAR_FILTER_BOOLS,
    VAR_FILTER_COLORS,
    VAR_FILTER_SPRITES
}VarFilter;

typedef enum
{
    LOG_LEVEL_NORMAL,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_SAVE,
    LOG_LEVEL_DEBUG
}LogLevel;

typedef struct LogEntry
{
    char message[256];
    LogLevel level;
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

    int valueIndex;
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
    Texture2D resizeButton, viewportFullscreenButton;
    char *currentPath;
    char *CGFilePath;
    Font font;
    Logs logs;
    bool delayFrames;
    FilePathList files;
    int draggingResizeButtonID;
    bool hasResizedBar;
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
    float editorZoom;
    bool wasBuilt;
    int showSaveWarning;
    bool isGameFullscreen;
    VarFilter varsFilter;
} EngineContext;

typedef enum
{
    FILE_FOLDER = 0,
    FILE_CG,
    FILE_IMAGE,
    FILE_OTHER
}FileType;