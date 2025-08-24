#include <stdio.h>
#include <raylib.h>
#include <direct.h>
#include <stdlib.h>
#include <time.h>
#include "shell_execute.h"
#include "CGEditor.h"
#include "ProjectManager.h"
#include "Engine.h"
#include "Interpreter.h"
#include "HitboxEditor.h"

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

EngineContext InitEngineContext()
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

    engine.viewport = LoadRenderTexture(engine.screenWidth * 2, engine.screenHeight * 2);
    engine.UI = LoadRenderTexture(engine.screenWidth, engine.screenHeight);
    engine.resizeButton = LoadTexture("textures//resize_btn.png");
    engine.viewportFullscreenButton = LoadTexture("textures//viewport_fullscreen.png");
    if (engine.UI.id == 0 || engine.viewport.id == 0 || engine.resizeButton.id == 0 || engine.viewportFullscreenButton.id == 0)
    {
        AddToLog(&engine, "Couldn't load textures", 2);
        EmergencyExit(&engine);
    }

    engine.delayFrames = true;
    engine.draggingResizeButtonID = 0;

    engine.font = LoadFontEx("fonts//arialbd.ttf", 128, NULL, 0);
    if (engine.font.texture.id == 0)
    {
        AddToLog(&engine, "Failed to load font: fonts/arialbd.ttf", 1);
        EmergencyExit(&engine);
    }

    engine.CGFilePath = malloc(MAX_PATH_LENGTH);
    engine.CGFilePath[0] = '\0';

    engine.hoveredUIElementIndex = -1;

    engine.viewportMode = VIEWPORT_CG_EDITOR;
    engine.isGameRunning = false;

    engine.save = LoadSound("sound//save.wav");

    engine.isSoundOn = true;

    engine.sideBarHalfSnap = false;

    engine.editorZoom = 1.0f;

    engine.wasBuilt = false;

    engine.showSaveWarning = 0;

    engine.varsFilter = 0;

    engine.isGameFullscreen = false;

    engine.isSaveButtonHovered = false;

    return engine;
}

void FreeEngineContext(EngineContext *engine)
{
    if (engine->currentPath)
    {
        free(engine->currentPath);
        engine->currentPath = NULL;
    }

    if (engine->currentPath)
    {
        free(engine->projectPath);
        engine->projectPath = NULL;
    }

    if (engine->CGFilePath)
        free(engine->CGFilePath);

    if (engine->logs.entries)
    {
        free(engine->logs.entries);
        engine->logs.entries = NULL;
    }

    UnloadDirectoryFiles(engine->files);

    UnloadRenderTexture(engine->viewport);
    UnloadRenderTexture(engine->UI);
    UnloadTexture(engine->resizeButton);
    UnloadTexture(engine->viewportFullscreenButton);

    UnloadFont(engine->font);

    UnloadSound(engine->save);
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
            char level[512];
            switch (engine->logs.entries[i].level)
            {
            case LOG_LEVEL_NORMAL:
                strcpy(level, "INFO");
                break;
            case LOG_LEVEL_WARNING:
                strcpy(level, "WARNING");
                break;
            case LOG_LEVEL_ERROR:
                strcpy(level, "ERROR");
                break;
            case LOG_LEVEL_SAVE:
                strcpy(level, "SAVE");
                break;
            case LOG_LEVEL_DEBUG:
                strcpy(level, "DEBUG");
                break;
            default:
                strcpy(level, "UNKNOWN");
            }
            fprintf(logFile, "[%s] %s\n", level, engine->logs.entries[i].message);
        }
        fclose(logFile);
    }

    exit(1);
}

char *SetProjectFolderPath(char *fileName)
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

FileType GetFileType(const char *fileName)
{
    const char *ext = GetFileExtension(fileName);
    if (!ext || *(ext + 1) == '\0')
        return FILE_FOLDER;

    if (strcmp(ext + 1, "cg") == 0)
        return FILE_CG;
    else if (strcmp(ext + 1, "png") == 0 || strcmp(ext + 1, "jpg") == 0 || strcmp(ext + 1, "jpeg") == 0)
        return FILE_IMAGE;

    return FILE_OTHER;
}

void PrepareCGFilePath(EngineContext *engine, const char *projectName)
{
    char cwd[MAX_PATH_LENGTH];

    if (!getcwd(cwd, sizeof(cwd)))
    {
        perror("Failed to get current working directory");
        exit(EXIT_FAILURE);
    }

    size_t len = strlen(cwd);

    if (len <= 7)
    {
        fprintf(stderr, "Current directory path too short to truncate 7 chars\n");
        exit(EXIT_FAILURE);
    }

    cwd[len - 7] = '\0';

    snprintf(engine->CGFilePath, MAX_PATH_LENGTH, "%s\\Projects\\%s\\%s.cg", cwd, projectName, projectName);

    for (int i = 0; i < engine->files.count; i++)
    {
        if (strcmp(engine->CGFilePath, engine->files.paths[i]) == 0)
        {
            return;
        }
    }

    FILE *f = fopen(engine->CGFilePath, "w");
    if (f)
    {
        fclose(f);
    }
}

int DrawSaveWarning(EngineContext *engine, GraphContext *graph, EditorContext *editor)
{
    engine->isViewportFocused = false;
    int popupWidth = 500;
    int popupHeight = 150;
    int screenWidth = engine->screenWidth;
    int screenHeight = engine->screenHeight;
    int popupX = (screenWidth - popupWidth) / 2;
    int popupY = (screenHeight - popupHeight) / 2 - 100;

    const char *message = "Save changes before exiting?";
    int textWidth = MeasureTextEx(engine->font, message, 30, 0).x;

    int btnWidth = 120;
    int btnHeight = 30;
    int btnSpacing = 10;
    int btnY = popupY + popupHeight - btnHeight - 20;

    int totalBtnWidth = btnWidth * 3 + btnSpacing * 2 + 30;
    int btnStartX = popupX + (popupWidth + 5 - totalBtnWidth) / 2;

    Rectangle saveBtn = {btnStartX, btnY, btnWidth, btnHeight};
    Rectangle closeBtn = {btnStartX + btnWidth + btnSpacing, btnY, btnWidth + 20, btnHeight};
    Rectangle cancelBtn = {btnStartX + 2 * (btnWidth + btnSpacing) + 20, btnY, btnWidth, btnHeight};

    DrawRectangle(0, 0, screenWidth, screenHeight, (Color){0, 0, 0, 150});

    DrawRectangleRounded((Rectangle){popupX, popupY, popupWidth, popupHeight}, 0.4f, 8, (Color){30, 30, 30, 255});
    DrawRectangleRoundedLines((Rectangle){popupX, popupY, popupWidth, popupHeight}, 0.4f, 8, (Color){200, 200, 200, 255});

    DrawTextEx(engine->font, message, (Vector2){popupX + (popupWidth - textWidth) / 2, popupY + 20}, 30, 0, WHITE);

    DrawRectangleRounded(saveBtn, 0.2f, 2, DARKGREEN);
    DrawTextEx(engine->font, "Save", (Vector2){saveBtn.x + 35, saveBtn.y + 4}, 24, 0, WHITE);

    DrawRectangleRounded(closeBtn, 0.2f, 2, (Color){210, 21, 35, 255});
    DrawTextEx(engine->font, "Don't save", (Vector2){closeBtn.x + 18, closeBtn.y + 4}, 24, 0, WHITE);

    DrawRectangleRounded(cancelBtn, 0.2f, 2, (Color){80, 80, 80, 255});
    DrawTextEx(engine->font, "Cancel", (Vector2){cancelBtn.x + 25, cancelBtn.y + 4}, 24, 0, WHITE);

    if (CheckCollisionPointRec(engine->mousePos, saveBtn))
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (SaveGraphToFile(engine->CGFilePath, graph) == 0)
            {
                AddToLog(engine, "Saved successfully!", 3);
                editor->hasChanged = false;
            }
            else
            {
                AddToLog(engine, "ERROR SAVING CHANGES!", 1);
            }
            return 2;
        }
    }
    else if (CheckCollisionPointRec(engine->mousePos, closeBtn))
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            return 2;
        }
    }
    else if (CheckCollisionPointRec(engine->mousePos, cancelBtn))
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            return 0;
        }
    }
    else
    {
        SetMouseCursor(MOUSE_CURSOR_ARROW);
    }

    return 1;
}

