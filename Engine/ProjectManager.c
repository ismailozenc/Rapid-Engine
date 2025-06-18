#include "raylib.h"
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include "shell_execute.h"

int MainWindow(Vector2 mousePoint)
{
    Rectangle btnLoad = {200, 300, 500, 200};
    Color btnLoadColor = DARKGRAY;

    Rectangle btnCreate = {900, 300, 500, 200};
    Color btnCreateColor = DARKGRAY;

    if (CheckCollisionPointRec(mousePoint, btnLoad) || CheckCollisionPointRec(mousePoint, btnCreate))
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    }
    else
    {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    if (CheckCollisionPointRec(mousePoint, btnLoad))
    {
        btnLoadColor = GRAY;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            TraceLog(LOG_INFO, "Load Button Clicked!");
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            return 2;
        }
    }
    else
    {
        btnLoadColor = DARKGRAY;
    }

    if (CheckCollisionPointRec(mousePoint, btnCreate))
    {
        btnCreateColor = GRAY;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            TraceLog(LOG_INFO, "Create Button Clicked!");
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            return 1;
        }
    }
    else
    {
        btnCreateColor = DARKGRAY;
    }

    BeginDrawing();
    ClearBackground(BLACK);

    DrawRectangleRec(btnLoad, btnLoadColor);
    DrawRectangleLinesEx(btnLoad, 2, BLACK);
    DrawText("Load project", btnLoad.x + 120, btnLoad.y + 80, 40, WHITE);

    DrawRectangleRec(btnCreate, btnCreateColor);
    DrawRectangleLinesEx(btnCreate, 2, BLACK);
    DrawText("Create project", btnCreate.x + 120, btnCreate.y + 80, 40, WHITE);

    return 0;
}

int CreateProject(char *inputText)
{
    char projectPath[512];

    sprintf(projectPath, "C:\\Users\\user\\Desktop\\GameEngine\\Projects\\%s", inputText);

    if (_mkdir(projectPath) == 0)
    {
        printf("Project folder created: %s\n", projectPath);
    }
    else
    {
        printf("Failed to create folder: %s\n", projectPath);
        return -1;
    }

    char mainPath[512];

    sprintf(mainPath, "%s\\%s.c", projectPath, inputText);

    FILE *file = fopen(mainPath, "w");

    if (file == NULL)
    {
        printf("Error creating file!\n");
        return 1;
    }

    printf("File created successfully: %s\n", mainPath);

    fclose(file);

    char foldersPath[512];

    sprintf(foldersPath, "%s\\Assets", projectPath);

    if (_mkdir(foldersPath) == 0)
    {
        printf("Project folder created: %s\n", foldersPath);
    }
    else
    {
        printf("Failed to create folder: %s\n", foldersPath);
        return -1;
    }
}

int WindowCreateProject(char *projectFileName)
{
    Rectangle backButton = {20, 20, 65, 30};
    static Rectangle textBox = {700, 230, 250, 40};
    static char inputText[99] = "";
    static int letterCount = 0;
    static bool isFocused = true;

    Vector2 mousePoint = GetMousePosition();

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        isFocused = CheckCollisionPointRec(mousePoint, textBox);
    }

    if (isFocused)
    {
        int key = GetCharPressed();
        while (key > 0)
        {
            if ((key >= 32) && (key <= 125) && (letterCount < 99))
            {
                inputText[letterCount] = (char)key;
                letterCount++;
                inputText[letterCount] = '\0';
            }
            key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE) && letterCount > 0)
        {
            letterCount--;
            inputText[letterCount] = '\0';
        }

        if (IsKeyPressed(KEY_ENTER) && letterCount > 0)
        {
            CreateProject(inputText);
            strcpy(projectFileName, inputText);
            return 3;
        }
    }

    BeginDrawing();
    ClearBackground(BLACK);

    DrawRectangleRec(backButton, CLITERAL(Color){70, 70, 70, 150});
    DrawText("Back", backButton.x + 8, backButton.y + 5, 20, WHITE);

    if (CheckCollisionPointRec(mousePoint, backButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        return 0;
    }

    if (CheckCollisionPointRec(mousePoint, backButton))
    {
        DrawRectangleRec(backButton, CLITERAL(Color){255, 255, 255, 100});
    }

    DrawRectangleRec(textBox, LIGHTGRAY);
    DrawRectangleLinesEx(textBox, 2, isFocused ? WHITE : DARKGRAY);
    DrawText(inputText, textBox.x + 5, textBox.y + 10, 30, BLACK);

    if (isFocused && (letterCount < 99) && (GetTime() - (int)GetTime() < 0.5))
    {
        DrawText("_", textBox.x + 5 + MeasureText(inputText, 30), textBox.y + 10, 30, BLACK);
    }

    DrawText("Enter project name...", textBox.x, textBox.y - 25, 20, DARKGRAY);

    return 1;
}

int WindowLoadProject(char *projectFileName)
{
    Rectangle backButton = {20, 20, 65, 30};

    Vector2 mousePoint = GetMousePosition();

    FilePathList files = LoadDirectoryFiles("C:\\Users\\user\\Desktop\\GameEngine\\Projects"); //////hardcoded filepath

    int yPosition = 80;

    BeginDrawing();
    ClearBackground(BLACK);
    
    DrawRectangleRec(backButton, CLITERAL(Color){70, 70, 70, 150});
    DrawText("Back", backButton.x + 8, backButton.y + 5, 20, WHITE);

    if (CheckCollisionPointRec(mousePoint, backButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        return 0;
    }

    if (CheckCollisionPointRec(mousePoint, backButton))
    {
        DrawRectangleRec(backButton, CLITERAL(Color){255, 255, 255, 100});
    }

    for (int i = 0; i < files.count; i++)
    {
        const char *fileName = GetFileName(files.paths[i]);
        DrawText(fileName, 20, yPosition, 35, WHITE);
        if (CheckCollisionPointRec(mousePoint, (Rectangle){20, yPosition, MeasureText(fileName, 20), 20}) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            strcpy(projectFileName, fileName);
            return 3;
        }
        yPosition += 50;
    }

    UnloadDirectoryFiles(files);

    return 2;
}

void EndProjectManager(char *projectFileName)
{
    EndDrawing;
    CloseWindow();
    ShellExecuteA(NULL, "open", "Engine.exe", projectFileName, NULL, SW_SHOWNORMAL);
}

int main(void)
{
    InitWindow(1600, 1000, "Project manager");
    SetTargetFPS(30);

    int windowMode = 0;
    char *projectFileName;

    while (!WindowShouldClose())
    {
        Vector2 mousePoint = GetMousePosition();

        if (windowMode == 0)
        {
            windowMode = MainWindow(mousePoint);
        }
        else if (windowMode == 1)
        {
            windowMode = WindowCreateProject(projectFileName);
        }

        else if (windowMode == 2)
        {
            windowMode = WindowLoadProject(projectFileName);
        }

        else if (windowMode == 3)
        {
            EndProjectManager(projectFileName);
            return 0;
        }

        EndDrawing();
    }

    return 0;
}