#include <stdio.h>
#include <raylib.h>
#include <direct.h>
#include <stdlib.h>
#include <time.h>
#include "shell_execute.h"
#include "CGEditor.h"
#include "ProjectManager.h"

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
    bool isDraggingResizeButton;
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

    engine.sideBarMiddleY = (engine.screenHeight - engine.bottomBarHeight) / 2;

    engine.mousePos = GetMousePosition();

    engine.viewport = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    engine.UI = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    engine.resizeButton = LoadTexture("resize_btn2.png");
    if (engine.UI.id == 0 || engine.viewport.id == 0 || engine.resizeButton.id == 0)
    {
        AddToLog(&engine, "Couldn't load textures", 2);
        EmergencyExit(&engine);
    }

    engine.delayFrames = true;
    engine.isDraggingResizeButton = false;

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

void LoadFiles(FilePathList *files, int screenHeight, int screenWidth, int bottomBarHeight, char *projectPath, Font font)
{

    int xOffset = 50;
    int yOffset = screenHeight - bottomBarHeight + 70;
    Vector2 mousePos = GetMousePosition();
    bool showTooltip = false;
    char tooltipText[256] = "";
    Rectangle tooltipRect = {0};
    char *currentPath = projectPath;

    Rectangle backButton = {30, screenHeight - bottomBarHeight + 10, 65, 30};
    DrawRectangleRec(backButton, CLITERAL(Color){70, 70, 70, 150});
    DrawTextEx(font, "Back", (Vector2){backButton.x + 5, backButton.y + 2}, 25, 0, WHITE);

    Rectangle refreshButton = {110, screenHeight - bottomBarHeight + 10, 100, 30};
    DrawRectangleRec(refreshButton, CLITERAL(Color){70, 70, 70, 150});
    DrawTextEx(font, "Refresh", (Vector2){refreshButton.x + 9, refreshButton.y + 2}, 25, 0, WHITE);

    DrawTextEx(font, currentPath, (Vector2){230, screenHeight - bottomBarHeight + 15}, 22, 2, WHITE);

    if (CheckCollisionPointRec(mousePos, backButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        char *lastSlash = strrchr(currentPath, '\\');

        if (lastSlash != NULL)
        {
            *lastSlash = '\0';
        }

        RefreshBottomBar(files, currentPath);
    }

    if (CheckCollisionPointRec(mousePos, refreshButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        RefreshBottomBar(files, currentPath);
    }

    if (CheckCollisionPointRec(mousePos, backButton))
    {
        DrawRectangleRec(backButton, CLITERAL(Color){255, 255, 255, 100});
    }

    if (CheckCollisionPointRec(mousePos, refreshButton))
    {
        DrawRectangleRec(refreshButton, CLITERAL(Color){255, 255, 255, 100});
    }

    for (int i = 0; i < (*files).count; i++)
    {
        const char *fileName = GetFileName((*files).paths[i]);
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

        DrawRectangleRounded(fileRect, 0.5f, 8, fileColor);

        bool isHovered = CheckCollisionPointRec(mousePos, fileRect);

        if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            double currentTime = GetTime();

            if (currentTime - lastClickTime <= doubleClickThreshold)
            {
                if (GetFileType(fileName) == 2)
                {
                    char *buff = malloc(sizeof(fileName));
                    strcpy(buff, fileName);
                    buff[strlen(fileName) - 3] = '\0';
                    strcpy(openedFileName, buff);
                    free(buff);
                    isEditorOpened = true;
                }
                else if (GetFileType(fileName) != 0)
                {
                    char command[512];
                    snprintf(command, sizeof(command), "start \"\" \"%s\"", (*files).paths[i]);
                    system(command);
                }
                else
                {
                    strcat(currentPath, "\\");
                    strcat(currentPath, fileName);
                    RefreshBottomBar(files, currentPath);
                }
            }

            lastClickTime = currentTime;
        }

        char buff[256];
        strcpy(buff, fileName);

        if (MeasureTextEx(font, fileName, 25, 0).x > 135)
        {
            for (int i = sizeof(buff); i > 0; i--)
            {
                buff[i] = '\0';

                if (MeasureText(buff, 25) < 135)
                {
                    buff[i - 2] = '\0';
                    strcat(buff, "...");
                    break;
                }
            }
        }

        DrawTextEx(font, buff, (Vector2){xOffset + 10, yOffset + 16}, 25, 0, BLACK);

        if (isHovered)
        {
            DrawRectangleRounded(fileRect, 0.5f, 8, CLITERAL(Color){255, 255, 255, 100});

            snprintf(tooltipText, sizeof(tooltipText), "File: %s\nSize: %ld bytes", fileName, GetFileLength((*files).paths[i]));
            tooltipRect = (Rectangle){xOffset, yOffset - 61, MeasureTextEx(font, tooltipText, 20, 0).x + 20, 60};
            showTooltip = true;
        }

        xOffset += 250;

        if (xOffset + 100 >= screenWidth)
        {
            xOffset = 50;
            yOffset += 120;
        }
    }

    if (showTooltip)
    {
        DrawRectangleRec(tooltipRect, DARKGRAY);
        DrawTextEx(font, tooltipText, (Vector2){tooltipRect.x + 10, tooltipRect.y + 10}, 20, 0, WHITE);
    }
}

void BuildUITexture(int screenWidth, int screenHeight, int sideBarWidth, int bottomBarHeight, FilePathList *files, char *projectPath, EngineContext *engine, GraphContext *graph, char *CGFilePath)
{
    BeginTextureMode(engine->UI);
    ClearBackground((Color){255, 255, 255, 0});

    Color BottomBarColor = {28, 28, 28, 255};
    Color VariablesBarColor = {28, 28, 28, 255};
    Color LogColor = {15, 15, 15, 255};

    if (screenWidth > screenHeight && screenWidth > 1000)
    {
        DrawRectangle(0, 0, sideBarWidth, engine->sideBarMiddleY, VariablesBarColor);
        DrawRectangle(0, engine->sideBarMiddleY, sideBarWidth, screenHeight - bottomBarHeight, LogColor);
        DrawLineEx((Vector2){sideBarWidth, 0}, (Vector2){sideBarWidth, screenHeight - bottomBarHeight}, 2, WHITE);

        DrawLineEx((Vector2){0, engine->sideBarMiddleY}, (Vector2){sideBarWidth, engine->sideBarMiddleY}, 2, WHITE);
        DrawTexture(engine->resizeButton, sideBarWidth / 2 - 10, engine->sideBarMiddleY - 10, WHITE);
        engine->mousePos = GetMousePosition();
        if (CheckCollisionPointCircle(engine->mousePos, (Vector2){sideBarWidth / 2, engine->sideBarMiddleY}, 10))
        {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
            {
                engine->isDraggingResizeButton = true;
                SetMouseCursor(MOUSE_CURSOR_RESIZE_NS);
            }
        }
        if (engine->isDraggingResizeButton)
        {
            engine->sideBarMiddleY += GetMouseDelta().y;
            if (IsMouseButtonUp(MOUSE_LEFT_BUTTON))
            {
                engine->isDraggingResizeButton = false;
                SetMouseCursor(MOUSE_CURSOR_ARROW);
            }
        }

        DrawRectangleRounded((Rectangle){sideBarWidth - 140, engine->sideBarMiddleY + 15, 60, 30}, 0.2f, 8, CLITERAL(Color){255, 255, 255, 50});
        DrawTextEx(engine->font, "Save", (Vector2){sideBarWidth - 135, engine->sideBarMiddleY + 20}, 20, 2, WHITE);
        if (CheckCollisionPointRec(engine->mousePos, (Rectangle){sideBarWidth - 140, engine->sideBarMiddleY + 15, 60, 30}))
        {
            DrawRectangleRounded((Rectangle){sideBarWidth - 140, engine->sideBarMiddleY + 15, 60, 30}, 0.2f, 8, Fade(WHITE, 0.6f));
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if (SaveGraphToFile(CGFilePath, graph) == 0)
                {
                    AddToLog(engine, "Saved successfully!", 0);
                }
                else
                {
                    AddToLog(engine, "ERROR SAVING CHANGES!", 1);
                }
            }
        }

        DrawRectangleRounded((Rectangle){sideBarWidth - 70, engine->sideBarMiddleY + 15, 60, 30}, 0.2f, 8, CLITERAL(Color){255, 255, 255, 50});
        DrawTextEx(engine->font, "Run", (Vector2){sideBarWidth - 60, engine->sideBarMiddleY + 20}, 20, 2, WHITE);
        if (CheckCollisionPointRec(engine->mousePos, (Rectangle){sideBarWidth - 70, engine->sideBarMiddleY + 15, 60, 30}))
        {
            DrawRectangleRounded((Rectangle){sideBarWidth - 70, engine->sideBarMiddleY + 15, 60, 30}, 0.2f, 8, Fade(WHITE, 0.6f));
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                // Interpret CG
                AddToLog(engine, "Interpreter not ready", 2);
            }
            SetMouseCursor(MOUSE_CURSOR_NOT_ALLOWED);
        }
        else{
            SetMouseCursor(MOUSE_CURSOR_ARROW);
        }

        int y = screenHeight - bottomBarHeight - 30;

        Color messageColor;

        for (int i = engine->logs.count - 1; i >= 0 && y > engine->sideBarMiddleY + 50; i--)
        {
            messageColor = (engine->logs.entries[i].level == 0) ? WHITE : (engine->logs.entries[i].level == 1) ? YELLOW
                                                                                                               : RED;

            DrawTextEx(engine->font, engine->logs.entries[i].message, (Vector2){20, y}, 20, 2, messageColor);
            y -= 25;
        }
    }

    DrawRectangle(0, screenHeight - bottomBarHeight, screenWidth, bottomBarHeight, BottomBarColor);
    DrawLineEx((Vector2){0, screenHeight - bottomBarHeight}, (Vector2){screenWidth, screenHeight - bottomBarHeight}, 2, WHITE);

    LoadFiles(files, screenHeight, screenWidth, bottomBarHeight, projectPath, engine->font);

    DrawTopBar();

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

    if (CheckCollisionPointRec(mousePos, backButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        return 1;
    }

    if (CheckCollisionPointRec(mousePos, refreshButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
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

    if (CheckCollisionPointCircle(engine->mousePos, (Vector2){engine->sideBarWidth / 2, engine->sideBarMiddleY}, 10) || engine->isDraggingResizeButton)
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
        engine.sideBarWidth = engine.screenWidth * 0.2;
        engine.bottomBarHeight = engine.screenHeight * 0.25;

        int collisionResult = CheckCollisions(&engine, engine.files.count, projectPath, editor.CGFilePath, &graph);

        if (collisionResult == 1 || prevScreenWidth != engine.screenWidth || prevScreenHeight != engine.screenHeight)
        {
            BuildUITexture(engine.screenWidth, engine.screenHeight, engine.sideBarWidth, engine.bottomBarHeight, &engine.files, projectPath, &engine, &graph, editor.CGFilePath);
            engine.delayFrames = true;
            prevScreenWidth = engine.screenWidth;
            prevScreenHeight = engine.screenHeight;
            SetTargetFPS(140);
        }
        else if (engine.delayFrames)
        {
            BuildUITexture(engine.screenWidth, engine.screenHeight, engine.sideBarWidth, engine.bottomBarHeight, &engine.files, projectPath, &engine, &graph, editor.CGFilePath);
            engine.delayFrames = false;
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
