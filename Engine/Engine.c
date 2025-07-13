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

    engine.viewport = LoadRenderTexture(10000, 10000);
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
        engine.currentPath = projectPath;
    }

    engine.CGFilePath = malloc(MAX_PATH_LENGTH);
    engine.CGFilePath[0] = '\0';

    engine.cursor = MOUSE_CURSOR_POINTING_HAND;

    engine.files = LoadDirectoryFilesEx(projectPath, NULL, false);

    engine.hoveredUIElementIndex = -1;

    engine.isEditorOpened = true;
    engine.isGameRunning = false;

    engine.save = LoadSound("save.wav");

    engine.isSoundOn = true;

    engine.sideBarHalfSnap = false;
    engine.sideBarFullSnap = false;

    engine.editorZoom = 1.0f;

    engine.wasBuilt = false;

    engine.showSaveWarning = 0;

    return engine;
}

void FreeEngineContext(EngineContext *engine)
{
    if (engine->currentPath)
    {
        free(engine->currentPath);
        engine->currentPath = NULL;
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

void SetProjectPaths(EngineContext *engine, const char *projectName)
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
}

int DrawSaveWarning(EngineContext *engine, GraphContext *graph)
{
    engine->isViewportFocused = false;
    int popupWidth = 500;
    int popupHeight = 150;
    int screenWidth = engine->screenWidth;
    int screenHeight = engine->screenHeight;
    int popupX = (screenWidth - popupWidth) / 2;
    int popupY = (screenHeight - popupHeight) / 2 - 100;

    DrawRectangle(0, 0, screenWidth, screenHeight, (Color){0, 0, 0, 150});

    DrawRectangleRounded((Rectangle){popupX, popupY, popupWidth, popupHeight}, 0.4f, 8, LIGHTGRAY);
    DrawRectangleRoundedLines((Rectangle){popupX, popupY, popupWidth, popupHeight}, 0.4f, 8, DARKGRAY);

    const char *message = "Save changes before exit?";
    int textWidth = MeasureTextEx(engine->font, message, 30, 0).x;
    DrawTextEx(engine->font, message, (Vector2){popupX + (popupWidth - textWidth) / 2, popupY + 20}, 30, 0, BLACK);

    // Button dimensions
    int btnWidth = 120;
    int btnHeight = 30;
    int btnSpacing = 10;
    int btnY = popupY + popupHeight - btnHeight - 20;

    // Calculate button X positions for 3 buttons spaced evenly
    int totalBtnWidth = btnWidth * 3 + btnSpacing * 2 + 30;
    int btnStartX = popupX + (popupWidth + 5 - totalBtnWidth) / 2;

    // Draw and handle buttons
    Rectangle saveBtn = {btnStartX, btnY, btnWidth, btnHeight};
    Rectangle closeBtn = {btnStartX + btnWidth + btnSpacing, btnY, btnWidth + 34, btnHeight};
    Rectangle cancelBtn = {btnStartX + 2 * (btnWidth + btnSpacing) + 34, btnY, btnWidth, btnHeight};

    DrawRectangleRounded(saveBtn, 0.2f, 4, GRAY);
    DrawTextEx(engine->font, "Save and Close", (Vector2){saveBtn.x + 10, saveBtn.y + 7}, 16, 0, BLACK);

    DrawRectangleRounded(closeBtn, 0.2f, 4, GRAY);
    DrawTextEx(engine->font, "Close without Saving", (Vector2){closeBtn.x + 7, closeBtn.y + 7}, 16, 0, BLACK);

    DrawRectangleRounded(cancelBtn, 0.2f, 4, GRAY);
    DrawTextEx(engine->font, "Cancel", (Vector2){cancelBtn.x + 35, cancelBtn.y + 7}, 16, 0, BLACK);

    // Mouse click detection
    Vector2 mousePos = GetMousePosition();
    DrawText(TextFormat("Mouse: %.0f %.0f", mousePos.x, mousePos.y), 10, 10, 20, RED);
DrawText(TextFormat("Btn: %d", IsMouseButtonPressed(MOUSE_LEFT_BUTTON)), 10, 30, 20, RED);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (CheckCollisionPointRec(mousePos, saveBtn))
        {
            if (SaveGraphToFile(engine->CGFilePath, graph) == 0)
            {
                AddToLog(engine, "Saved successfully!", 0);
            }
            else
            {
                AddToLog(engine, "ERROR SAVING CHANGES!", 1);
            }
            return 2;
        }
        else if (CheckCollisionPointRec(mousePos, closeBtn))
        {
            return 2;
        }
        else if (CheckCollisionPointRec(mousePos, cancelBtn))
        {
            return 0;
        }
    }

    return 1;
}