void CountingSortByLayer(EngineContext *engine)
{
    int **elements = malloc(MAX_LAYER_COUNT * sizeof(int *));
    int *layerCount = calloc(MAX_LAYER_COUNT, sizeof(int));
    for (int i = 0; i < MAX_LAYER_COUNT; i++)
    {
        elements[i] = malloc(engine->uiElementCount * sizeof(int));
    }

    for (int i = 0; i < engine->uiElementCount; i++)
    {
        if (engine->uiElements[i].layer < MAX_LAYER_COUNT)
        {
            elements[engine->uiElements[i].layer][layerCount[engine->uiElements[i].layer]] = i;
            layerCount[engine->uiElements[i].layer]++;
        }
    }

    UIElement *sorted = malloc(sizeof(UIElement) * engine->uiElementCount);
    int sortedCount = 0;

    for (int i = 0; i < MAX_LAYER_COUNT; i++)
    {
        for (int j = 0; j < layerCount[i]; j++)
        {
            sorted[sortedCount++] = engine->uiElements[elements[i][j]];
        }
    }

    memcpy(engine->uiElements, sorted, sizeof(UIElement) * sortedCount);
    free(sorted);
    free(layerCount);

    for (int i = 0; i < MAX_LAYER_COUNT; i++)
    {
        free(elements[i]);
    }
    free(elements);
}

void DrawUIElements(EngineContext *engine, GraphContext *graph, EditorContext *editor, InterpreterContext *interpreter, RuntimeGraphContext *runtimeGraph)
{
    engine->isSaveButtonHovered = false;
    char temp[256];
    if (engine->hoveredUIElementIndex != -1)
    {
        switch (engine->uiElements[engine->hoveredUIElementIndex].type)
        {
        case UI_ACTION_NO_COLLISION_ACTION:
            break;
        case UI_ACTION_SAVE_CG:
            engine->isSaveButtonHovered = true;
            if (engine->isGameRunning)
            {
                break;
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if (engine->isSoundOn)
                {
                    PlaySound(engine->save);
                }
                if (SaveGraphToFile(engine->CGFilePath, graph) == 0)
                {
                    editor->hasChanged = false;
                    AddToLog(engine, "Saved successfully!", 3);
                }
                else
                    AddToLog(engine, "ERROR SAVING CHANGES!", 1);
            }
            break;
        case UI_ACTION_STOP_GAME:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                engine->viewportMode = VIEWPORT_CG_EDITOR;
                editor->isFirstFrame = true;
                engine->isGameRunning = false;
                engine->wasBuilt = false;
                FreeInterpreterContext(interpreter);
            }
            break;
        case UI_ACTION_RUN_GAME:
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if (editor->hasChanged)
                {
                    AddToLog(engine, "Project not saved!", 1);
                    break;
                }
                else if (!engine->wasBuilt)
                {
                    AddToLog(engine, "Project has not been built", 1);
                    break;
                }
                engine->viewportMode = VIEWPORT_GAME_SCREEN;
                engine->isGameRunning = true;
                interpreter->isFirstFrame = true;
            }
            break;
        case UI_ACTION_BUILD_GRAPH:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (editor->hasChanged)
                {
                    AddToLog(engine, "Project not saved!", 1);
                    break;
                }
                *runtimeGraph = ConvertToRuntimeGraph(graph, interpreter);
                engine->delayFrames = true;
                if (runtimeGraph != NULL)
                {
                    AddToLog(engine, "Build Successfull", 0);
                    engine->wasBuilt = true;
                }
                else
                {
                    AddToLog(engine, "Build failed", 0);
                }
            }
            break;
        case UI_ACTION_BACK_FILEPATH:
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
        case UI_ACTION_REFRESH_FILES:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                engine->files = LoadDirectoryFilesEx(engine->currentPath, NULL, false);
            }
            break;
        case UI_ACTION_CLOSE_WINDOW:
            engine->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (editor->hasChanged)
                {
                    engine->showSaveWarning = true;
                }
                else
                {
                    CloseWindow();
                    return;
                }
            }
            break;
        case UI_ACTION_MINIMIZE_WINDOW:
            engine->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                MinimizeWindow();
            }
            break;
        case UI_ACTION_RESIZE_BOTTOM_BAR:
            engine->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && engine->draggingResizeButtonID == 0)
            {
                engine->draggingResizeButtonID = 1;
            }
            break;
        case UI_ACTION_RESIZE_SIDE_BAR:
            engine->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && engine->draggingResizeButtonID == 0)
            {
                engine->draggingResizeButtonID = 2;
            }
            break;
        case UI_ACTION_RESIZE_SIDE_BAR_MIDDLE:
            engine->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && engine->draggingResizeButtonID == 0)
            {
                engine->draggingResizeButtonID = 3;
            }
            break;
        case UI_ACTION_OPEN_FILE:
            char tooltipText[256] = "";
            snprintf(tooltipText, sizeof(tooltipText), "File: %s\nSize: %ld bytes", GetFileName(engine->uiElements[engine->hoveredUIElementIndex].name), GetFileLength(engine->uiElements[engine->hoveredUIElementIndex].name));
            Rectangle tooltipRect = {engine->uiElements[engine->hoveredUIElementIndex].rect.pos.x + 10, engine->uiElements[engine->hoveredUIElementIndex].rect.pos.y - 61, MeasureTextEx(engine->font, tooltipText, 20, 0).x + 20, 60};
            AddUIElement(engine, (UIElement){
                                     .name = "FileTooltip",
                                     .shape = UIRectangle,
                                     .type = UI_ACTION_NO_COLLISION_ACTION,
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
                    if (GetFileType(GetFileName(engine->uiElements[engine->hoveredUIElementIndex].name)) == FILE_CG)
                    {
                        char *openedFileName = malloc(strlen(engine->uiElements[engine->hoveredUIElementIndex].text.string) + 1);
                        strcpy(openedFileName, engine->uiElements[engine->hoveredUIElementIndex].text.string);
                        openedFileName[strlen(engine->uiElements[engine->hoveredUIElementIndex].text.string) - 3] = '\0';

                        *editor = InitEditorContext();
                        *graph = InitGraphContext();

                        PrepareCGFilePath(engine, openedFileName);

                        LoadGraphFromFile(engine->CGFilePath, graph);

                        engine->viewportMode = VIEWPORT_CG_EDITOR;
                    }
                    else if (GetFileType(GetFileName(engine->uiElements[engine->hoveredUIElementIndex].name)) != FILE_FOLDER)
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

        case UI_ACTION_VAR_TOOLTIP_RUNTIME:
            sprintf(temp, "%s %s = %s", ValueTypeToString(interpreter->values[engine->uiElements[engine->hoveredUIElementIndex].valueIndex].type), interpreter->values[engine->uiElements[engine->hoveredUIElementIndex].valueIndex].name, ValueToString(interpreter->values[engine->uiElements[engine->hoveredUIElementIndex].valueIndex]));
            AddUIElement(engine, (UIElement){
                                     .name = "VarTooltip",
                                     .shape = UIRectangle,
                                     .type = UI_ACTION_NO_COLLISION_ACTION,
                                     .rect = {.pos = {engine->sideBarWidth, engine->uiElements[engine->hoveredUIElementIndex].rect.pos.y}, .recSize = {MeasureTextEx(engine->font, temp, 20, 0).x + 20, 40}, .roundness = 0.4f, .roundSegments = 8},
                                     .color = DARKGRAY,
                                     .layer = 1,
                                     .text = {.textPos = {engine->sideBarWidth + 10, engine->uiElements[engine->hoveredUIElementIndex].rect.pos.y + 10}, .textSize = 20, .textSpacing = 0, .textColor = WHITE}});
            sprintf(engine->uiElements[engine->uiElementCount - 1].text.string, "%s", temp);
            break;

        case UI_ACTION_CHANGE_VARS_FILTER:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                engine->varsFilter++;
                if (engine->varsFilter > 5)
                {
                    engine->varsFilter = 0;
                }
            }
            break;
        case UI_ACTION_FULLSCREEN_BUTTON_VIEWPORT:
            engine->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                engine->isGameFullscreen = true;
            }
        }

        if (engine->uiElements[engine->hoveredUIElementIndex].shape == 0)
        {
            Color col = engine->uiElements[engine->hoveredUIElementIndex].color;
            engine->uiElements[engine->hoveredUIElementIndex].color = (Color){
                Clamp(col.r + 50, 0, 255),
                Clamp(col.g + 50, 0, 255),
                Clamp(col.b + 50, 0, 255),
                col.a};
            if (engine->uiElements[engine->hoveredUIElementIndex].type == UI_ACTION_CLOSE_WINDOW || engine->uiElements[engine->hoveredUIElementIndex].type == UI_ACTION_MINIMIZE_WINDOW)
            {
                AddUIElement(engine, (UIElement){
                                         .name = "HoverBlink",
                                         .shape = UIRectangle,
                                         .type = UI_ACTION_NO_COLLISION_ACTION,
                                         .rect = {.pos = {engine->uiElements[engine->hoveredUIElementIndex].rect.pos.x, engine->uiElements[engine->hoveredUIElementIndex].rect.pos.y}, .recSize = {engine->uiElements[engine->hoveredUIElementIndex].rect.recSize.x, engine->uiElements[engine->hoveredUIElementIndex].rect.recSize.y}, .roundness = engine->uiElements[engine->hoveredUIElementIndex].rect.roundness, .roundSegments = engine->uiElements[engine->hoveredUIElementIndex].rect.roundSegments},
                                         .color = engine->uiElements[engine->hoveredUIElementIndex].rect.hoverColor,
                                         .layer = 99});
            }
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
}

