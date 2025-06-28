#include <stdio.h>
#include <raylib.h>
#include <direct.h>
#include <stdlib.h>
#include <time.h>
#include "shell_execute.h"
#include "CGEditor.h"
#include "ProjectManager.h"

#define MAX_UI_ELEMENTS 100

double lastClickTime = 0;
const double doubleClickThreshold = 0.5;

char openedFileName[32] = "Game";
bool isEditorOpened = false;

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

Logs InitLogs()
{
    Logs logs;
    logs.count = 0;
    logs.capacity = 100;
    logs.entries = malloc(sizeof(LogEntry) * logs.capacity);
    return logs;
}

typedef struct UIElement
{
    char name[32];
    int type;
    union
    {
        struct
        {
            Vector2 pos;
            Vector2 recSize;
            float roundness;
            int roundSegments;
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
    Vector2 mousePos;
    RenderTexture2D viewport, UI;
    Texture2D resizeButton;
    char *projectPath;
    Font font;
    Logs logs;
    bool delayFrames;
    FilePathList files;
    int draggingResizeButtonID;
    int cursor;
    UIElement uiElements[MAX_UI_ELEMENTS]; // temporary hardcoded size
    int uiElementCount;
} EngineContext;

void AddToLog(EngineContext *engine, const char *newLine, int level);

void EmergencyExit(EngineContext *engine);

EngineContext InitEngineContext(char *projectPath)
{
    EngineContext engine = {0};

    engine.logs = InitLogs();

    engine.screenWidth = GetScreenWidth();
    engine.screenHeight = GetScreenHeight();
    engine.sideBarWidth = engine.screenWidth * 0.2;
    engine.bottomBarHeight = engine.screenHeight * 0.25;

    engine.sideBarMiddleY = (engine.screenHeight - engine.bottomBarHeight) / 2 + 20;

    engine.mousePos = GetMousePosition();

    engine.viewport = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    engine.UI = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    engine.resizeButton = LoadTexture("resize_btn.png");
    if (engine.UI.id == 0 || engine.viewport.id == 0 || engine.resizeButton.id == 0)
    {
        AddToLog(&engine, "Couldn't load textures", 2);
        EmergencyExit(&engine);
    }

    engine.delayFrames = true;
    engine.draggingResizeButtonID = 0;

    engine.font = LoadFontEx("fonts/arialbd.ttf", 128, NULL, 0);
    if (engine.font.texture.id == 0)
    {
        AddToLog(&engine, "Failed to load font: fonts/arialbd.ttf", 1);
        EmergencyExit(&engine);
    }

    if (projectPath == NULL || projectPath[0] == '\0')
    {
        AddToLog(&engine, "Couldn't find file", 2);
        EmergencyExit(&engine);
    }
    else
    {
        engine.projectPath = projectPath;
    }

    engine.cursor = MOUSE_CURSOR_POINTING_HAND;

    engine.files = LoadDirectoryFilesEx(projectPath, NULL, false);

    return engine;
}

void FreeEngineContext(EngineContext *engine)
{
    if (engine->projectPath)
    {
        free(engine->projectPath);
        engine->projectPath = NULL;
    }

    if (engine->logs.entries)
    {
        free(engine->logs.entries);
        engine->logs.entries = NULL;
    }

    UnloadRenderTexture(engine->viewport);
    UnloadRenderTexture(engine->UI);
    UnloadTexture(engine->resizeButton);
}

void AddUIElement(EngineContext *engine, UIElement element)
{
    if (engine->uiElementCount < MAX_UI_ELEMENTS)
    {
        engine->uiElements[engine->uiElementCount++] = element;
    }
    else
    {
        AddToLog(engine, "UIElement limit reached", 2);
    }
}

void AddToLog(EngineContext *engine, const char *newLine, int level)
{
    if (engine->logs.count >= engine->logs.capacity)
    {
        engine->logs.capacity += 100;
        engine->logs.entries = realloc(engine->logs.entries, sizeof(LogEntry) * engine->logs.capacity);
        if (!engine->logs.entries)
        {
            // handle out-of-memory error gracefully
            exit(1);
        }
    }

    time_t timestamp = time(NULL);
    struct tm *tm_info = localtime(&timestamp);

    char timeStampedMessage[256];
    snprintf(timeStampedMessage, sizeof(timeStampedMessage), "%02d:%02d:%02d %s",
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, newLine);

    strncpy(engine->logs.entries[engine->logs.count].message, timeStampedMessage, sizeof(engine->logs.entries[0].message));
    engine->logs.entries[engine->logs.count].level = level;

    engine->logs.count++;
    engine->delayFrames = true;
}

void EmergencyExit(EngineContext *engine)
{
    FILE *logFile = fopen("engine_log.txt", "w");
    if (logFile)
    {
        for (int i = 0; i < engine->logs.count; i++)
        {
            fprintf(logFile, "[%s] %s\n",
                    engine->logs.entries[i].level == 0 ? "INFO" : engine->logs.entries[i].level == 1 ? "WARN"
                                                                                                     : "ERROR",
                    engine->logs.entries[i].message);
        }
        fclose(logFile);
    }

    exit(1);
}

void DrawTopBar()
{
    DrawRectangleLinesEx((Rectangle){1, 1, GetScreenWidth() - 1, GetScreenHeight() - 1}, 4.0f, WHITE);

    float half = 10;

    Vector2 p1 = {GetScreenWidth() - 25 - half, 25 - half};
    Vector2 p2 = {GetScreenWidth() - 25 + half, 25 + half};
    Vector2 p3 = {GetScreenWidth() - 25 - half, 25 + half};
    Vector2 p4 = {GetScreenWidth() - 25 + half, 25 - half};

    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){GetScreenWidth() - 50, 0, 50, 50}))
    {
        DrawRectangle(GetScreenWidth() - 50, 0, 50, 50, RED);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            CloseWindow();
            return;
        }
    }

    DrawLineEx(p1, p2, 2, WHITE);
    DrawLineEx(p3, p4, 2, WHITE);

    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){GetScreenWidth() - 100, 0, 50, 50}))
    {
        DrawRectangle(GetScreenWidth() - 100, 0, 50, 50, GRAY);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            MinimizeWindow();
        }
    }

    DrawLineEx((Vector2){GetScreenWidth() - 85, 25}, (Vector2){GetScreenWidth() - 65, 25}, 2, WHITE);
}

