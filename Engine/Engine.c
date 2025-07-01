#include <stdio.h>
#include <raylib.h>
#include <direct.h>
#include <stdlib.h>
#include <time.h>
#include "shell_execute.h"
#include "CGEditor.h"
#include "ProjectManager.h"
#include "Engine.h"

char openedFileName[32] = "Game";

Logs InitLogs()
{
    Logs logs;
    logs.count = 0;
    logs.capacity = 100;
    logs.entries = malloc(sizeof(LogEntry) * logs.capacity);
    return logs;
}

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

    engine.prevScreenWidth = engine.screenWidth;
    engine.prevScreenHeight = engine.screenHeight;

    engine.viewportWidth = engine.screenWidth - engine.sideBarWidth;
    engine.viewportHeight = engine.screenHeight - engine.bottomBarHeight;

    engine.sideBarMiddleY = (engine.screenHeight - engine.bottomBarHeight) / 2 + 20;

    engine.mousePos = GetMousePosition();

    engine.viewport = LoadRenderTexture(GetScreenWidth() - engine.sideBarWidth, GetScreenHeight() - engine.bottomBarHeight);
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
        engine.currentPath = projectPath;
    }

    engine.cursor = MOUSE_CURSOR_POINTING_HAND;

    engine.files = LoadDirectoryFilesEx(projectPath, NULL, false);

    engine.hoveredUIElementIndex = -1;

    engine.isEditorOpened = false;

    engine.save = LoadSound("save.wav");

    engine.isSoundOn = true;

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

    DrawLineEx((Vector2){GetScreenWidth() - 35, 15}, (Vector2){GetScreenWidth() - 15, 35}, 2, WHITE);
    DrawLineEx((Vector2){GetScreenWidth() - 35, 35}, (Vector2){GetScreenWidth() - 15, 15}, 2, WHITE);

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

