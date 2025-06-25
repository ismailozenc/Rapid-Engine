#include <stdio.h>
#include <raylib.h>
#include <direct.h>
#include <stdlib.h>
#include <time.h>
#include "shell_execute.h"
#include "CGEditor.h"

double lastClickTime = 0;
const double doubleClickThreshold = 0.5;

char openedFileName[32] = "Game";
bool isEditorOpened = false;

char *PrepareProjectPath(int argc, char *argv[])
{
    char *projectPath = malloc(260);
    if (!projectPath)
        return NULL;

    getcwd(projectPath, 260);

    projectPath[strlen(projectPath) - 7] = '\0';

    strcat(projectPath, "\\Projects\\");
    strcat(projectPath, argv[1]);

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

void BuildUITexture(int screenWidth, int screenHeight, int sideBarWidth, int bottomBarHeight, FilePathList *files, char *projectPath, RenderTexture2D UI, Font font, EditorContext *EC)
{
    BeginTextureMode(UI);

    Color BottomBarColor = {28, 28, 28, 255};
    Color LeftSideBarColor = {50, 50, 50, 255};

    if (screenWidth > screenHeight)
    {
        DrawRectangle(0, 0, sideBarWidth, screenHeight - bottomBarHeight, LeftSideBarColor);
        DrawLineEx((Vector2){sideBarWidth, 0}, (Vector2){sideBarWidth, screenHeight - bottomBarHeight}, 2, WHITE);
    }

    DrawRectangle(0, screenHeight - bottomBarHeight, screenWidth, bottomBarHeight, BottomBarColor);
    DrawLineEx((Vector2){0, screenHeight - bottomBarHeight}, (Vector2){screenWidth, screenHeight - bottomBarHeight}, 2, WHITE);

    LoadFiles(files, screenHeight, screenWidth, bottomBarHeight, projectPath, font, EC);

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

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        ShellExecuteA(NULL, "open", "ProjectManager.exe", NULL, NULL, SW_SHOWNORMAL);
        return 1;
    }

    char *projectPath = PrepareProjectPath(argc, argv);

    FilePathList files = LoadDirectoryFilesEx(projectPath, NULL, false);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1600, 1000, "RapidEngine");
    MaximizeWindow();
    SetTargetFPS(240);

    RenderTexture2D UI = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

    RenderTexture2D viewport = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    int mode = 1;
    bool editorInitialized = false;

    bool isFirstFrame = true;
    int refreshDelayFrames = 0;
    int prevScreenWidth = GetScreenWidth();
    int prevScreenHeight = GetScreenHeight();

    Font font = LoadFontEx("fonts/arialbd.ttf", 64, NULL, 0);

    EditorContext EC = InitEditorContext();
    GraphContext graph = InitGraphContext();
    ;

    while (!WindowShouldClose())
    {
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        int sideBarWidth = screenWidth * 0.2;
        int bottomBarHeight = screenHeight * 0.25;

        int collisionResult = CheckCollisions(files.count, screenHeight, screenWidth, bottomBarHeight, projectPath);

        if (collisionResult == 1 || prevScreenWidth != screenWidth || prevScreenHeight != screenHeight)
        {
            BuildUITexture(screenWidth, screenHeight, sideBarWidth, bottomBarHeight, &files, projectPath, UI, font, &EC);
            refreshDelayFrames = 1;
            prevScreenWidth = screenWidth;
            prevScreenHeight = screenHeight;
        }
        else if (refreshDelayFrames > 0)
        {
            BuildUITexture(screenWidth, screenHeight, sideBarWidth, bottomBarHeight, &files, projectPath, UI, font, &EC);
            refreshDelayFrames--;
        }
        else if (isFirstFrame)
        {
            BuildUITexture(screenWidth, screenHeight, sideBarWidth, bottomBarHeight, &files, projectPath, UI, font, &EC);
            isFirstFrame = false;
        }

        BeginDrawing();
        ClearBackground(BLACK);

        DrawFPS(10, 10);

        DrawTextureRec(viewport.texture, (Rectangle){0, 0, UI.texture.width, -UI.texture.height}, (Vector2){0, 0}, WHITE);

        DrawTextureRec(UI.texture, (Rectangle){0, 0, UI.texture.width, -UI.texture.height}, (Vector2){0, 0}, WHITE);

        // DrawTextureRec(viewport.texture, (Rectangle){sideBarWidth, 0, screenWidth - sideBarWidth, -(screenHeight - bottomBarHeight)}, (Vector2){sideBarWidth, 0}, WHITE);

        if (!isEditorOpened)
        {
            BeginTextureMode(viewport);
            // ClearBackground(BLACK);
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
            }

            handleEditor(&EC, &graph, &viewport);
        }

        EndDrawing();
    }

    UnloadDirectoryFiles(files);

    UnloadRenderTexture(UI);
    UnloadRenderTexture(viewport);

    UnloadFont(font);
    UnloadFont(EC.font);

    FreeEditorContext(&EC);
    FreeGraphContext(&graph);

    return 0;
}