char *PrepareProjectPath(char *fileName)
{
    char *projectPath = malloc(260);
    if (!projectPath)
        return NULL;

    getcwd(projectPath, 260);

    projectPath[strlen(projectPath) - 7] = '\0';

    strcat(projectPath, "\\Projects\\");
    strcat(projectPath, fileName);

    return projectPath;
}

int GetFileType(const char *fileName)
{
    const char *ext = strrchr(fileName, '.');
    if (!ext || *(ext + 1) == '\0')
        return 0;

    if (strcmp(ext + 1, "c") == 0)
        return 1;
    else if (strcmp(ext + 1, "cg") == 0)
        return 2;
    else if (strcmp(ext + 1, "txt") == 0)
        return 3;
    else if (strcmp(ext + 1, "png") == 0 || strcmp(ext + 1, "jpg") == 0 || strcmp(ext + 1, "jpeg") == 0)
        return 4;

    return -1;
}

void RefreshBottomBar(FilePathList *files, char *projectPath)
{
    *files = LoadDirectoryFilesEx(projectPath, NULL, false);
}

void LoadFiles(EngineContext *engine)
{
    int xOffset = 50;
    int yOffset = engine->screenHeight - engine->bottomBarHeight + 70;
    Vector2 mousePos = GetMousePosition();
    bool showTooltip = false;
    char tooltipText[256] = "";
    Rectangle tooltipRect = {0};
    char *currentPath = engine->projectPath;

    AddUIElement(engine, (UIElement){
                             .name = "BackButton",
                             .type = 0, // rect
                             .rect = {.pos = {30, engine->screenHeight - engine->bottomBarHeight + 10}, .recSize = {65, 30}, .roundness = 0, .roundSegments = 0},
                             .color = (Color){70, 70, 70, 150},
                             .layer = 0,
                             .text = {.string = "Back", .textPos = {35, engine->screenHeight - engine->bottomBarHeight + 12}, .textSize = 25, .textSpacing = 0, .textColor = WHITE}});

    AddUIElement(engine, (UIElement){
                             .name = "RefreshButton",
                             .type = 0, // rect
                             .rect = {.pos = {110, engine->screenHeight - engine->bottomBarHeight + 10}, .recSize = {100, 30}, .roundness = 0, .roundSegments = 0},
                             .color = (Color){70, 70, 70, 150},
                             .layer = 0,
                             .text = {.string = "Refresh", .textPos = {119, engine->screenHeight - engine->bottomBarHeight + 12}, .textSize = 25, .textSpacing = 0, .textColor = WHITE}});

    AddUIElement(engine, (UIElement){
                             .name = "CurrentPathText",
                             .type = 3, // text only
                             .color = (Color){0, 0, 0, 0},
                             .layer = 0,
                             .text = {.string = "", .textPos = {230, engine->screenHeight - engine->bottomBarHeight + 15}, .textSize = 22, .textSpacing = 2, .textColor = WHITE}});
    strncpy(engine->uiElements[engine->uiElementCount - 1].text.string, currentPath, 256);
    engine->uiElements[engine->uiElementCount - 1].text.string[256] = '\0';

    Rectangle backButton = {30, engine->screenHeight - engine->bottomBarHeight + 10, 65, 30};
    if (CheckCollisionPointRec(mousePos, backButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        char *lastSlash = strrchr(currentPath, '\\');
        if (lastSlash != NULL)
        {
            *lastSlash = '\0';
        }
        RefreshBottomBar(&engine->files, currentPath);
    }

    Rectangle refreshButton = {110, engine->screenHeight - engine->bottomBarHeight + 10, 100, 30};
    if (CheckCollisionPointRec(mousePos, refreshButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        RefreshBottomBar(&engine->files, currentPath);
    }

    if (CheckCollisionPointRec(mousePos, backButton))
    {
        AddUIElement(engine, (UIElement){
                                 .name = "BackButtonHover",
                                 .type = 0,
                                 .rect = {.pos = {30, engine->screenHeight - engine->bottomBarHeight + 10}, .recSize = {65, 30}, .roundness = 0, .roundSegments = 0},
                                 .color = (Color){255, 255, 255, 100},
                                 .layer = 1,
                                 .text = {.string = "", .textPos = {0, 0}, .textSize = 0, .textSpacing = 0, .textColor = {0, 0, 0, 0}} // no text
                             });
    }

    if (CheckCollisionPointRec(mousePos, refreshButton))
    {
        AddUIElement(engine, (UIElement){
                                 .name = "RefreshButtonHover",
                                 .type = 0,
                                 .rect = {.pos = {110, engine->screenHeight - engine->bottomBarHeight + 10}, .recSize = {100, 30}, .roundness = 0, .roundSegments = 0},
                                 .color = (Color){255, 255, 255, 100},
                                 .layer = 1,
                                 .text = {.string = "", .textPos = {0, 0}, .textSize = 0, .textSpacing = 0, .textColor = {0, 0, 0, 0}} // no text
                             });
    }

    for (int i = 0; i < engine->files.count; i++)
    {
        const char *fileName = GetFileName(engine->files.paths[i]);
        Rectangle fileRect = {xOffset, yOffset, 150, 60};

        Color fileColor;

        switch (GetFileType(fileName))
        {
        case 0:
            fileColor = (Color){200, 160, 50, 255};
            break;
        case 1:
            fileColor = (Color){40, 100, 180, 255};
            break;
        case 2:
            fileColor = (Color){80, 150, 80, 255};
            break;
        case 3:
            fileColor = (Color){120, 90, 180, 255};
            break;
        case 4:
            fileColor = (Color){160, 40, 40, 255};
            break;
        default:
            fileColor = (Color){110, 110, 110, 255};
            break;
        }

        AddUIElement(engine, (UIElement){
                                 .name = "FileRect",
                                 .type = 0,
                                 .rect = {.pos = {fileRect.x, fileRect.y}, .recSize = {fileRect.width, fileRect.height}, .roundness = 0.5f, .roundSegments = 8},
                                 .color = fileColor,
                                 .layer = 0,
                                 .text = {.string = "", .textPos = {0, 0}, .textSize = 0, .textSpacing = 0, .textColor = {0, 0, 0, 0}}});

        char buff[256];
        strncpy(buff, fileName, sizeof(buff) - 1);
        buff[sizeof(buff) - 1] = '\0';

        if (MeasureTextEx(engine->font, fileName, 25, 0).x > 135)
        {
            for (int j = (int)strlen(buff) - 1; j >= 0; j--)
            {
                buff[j] = '\0';
                if (MeasureText(buff, 25) < 135)
                {
                    if (j > 2)
                    {
                        buff[j - 2] = '\0';
                        strcat(buff, "...");
                    }
                    break;
                }
            }
        }

        AddUIElement(engine, (UIElement){
                                 .name = "FileNameText",
                                 .type = 3,
                                 .color = (Color){0, 0, 0, 0},
                                 .layer = 1,
                                 .text = {.string = "", .textPos = {fileRect.x + 10, fileRect.y + 16}, .textSize = 25, .textSpacing = 0, .textColor = BLACK}});
        strncpy(engine->uiElements[engine->uiElementCount - 1].text.string, buff, 31);
        engine->uiElements[engine->uiElementCount - 1].text.string[256] = '\0';

        bool isHovered = CheckCollisionPointRec(mousePos, fileRect);
        if (isHovered)
        {
            AddUIElement(engine, (UIElement){
                                     .name = "FilesHover",
                                     .type = 0,
                                     .rect = {.pos = {fileRect.x, fileRect.y}, .recSize = {fileRect.width, fileRect.height}, .roundness = 0.5f, .roundSegments = 8},
                                     .color = (Color){255, 255, 255, 100},
                                     .layer = 2,
                                     .text = {.string = "", .textPos = {0, 0}, .textSize = 0, .textSpacing = 0, .textColor = {0, 0, 0, 0}}});

            snprintf(tooltipText, sizeof(tooltipText), "File: %s\nSize: %ld bytes", fileName, GetFileLength(engine->files.paths[i]));
            tooltipRect = (Rectangle){xOffset, yOffset - 61, MeasureTextEx(engine->font, tooltipText, 20, 0).x + 20, 60};
            showTooltip = true;
        }

        if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            double currentTime = GetTime();
            static double lastClickTime = 0;
            static const double doubleClickThreshold = 0.3;

            if (currentTime - lastClickTime <= doubleClickThreshold)
            {
                if (GetFileType(fileName) == 2)
                {
                    char *buff = malloc(strlen(fileName) + 1);
                    strcpy(buff, fileName);
                    buff[strlen(fileName) - 3] = '\0';
                    strcpy(openedFileName, buff);
                    free(buff);
                    isEditorOpened = true;
                }
                else if (GetFileType(fileName) != 0)
                {
                    char command[512];
                    snprintf(command, sizeof(command), "start \"\" \"%s\"", engine->files.paths[i]);
                    system(command);
                }
                else
                {
                    strcat(currentPath, "\\");
                    strcat(currentPath, fileName);
                    RefreshBottomBar(&engine->files, currentPath);
                }
            }
            lastClickTime = currentTime;
        }

        xOffset += 250;
        if (xOffset + 100 >= engine->screenWidth)
        {
            xOffset = 50;
            yOffset += 120;
        }
    }

    if (showTooltip)
    {
        AddUIElement(engine, (UIElement){
                                 .name = "TooltipBackground",
                                 .type = 0,
                                 .rect = {.pos = {tooltipRect.x, tooltipRect.y}, .recSize = {tooltipRect.width, tooltipRect.height}, .roundness = 0, .roundSegments = 0},
                                 .color = DARKGRAY,
                                 .layer = 10,
                                 .text = {.string = "", .textPos = {0, 0}, .textSize = 0, .textSpacing = 0, .textColor = {0, 0, 0, 0}}});

        AddUIElement(engine, (UIElement){
                                 .name = "TooltipText",
                                 .type = 3,
                                 .color = (Color){0, 0, 0, 0},
                                 .layer = 11,
                                 .text = {.string = "", .textPos = {tooltipRect.x + 10, tooltipRect.y + 10}, .textSize = 20, .textSpacing = 0, .textColor = WHITE}});
        strncpy(engine->uiElements[engine->uiElementCount - 1].text.string, tooltipText, 255);
        engine->uiElements[engine->uiElementCount - 1].text.string[255] = '\0';
    }
}

void DrawUIElements(EngineContext *engine)
{
    for (int i = 0; i < engine->uiElementCount; i++)
    {
        UIElement *el = &engine->uiElements[i];
        switch (el->type)
        {
        case 0:
            DrawRectangleRounded((Rectangle){el->rect.pos.x, el->rect.pos.y, el->rect.recSize.x, el->rect.recSize.y}, el->rect.roundness, el->rect.roundSegments, el->color);
            break;
        case 1:
            DrawCircleV(el->circle.center, el->circle.radius, el->color);
            break;
        case 2:
            DrawLineEx(el->line.startPos, el->line.engPos, el->line.thickness, el->color);
            break;
        }

        if (el->text.string[0] != '\0')
        {
            DrawTextEx(engine->font, el->text.string, el->text.textPos, el->text.textSize, el->text.textSpacing, el->text.textColor);
        }
    }
}

void BuildUITexture(int screenWidth, int screenHeight, int sideBarWidth, int bottomBarHeight, FilePathList *files, char *projectPath, EngineContext *engine, GraphContext *graph, char *CGFilePath)
{
    engine->uiElementCount = 0; //

    BeginTextureMode(engine->UI);
    ClearBackground((Color){255, 255, 255, 0});

    if (engine->screenWidth > engine->screenHeight && engine->screenWidth > 1000)
    {
        AddUIElement(engine, (UIElement){
                                 .name = "SideBarVars",
                                 .type = 0,
                                 .rect = {.pos = {0, 0}, .recSize = {engine->sideBarWidth, engine->sideBarMiddleY}, .roundness = 0.0f, .roundSegments = 0},
                                 .color = (Color){28, 28, 28, 255},
                                 .layer = 0,
                             });

        AddUIElement(engine, (UIElement){
                                 .name = "SideBarLog",
                                 .type = 0,
                                 .rect = {.pos = {0, engine->sideBarMiddleY}, .recSize = {sideBarWidth, screenHeight - bottomBarHeight}, .roundness = 0.0f, .roundSegments = 0},
                                 .color = (Color){15, 15, 15, 255},
                                 .layer = 0,
                             });

        AddUIElement(engine, (UIElement){
                                 .name = "SideBarMiddleLine",
                                 .type = 2,
                                 .line = {.startPos = {sideBarWidth, 0}, .engPos = {sideBarWidth, screenHeight - bottomBarHeight}, .thickness = 2},
                                 .color = WHITE,
                                 .layer = 0,
                             });

        AddUIElement(engine, (UIElement){
                                 .name = "SideBarFromViewportDividerLine",
                                 .type = 2,
                                 .line = {.startPos = {0, engine->sideBarMiddleY}, .engPos = {engine->sideBarWidth, engine->sideBarMiddleY}, .thickness = 2},
                                 .color = WHITE,
                                 .layer = 0,
                             });

        AddUIElement(engine, (UIElement){
                                 .name = "SaveButton",
                                 .type = 0,
                                 .rect = {.pos = {sideBarWidth - 140, engine->sideBarMiddleY + 15}, .recSize = {60, 30}, .roundness = 0.2f, .roundSegments = 8},
                                 .color = (Color){255, 255, 255, 50},
                                 .layer = 1,
                                 .text = {.string = "Save", .textPos = {sideBarWidth - 135, engine->sideBarMiddleY + 20}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                             });

        if (CheckCollisionPointRec(engine->mousePos, (Rectangle){engine->sideBarWidth - 140, engine->sideBarMiddleY + 15, 60, 30}))
        {
            AddUIElement(engine, (UIElement){
                                     .name = "SaveButtonHover",
                                     .type = 0,
                                     .rect = {.pos = {sideBarWidth - 140, engine->sideBarMiddleY + 15}, .recSize = {60, 30}, .roundness = 0.2f, .roundSegments = 8},
                                     .color = Fade(WHITE, 0.6f),
                                     .layer = 2,
                                 });

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if (SaveGraphToFile(CGFilePath, graph) == 0)
                    AddToLog(engine, "Saved successfully!", 0);
                else
                    AddToLog(engine, "ERROR SAVING CHANGES!", 1);
            }
        }

        AddUIElement(engine, (UIElement){
                                 .name = "RunButton",
                                 .type = 0,
                                 .rect = {.pos = {sideBarWidth - 70, engine->sideBarMiddleY + 15}, .recSize = {60, 30}, .roundness = 0.2f, .roundSegments = 8},
                                 .color = (Color){255, 255, 255, 50},
                                 .layer = 1,
                                 .text = {.string = "Run", .textPos = {engine->sideBarWidth - 60, engine->sideBarMiddleY + 20}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                             });

        if (CheckCollisionPointRec(engine->mousePos, (Rectangle){sideBarWidth - 70, engine->sideBarMiddleY + 15, 60, 30}))
        {
            AddUIElement(engine, (UIElement){
                                     .name = "RunButtonHover",
                                     .type = 0,
                                     .rect = {.pos = {sideBarWidth - 70, engine->sideBarMiddleY + 15}, .recSize = {60, 30}, .roundness = 0.2f, .roundSegments = 8},
                                     .color = Fade(WHITE, 0.6f),
                                     .layer = 2,
                                 });

            engine->cursor = MOUSE_CURSOR_NOT_ALLOWED;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                AddToLog(engine, "Interpreter not ready", 2);
            }
        }

        // Logs (just adding text elements)
        int y = screenHeight - bottomBarHeight - 30;
        for (int i = engine->logs.count - 1; i >= 0 && y > engine->sideBarMiddleY + 50; i--)
        {
            Color messageColor = (engine->logs.entries[i].level == 0) ? WHITE : (engine->logs.entries[i].level == 1) ? YELLOW
                                                                                                                     : RED;

            AddUIElement(engine, (UIElement){
                                     .name = "LogText",
                                     .type = 3,
                                     .text = {.textPos = {20, y}, .textSize = 20, .textSpacing = 2, .textColor = messageColor},
                                     .layer = 0,
                                     .color = messageColor,
                                 });
            strncpy(engine->uiElements[engine->uiElementCount - 1].text.string, engine->logs.entries[i].message, 127);
            engine->uiElements[engine->uiElementCount - 1].text.string[128] = '\0';
            y -= 25;
        }
    }

    AddUIElement(engine, (UIElement){
                             .name = "BottomBar",
                             .type = 0,
                             .rect = {.pos = {0, screenHeight - bottomBarHeight}, .recSize = {screenWidth, bottomBarHeight}, .roundness = 0.0f, .roundSegments = 0},
                             .color = (Color){28, 28, 28, 255},
                             .layer = 0,
                         });

    AddUIElement(engine, (UIElement){
                             .name = "BottomBarFromViewportDividerLine",
                             .type = 2,
                             .line = {.startPos = {0, screenHeight - bottomBarHeight}, .engPos = {screenWidth, screenHeight - bottomBarHeight}, .thickness = 2},
                             .color = WHITE,
                             .layer = 0,
                         });

    LoadFiles(engine);
    DrawUIElements(engine);

    DrawTopBar(); //

    DrawTexture(engine->resizeButton, engine->screenWidth / 2 - 10, engine->screenHeight - engine->bottomBarHeight - 10, WHITE);
    DrawTexturePro(engine->resizeButton, (Rectangle){0, 0, 20, 20}, (Rectangle){sideBarWidth, (engine->screenHeight - engine->bottomBarHeight) / 2, 20, 20}, (Vector2){10, 10}, 90.0f, WHITE);
    DrawTexture(engine->resizeButton, sideBarWidth / 2 - 10, engine->sideBarMiddleY - 10, WHITE);
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
    {
        if (CheckCollisionPointCircle(engine->mousePos, (Vector2){engine->screenWidth / 2, engine->screenHeight - engine->bottomBarHeight}, 10))
        {
            engine->draggingResizeButtonID = 1;
        }
        else if (CheckCollisionPointCircle(engine->mousePos, (Vector2){engine->sideBarWidth, (engine->screenHeight - engine->bottomBarHeight) / 2}, 10))
        {
            engine->draggingResizeButtonID = 2;
        }
        else if (CheckCollisionPointCircle(engine->mousePos, (Vector2){engine->sideBarWidth / 2, engine->sideBarMiddleY}, 10))
        {
            engine->draggingResizeButtonID = 3;
        }
    }

    if (IsMouseButtonUp(MOUSE_LEFT_BUTTON))
    {
        engine->draggingResizeButtonID = 0;
    }

    switch (engine->draggingResizeButtonID)
    {
    case 0:
        break;
    case 1:
        engine->bottomBarHeight -= GetMouseDelta().y;
        engine->cursor = MOUSE_CURSOR_RESIZE_NS;
        break;
    case 2:
        engine->sideBarWidth += GetMouseDelta().x;
        engine->cursor = MOUSE_CURSOR_RESIZE_EW;
        break;
    case 3:
        engine->sideBarMiddleY += GetMouseDelta().y;
        engine->cursor = MOUSE_CURSOR_RESIZE_NS;
        break;
    default:
        break;
    }

    EndTextureMode();
}

int CheckCollisions(EngineContext *engine, int fileCount, char *projectPath, char *CGFilePath, GraphContext *graph)
{
    int xOffset = 50;
    int yOffset = engine->screenHeight - engine->bottomBarHeight + 70;
    Vector2 mousePos = GetMousePosition();
    Rectangle tooltipRect = {0};
    char *currentPath = projectPath;

    Rectangle backButton = {30, engine->screenHeight - engine->bottomBarHeight + 10, 65, 30};

    Rectangle refreshButton = {110, engine->screenHeight - engine->bottomBarHeight + 10, 100, 30};

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S))
    {
        if (SaveGraphToFile(CGFilePath, graph) == 0)
        {
            AddToLog(engine, "Saved successfully!", 0);
        }
        else
        {
            AddToLog(engine, "ERROR SAVING CHANGES!", 2);
        }
    }

    if (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_LEFT_BUTTON))
    {
        return 1;
    }

    if (CheckCollisionPointRec(mousePos, backButton) || CheckCollisionPointRec(mousePos, refreshButton))
    {
        return 1;
    }

    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){GetScreenWidth() - 100, 0, 100, 50}))
    {
        return 1;
    }

    if (CheckCollisionPointRec(mousePos, (Rectangle){engine->sideBarWidth - 140, engine->sideBarMiddleY + 15, 60, 30}) || CheckCollisionPointRec(mousePos, (Rectangle){engine->sideBarWidth - 70, engine->sideBarMiddleY + 15, 60, 30}))
    {
        return 1;
    }

    if (CheckCollisionPointCircle(engine->mousePos, (Vector2){engine->screenWidth / 2, engine->screenHeight - engine->bottomBarHeight}, 10) || CheckCollisionPointCircle(engine->mousePos, (Vector2){engine->sideBarWidth, (engine->screenHeight - engine->bottomBarHeight) / 2}, 10) || CheckCollisionPointCircle(engine->mousePos, (Vector2){engine->sideBarWidth / 2, engine->sideBarMiddleY}, 10))
    {
        return 1;
    }

    for (int i = 0; i < fileCount; i++)
    {
        Rectangle fileRect = {xOffset, yOffset, 150, 60};

        if (CheckCollisionPointRec(mousePos, fileRect))
        {
            return 1;
        }

        xOffset += 250;

        if (xOffset + 100 >= engine->screenWidth)
        {
            xOffset = 50;
            yOffset += 120;
        }
    }

    return 0;
}