void DrawUIElements(EngineContext *engine, char *CGFilePath, GraphContext *graph, EditorContext *editor, InterpreterContext *interpreter, RuntimeGraphContext *runtimeGraph)
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
                if (engine->isSoundOn)
                {
                    PlaySound(engine->save);
                }
                if (SaveGraphToFile(CGFilePath, graph) == 0)
                {
                    editor->hasChanged = false;
                    AddToLog(engine, "Saved successfully!", 0);
                }
                else
                    AddToLog(engine, "ERROR SAVING CHANGES!", 1);
            }
            break;
        case STOP_GAME:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                engine->isEditorOpened = true;
                editor->delayFrames = true;
            }
            break;
        case RUN_GAME:
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
                engine->isEditorOpened = false;
                engine->isGameRunning = true;
                interpreter->isFirstFrame = true;
            }
            break;
        case BUILD_GRAPH:
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
        case MINIMIZE_WINDOW:
            engine->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                MinimizeWindow();
            }
            break;
        case RESIZE_BOTTOM_BAR:
            engine->isViewportFocused = false;
            engine->cursor = MOUSE_CURSOR_RESIZE_NS;
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && engine->draggingResizeButtonID == 0)
            {
                engine->draggingResizeButtonID = 1;
            }
            break;
        case RESIZE_SIDE_BAR:
            engine->isViewportFocused = false;
            engine->cursor = MOUSE_CURSOR_RESIZE_EW;
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && engine->draggingResizeButtonID == 0)
            {
                engine->draggingResizeButtonID = 2;
            }
            break;
        case RESIZE_SIDE_BAR_MIDDLE:
            engine->isViewportFocused = false;
            engine->cursor = MOUSE_CURSOR_RESIZE_NS;
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
                                     .name = "FileTooltip",
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
                        char *openedFileName = malloc(strlen(engine->uiElements[engine->hoveredUIElementIndex].text.string) + 1);
                        strcpy(openedFileName, engine->uiElements[engine->hoveredUIElementIndex].text.string);
                        openedFileName[strlen(engine->uiElements[engine->hoveredUIElementIndex].text.string) - 3] = '\0';

                        // FreeEditorContext(editor);
                        // FreeGraphContext(graph);

                        *editor = InitEditorContext();
                        *graph = InitGraphContext();

                        SetProjectPaths(engine, openedFileName);

                        LoadGraphFromFile(engine->CGFilePath, graph);

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

        case SHOW_VAR_INFO:
            char temp[256];
            char *valueString = ValueToString(interpreter->values[engine->uiElements[engine->hoveredUIElementIndex].valueIndex]);
            sprintf(temp, "%s: %s", interpreter->values[engine->uiElements[engine->hoveredUIElementIndex].valueIndex].name, valueString);
            AddUIElement(engine, (UIElement){
                                     .name = "VarTooltip",
                                     .shape = UIRectangle,
                                     .type = VAR_TOOLTIP,
                                     .rect = {.pos = {engine->sideBarWidth, engine->uiElements[engine->hoveredUIElementIndex].rect.pos.y}, .recSize = {MeasureTextEx(engine->font, temp, 20, 0).x + 20, 40}, .roundness = 0.4f, .roundSegments = 8},
                                     .color = DARKGRAY,
                                     .layer = 1,
                                     .text = {.textPos = {engine->sideBarWidth + 10, engine->uiElements[engine->hoveredUIElementIndex].rect.pos.y + 10}, .textSize = 20, .textSpacing = 0, .textColor = WHITE}});
            sprintf(engine->uiElements[engine->uiElementCount - 1].text.string, "%s", temp);
            free(valueString);
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

