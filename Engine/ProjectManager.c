#include "raylib.h"
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include "shell_execute.h"

void DrawMovingDotAlongRectangle() {
    static float t = 0.0f;
    float speed = 8.0f;

    int x = 475; 
    int y = 180;
    int w = 652;
    int h = 142;

    float perimeter = 2 * (w + h);

    t += speed;
    if (t > perimeter) t -= perimeter;

    float dotX = x;
    float dotY = y;

    if (t < w) {
        // Top edge
        dotX = x + t;
        dotY = y;
    } else if (t < w + h) {
        // Right edge
        dotX = x + w;
        dotY = y + (t - w);
    } else if (t < w + h + w) {
        // Bottom edge
        dotX = x + w - (t - w - h);
        dotY = y + h;
    } else {
        // Left edge
        dotX = x;
        dotY = y + h - (t - w - h - w);
    }

    DrawCircle((int)dotX, (int)dotY, 5, (Color){180, 100, 200, 255});
}

int MainWindow(Vector2 mousePoint, Font font, Font fontRE)
{
    Rectangle btnLoad = {0, 0, 798, 1000};
    Rectangle btnCreate = {802, 0, 796, 1000};

    BeginDrawing();
    ClearBackground((Color){40, 42, 54, 255});

    if(CheckCollisionPointRec(mousePoint, (Rectangle){484, 189, 632, 122})){
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }
    else if (CheckCollisionPointRec(mousePoint, btnLoad))
    {
        DrawRectangle(0, 0, 800, 1000, (Color){128, 128, 128, 20});
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            TraceLog(LOG_INFO, "Load Button Clicked!");
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            return 2;
        }
    }
    else if (CheckCollisionPointRec(mousePoint, btnCreate))
    {
        DrawRectangle(800, 0, 1600, 1000, (Color){128, 128, 128, 20});
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            TraceLog(LOG_INFO, "Create Button Clicked!");
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            return 1;
        }
    }
    else{
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    DrawLine(800, 0, 800, 1000, WHITE);

    DrawRectangleRounded((Rectangle){483, 188, 634, 124}, 0.6f, 16, WHITE);
    DrawRectangleRounded((Rectangle){485, 190, 630, 120}, 0.6f, 16, (Color){180, 100, 200, 255});

    DrawTextEx(fontRE, "R", (Vector2){500, 180}, 130, 0, (Color){255, 255, 255, 255});
    DrawTextEx(font, "apid Engine", (Vector2){605, 200}, 100, 0, (Color){255, 255, 255, 255});

    //DrawMovingDotAlongRectangle();

    DrawTextEx(font, "Load", (Vector2){290, 540}, 80, 0, WHITE);
    DrawTextEx(font, "project", (Vector2){260, 630}, 80, 0, WHITE);

    DrawTextEx(font, "Create", (Vector2){1080, 540}, 80, 0, WHITE);
    DrawTextEx(font, "project", (Vector2){1075, 630}, 80, 0, WHITE);

    DrawMovingDotAlongRectangle();

    return 0;
}

int CreateProject(char *inputText)
{
    char projectPath[512];

    sprintf(projectPath, "C:\\Users\\user\\Desktop\\RapidEngine\\Projects\\%s", inputText);

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
    ClearBackground((Color){40, 42, 54, 255});

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

    FilePathList files = LoadDirectoryFiles("C:\\Users\\user\\Desktop\\RapidEngine\\Projects"); //////hardcoded filepath

    int yPosition = 80;

    BeginDrawing();
    ClearBackground((Color){40, 42, 54, 255});
    
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
    SetTargetFPS(100);

    Font font = LoadFontEx("fonts/arialbd.ttf", 128, NULL, 0);
    Font fontRE = LoadFontEx("fonts/sonsie.ttf", 256, NULL, 0);

    int windowMode = 0;
    char *projectFileName;

    while (!WindowShouldClose())
    {
        Vector2 mousePoint = GetMousePosition();

        if (windowMode == 0)
        {
            windowMode = MainWindow(mousePoint, font, fontRE);
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