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
    UI_ACTION_NO_COLLISION_ACTION,
    UI_ACTION_SAVE_CG,
    UI_ACTION_STOP_GAME,
    UI_ACTION_RUN_GAME,
    UI_ACTION_BUILD_GRAPH,
    UI_ACTION_BACK_FILEPATH,
    UI_ACTION_REFRESH_FILES,
    UI_ACTION_CLOSE_WINDOW,
    UI_ACTION_MINIMIZE_WINDOW,
    UI_ACTION_OPEN_SETTINGS,
    UI_ACTION_RESIZE_BOTTOM_BAR,
    UI_ACTION_RESIZE_SIDE_BAR,
    UI_ACTION_RESIZE_SIDE_BAR_MIDDLE,
    UI_ACTION_OPEN_FILE,
    UI_ACTION_CHANGE_VARS_FILTER,
    UI_ACTION_VAR_TOOLTIP_RUNTIME,
    UI_ACTION_FULLSCREEN_BUTTON_VIEWPORT
} UIAction;

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
    UIAction type;
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

typedef enum{
    VIEWPORT_CG_EDITOR,
    VIEWPORT_GAME_SCREEN,
    VIEWPORT_HITBOX_EDITOR
}ViewportMode;

typedef struct EngineContext
{
    int screenWidth, screenHeight, bottomBarHeight, sideBarWidth, sideBarMiddleY;
    int prevScreenWidth;
    int prevScreenHeight;
    int viewportWidth;
    int viewportHeight;
    Vector2 mousePos;
    RenderTexture2D viewport, UI;
    Texture2D resizeButton, viewportFullscreenButton, settingsGear;
    char *currentPath;
    char *projectPath;
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
    bool showSettingsMenu;
    bool isGameFullscreen;
    VarFilter varsFilter;
    bool isSaveButtonHovered;
    ViewportMode viewportMode;
    bool isAutoSaveON;
    float autoSaveTimer;
    int fpsLimit;
} EngineContext;

typedef enum
{
    FILE_FOLDER = 0,
    FILE_CG,
    FILE_IMAGE,
    FILE_OTHER
}FileType;

typedef enum
{
    SETTINGS_MODE_ENGINE,
    SETTINGS_MODE_GAME,
    SETTINGS_MODE_KEYBINDS,
    SETTINGS_MODE_EXPORT
}SettingsMode;