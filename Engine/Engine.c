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

void EmergencyExit(EditorContext *EC)
{
    // write logs to file

    exit(1);
}

void AddToLog(EditorContext *EC, const char *newLine)
{
    if (EC->logCount >= EC->logCapacity)
    {
        EC->logCapacity += 100;
        EC->logMessages = realloc(EC->logMessages, sizeof(char *) * EC->logCapacity);
    }

    time_t timestamp = time(NULL);
    struct tm *tm_info = localtime(&timestamp);

    int hour = tm_info->tm_hour;
    int minute = tm_info->tm_min;
    int second = tm_info->tm_sec;

    size_t lineLength = strlen(newLine);
    char *message = malloc(16 + lineLength + 6);

    snprintf(message, 16 + lineLength + 6, "%02d:%02d:%02d %s", hour, minute, second, newLine);

    EC->logMessages[EC->logCount] = malloc(strlen(message) + 1);
    strcpy(EC->logMessages[EC->logCount], message);

    EC->logCount++;
    free(message);

    EC->engineDelayFrames = true;
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

void LoadFiles(FilePathList *files, int screenHeight, int screenWidth, int bottomBarHeight, char *projectPath, Font font, EditorContext *EC)
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
            fileColor = YELLOW;
            break;
        case 1:
            fileColor = BLUE;
            break;
        case 2:
            fileColor = GREEN;
            break;
        case 3:
            fileColor = WHITE;
            break;
        case 4:
            fileColor = RED;
            break;
        default:
            fileColor = GRAY;
            break;
        }

        DrawRectangleRec(fileRect, fileColor);

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

        DrawTextEx(font, buff, (Vector2){xOffset + 10, yOffset + 15}, 25, 0, BLACK);

        if (isHovered)
        {
            DrawRectangleRec(fileRect, CLITERAL(Color){255, 255, 255, 100});

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

void BuildUITexture(int screenWidth, int screenHeight, int sideBarWidth, int bottomBarHeight, FilePathList *files, char *projectPath, RenderTexture2D *UI, Font font, EditorContext *EC)
{
    BeginTextureMode(*UI);
    ClearBackground((Color){255, 255, 255, 0});

    Color BottomBarColor = {28, 28, 28, 255};
    Color VariablesBarColor = {50, 50, 50, 255};
    Color LogColor = {15, 15, 15, 255};

    if (screenWidth > screenHeight && screenWidth > 1000)
    {
        int sideBarMiddleY = (screenHeight - bottomBarHeight) / 2;

        DrawRectangle(0, 0, sideBarWidth, sideBarMiddleY, VariablesBarColor);
        DrawRectangle(0, sideBarMiddleY, sideBarWidth, screenHeight - bottomBarHeight, LogColor);
        DrawLineEx((Vector2){sideBarWidth, 0}, (Vector2){sideBarWidth, screenHeight - bottomBarHeight}, 2, WHITE);

        DrawLineEx((Vector2){0, sideBarMiddleY}, (Vector2){sideBarWidth, sideBarMiddleY}, 2, WHITE);

        int y = screenHeight - bottomBarHeight - 25;

        for (int i = EC->logCount - 1; i >= 0 && y > (screenHeight - bottomBarHeight) / 2; i--)
        {
            DrawTextEx(EC->font, EC->logMessages[i], (Vector2){20, y}, 20, 2, WHITE);
            y -= 25;
        }
    }

    DrawRectangle(0, screenHeight - bottomBarHeight, screenWidth, bottomBarHeight, BottomBarColor);
    DrawLineEx((Vector2){0, screenHeight - bottomBarHeight}, (Vector2){screenWidth, screenHeight - bottomBarHeight}, 2, WHITE);

    LoadFiles(files, screenHeight, screenWidth, bottomBarHeight, projectPath, font, EC);

    DrawTopBar();

    EndTextureMode();
}

int CheckCollisions(int fileCount, int screenHeight, int screenWidth, int bottomBarHeight, char *projectPath)
{
    int xOffset = 50;
    int yOffset = screenHeight - bottomBarHeight + 70;
    Vector2 mousePos = GetMousePosition();
    Rectangle tooltipRect = {0};
    char *currentPath = projectPath;

    Rectangle backButton = {30, screenHeight - bottomBarHeight + 10, 65, 30};

    Rectangle refreshButton = {110, screenHeight - bottomBarHeight + 10, 100, 30};

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

    for (int i = 0; i < fileCount; i++)
    {
        Rectangle fileRect = {xOffset, yOffset, 150, 60};

        if (CheckCollisionPointRec(mousePos, fileRect))
        {
            return 1;
        }

        xOffset += 250;

        if (xOffset + 100 >= screenWidth)
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
    SetTargetFPS(240);

    EditorContext EC = InitEditorContext();
    GraphContext graph = InitGraphContext();

    AddToLog(&EC, "Initialized window");

    if (EC.font.texture.id == 0)
    {
        AddToLog(&EC, "Couldn't load font");
    }

    char *projectPath = PrepareProjectPath(/*handleProjectManager()*/ "Tetris"); // temporary hardcode

    if (projectPath == NULL || projectPath[0] == '\0')
    {
        AddToLog(&EC, "Couldn't find file");
        EmergencyExit(&EC);
    }

    MaximizeWindow();

    FilePathList files = LoadDirectoryFilesEx(projectPath, NULL, false);

    RenderTexture2D UI = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

    RenderTexture2D viewport = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

    if (UI.id == 0 || viewport.id == 0)
    {
        AddToLog(&EC, "Couldn't load textures");
        EmergencyExit(&EC);
    }

    AddToLog(&EC, "All resources loaded");

    int mode = 1;
    bool editorInitialized = false;

    int prevScreenWidth = GetScreenWidth();
    int prevScreenHeight = GetScreenHeight();

    AddToLog(&EC, "Welcome!");

    while (!WindowShouldClose())
    {
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        int sideBarWidth = screenWidth * 0.2;
        int bottomBarHeight = screenHeight * 0.25;

        int collisionResult = CheckCollisions(files.count, screenHeight, screenWidth, bottomBarHeight, projectPath);

        if (collisionResult == 1 || prevScreenWidth != screenWidth || prevScreenHeight != screenHeight)
        {
            BuildUITexture(screenWidth, screenHeight, sideBarWidth, bottomBarHeight, &files, projectPath, &UI, EC.font, &EC);
            EC.engineDelayFrames = true;
            prevScreenWidth = screenWidth;
            prevScreenHeight = screenHeight;
        }
        else if (EC.engineDelayFrames)
        {
            BuildUITexture(screenWidth, screenHeight, sideBarWidth, bottomBarHeight, &files, projectPath, &UI, EC.font, &EC);
            EC.engineDelayFrames = false;
        }

        BeginDrawing();
        ClearBackground(BLACK);

        DrawTextureRec(viewport.texture, (Rectangle){0, 0, viewport.texture.width, -viewport.texture.height}, (Vector2){0, 0}, WHITE);

        DrawTextureRec(UI.texture, (Rectangle){0, 0, UI.texture.width, -UI.texture.height}, (Vector2){0, 0}, WHITE);

        if (!isEditorOpened)
        {
            BeginTextureMode(viewport);
            ClearBackground(BLACK);
            EndTextureMode();
        }
        else if (isEditorOpened)
        {
            if (strcmp(openedFileName, EC.fileName) != 0)
            {
                FreeEditorContext(&EC);
                FreeGraphContext(&graph);

                EC = InitEditorContext();
                graph = InitGraphContext();

                SetProjectPaths(&EC, openedFileName);

                LoadGraphFromFile(EC.CGFilePath, &graph);

                strcpy(EC.fileName, openedFileName);

                DrawFullTexture(&EC, &graph, viewport);
            }

            handleEditor(&EC, &graph, &viewport);
        }

        EndDrawing();
    }

    free(projectPath);

    UnloadDirectoryFiles(files);

    UnloadRenderTexture(UI);
    UnloadRenderTexture(viewport);

    UnloadFont(EC.font);

    FreeEditorContext(&EC);
    FreeGraphContext(&graph);

    return 0;
}