void BuildUITexture(EngineContext *engine, GraphContext *graph, char *CGFilePath, EditorContext *editor, InterpreterContext *interpreter, RuntimeGraphContext *runtimeGraph)
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

        Vector2 saveButtonPos = {
            engine->sideBarHalfSnap ? engine->sideBarWidth - 70 : engine->sideBarWidth - 145,
            engine->sideBarHalfSnap ? engine->sideBarMiddleY + 60 : engine->sideBarMiddleY + 15};

        AddUIElement(engine, (UIElement){
                                 .name = "SaveButton",
                                 .shape = UIRectangle,
                                 .type = SAVE_CG,
                                 .rect = {.pos = saveButtonPos, .recSize = {64, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                 .color = (Color){255, 255, 255, 50},
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

        if (!engine->isEditorOpened)
        {
            AddUIElement(engine, (UIElement){
                                     .name = "StopButton",
                                     .shape = UIRectangle,
                                     .type = STOP_GAME,
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
                                     .type = RUN_GAME,
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
                                     .type = BUILD_GRAPH,
                                     .rect = {.pos = {engine->sideBarWidth - 70, engine->sideBarMiddleY + 15}, .recSize = {64, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                     .color = (Color){255, 255, 255, 50},
                                     .layer = 1,
                                     .text = {.string = "Build", .textPos = {engine->sideBarWidth - 64, engine->sideBarMiddleY + 20}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                                 });
        }

        int logY = engine->screenHeight - engine->bottomBarHeight - 30;
        char cutMessage[256];
        for (int i = engine->logs.count - 1; i >= 0 && logY > engine->sideBarMiddleY + 50; i--)
        {
            if (engine->sideBarHalfSnap)
            {
                strncpy(cutMessage, engine->logs.entries[i].message + 9, 255);
                cutMessage[255] = '\0';
            }
            else
            {
                strncpy(cutMessage, engine->logs.entries[i].message, 255);
            }
            int j;
            for (j = 0; j < strlen(cutMessage); j++)
            {
                char temp[256];
                strncpy(temp, cutMessage, j);
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
            cutMessage[j] = '\0';
            AddUIElement(engine, (UIElement){
                                     .name = "LogText",
                                     .shape = UIText,
                                     .type = NO_COLLISION_ACTION,
                                     .text = {.textPos = {10, logY}, .textSize = 20, .textSpacing = 2, .textColor = (engine->logs.entries[i].level == 0) ? WHITE : (engine->logs.entries[i].level == 1) ? YELLOW
                                                                                                                                                                                                        : RED},
                                     .layer = 0});
            strncpy(engine->uiElements[engine->uiElementCount - 1].text.string, cutMessage, 127);
            engine->uiElements[engine->uiElementCount - 1].text.string[128] = '\0';
            logY -= 25;
        }

        int varsY = 40;
        for (int i = 0; i < interpreter->valueCount; i++)
        {
            if (!interpreter->values[i].isVariable)
            {
                continue;
            }
            AddUIElement(engine, (UIElement){
                                     .name = "Variable Background",
                                     .shape = UIRectangle,
                                     .type = SHOW_VAR_INFO,
                                     .rect = {.pos = {15, varsY - 5}, .recSize = {engine->sideBarWidth - 25, 35}, .roundness = 0.6f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                     .color = (Color){59, 59, 59, 255},
                                     .layer = 1,
                                     .valueIndex = i});

            Color varColor;
            sprintf(cutMessage, "%s", interpreter->values[i].name);
            switch (interpreter->values[i].type)
            {
            case VAL_NUMBER:
            case VAL_STRING:
            case VAL_BOOL:
            case VAL_COLOR:
                varColor = (Color){145, 0, 0, 255};
                break;
            case VAL_SPRITE:
                varColor = (Color){3, 161, 252, 255};
                break;
            default:
                varColor = LIGHTGRAY;
            }
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
                                     .type = NO_COLLISION_ACTION,
                                     .circle = {.center = (Vector2){textHidden ? engine->sideBarWidth / 2 : engine->sideBarWidth - 25, varsY + 14}, .radius = 8},
                                     .color = varColor,
                                     .text = {.textPos = {20, varsY}, .textSize = 24, .textSpacing = 2, .textColor = WHITE},
                                     .layer = 0});

            strncpy(engine->uiElements[engine->uiElementCount - 1].text.string, cutMessage, 127);
            engine->uiElements[engine->uiElementCount - 1].text.string[128] = '\0';
            varsY += 40;
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

    DrawUIElements(engine, CGFilePath, graph, editor, interpreter, runtimeGraph);

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

bool HandleUICollisions(EngineContext *engine, int fileCount, char *projectPath, char *CGFilePath, GraphContext *graph, InterpreterContext *interpreter, EditorContext *editor, RuntimeGraphContext *runtimeGraph)
{
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S))
    {
        if (engine->isSoundOn)
        {
            PlaySound(engine->save);
        }
        if (SaveGraphToFile(CGFilePath, graph) == 0)
        {
            editor->hasChanged = false;
            AddToLog(engine, "Saved successfully!", 0);
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
            engine->isEditorOpened = false;
            engine->isGameRunning = true;
            interpreter->isFirstFrame = true;
        }
    }
    else if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_E))
    {
        engine->isEditorOpened = true;
        editor->delayFrames = true;
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
            if (engine->bottomBarHeight < 50 && GetMouseDelta().y > 0)
            {
                engine->bottomBarHeight = 5;
            }
            engine->bottomBarHeight -= GetMouseDelta().y;
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
            }
            break;
        case 3:
            engine->sideBarMiddleY += GetMouseDelta().y;
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

void ContextChangePerFrame(EngineContext *engine)
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

bool ProjectCGFileExists(EngineContext *engine)
{
    for (int i = 0; i < engine->files.count; i++)
    {
        if (strcmp(engine->CGFilePath, engine->files.paths[i]) == 0)
        {
            return true;
        }
    }

    return false;
}

int main()
{

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1600, 1000, "RapidEngine");
    SetTargetFPS(140);
    char fileName[32];
    strcpy(fileName, /*handleProjectManager()*/ "Tetris"); // temporary hardcode
    char *projectPath = PrepareProjectPath(fileName);

    SetTargetFPS(60);

    MaximizeWindow();

    InitAudioDevice();

    EngineContext engine = InitEngineContext(projectPath);
    EditorContext editor = InitEditorContext();
    GraphContext graph = InitGraphContext();
    InterpreterContext interpreter = InitInterpreterContext();
    RuntimeGraphContext runtimeGraph = {0};

    AddToLog(&engine, "All resources loaded", 0);

    AddToLog(&engine, "Welcome!", 0);

    SetProjectPaths(&engine, "Tetris");

    ProjectCGFileExists(&engine);

    LoadGraphFromFile(engine.CGFilePath, &graph);

    bool windowShouldClose = false;

    while (!windowShouldClose && !WindowShouldClose())
    {
        ContextChangePerFrame(&engine);

        if (HandleUICollisions(&engine, engine.files.count, projectPath, engine.CGFilePath, &graph, &interpreter, &editor, &runtimeGraph))
        {
            if (engine.cursor == MOUSE_CURSOR_ARROW)
            {
                engine.cursor = MOUSE_CURSOR_POINTING_HAND;
            }
            BuildUITexture(&engine, &graph, engine.CGFilePath, &editor, &interpreter, &runtimeGraph);
            engine.fps = 140;
            engine.delayFrames = true;
        }
        else if (engine.delayFrames)
        {
            BuildUITexture(&engine, &graph, engine.CGFilePath, &editor, &interpreter, &runtimeGraph);
            engine.cursor = MOUSE_CURSOR_ARROW;
            engine.fps = 60;
            engine.delayFrames = false;
        }

        if (engine.isViewportFocused)
        {
            engine.cursor = editor.cursor; // should be viewport.cursor
            engine.fps = editor.fps;
        }
        SetMouseCursor(engine.cursor);
        SetTargetFPS(engine.fps);

        if (GetMouseWheelMove() != 0 && engine.isViewportFocused && !editor.menuOpen)
        {
            editor.delayFrames = true;

            float wheel = GetMouseWheelMove();
            float zoom = engine.editorZoom;

            if (wheel > 0 && zoom < 1.5f)
                engine.editorZoom = zoom + 0.5f;

            if (wheel < 0 && zoom > 0.5f)
                engine.editorZoom = zoom - 0.5f;
        }
        editor.zoom = engine.editorZoom;

        float srcW = engine.viewportWidth / engine.editorZoom;
        float srcH = engine.viewportHeight / engine.editorZoom;

        int textureX = (GetMouseX() - engine.sideBarWidth) / engine.editorZoom + (engine.viewport.texture.width - srcW) / 2.0f;
        int textureY = GetMouseY() / engine.editorZoom + (engine.viewport.texture.height - srcH) / 2.0f;

        BeginDrawing();
        ClearBackground(BLACK);

        if (engine.isEditorOpened)
        {
            if (engine.CGFilePath[0] != '\0' && (engine.isViewportFocused || editor.delayFrames || editor.draggingNodeIndex != 0))
            {
                HandleEditor(&editor, &graph, &engine.viewport, (Vector2){textureX, textureY}, engine.draggingResizeButtonID != 0);
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
            if (editor.hasChanged)
            {
                engine.delayFrames = true;
                ;
                engine.wasBuilt = false;
            }
        }
        else
        {
            BeginTextureMode(engine.viewport);
            ClearBackground(BLACK);

            if (engine.isGameRunning && engine.CGFilePath[0] != '\0')
            {
                engine.isGameRunning = HandleGameScreen(&interpreter, &runtimeGraph);

                if (interpreter.newLogMessage)
                {
                    AddToLog(&engine, interpreter.logMessage, interpreter.logMessageLevel);
                }
            }
            else
            {
                DrawTextEx(engine.font, "Game Screen", (Vector2){(engine.screenWidth - engine.sideBarWidth) / 2 - 100, (engine.screenHeight - engine.bottomBarHeight) / 2}, 50, 0, WHITE);
            }
            EndTextureMode();
        }

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

        DrawTextureRec(engine.UI.texture, (Rectangle){0, 0, engine.UI.texture.width, -engine.UI.texture.height}, (Vector2){0, 0}, WHITE);

        if (engine.CGFilePath[0] != '\0' && engine.isEditorOpened)
        {
            DrawTextEx(GetFontDefault(), "CoreGraph", (Vector2){engine.sideBarWidth + 20, 30}, 40, 4, Fade(WHITE, 0.2f));
            DrawTextEx(GetFontDefault(), "TM", (Vector2){engine.sideBarWidth + 230, 20}, 15, 1, Fade(WHITE, 0.2f));
        }

        //DrawFPS(engine.screenWidth / 2, 10);

        if (engine.showSaveWarning == 1)
        {
            engine.showSaveWarning = DrawSaveWarning(&engine, &graph);
        }
        if (engine.showSaveWarning == 2)
        {
            windowShouldClose = true;
        }

        EndDrawing();
    }

    free(projectPath);

    FreeEngineContext(&engine);
    FreeEditorContext(&editor);
    FreeGraphContext(&graph);
    FreeInterpreterContext(&interpreter);

    CloseAudioDevice();

    return 0;
}