int main()
{

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED);
    InitWindow(1600, 1000, "RapidEngine");
    SetTargetFPS(140);

    char *projectPath = PrepareProjectPath(/*handleProjectManager()*/ "Tetris"); // temporary hardcode

    SetTargetFPS(60);

    MaximizeWindow();

    EngineContext engine = InitEngineContext(projectPath);
    EditorContext editor = InitEditorContext();
    GraphContext graph = InitGraphContext();

    AddToLog(&engine, "All resources loaded", 0);

    int prevScreenWidth = GetScreenWidth();
    int prevScreenHeight = GetScreenHeight();

    AddToLog(&engine, "Welcome!", 0);

    while (!WindowShouldClose())
    {
        engine.mousePos = GetMousePosition();
        engine.screenWidth = GetScreenWidth();
        engine.screenHeight = GetScreenHeight();

        int collisionResult = CheckCollisions(&engine, engine.files.count, projectPath, editor.CGFilePath, &graph);

        if (prevScreenWidth != engine.screenWidth || prevScreenHeight != engine.screenHeight)
        {
            engine.delayFrames = true;
        }

        if (collisionResult == 1)
        {
            engine.cursor = MOUSE_CURSOR_POINTING_HAND;
            BuildUITexture(engine.screenWidth, engine.screenHeight, engine.sideBarWidth, engine.bottomBarHeight, &engine.files, projectPath, &engine, &graph, editor.CGFilePath);
            engine.delayFrames = true;
            prevScreenWidth = engine.screenWidth;
            prevScreenHeight = engine.screenHeight;
            SetMouseCursor(engine.cursor);
            SetTargetFPS(140);
        }
        else if (engine.delayFrames)
        {
            BuildUITexture(engine.screenWidth, engine.screenHeight, engine.sideBarWidth, engine.bottomBarHeight, &engine.files, projectPath, &engine, &graph, editor.CGFilePath);
            engine.delayFrames = false;
            engine.cursor = MOUSE_CURSOR_ARROW;
            SetMouseCursor(engine.cursor);
            SetTargetFPS(60);
        }

        BeginDrawing();
        ClearBackground(BLACK);

        DrawTextureRec(engine.viewport.texture, (Rectangle){0, 0, engine.viewport.texture.width, -engine.viewport.texture.height}, (Vector2){0, 0}, WHITE);

        DrawTextureRec(engine.UI.texture, (Rectangle){0, 0, engine.UI.texture.width, -engine.UI.texture.height}, (Vector2){0, 0}, WHITE);

        if (!isEditorOpened)
        {
            BeginTextureMode(engine.viewport);
            ClearBackground(BLACK);
            EndTextureMode();
        }
        else if (isEditorOpened)
        {
            if (strcmp(openedFileName, editor.fileName) != 0)
            {
                OpenNewCGFile(&editor, &graph, openedFileName);
            }

            handleEditor(&editor, &graph, &engine.viewport);
            if (editor.newLogMessage)
            {
                AddToLog(&engine, editor.logMessage, editor.logMessageLevel);
            }
        }

        DrawFPS(10, 10);

        EndDrawing();
    }

    free(projectPath);

    UnloadDirectoryFiles(engine.files);

    UnloadFont(editor.font);
    UnloadFont(engine.font);

    FreeEngineContext(&engine);
    FreeEditorContext(&editor);
    FreeGraphContext(&graph);

    return 0;
}