void DrawUIElements(EngineContext *engine, char *CGFilePath, GraphContext *graph)
{
    if (engine->hoveredUIElementIndex != -1)
    {
        if (engine->uiElements[engine->hoveredUIElementIndex].shape == 0)
        {
            DrawRectangleRounded((Rectangle){engine->uiElements[engine->hoveredUIElementIndex].rect.pos.x, engine->uiElements[engine->hoveredUIElementIndex].rect.pos.y, engine->uiElements[engine->hoveredUIElementIndex].rect.recSize.x, engine->uiElements[engine->hoveredUIElementIndex].rect.recSize.y}, engine->uiElements[engine->hoveredUIElementIndex].rect.roundness, engine->uiElements[engine->hoveredUIElementIndex].rect.roundSegments, engine->uiElements[engine->hoveredUIElementIndex].rect.hoverColor);
        }

        switch (engine->uiElements[engine->hoveredUIElementIndex].type)
        {
        case NO_COLLISION_ACTION:
            break;
        case SAVE_CG:
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if(engine->isSoundOn){PlaySound(engine->save);}
                if (SaveGraphToFile(CGFilePath, graph) == 0)
                    AddToLog(engine, "Saved successfully!", 0);
                else
                    AddToLog(engine, "ERROR SAVING CHANGES!", 1);
            }
            break;
        case RUN_GAME:
            engine->cursor = MOUSE_CURSOR_NOT_ALLOWED;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                engine->isEditorOpened = false;
                AddToLog(engine, "Interpreter not ready", 2);
            }
            break;
        case BACK_FILEPATH:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                char *lastSlash = strrchr(engine->currentPath, '\\');
                if (lastSlash != NULL)
                {
                    *lastSlash = '\0';
                }
                engine->files = LoadDirectoryFilesEx(engine->currentPath, NULL, false);
            }
            break;
        case REFRESH_FILES:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                engine->files = LoadDirectoryFilesEx(engine->currentPath, NULL, false);
            }
            break;
        case CLOSE_WINDOW:
            engine->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                CloseWindow();
                return;
            }
            break;
        case MINIMIZE_WINDOW:
            engine->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                MinimizeWindow();
            }
            break;
        case RESIZE_BOTTOM_BAR:
            engine->isViewportFocused = false;
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && engine->draggingResizeButtonID == 0)
            {
                engine->draggingResizeButtonID = 1;
            }
            break;
        case RESIZE_SIDE_BAR:
            engine->isViewportFocused = false;
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && engine->draggingResizeButtonID == 0)
            {
                engine->draggingResizeButtonID = 2;
            }
            break;
        case RESIZE_SIDE_BAR_MIDDLE:
            engine->isViewportFocused = false;
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && engine->draggingResizeButtonID == 0)
            {
                engine->draggingResizeButtonID = 3;
            }
            break;
        case OPEN_FILE:
            char tooltipText[256] = "";
            snprintf(tooltipText, sizeof(tooltipText), "File: %s\nSize: %ld bytes", engine->uiElements[engine->hoveredUIElementIndex].text.string, GetFileLength(engine->uiElements[engine->hoveredUIElementIndex].name));
            Rectangle tooltipRect = {engine->uiElements[engine->hoveredUIElementIndex].rect.pos.x + 10, engine->uiElements[engine->hoveredUIElementIndex].rect.pos.y - 61, MeasureTextEx(engine->font, tooltipText, 20, 0).x + 20, 60};
            AddUIElement(engine, (UIElement){
                                     .name = "Tooltip",
                                     .shape = UIRectangle,
                                     .type = NO_COLLISION_ACTION,
                                     .rect = {.pos = {tooltipRect.x, tooltipRect.y}, .recSize = {tooltipRect.width, tooltipRect.height}, .roundness = 0, .roundSegments = 0},
                                     .color = DARKGRAY,
                                     .layer = 1,
                                     .text = {.string = "", .textPos = {tooltipRect.x + 10, tooltipRect.y + 10}, .textSize = 20, .textSpacing = 0, .textColor = WHITE}});
            strncpy(engine->uiElements[engine->uiElementCount - 1].text.string, tooltipText, 255);
            engine->uiElements[engine->uiElementCount - 1].text.string[255] = '\0';
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                double currentTime = GetTime();
                static double lastClickTime = 0;

                if (currentTime - lastClickTime <= doubleClickThreshold)
                {
                    if (GetFileType(GetFileName(engine->uiElements[engine->hoveredUIElementIndex].name)) == 2)
                    {
                        char *buff = malloc(strlen(engine->uiElements[engine->hoveredUIElementIndex].text.string) + 1);
                        strcpy(buff, engine->uiElements[engine->hoveredUIElementIndex].text.string);
                        buff[strlen(engine->uiElements[engine->hoveredUIElementIndex].text.string) - 3] = '\0';
                        strcpy(openedFileName, buff);
                        free(buff);
                        engine->isEditorOpened = true;
                    }
                    else if (GetFileType(GetFileName(engine->uiElements[engine->hoveredUIElementIndex].name)) != 0)
                    {
                        char command[512];
                        snprintf(command, sizeof(command), "start \"\" \"%s\"", engine->uiElements[engine->hoveredUIElementIndex].name);
                        system(command);
                    }
                    else
                    {
                        strcat(engine->currentPath, "\\");
                        strcat(engine->currentPath, engine->uiElements[engine->hoveredUIElementIndex].text.string);
                        engine->files = LoadDirectoryFilesEx(engine->currentPath, NULL, false);
                    }
                }
                lastClickTime = currentTime;
            }

            break;
        }
    }

    for (int i = 0; i < engine->uiElementCount; i++)
    {
        UIElement *el = &engine->uiElements[i];
        switch (el->shape)
        {
        case UIRectangle:
            DrawRectangleRounded((Rectangle){el->rect.pos.x, el->rect.pos.y, el->rect.recSize.x, el->rect.recSize.y}, el->rect.roundness, el->rect.roundSegments, el->color);
            break;
        case UICircle:
            DrawCircleV(el->circle.center, el->circle.radius, el->color);
            break;
        case UILine:
            DrawLineEx(el->line.startPos, el->line.engPos, el->line.thickness, el->color);
            break;
        }

        if (el->text.string[0] != '\0')
        {
            DrawTextEx(engine->font, el->text.string, el->text.textPos, el->text.textSize, el->text.textSpacing, el->text.textColor);
        }
    }

    if (engine->hoveredUIElementIndex != -1)
    {
        if (engine->uiElements[engine->hoveredUIElementIndex].shape == 0)
        {
            DrawRectangleRounded((Rectangle){engine->uiElements[engine->hoveredUIElementIndex].rect.pos.x, engine->uiElements[engine->hoveredUIElementIndex].rect.pos.y, engine->uiElements[engine->hoveredUIElementIndex].rect.recSize.x, engine->uiElements[engine->hoveredUIElementIndex].rect.recSize.y}, engine->uiElements[engine->hoveredUIElementIndex].rect.roundness, engine->uiElements[engine->hoveredUIElementIndex].rect.roundSegments, engine->uiElements[engine->hoveredUIElementIndex].rect.hoverColor);
        }
    }
}