void BuildUITexture(EngineContext *engine, GraphContext *graph, EditorContext *editor, InterpreterContext *interpreter, RuntimeGraphContext *runtimeGraph)
{
    engine->uiElementCount = 0;

    BeginTextureMode(engine->UI);
    ClearBackground((Color){255, 255, 255, 0});

    if (engine->screenWidth > engine->screenHeight && engine->screenWidth > 1000)
    {
        AddUIElement(engine, (UIElement){
                                 .name = "SideBarVars",
                                 .shape = UIRectangle,
                                 .type = UI_ACTION_NO_COLLISION_ACTION,
                                 .rect = {.pos = {0, 0}, .recSize = {engine->sideBarWidth, engine->sideBarMiddleY}, .roundness = 0.0f, .roundSegments = 0},
                                 .color = (Color){28, 28, 28, 255},
                                 .layer = 0,
                             });

        AddUIElement(engine, (UIElement){
                                 .name = "SideBarLog",
                                 .shape = UIRectangle,
                                 .type = UI_ACTION_NO_COLLISION_ACTION,
                                 .rect = {.pos = {0, engine->sideBarMiddleY}, .recSize = {engine->sideBarWidth, engine->screenHeight - engine->bottomBarHeight}, .roundness = 0.0f, .roundSegments = 0},
                                 .color = (Color){15, 15, 15, 255},
                                 .layer = 0,
                             });

        AddUIElement(engine, (UIElement){
                                 .name = "SideBarMiddleLine",
                                 .shape = UILine,
                                 .type = UI_ACTION_NO_COLLISION_ACTION,
                                 .line = {.startPos = {engine->sideBarWidth, 0}, .engPos = {engine->sideBarWidth, engine->screenHeight - engine->bottomBarHeight}, .thickness = 2},
                                 .color = WHITE,
                                 .layer = 0,
                             });

        AddUIElement(engine, (UIElement){
                                 .name = "SideBarFromViewportDividerLine",
                                 .shape = UILine,
                                 .type = UI_ACTION_NO_COLLISION_ACTION,
                                 .line = {.startPos = {0, engine->sideBarMiddleY}, .engPos = {engine->sideBarWidth, engine->sideBarMiddleY}, .thickness = 2},
                                 .color = WHITE,
                                 .layer = 0,
                             });

        Vector2 saveButtonPos = {
            engine->sideBarHalfSnap ? engine->sideBarWidth - 70 : engine->sideBarWidth - 145,
            engine->sideBarHalfSnap ? engine->sideBarMiddleY + 60 : engine->sideBarMiddleY + 15};

        AddUIElement(engine, (UIElement){
                                 .name = "SaveButton",
                                 .shape = UIRectangle,
                                 .type = UI_ACTION_SAVE_CG,
                                 .rect = {.pos = saveButtonPos, .recSize = {64, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                 .color = (Color){70, 70, 70, 200},
                                 .layer = 1,
                                 .text = {.textPos = {editor->hasChanged ? saveButtonPos.x + 5 : saveButtonPos.x + 8, saveButtonPos.y + 5}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                             });
        if (editor->hasChanged)
        {
            strcpy(engine->uiElements[engine->uiElementCount - 1].text.string, "Save*");
        }
        else
        {
            strcpy(engine->uiElements[engine->uiElementCount - 1].text.string, "Save");
        }

        if (engine->viewportMode == VIEWPORT_GAME_SCREEN)
        {
            AddUIElement(engine, (UIElement){
                                     .name = "StopButton",
                                     .shape = UIRectangle,
                                     .type = UI_ACTION_STOP_GAME,
                                     .rect = {.pos = {engine->sideBarWidth - 70, engine->sideBarMiddleY + 15}, .recSize = {64, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                     .color = RED,
                                     .layer = 1,
                                     .text = {.string = "Stop", .textPos = {engine->sideBarWidth - 62, engine->sideBarMiddleY + 20}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                                 });
        }
        else if (engine->wasBuilt)
        {
            AddUIElement(engine, (UIElement){
                                     .name = "RunButton",
                                     .shape = UIRectangle,
                                     .type = UI_ACTION_RUN_GAME,
                                     .rect = {.pos = {engine->sideBarWidth - 70, engine->sideBarMiddleY + 15}, .recSize = {64, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                     .color = DARKGREEN,
                                     .layer = 1,
                                     .text = {.string = "Run", .textPos = {engine->sideBarWidth - 56, engine->sideBarMiddleY + 20}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                                 });
        }
        else
        {
            AddUIElement(engine, (UIElement){
                                     .name = "BuildButton",
                                     .shape = UIRectangle,
                                     .type = UI_ACTION_BUILD_GRAPH,
                                     .rect = {.pos = {engine->sideBarWidth - 70, engine->sideBarMiddleY + 15}, .recSize = {64, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                     .color = (Color){70, 70, 70, 200},
                                     .layer = 1,
                                     .text = {.string = "Build", .textPos = {engine->sideBarWidth - 64, engine->sideBarMiddleY + 20}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                                 });
        }

        int logY = engine->screenHeight - engine->bottomBarHeight - 30;
        char cutMessage[256];
        for (int i = engine->logs.count - 1; i >= 0 && logY > engine->sideBarMiddleY + 60 + engine->sideBarHalfSnap * 40; i--)
        {
            const char *msgNoTimestamp = engine->logs.entries[i].message + 9;

            char finalMsg[256];
            strncpy(finalMsg, engine->logs.entries[i].message, 255);
            finalMsg[255] = '\0';

            int repeatCount = 1;
            while (i - repeatCount >= 0)
            {
                const char *prevMsgNoTimestamp = engine->logs.entries[i - repeatCount].message + 9;
                if (strcmp(msgNoTimestamp, prevMsgNoTimestamp) != 0)
                    break;
                repeatCount++;
            }

            if (repeatCount > 1)
            {
                snprintf(finalMsg, sizeof(finalMsg), "[x%d] %s", repeatCount, engine->logs.entries[i].message);
                i -= (repeatCount - 1);
            }

            int j;
            for (j = 0; j < (int)strlen(finalMsg); j++)
            {
                char temp[256];
                strncpy(temp, finalMsg, j);
                temp[j] = '\0';

                if (MeasureTextEx(engine->font, temp, 20, 2).x < engine->sideBarWidth - 25)
                {
                    continue;
                }
                else
                {
                    break;
                }
            }
            strncpy(cutMessage, finalMsg, j);
            cutMessage[j] = '\0';

            Color logColor;
            switch (engine->logs.entries[i].level)
            {
            case LOG_LEVEL_NORMAL:
                logColor = WHITE;
                break;
            case LOG_LEVEL_WARNING:
                logColor = YELLOW;
                break;
            case LOG_LEVEL_ERROR:
                logColor = RED;
                break;
            case LOG_LEVEL_DEBUG:
                logColor = PURPLE;
                break;
            case LOG_LEVEL_SAVE:
                logColor = GREEN;
                break;
            default:
                logColor = WHITE;
                break;
            }

            AddUIElement(engine, (UIElement){
                                     .name = "LogText",
                                     .shape = UIText,
                                     .type = UI_ACTION_NO_COLLISION_ACTION,
                                     .text = {.textPos = {10, logY}, .textSize = 20, .textSpacing = 2, .textColor = logColor},
                                     .layer = 0});

            strncpy(engine->uiElements[engine->uiElementCount - 1].text.string, cutMessage, 127);
            engine->uiElements[engine->uiElementCount - 1].text.string[127] = '\0';

            logY -= 25;
        }

        if (engine->sideBarMiddleY > 45)
        {
            AddUIElement(engine, (UIElement){
                                     .name = "VarsFilterShowText",
                                     .shape = UIText,
                                     .type = UI_ACTION_NO_COLLISION_ACTION,
                                     .text = {.string = "Show:", .textPos = {engine->sideBarWidth - 155, 20}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                                     .layer = 0});
            char varsFilterText[10];
            Color varFilterColor;
            switch (engine->varsFilter)
            {
            case VAR_FILTER_ALL:
                strcpy(varsFilterText, "All");
                varFilterColor = RAYWHITE;
                break;
            case VAR_FILTER_NUMBERS:
                strcpy(varsFilterText, "Nums");
                varFilterColor = (Color){24, 119, 149, 255};
                break;
            case VAR_FILTER_STRINGS:
                strcpy(varsFilterText, "Strings");
                varFilterColor = (Color){180, 178, 40, 255};
                break;
            case VAR_FILTER_BOOLS:
                strcpy(varsFilterText, "Bools");
                varFilterColor = (Color){27, 64, 121, 255};
                break;
            case VAR_FILTER_COLORS:
                strcpy(varsFilterText, "Colors");
                varFilterColor = (Color){217, 3, 104, 255};
                break;
            case VAR_FILTER_SPRITES:
                strcpy(varsFilterText, "Sprites");
                varFilterColor = (Color){3, 206, 164, 255};
                break;
            default:
                engine->varsFilter = 0;
                strcpy(varsFilterText, "All");
                varFilterColor = RAYWHITE;
                break;
            }
            AddUIElement(engine, (UIElement){
                                     .name = "VarsFilterButton",
                                     .shape = UIRectangle,
                                     .type = UI_ACTION_CHANGE_VARS_FILTER,
                                     .rect = {.pos = {engine->sideBarWidth - 85, 15}, .recSize = {78, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                     .color = (Color){70, 70, 70, 200},
                                     .layer = 1,
                                     .text = {.textPos = {engine->sideBarWidth - 85 + (78 - MeasureTextEx(engine->font, varsFilterText, 20, 1).x) / 2, 20}, .textSize = 20, .textSpacing = 1, .textColor = varFilterColor},
                                 });
            strcpy(engine->uiElements[engine->uiElementCount - 1].text.string, varsFilterText);
        }

        int varsY = 60;
        for (int i = 0; i < (engine->isGameRunning ? interpreter->valueCount : graph->variablesCount) && varsY < engine->sideBarMiddleY - 40; i++)
        {
            if (engine->isGameRunning)
            {
                if (!interpreter->values[i].isVariable)
                {
                    continue;
                }
            }
            else
            {
                if (i == 0)
                {
                    continue;
                }
            }

            Color varColor;
            sprintf(cutMessage, "%s", engine->isGameRunning ? interpreter->values[i].name : graph->variables[i]);
            switch (engine->isGameRunning ? interpreter->values[i].type : graph->variableTypes[i])
            {
            case VAL_NUMBER:
            case NODE_CREATE_NUMBER:
                varColor = (Color){24, 119, 149, 255};
                if (engine->varsFilter != VAR_FILTER_NUMBERS && engine->varsFilter != VAR_FILTER_ALL)
                {
                    continue;
                }
                break;
            case VAL_STRING:
            case NODE_CREATE_STRING:
                varColor = (Color){180, 178, 40, 255};
                if (engine->varsFilter != VAR_FILTER_STRINGS && engine->varsFilter != VAR_FILTER_ALL)
                {
                    continue;
                }
                break;
            case VAL_BOOL:
            case NODE_CREATE_BOOL:
                varColor = (Color){27, 64, 121, 255};
                if (engine->varsFilter != VAR_FILTER_BOOLS && engine->varsFilter != VAR_FILTER_ALL)
                {
                    continue;
                }
                break;
            case VAL_COLOR:
            case NODE_CREATE_COLOR:
                varColor = (Color){217, 3, 104, 255};
                if (engine->varsFilter != VAR_FILTER_COLORS && engine->varsFilter != VAR_FILTER_ALL)
                {
                    continue;
                }
                break;
            case VAL_SPRITE:
            case NODE_CREATE_SPRITE:
                varColor = (Color){3, 206, 164, 255};
                if (engine->varsFilter != VAR_FILTER_SPRITES && engine->varsFilter != VAR_FILTER_ALL)
                {
                    continue;
                }
                break;
            default:
                varColor = LIGHTGRAY;
            }

            AddUIElement(engine, (UIElement){
                                     .name = "Variable Background",
                                     .shape = UIRectangle,
                                     .type = engine->isGameRunning ? UI_ACTION_VAR_TOOLTIP_RUNTIME : UI_ACTION_NO_COLLISION_ACTION,
                                     .rect = {.pos = {15, varsY - 5}, .recSize = {engine->sideBarWidth - 25, 35}, .roundness = 0.6f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                     .color = (Color){59, 59, 59, 255},
                                     .layer = 1,
                                     .valueIndex = i});

            float dotsWidth = MeasureTextEx(engine->font, "...", 24, 2).x;
            int j;
            bool wasCut = false;
            for (j = 1; j <= strlen(cutMessage); j++)
            {
                char temp[256];
                strncpy(temp, cutMessage, j);
                temp[j] = '\0';

                float textWidth = MeasureTextEx(engine->font, temp, 24, 2).x;
                if (textWidth + dotsWidth < engine->sideBarWidth - 80)
                    continue;

                wasCut = true;
                j--;
                break;
            }

            strncpy(cutMessage, cutMessage, j);
            cutMessage[j] = '\0';

            bool textHidden = false;

            if (wasCut && j < 252)
                strcat(cutMessage, "...");
            if (wasCut && j == 0)
            {
                cutMessage[0] = '\0';
                textHidden = true;
            }

            AddUIElement(engine, (UIElement){
                                     .name = "Variable",
                                     .shape = UICircle,
                                     .type = UI_ACTION_NO_COLLISION_ACTION,
                                     .circle = {.center = (Vector2){textHidden ? engine->sideBarWidth / 2 + 3 : engine->sideBarWidth - 25, varsY + 14}, .radius = 8},
                                     .color = varColor,
                                     .text = {.textPos = {20, varsY}, .textSize = 24, .textSpacing = 2, .textColor = WHITE},
                                     .layer = 2});

            strncpy(engine->uiElements[engine->uiElementCount - 1].text.string, cutMessage, 127);
            engine->uiElements[engine->uiElementCount - 1].text.string[128] = '\0';
            varsY += 40;
        }
    }

    AddUIElement(engine, (UIElement){
                             .name = "BottomBar",
                             .shape = UIRectangle,
                             .type = UI_ACTION_NO_COLLISION_ACTION,
                             .rect = {.pos = {0, engine->screenHeight - engine->bottomBarHeight}, .recSize = {engine->screenWidth, engine->bottomBarHeight}, .roundness = 0.0f, .roundSegments = 0},
                             .color = (Color){28, 28, 28, 255},
                             .layer = 0,
                         });

    AddUIElement(engine, (UIElement){
                             .name = "BottomBarFromViewportDividerLine",
                             .shape = UILine,
                             .type = UI_ACTION_NO_COLLISION_ACTION,
                             .line = {.startPos = {0, engine->screenHeight - engine->bottomBarHeight}, .engPos = {engine->screenWidth, engine->screenHeight - engine->bottomBarHeight}, .thickness = 2},
                             .color = WHITE,
                             .layer = 0,
                         });

    AddUIElement(engine, (UIElement){
                             .name = "BackButton",
                             .shape = UIRectangle,
                             .type = UI_ACTION_BACK_FILEPATH,
                             .rect = {.pos = {30, engine->screenHeight - engine->bottomBarHeight + 10}, .recSize = {65, 30}, .roundness = 0, .roundSegments = 0, .hoverColor = Fade(WHITE, 0.6f)},
                             .color = (Color){70, 70, 70, 150},
                             .layer = 1,
                             .text = {.string = "Back", .textPos = {35, engine->screenHeight - engine->bottomBarHeight + 12}, .textSize = 25, .textSpacing = 0, .textColor = WHITE}});

    AddUIElement(engine, (UIElement){
                             .name = "RefreshButton",
                             .shape = UIRectangle,
                             .type = UI_ACTION_REFRESH_FILES,
                             .rect = {.pos = {110, engine->screenHeight - engine->bottomBarHeight + 10}, .recSize = {100, 30}, .roundness = 0, .roundSegments = 0, .hoverColor = Fade(WHITE, 0.6f)},
                             .color = (Color){70, 70, 70, 150},
                             .layer = 1,
                             .text = {.string = "Refresh", .textPos = {119, engine->screenHeight - engine->bottomBarHeight + 12}, .textSize = 25, .textSpacing = 0, .textColor = WHITE}});

    AddUIElement(engine, (UIElement){
                             .name = "CurrentPath",
                             .shape = UIText,
                             .type = UI_ACTION_NO_COLLISION_ACTION,
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

        Color fileOutlineColor;
        Color fileTextColor;

        switch (GetFileType(fileName))
        {
        case FILE_FOLDER:
            fileOutlineColor = (Color){205, 205, 50, 200}; //(Color){200, 170, 50, 200}
            fileTextColor = (Color){240, 240, 120, 255};
            break;
        case FILE_CG:
            fileOutlineColor = (Color){220, 140, 240, 200};
            fileTextColor = (Color){245, 200, 255, 255};
            break;
        case FILE_IMAGE:
            fileOutlineColor = (Color){205, 30, 30, 200};
            fileTextColor = (Color){255, 60, 60, 255};
            break;
        case FILE_OTHER:
            fileOutlineColor = (Color){160, 160, 160, 255};
            fileTextColor = (Color){220, 220, 220, 255};
            break;
        default:
            AddToLog(engine, "File error", 2);
            fileOutlineColor = (Color){160, 160, 160, 255};
            fileTextColor = (Color){220, 220, 220, 255};
            break;
        }

        char buff[256];
        strncpy(buff, fileName, sizeof(buff) - 1);
        buff[sizeof(buff) - 1] = '\0';

        if (MeasureTextEx(engine->font, fileName, 25, 0).x > 135)
        {
            const char *ext = GetFileExtension(fileName);
            int extLen = strlen(ext);
            int maxLen = strlen(buff) - extLen;

            for (int j = maxLen - 1; j >= 0; j--)
            {
                buff[j] = '\0';
                if (MeasureText(buff, 25) < 115)
                {
                    if (j > 3)
                    {
                        buff[j - 3] = '\0';
                        strcat(buff, "...");
                        strcat(buff, ext);
                    }
                    break;
                }
            }
        }

        AddUIElement(engine, (UIElement){
                                 .name = "FileOutline",
                                 .shape = UIRectangle,
                                 .type = UI_ACTION_NO_COLLISION_ACTION,
                                 .rect = {.pos = {xOffset - 1, yOffset - 1}, .recSize = {152, 62}, .roundness = 0.5f, .roundSegments = 8},
                                 .color = fileOutlineColor,
                                 .layer = 0});

        AddUIElement(engine, (UIElement){
                                 .name = "File",
                                 .shape = UIRectangle,
                                 .type = UI_ACTION_OPEN_FILE,
                                 .rect = {.pos = {xOffset, yOffset}, .recSize = {150, 60}, .roundness = 0.5f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                 .color = (Color){40, 40, 40, 255},
                                 .layer = 1,
                                 .text = {.string = "", .textPos = {xOffset + 10, yOffset + 16}, .textSize = 25, .textSpacing = 0, .textColor = fileTextColor}});
        strncpy(engine->uiElements[engine->uiElementCount - 1].name, engine->files.paths[i], 256);
        strncpy(engine->uiElements[engine->uiElementCount - 1].text.string, buff, 31);
        engine->uiElements[engine->uiElementCount - 1].text.string[256] = '\0';

        xOffset += 200;
        if (xOffset + 100 >= engine->screenWidth)
        {
            xOffset = 50;
            yOffset += 120;
        }
    }

    AddUIElement(engine, (UIElement){
                             .name = "TopBarClose",
                             .shape = UIRectangle,
                             .type = UI_ACTION_CLOSE_WINDOW,
                             .rect = {.pos = {engine->screenWidth - 50, 0}, .recSize = {50, 50}, .roundness = 0.0f, .roundSegments = 0, .hoverColor = RED},
                             .color = (Color){0, 0, 0, 0},
                             .layer = 1,
                         });
    AddUIElement(engine, (UIElement){
                             .name = "TopBarMinimize",
                             .shape = UIRectangle,
                             .type = UI_ACTION_MINIMIZE_WINDOW,
                             .rect = {.pos = {engine->screenWidth - 100, 0}, .recSize = {50, 50}, .roundness = 0.0f, .roundSegments = 0, .hoverColor = GRAY},
                             .color = (Color){0, 0, 0, 0},
                             .layer = 1,
                         });

    AddUIElement(engine, (UIElement){
                             .name = "BottomBarResizeButton",
                             .shape = UICircle,
                             .type = UI_ACTION_RESIZE_BOTTOM_BAR,
                             .circle = {.center = (Vector2){engine->screenWidth / 2, engine->screenHeight - engine->bottomBarHeight}, .radius = 10},
                             .color = (Color){255, 255, 255, 1},
                             .layer = 1,
                         });
    AddUIElement(engine, (UIElement){
                             .name = "SideBarResizeButton",
                             .shape = UICircle,
                             .type = UI_ACTION_RESIZE_SIDE_BAR,
                             .circle = {.center = (Vector2){engine->sideBarWidth, (engine->screenHeight - engine->bottomBarHeight) / 2}, .radius = 10},
                             .color = (Color){255, 255, 255, 1},
                             .layer = 1,
                         });
    AddUIElement(engine, (UIElement){
                             .name = "SideBarMiddleResizeButton",
                             .shape = UICircle,
                             .type = UI_ACTION_RESIZE_SIDE_BAR_MIDDLE,
                             .circle = {.center = (Vector2){engine->sideBarWidth / 2, engine->sideBarMiddleY}, .radius = 10},
                             .color = (Color){255, 255, 255, 1},
                             .layer = 1,
                         });

    if (engine->isGameRunning)
    {
        AddUIElement(engine, (UIElement){
                                 .name = "ViewportFullscreenButton",
                                 .shape = UIRectangle,
                                 .type = UI_ACTION_FULLSCREEN_BUTTON_VIEWPORT,
                                 .rect = {.pos = {engine->sideBarWidth + 8, 10}, .recSize = {50, 50}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = GRAY},
                                 .color = (Color){60, 60, 60, 255},
                                 .layer = 1,
                             });
    }

    CountingSortByLayer(engine);

    DrawUIElements(engine, graph, editor, interpreter, runtimeGraph);

    // special symbols and textures
    DrawRectangleLinesEx((Rectangle){0, 0, engine->screenWidth, engine->screenHeight}, 4.0f, WHITE);

    DrawLineEx((Vector2){engine->screenWidth - 35, 15}, (Vector2){engine->screenWidth - 15, 35}, 2, WHITE);
    DrawLineEx((Vector2){engine->screenWidth - 35, 35}, (Vector2){engine->screenWidth - 15, 15}, 2, WHITE);

    DrawLineEx((Vector2){engine->screenWidth - 85, 25}, (Vector2){engine->screenWidth - 65, 25}, 2, WHITE);

    DrawTexture(engine->resizeButton, engine->screenWidth / 2 - 10, engine->screenHeight - engine->bottomBarHeight - 10, WHITE);
    DrawTexturePro(engine->resizeButton, (Rectangle){0, 0, 20, 20}, (Rectangle){engine->sideBarWidth, (engine->screenHeight - engine->bottomBarHeight) / 2, 20, 20}, (Vector2){10, 10}, 90.0f, WHITE);
    if (engine->sideBarWidth > 150)
    {
        DrawTexture(engine->resizeButton, engine->sideBarWidth / 2 - 10, engine->sideBarMiddleY - 10, WHITE);
    }

    if (engine->isGameRunning)
    {
        DrawTexturePro(engine->viewportFullscreenButton, (Rectangle){0, 0, engine->viewportFullscreenButton.width, engine->viewportFullscreenButton.height}, (Rectangle){engine->sideBarWidth + 8, 10, 50, 50}, (Vector2){0, 0}, 0, WHITE);
    }

    EndTextureMode();
}

bool HandleUICollisions(EngineContext *engine, GraphContext *graph, InterpreterContext *interpreter, EditorContext *editor, RuntimeGraphContext *runtimeGraph)
{
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S) && !IsKeyDown(KEY_LEFT_SHIFT) && !engine->isGameRunning)
    {
        if (engine->isSoundOn)
        {
            PlaySound(engine->save);
        }
        if (SaveGraphToFile(engine->CGFilePath, graph) == 0)
        {
            editor->hasChanged = false;
            AddToLog(engine, "Saved successfully!", 3);
        }
        else
        {
            AddToLog(engine, "ERROR SAVING CHANGES!", 1);
        }
    }
    else if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_R))
    {
        if (editor->hasChanged)
        {
            AddToLog(engine, "Project not saved!", 1);
        }
        else if (!engine->wasBuilt)
        {
            AddToLog(engine, "Project has not been built", 1);
        }
        else
        {
            engine->viewportMode = VIEWPORT_GAME_SCREEN;
            engine->isGameRunning = true;
            interpreter->isFirstFrame = true;
        }
    }
    else if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_E))
    {
        editor->delayFrames = true;
        engine->delayFrames = true;
        engine->viewportMode = VIEWPORT_CG_EDITOR;
        editor->isFirstFrame = true;
        engine->isGameRunning = false;
        engine->wasBuilt = false;
        engine->isGameFullscreen = false;
        FreeInterpreterContext(interpreter);
    }
    else if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_B))
    {
        if (editor->hasChanged)
        {
            AddToLog(engine, "Project not saved!", 1);
        }
        else
        {
            *runtimeGraph = ConvertToRuntimeGraph(graph, interpreter);
            engine->delayFrames = true;
            if (runtimeGraph != NULL)
            {
                AddToLog(engine, "Build Successfull", 0);
                engine->wasBuilt = true;
            }
            else
            {
                AddToLog(engine, "Build failed", 0);
            }
        }
    }
    else if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S) && IsKeyDown(KEY_LEFT_SHIFT))
    {
        editor->delayFrames = true;
        engine->delayFrames = true;
        engine->viewportMode = VIEWPORT_CG_EDITOR;
        editor->isFirstFrame = true;
        engine->isGameRunning = false;
        engine->wasBuilt = false;
        engine->isGameFullscreen = false;
        FreeInterpreterContext(interpreter);
    }
    else if (IsKeyPressed(KEY_ESCAPE))
    {
        engine->isGameFullscreen = false;
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
            if (engine->bottomBarHeight <= 5)
            {
                engine->bottomBarHeight = 5;
            }
            else if (engine->bottomBarHeight >= 3 * engine->screenHeight / 4)
            {
                engine->bottomBarHeight = 3 * engine->screenHeight / 4;
            }
            else
            {
                engine->sideBarMiddleY += GetMouseDelta().y / 2;
            }
            break;
        case 2:
            engine->sideBarWidth += GetMouseDelta().x;

            if (engine->sideBarWidth < 160 && GetMouseDelta().x < 0)
            {
                engine->sideBarWidth = 80;
                engine->sideBarHalfSnap = true;
            }
            else if (engine->sideBarWidth > 110)
            {
                engine->sideBarHalfSnap = false;
                if (engine->sideBarWidth >= 3 * engine->screenWidth / 4)
                {
                    engine->sideBarWidth = 3 * engine->screenWidth / 4;
                }
            }
            break;
        case 3:
            engine->sideBarMiddleY += GetMouseDelta().y;
            break;
        default:
            break;
        }

        if (engine->sideBarMiddleY >= engine->screenHeight - engine->bottomBarHeight - 60 - engine->sideBarHalfSnap * 40)
        {
            engine->sideBarMiddleY = engine->screenHeight - engine->bottomBarHeight - 60 - engine->sideBarHalfSnap * 40;
        }
        else if (engine->sideBarMiddleY <= 5)
        {
            engine->sideBarMiddleY = 5;
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

void ContextChangePerFrame(EngineContext *engine)
{
    engine->mousePos = GetMousePosition();
    engine->isViewportFocused = CheckCollisionPointRec(engine->mousePos, (Rectangle){engine->sideBarWidth, 0, engine->screenWidth - engine->sideBarWidth, engine->screenHeight - engine->bottomBarHeight});

    if (engine->prevScreenWidth != engine->screenWidth || engine->prevScreenHeight != engine->screenHeight || engine->hasResizedBar)
    {
        engine->prevScreenWidth = engine->screenWidth;
        engine->prevScreenHeight = engine->screenHeight;
        engine->screenWidth = GetScreenWidth();
        engine->screenHeight = GetScreenHeight();
        engine->viewportWidth = engine->screenWidth - engine->sideBarWidth;
        engine->viewportHeight = engine->screenHeight - engine->bottomBarHeight;
        engine->hasResizedBar = false;
        engine->delayFrames = true;
    }
}

int GetEngineMouseCursor(EngineContext *engine, EditorContext *editor)
{
    if (engine->draggingResizeButtonID == 1 || engine->draggingResizeButtonID == 3)
    {
        return MOUSE_CURSOR_RESIZE_NS;
    }
    else if (engine->draggingResizeButtonID == 2)
    {
        return MOUSE_CURSOR_RESIZE_EW;
    }

    if (engine->isViewportFocused)
    {
        switch (engine->viewportMode)
        {
        case VIEWPORT_CG_EDITOR:
            return editor->cursor;
        case VIEWPORT_GAME_SCREEN:
            // return interpreter->cursor;
            return MOUSE_CURSOR_ARROW;
        case VIEWPORT_HITBOX_EDITOR:
            return MOUSE_CURSOR_CROSSHAIR; //
        default:
            engine->viewportMode = VIEWPORT_CG_EDITOR;
            return editor->cursor;
        }
    }

    if (engine->isSaveButtonHovered && engine->isGameRunning)
    {
        return MOUSE_CURSOR_NOT_ALLOWED;
    }

    if (engine->hoveredUIElementIndex != -1)
    {
        return MOUSE_CURSOR_POINTING_HAND;
    }

    return MOUSE_CURSOR_ARROW;
}

int GetEngineFPS(EngineContext *engine, EditorContext *editor, InterpreterContext *interpreter)
{
    int fps;

    if (engine->isViewportFocused)
    {
        switch (engine->viewportMode)
        {
        case VIEWPORT_CG_EDITOR:
            fps = editor->fps;
            break;
        case VIEWPORT_GAME_SCREEN:
            fps = interpreter->fps;
            break;
        case VIEWPORT_HITBOX_EDITOR:
            fps = 60;
            break;
        default:
            fps = 60;
            engine->viewportMode = VIEWPORT_CG_EDITOR;
            break;
        }
    }
    else
    {
        fps = engine->fps;
    }

    return fps;
}

int main()
{

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1600, 1000, "RapidEngine");
    SetTargetFPS(140);
    char fileName[128];
    strcpy(fileName, "Tetris" /*HandleProjectManager()*/); // temporary hardcode

    SetTargetFPS(60);

    MaximizeWindow();

    InitAudioDevice();

    EngineContext engine = InitEngineContext();
    EditorContext editor = InitEditorContext();
    GraphContext graph = InitGraphContext();
    InterpreterContext interpreter = InitInterpreterContext();
    RuntimeGraphContext runtimeGraph = {0};

    engine.currentPath = SetProjectFolderPath(fileName);

    engine.files = LoadDirectoryFilesEx(engine.currentPath, NULL, false);

    PrepareCGFilePath(&engine, fileName);

    interpreter.projectPath = strdup(engine.currentPath);
    engine.projectPath = strdup(engine.currentPath); // should be moved

    LoadGraphFromFile(engine.CGFilePath, &graph);

    AddToLog(&engine, "All resources loaded. Welcome!", 0);

    while (!WindowShouldClose() || IsKeyDown(KEY_ESCAPE))
    {
        if (!IsWindowReady())
        {
            EmergencyExit(&engine);
        }

        ContextChangePerFrame(&engine);

        int prevHoveredUIIndex = engine.hoveredUIElementIndex;

        if (HandleUICollisions(&engine, &graph, &interpreter, &editor, &runtimeGraph) && !engine.isGameFullscreen)
        {
            if (((prevHoveredUIIndex != engine.hoveredUIElementIndex || IsMouseButtonDown(MOUSE_LEFT_BUTTON)) && engine.showSaveWarning != 1) || engine.delayFrames)
            {
                BuildUITexture(&engine, &graph, &editor, &interpreter, &runtimeGraph);
                engine.fps = 140;
            }
            engine.delayFrames = true;
        }
        else if (engine.delayFrames && !engine.isGameFullscreen)
        {
            BuildUITexture(&engine, &graph, &editor, &interpreter, &runtimeGraph);
            engine.fps = 60;
            engine.delayFrames = false;
        }

        SetMouseCursor(GetEngineMouseCursor(&engine, &editor));

        SetTargetFPS(GetEngineFPS(&engine, &editor, &interpreter));

        if (GetMouseWheelMove() != 0 && engine.isViewportFocused && !editor.menuOpen && engine.viewportMode == VIEWPORT_CG_EDITOR)
        {
            editor.delayFrames = true;

            float wheel = GetMouseWheelMove();
            float zoom = engine.editorZoom;

            if (wheel > 0 && zoom < 1.5f)
                engine.editorZoom = zoom + 0.25f;

            if (wheel < 0 && zoom > 0.5f)
                engine.editorZoom = zoom - 0.25f;
        }
        editor.zoom = engine.editorZoom;

        float srcW;
        float srcH;

        int textureMouseX;
        int textureMouseY;

        if (engine.viewportMode == VIEWPORT_CG_EDITOR)
        {
            srcW = engine.viewportWidth / engine.editorZoom;
            srcH = engine.viewportHeight / engine.editorZoom;

            textureMouseX = (engine.mousePos.x - engine.sideBarWidth) / engine.editorZoom + (engine.viewport.texture.width - srcW) / 2.0f;
            textureMouseY = engine.mousePos.y / engine.editorZoom + (engine.viewport.texture.height - srcH) / 2.0f;
        }
        else
        {
            srcW = engine.viewportWidth;
            srcH = engine.viewportHeight;

            textureMouseX = engine.mousePos.x - (engine.isGameFullscreen ? 0 : engine.sideBarWidth) + (engine.viewport.texture.width - (engine.isGameFullscreen ? engine.screenWidth : engine.viewportWidth)) / 2.0f;
            textureMouseY = engine.mousePos.y + (engine.viewport.texture.height - (engine.isGameFullscreen ? engine.screenHeight : engine.viewportHeight)) / 2.0f;
        }

        Rectangle viewportBoundaryTranslated = (Rectangle){
            engine.sideBarWidth - (engine.isGameFullscreen ? 0 : engine.sideBarWidth) + (engine.viewport.texture.width - (engine.isGameFullscreen ? engine.screenWidth : engine.viewportWidth)) / 2.0f,
            (engine.viewport.texture.height - (engine.isGameFullscreen ? engine.screenHeight : engine.viewportHeight)) / 2.0f,
            engine.screenWidth - (engine.isGameFullscreen ? 0 : engine.sideBarWidth),
            engine.screenHeight - (engine.isGameFullscreen ? 0 : engine.bottomBarHeight)};

        BeginDrawing();
        ClearBackground(BLACK);

        Polygon test;

        if (engine.showSaveWarning == 1)
        {
            engine.isViewportFocused = false;
        }

        switch (engine.viewportMode)
        {
        case VIEWPORT_CG_EDITOR:
        {
            if (engine.CGFilePath[0] != '\0' && (engine.isViewportFocused || editor.isFirstFrame))
            {
                editor.leftBorderLimit = (engine.viewport.texture.width - srcW) / 2.0f;
                editor.bottomBorderLimit = (engine.screenHeight - engine.bottomBarHeight) / engine.editorZoom + (engine.viewport.texture.height - srcH) / 2.0f;
                editor.rightBorderLimit = (engine.screenWidth - engine.sideBarWidth) / engine.editorZoom + (engine.viewport.texture.width - srcW) / 2.0f;
                HandleEditor(&editor, &graph, &engine.viewport, (Vector2){textureMouseX, textureMouseY}, engine.draggingResizeButtonID != 0);
            }

            if (editor.newLogMessage)
            {
                AddToLog(&engine, editor.logMessage, editor.logMessageLevel);
            }
            if (editor.engineDelayFrames)
            {
                editor.engineDelayFrames = false;
                engine.delayFrames = true;
            }
            if (editor.hasChangedInLastFrame)
            {
                engine.delayFrames = true;
                editor.hasChangedInLastFrame = false;
                engine.wasBuilt = false;
            }
            if (editor.shouldOpenHitboxEditor)
            {
                editor.shouldOpenHitboxEditor = false;
                engine.viewportMode = VIEWPORT_HITBOX_EDITOR;
            }

            break;
        }
        case VIEWPORT_GAME_SCREEN:
        {
            BeginTextureMode(engine.viewport);
            ClearBackground(BLACK);

            for (int i = 0; i < interpreter.componentCount; i++)
            {
                if (interpreter.components[i].isSprite)
                {
                    interpreter.components[i].sprite.hitbox.type = HITBOX_POLY;
                    memcpy(interpreter.components[i].sprite.hitbox.polygonHitbox.vertices, test.vertices, sizeof(Vector2) * MAX_HITBOX_VERTICES);
                    interpreter.components[i].sprite.hitbox.polygonHitbox.count = test.count;
                }
            }

            engine.isGameRunning = HandleGameScreen(&interpreter, &runtimeGraph, (Vector2){textureMouseX, textureMouseY}, viewportBoundaryTranslated);

            if (!engine.isGameRunning)
            {
                engine.viewportMode = VIEWPORT_CG_EDITOR;
                editor.isFirstFrame = true;
                engine.wasBuilt = false;
                FreeInterpreterContext(&interpreter);
            }
            if (interpreter.newLogMessage)
            {
                for (int i = 0; i < interpreter.logMessageCount; i++)
                    AddToLog(&engine, interpreter.logMessages[i], interpreter.logMessageLevels[i]);

                interpreter.newLogMessage = false;
                interpreter.logMessageCount = 0;
                engine.delayFrames = true;
            }
            EndTextureMode();

            break;
        }
        case VIEWPORT_HITBOX_EDITOR:
        {
            static HitboxEditorContext hitboxEditor = {0};

            if (hitboxEditor.texture.id == 0)
            {
                char path[128];
                snprintf(path, sizeof(path), "%s%c%s", engine.projectPath, PATH_SEPARATOR, editor.hitboxEditorFileName);

                Image img = LoadImage(path);
                if (img.data == NULL)
                {
                    AddToLog(&engine, "Invalid texture file name", 2);
                    engine.viewportMode = VIEWPORT_CG_EDITOR;
                }
                else
                {
                    int newWidth, newHeight;
                    if (img.width >= img.height)
                    {
                        newWidth = 500;
                        newHeight = (int)((float)img.height * 500 / img.width);
                    }
                    else
                    {
                        newHeight = 500;
                        newWidth = (int)((float)img.width * 500 / img.height);
                    }

                    ImageResize(&img, newWidth, newHeight);

                    Texture2D tex = LoadTextureFromImage(img);
                    UnloadImage(img);

                    Vector2 texPos = (Vector2){
                        engine.viewport.texture.width / 2.0f,
                        engine.viewport.texture.height / 2.0f};

                    hitboxEditor = InitHitboxEditor(tex, texPos);

                    float scaleX = (float)hitboxEditor.texture.width / hitboxEditor.texture.width;
                    float scaleY = (float)hitboxEditor.texture.height / hitboxEditor.texture.height;

                    for (int i = 0; i < hitboxEditor.poly.count; i++)
                    {
                        hitboxEditor.poly.vertices[i].x *= scaleX;
                        hitboxEditor.poly.vertices[i].y *= scaleY;
                        
                        hitboxEditor.poly.vertices[i].x -= hitboxEditor.texture.width / 2.0f;
                        hitboxEditor.poly.vertices[i].y -= hitboxEditor.texture.height / 2.0f;
                    }
                }
            }

            if (engine.isViewportFocused)
            {
                UpdateEditor(&hitboxEditor, (Vector2){textureMouseX, textureMouseY});
            }

            BeginTextureMode(engine.viewport);

            DrawEditor(&hitboxEditor, (Vector2){textureMouseX, textureMouseY});

            EndTextureMode();

            if (IsKeyPressed(KEY_ESCAPE))
            {
                if (hitboxEditor.poly.closed)
                {
                    test = hitboxEditor.poly;
                    engine.viewportMode = VIEWPORT_CG_EDITOR;
                }
                else
                {
                    engine.viewportMode = VIEWPORT_CG_EDITOR;
                }
            }

            break;
        }
        default:
        {
            AddToLog(&engine, "Viewport error!", LOG_ERROR);
            engine.viewportMode = VIEWPORT_CG_EDITOR;
        }
        }

        if (!engine.isGameRunning || !engine.isGameFullscreen)
        {
            DrawTexturePro(
                engine.viewport.texture,
                (Rectangle){
                    (engine.viewport.texture.width - srcW) / 2.0f,
                    (engine.viewport.texture.height - srcH) / 2.0f,
                    srcW,
                    -srcH},
                (Rectangle){
                    engine.sideBarWidth,
                    0,
                    engine.screenWidth - engine.sideBarWidth,
                    engine.screenHeight - engine.bottomBarHeight},
                (Vector2){0, 0},
                0.0f,
                WHITE);
        }

        if (engine.viewportMode == VIEWPORT_CG_EDITOR)
        {
            DrawTextEx(GetFontDefault(), "CoreGraph", (Vector2){engine.sideBarWidth + 20, 30}, 40, 4, Fade(WHITE, 0.2f));
            DrawTextEx(GetFontDefault(), "TM", (Vector2){engine.sideBarWidth + 230, 20}, 15, 1, Fade(WHITE, 0.2f));
        }
        else if (engine.viewportMode == VIEWPORT_HITBOX_EDITOR)
        {
            DrawTextEx(engine.font, "Press ESC to Save & Exit Hitbox Editor", (Vector2){engine.sideBarWidth + 30, 30}, 30, 1, GRAY);
        }

        DrawTextureRec(engine.UI.texture, (Rectangle){0, 0, engine.UI.texture.width, -engine.UI.texture.height}, (Vector2){0, 0}, WHITE);

        if (engine.isGameRunning && engine.isGameFullscreen)
        {
            DrawTexturePro(
                engine.viewport.texture,
                (Rectangle){
                    (engine.viewport.texture.width - engine.screenWidth) / 2.0f,
                    (engine.viewport.texture.height - engine.screenHeight) / 2.0f,
                    engine.screenWidth,
                    -engine.screenHeight},
                (Rectangle){
                    0,
                    0,
                    engine.screenWidth,
                    engine.screenHeight},
                (Vector2){0, 0},
                0.0f,
                WHITE);
        }

        if (engine.showSaveWarning == 1)
        {
            engine.showSaveWarning = DrawSaveWarning(&engine, &graph, &editor);
            if (engine.showSaveWarning == 2)
            {
                CloseWindow();
                break;
            }
        }

        EndDrawing();
    }

    FreeEngineContext(&engine);
    FreeEditorContext(&editor);
    FreeGraphContext(&graph);
    FreeInterpreterContext(&interpreter);

    free(interpreter.projectPath);
    interpreter.projectPath = NULL;

    CloseAudioDevice();

    return 0;
}