void BuildUITexture(EngineContext *engine, GraphContext *graph, char *CGFilePath)
{
    engine->uiElementCount = 0;

    BeginTextureMode(engine->UI);
    ClearBackground((Color){255, 255, 255, 0});

    if (engine->screenWidth > engine->screenHeight && engine->screenWidth > 1000)
    {
        AddUIElement(engine, (UIElement){
                                 .name = "SideBarVars",
                                 .shape = UIRectangle,
                                 .type = NO_COLLISION_ACTION,
                                 .rect = {.pos = {0, 0}, .recSize = {engine->sideBarWidth, engine->sideBarMiddleY}, .roundness = 0.0f, .roundSegments = 0},
                                 .color = (Color){28, 28, 28, 255},
                                 .layer = 0,
                             });

        AddUIElement(engine, (UIElement){
                                 .name = "SideBarLog",
                                 .shape = UIRectangle,
                                 .type = NO_COLLISION_ACTION,
                                 .rect = {.pos = {0, engine->sideBarMiddleY}, .recSize = {engine->sideBarWidth, engine->screenHeight - engine->bottomBarHeight}, .roundness = 0.0f, .roundSegments = 0},
                                 .color = (Color){15, 15, 15, 255},
                                 .layer = 0,
                             });

        AddUIElement(engine, (UIElement){
                                 .name = "SideBarMiddleLine",
                                 .shape = UILine,
                                 .type = NO_COLLISION_ACTION,
                                 .line = {.startPos = {engine->sideBarWidth, 0}, .engPos = {engine->sideBarWidth, engine->screenHeight - engine->bottomBarHeight}, .thickness = 2},
                                 .color = WHITE,
                                 .layer = 0,
                             });

        AddUIElement(engine, (UIElement){
                                 .name = "SideBarFromViewportDividerLine",
                                 .shape = UILine,
                                 .type = NO_COLLISION_ACTION,
                                 .line = {.startPos = {0, engine->sideBarMiddleY}, .engPos = {engine->sideBarWidth, engine->sideBarMiddleY}, .thickness = 2},
                                 .color = WHITE,
                                 .layer = 0,
                             });

        AddUIElement(engine, (UIElement){
                                 .name = "SaveButton",
                                 .shape = UIRectangle,
                                 .type = SAVE_CG,
                                 .rect = {.pos = {engine->sideBarWidth - 140, engine->sideBarMiddleY + 15}, .recSize = {60, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                 .color = (Color){255, 255, 255, 50},
                                 .layer = 1,
                                 .text = {.string = "Save", .textPos = {engine->sideBarWidth - 135, engine->sideBarMiddleY + 20}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                             });

        AddUIElement(engine, (UIElement){
                                 .name = "RunButton",
                                 .shape = UIRectangle,
                                 .type = RUN_GAME,
                                 .rect = {.pos = {engine->sideBarWidth - 70, engine->sideBarMiddleY + 15}, .recSize = {60, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                 .color = (Color){255, 255, 255, 50},
                                 .layer = 1,
                                 .text = {.string = "Run", .textPos = {engine->sideBarWidth - 60, engine->sideBarMiddleY + 20}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                             });

        int y = engine->screenHeight - engine->bottomBarHeight - 30;
        char cutMessage[256];
        for (int i = engine->logs.count - 1; i >= 0 && y > engine->sideBarMiddleY + 50; i--)
        {
            int j;
            for (j = 0; j < strlen(engine->logs.entries[i].message); j++)
            {
                char temp[256];
                strncpy(temp, engine->logs.entries[i].message, j);
                temp[j] = '\0';
                if (MeasureTextEx(engine->font, temp, 20, 2).x < engine->sideBarWidth - 35)
                {
                    continue;
                }
                else
                {
                    break;
                }
            }
            strncpy(cutMessage, engine->logs.entries[i].message, j);
            cutMessage[j] = '\0';
            AddUIElement(engine, (UIElement){
                                     .name = "LogText",
                                     .shape = UIText,
                                     .type = NO_COLLISION_ACTION,
                                     .text = {.textPos = {20, y}, .textSize = 20, .textSpacing = 2, .textColor = (engine->logs.entries[i].level == 0) ? WHITE : (engine->logs.entries[i].level == 1) ? YELLOW
                                                                                                                                                                                                     : RED},
                                     .layer = 0});
            strncpy(engine->uiElements[engine->uiElementCount - 1].text.string, cutMessage, 127);
            engine->uiElements[engine->uiElementCount - 1].text.string[128] = '\0';
            y -= 25;
        }
    }

    AddUIElement(engine, (UIElement){
                             .name = "BottomBar",
                             .shape = UIRectangle,
                             .type = NO_COLLISION_ACTION,
                             .rect = {.pos = {0, engine->screenHeight - engine->bottomBarHeight}, .recSize = {engine->screenWidth, engine->bottomBarHeight}, .roundness = 0.0f, .roundSegments = 0},
                             .color = (Color){28, 28, 28, 255},
                             .layer = 0,
                         });

    AddUIElement(engine, (UIElement){
                             .name = "BottomBarFromViewportDividerLine",
                             .shape = UILine,
                             .type = NO_COLLISION_ACTION,
                             .line = {.startPos = {0, engine->screenHeight - engine->bottomBarHeight}, .engPos = {engine->screenWidth, engine->screenHeight - engine->bottomBarHeight}, .thickness = 2},
                             .color = WHITE,
                             .layer = 0,
                         });

    AddUIElement(engine, (UIElement){
                             .name = "BackButton",
                             .shape = UIRectangle,
                             .type = BACK_FILEPATH,
                             .rect = {.pos = {30, engine->screenHeight - engine->bottomBarHeight + 10}, .recSize = {65, 30}, .roundness = 0, .roundSegments = 0, .hoverColor = Fade(WHITE, 0.6f)},
                             .color = (Color){70, 70, 70, 150},
                             .layer = 1,
                             .text = {.string = "Back", .textPos = {35, engine->screenHeight - engine->bottomBarHeight + 12}, .textSize = 25, .textSpacing = 0, .textColor = WHITE}});

    AddUIElement(engine, (UIElement){
                             .name = "RefreshButton",
                             .shape = UIRectangle,
                             .type = REFRESH_FILES,
                             .rect = {.pos = {110, engine->screenHeight - engine->bottomBarHeight + 10}, .recSize = {100, 30}, .roundness = 0, .roundSegments = 0, .hoverColor = Fade(WHITE, 0.6f)},
                             .color = (Color){70, 70, 70, 150},
                             .layer = 1,
                             .text = {.string = "Refresh", .textPos = {119, engine->screenHeight - engine->bottomBarHeight + 12}, .textSize = 25, .textSpacing = 0, .textColor = WHITE}});

    AddUIElement(engine, (UIElement){
                             .name = "CurrentPath",
                             .shape = UIText,
                             .type = NO_COLLISION_ACTION,
                             .color = (Color){0, 0, 0, 0},
                             .layer = 0,
                             .text = {.string = "", .textPos = {230, engine->screenHeight - engine->bottomBarHeight + 15}, .textSize = 22, .textSpacing = 2, .textColor = WHITE}});
    strncpy(engine->uiElements[engine->uiElementCount - 1].text.string, engine->currentPath, 256);
    engine->uiElements[engine->uiElementCount - 1].text.string[256] = '\0';

    int xOffset = 50;
    int yOffset = engine->screenHeight - engine->bottomBarHeight + 70;

    for (int i = 0; i < engine->files.count; i++)
    {
        const char *fileName = GetFileName(engine->files.paths[i]);

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
                                 .name = "File",
                                 .shape = UIRectangle,
                                 .type = OPEN_FILE,
                                 .rect = {.pos = {xOffset, yOffset}, .recSize = {150, 60}, .roundness = 0.5f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                 .color = fileColor,
                                 .layer = 1,
                                 .text = {.string = "", .textPos = {xOffset + 10, yOffset + 16}, .textSize = 25, .textSpacing = 0, .textColor = BLACK}});
        strncpy(engine->uiElements[engine->uiElementCount - 1].name, engine->files.paths[i], 256);
        strncpy(engine->uiElements[engine->uiElementCount - 1].text.string, buff, 31);
        engine->uiElements[engine->uiElementCount - 1].text.string[256] = '\0';

        xOffset += 250;
        if (xOffset + 100 >= engine->screenWidth)
        {
            xOffset = 50;
            yOffset += 120;
        }
    }

    AddUIElement(engine, (UIElement){
                             .name = "TopBarClose",
                             .shape = UIRectangle,
                             .type = CLOSE_WINDOW,
                             .rect = {.pos = {GetScreenWidth() - 50, 0}, .recSize = {50, 50}, .roundness = 0.0f, .roundSegments = 0, .hoverColor = RED},
                             .color = (Color){0, 0, 0, 0},
                             .layer = 1,
                         });
    AddUIElement(engine, (UIElement){
                             .name = "TopBarMinimize",
                             .shape = UIRectangle,
                             .type = MINIMIZE_WINDOW,
                             .rect = {.pos = {GetScreenWidth() - 100, 0}, .recSize = {50, 50}, .roundness = 0.0f, .roundSegments = 0, .hoverColor = GRAY},
                             .color = (Color){0, 0, 0, 0},
                             .layer = 1,
                         });

    AddUIElement(engine, (UIElement){
                             .name = "BottomBarResizeButton",
                             .shape = UICircle,
                             .type = RESIZE_BOTTOM_BAR,
                             .circle = {.center = (Vector2){engine->screenWidth / 2, engine->screenHeight - engine->bottomBarHeight}, .radius = 10},
                             .color = (Color){255, 255, 255, 1},
                             .layer = 1,
                         });
    AddUIElement(engine, (UIElement){
                             .name = "SideBarResizeButton",
                             .shape = UICircle,
                             .type = RESIZE_SIDE_BAR,
                             .circle = {.center = (Vector2){engine->sideBarWidth, (engine->screenHeight - engine->bottomBarHeight) / 2}, .radius = 10},
                             .color = (Color){255, 255, 255, 1},
                             .layer = 1,
                         });
    AddUIElement(engine, (UIElement){
                             .name = "SideBarMiddleResizeButton",
                             .shape = UICircle,
                             .type = RESIZE_SIDE_BAR_MIDDLE,
                             .circle = {.center = (Vector2){engine->sideBarWidth / 2, engine->sideBarMiddleY}, .radius = 10},
                             .color = (Color){255, 255, 255, 1},
                             .layer = 1,
                         });

    DrawUIElements(engine, CGFilePath, graph);

    // special symbols and textures
    DrawRectangleLinesEx((Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()}, 4.0f, WHITE);

    DrawLineEx((Vector2){GetScreenWidth() - 35, 15}, (Vector2){GetScreenWidth() - 15, 35}, 2, WHITE);
    DrawLineEx((Vector2){GetScreenWidth() - 35, 35}, (Vector2){GetScreenWidth() - 15, 15}, 2, WHITE);

    DrawLineEx((Vector2){GetScreenWidth() - 85, 25}, (Vector2){GetScreenWidth() - 65, 25}, 2, WHITE);

    DrawTexture(engine->resizeButton, engine->screenWidth / 2 - 10, engine->screenHeight - engine->bottomBarHeight - 10, WHITE);
    DrawTexturePro(engine->resizeButton, (Rectangle){0, 0, 20, 20}, (Rectangle){engine->sideBarWidth, (engine->screenHeight - engine->bottomBarHeight) / 2, 20, 20}, (Vector2){10, 10}, 90.0f, WHITE);
    if (engine->sideBarWidth > 150)
    {
        DrawTexture(engine->resizeButton, engine->sideBarWidth / 2 - 10, engine->sideBarMiddleY - 10, WHITE);
    }

    EndTextureMode();
}

bool HandleUICollisions(EngineContext *engine, int fileCount, char *projectPath, char *CGFilePath, GraphContext *graph)
{
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S))
    {
        if(engine->isSoundOn){PlaySound(engine->save);}
        if (SaveGraphToFile(CGFilePath, graph) == 0)
        {
            AddToLog(engine, "Saved successfully!", 0);
        }
        else
        {
            AddToLog(engine, "ERROR SAVING CHANGES!", 1);
        }
    }

    if (engine->draggingResizeButtonID != 0)
    {
        if (IsMouseButtonUp(MOUSE_LEFT_BUTTON))
        {
            engine->draggingResizeButtonID = 0;
        }

        engine->hasResizedBar = true;

        switch (engine->draggingResizeButtonID)
        {
        case 0:
            break;
        case 1:
            engine->bottomBarHeight -= GetMouseDelta().y;
            if (engine->bottomBarHeight < 50 && GetMouseDelta().y > 0)
            {
                engine->bottomBarHeight = 5;
            }
            engine->cursor = MOUSE_CURSOR_RESIZE_NS;
            break;
        case 2:
            engine->sideBarWidth += GetMouseDelta().x;
            if (engine->sideBarWidth < 50 && GetMouseDelta().x < 0)
            {
                engine->sideBarWidth = 5;
            }
            engine->cursor = MOUSE_CURSOR_RESIZE_EW;
            break;
        case 3:
            engine->sideBarMiddleY += GetMouseDelta().y;
            engine->cursor = MOUSE_CURSOR_RESIZE_NS;
            break;
        default:
            break;
        }
    }

    for (int i = 0; i < engine->uiElementCount; i++)
    {
        if (engine->uiElements[i].layer != 0)
        {
            if (engine->uiElements[i].shape == UIRectangle && CheckCollisionPointRec(engine->mousePos, (Rectangle){engine->uiElements[i].rect.pos.x, engine->uiElements[i].rect.pos.y, engine->uiElements[i].rect.recSize.x, engine->uiElements[i].rect.recSize.y}))
            {
                engine->hoveredUIElementIndex = i;
                return true;
            }
            else if (engine->uiElements[i].shape == UICircle && CheckCollisionPointCircle(engine->mousePos, engine->uiElements[i].circle.center, engine->uiElements[i].circle.radius))
            {
                engine->hoveredUIElementIndex = i;
                return true;
            }
        }
    }

    engine->hoveredUIElementIndex = -1;

    return false;
}

void contextChangePerFrame(EngineContext *engine)
{
    engine->mousePos = GetMousePosition();
    engine->isViewportFocused = CheckCollisionPointRec(engine->mousePos, (Rectangle){engine->sideBarWidth, 0, engine->screenWidth - engine->sideBarWidth, engine->screenHeight - engine->bottomBarHeight});

    if (engine->prevScreenWidth != engine->screenWidth || engine->prevScreenHeight != engine->screenHeight || engine->hasResizedBar)
    {
        engine->screenWidth = GetScreenWidth();
        engine->screenHeight = GetScreenHeight();
        engine->prevScreenWidth = engine->screenWidth;
        engine->prevScreenHeight = engine->screenHeight;
        engine->viewportWidth = engine->screenWidth - engine->sideBarWidth;
        engine->viewportHeight = engine->screenHeight - engine->bottomBarHeight;
        engine->hasResizedBar = false;
        engine->delayFrames = true;
    }
}

int main()
{

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED);
    InitWindow(1600, 1000, "RapidEngine");
    SetTargetFPS(140);

    char *projectPath = PrepareProjectPath(/*handleProjectManager()*/ "Tetris"); // temporary hardcode

    SetTargetFPS(60);

    MaximizeWindow();

    InitAudioDevice();

    EngineContext engine = InitEngineContext(projectPath);
    EditorContext editor = InitEditorContext();
    GraphContext graph = InitGraphContext();

    AddToLog(&engine, "All resources loaded", 0);

    AddToLog(&engine, "Welcome!", 0);

    while (!WindowShouldClose())
    {
        contextChangePerFrame(&engine);

        if (HandleUICollisions(&engine, engine.files.count, projectPath, editor.CGFilePath, &graph))
        {
            if (engine.cursor == MOUSE_CURSOR_ARROW)
            {
                engine.cursor = MOUSE_CURSOR_POINTING_HAND;
            }
            BuildUITexture(&engine, &graph, editor.CGFilePath);
            SetTargetFPS(140);
            engine.delayFrames = true;
        }
        else if (engine.delayFrames)
        {
            BuildUITexture(&engine, &graph, editor.CGFilePath);
            engine.cursor = MOUSE_CURSOR_ARROW;
            SetTargetFPS(60);
            engine.delayFrames = false;
        }

        if (engine.isViewportFocused)
        {
            // engine.cursor = editor.cursor; //should be viewport.cursor
        }
        SetMouseCursor(engine.cursor);

        BeginDrawing();
        ClearBackground(BLACK);

        int textureX = (engine.viewport.texture.width - engine.viewportWidth) / 2.0f + engine.mousePos.x - engine.sideBarWidth;
        int textureY = (engine.viewport.texture.height - engine.viewportHeight) / 2.0f + engine.mousePos.y;

        if (!engine.isEditorOpened)
        {
            BeginTextureMode(engine.viewport);
            ClearBackground(BLACK);
            DrawTextEx(engine.font, "Game Screen", (Vector2){(engine.screenWidth - engine.sideBarWidth) / 2 - 100, (engine.screenHeight - engine.bottomBarHeight) / 2}, 50, 0, WHITE);
            EndTextureMode();
        }
        else
        {
            if (strcmp(openedFileName, editor.fileName) != 0)
            {
                OpenNewCGFile(&editor, &graph, openedFileName);
            }

            if (engine.isViewportFocused || editor.delayFrames || editor.draggingNodeIndex != 0)
            {
                handleEditor(&editor, &graph, &engine.viewport, (Vector2){textureX, textureY}, engine.viewportWidth, engine.viewportHeight, engine.draggingResizeButtonID != 0);
            }

            if (editor.newLogMessage)
            {
                AddToLog(&engine, editor.logMessage, editor.logMessageLevel);
            }
        }

        DrawTexturePro(
            engine.viewport.texture,
            (Rectangle){(engine.viewport.texture.width - engine.viewportWidth) / 2.0f, (engine.viewport.texture.height - engine.viewportHeight) / 2.0f, engine.viewportWidth, -engine.viewportHeight},
            (Rectangle){engine.sideBarWidth, 0, engine.screenWidth - engine.sideBarWidth, engine.screenHeight - engine.bottomBarHeight},
            (Vector2){0, 0},
            0.0f,
            WHITE);

        DrawTextureRec(engine.UI.texture, (Rectangle){0, 0, engine.UI.texture.width, -engine.UI.texture.height}, (Vector2){0, 0}, WHITE);

        if (engine.isEditorOpened)
        {
            DrawTextEx(GetFontDefault(), "CoreGraph", (Vector2){engine.sideBarWidth + 20, 30}, 40, 4, Fade(WHITE, 0.2f));
            DrawTextEx(GetFontDefault(), "TM", (Vector2){engine.sideBarWidth + 230, 20}, 15, 1, Fade(WHITE, 0.2f));
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

    UnloadSound(engine.save);
    CloseAudioDevice();

    return 0;
}
