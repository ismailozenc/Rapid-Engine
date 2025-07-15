#include "ProjectManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>

#include "raylib.h"
#include "shell_execute.h"

void DrawMovingDotAlongRectangle()
{
    static float t = 0.0f;
    float speed = 8.0f;

    int x = 475;
    int y = 180;
    int w = 652;
    int h = 142;

    float perimeter = 2 * (w + h);

    t += speed;
    if (t > perimeter)
        t -= perimeter;

    float dotX = x;
    float dotY = y;

    if (t < w)
    {
        // Top edge
        dotX = x + t;
        dotY = y;
    }
    else if (t < w + h)
    {
        // Right edge
        dotX = x + w;
        dotY = y + (t - w);
    }
    else if (t < w + h + w)
    {
        // Bottom edge
        dotX = x + w - (t - w - h);
        dotY = y + h;
    }
    else
    {
        // Left edge
        dotX = x;
        dotY = y + h - (t - w - h - w);
    }

    DrawCircle((int)dotX, (int)dotY, 5, (Color){180, 100, 200, 255});
}

void DrawX(Vector2 center, float size, float thickness, Color color)
{
    float half = size / 2;

    Vector2 p1 = {center.x - half, center.y - half};
    Vector2 p2 = {center.x + half, center.y + half};
    Vector2 p3 = {center.x - half, center.y + half};
    Vector2 p4 = {center.x + half, center.y - half};

    DrawLineEx(p1, p2, thickness, color);
    DrawLineEx(p3, p4, thickness, color);
}

void DrawTopButtons()
{

    DrawLineEx((Vector2){0, 1}, (Vector2){1600, 1}, 4.0f, WHITE);
    DrawLineEx((Vector2){1, 0}, (Vector2){1, 1000}, 4.0f, WHITE);
    DrawLineEx((Vector2){0, 999}, (Vector2){1600, 999}, 4.0f, WHITE);
    DrawLineEx((Vector2){1599, 0}, (Vector2){1599, 1000}, 4.0f, WHITE);

    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){1550, 0, 50, 50}))
    {
        DrawRectangle(1550, 0, 50, 50, RED);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            CloseWindow();
        }
    }

    DrawX((Vector2){1575, 25}, 20, 2, WHITE);

    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){1500, 0, 50, 50}))
    {
        DrawRectangle(1500, 0, 50, 50, GRAY);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            MinimizeWindow();
        }
    }

    DrawLineEx((Vector2){1515, 25}, (Vector2){1535, 25}, 2, WHITE);
}

bool hasCursorMoved(Vector2 lastMousePos)
{
    return lastMousePos.x != GetMousePosition().x;
}

int MainWindow(Vector2 mousePoint, Font font, Font fontRE)
{
    Rectangle btnLoad = {0, 0, 798, 1000};
    Rectangle btnCreate = {802, 0, 796, 1000};

    static int isLoadBtnFocused = 0;
    static Vector2 lastMousePos;

    BeginDrawing();
    ClearBackground((Color){40, 42, 54, 255});

    if (hasCursorMoved(lastMousePos))
    {
        if (CheckCollisionPointRec(mousePoint, (Rectangle){484, 189, 632, 122}))
        {
            isLoadBtnFocused = 0;
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }
        else if (CheckCollisionPointRec(mousePoint, btnLoad))
        {
            isLoadBtnFocused = 1;
            SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        }
        else if (CheckCollisionPointRec(mousePoint, btnCreate) && !CheckCollisionPointRec(mousePoint, (Rectangle){1500, 0, 100, 50}))
        {
            isLoadBtnFocused = 2;
            SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        }
        else
        {
            isLoadBtnFocused = 0;
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }
    }
    else
    {
        if (IsKeyPressed(KEY_ENTER))
        {
            if (isLoadBtnFocused == 1)
            {
                isLoadBtnFocused = 0;
                return 1;
            }
            else if (isLoadBtnFocused == 2)
            {
                isLoadBtnFocused = 0;
                return 2;
            }
        }
        else if (IsKeyPressed(KEY_LEFT))
        {
            isLoadBtnFocused = 1;
        }
        else if (IsKeyPressed(KEY_RIGHT))
        {
            isLoadBtnFocused = 2;
        }
    }

    lastMousePos = GetMousePosition();

    if (isLoadBtnFocused == 1)
    {
        DrawRectangle(0, 0, 800, 1000, (Color){128, 128, 128, 20});
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePoint, btnLoad))
        {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            return 1;
        }
    }
    else if (isLoadBtnFocused == 2)
    {
        DrawRectangle(800, 0, 1600, 1000, (Color){128, 128, 128, 20});
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePoint, btnCreate) && !CheckCollisionPointRec(mousePoint, (Rectangle){1500, 0, 100, 50}))
        {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            return 2;
        }
    }

    DrawTopButtons();

    DrawLineEx((Vector2){800, 0}, (Vector2){800, 1000}, 4.0f, WHITE);

    DrawRectangleRounded((Rectangle){482, 187, 636, 126}, 0.6f, 16, WHITE);
    DrawRectangleRounded((Rectangle){485, 190, 630, 120}, 0.6f, 16, (Color){202, 97, 255, 255});

    DrawTextEx(fontRE, "R", (Vector2){500, 180}, 130, 0, (Color){255, 255, 255, 255});
    DrawTextEx(font, "apid Engine", (Vector2){605, 200}, 100, 0, (Color){255, 255, 255, 255});

    // DrawMovingDotAlongRectangle();

    DrawTextEx(font, "Load", (Vector2){290, 540}, 80, 0, WHITE);
    DrawTextEx(font, "project", (Vector2){260, 630}, 80, 0, WHITE);

    DrawTextEx(font, "Create", (Vector2){1080, 540}, 80, 0, WHITE);
    DrawTextEx(font, "project", (Vector2){1075, 630}, 80, 0, WHITE);

    DrawMovingDotAlongRectangle();

    return 0;
}

int WindowLoadProject(char *projectFileName, Font font)
{
    Rectangle backButton = {1, 0, 65, 1600};

    Vector2 mousePoint = GetMousePosition();

    FilePathList files = LoadDirectoryFiles("C:\\Users\\user\\Desktop\\RapidEngine\\Projects"); //////hardcoded filepath

    int yPosition = 80;

    static int selectedProject = 0;
    static bool showSelectorArrow = true;

    if (IsKeyPressed(KEY_LEFT))
    {
        return 0;
    }

    BeginDrawing();
    ClearBackground((Color){40, 42, 54, 255});

    DrawTopButtons();

    DrawRectangleRec(backButton, CLITERAL(Color){70, 70, 70, 150});

    if (CheckCollisionPointRec(mousePoint, backButton))
    {
        DrawRectangleRec(backButton, CLITERAL(Color){255, 255, 255, 50});
        DrawTextEx(font, "<", (Vector2){10, 490}, 70, 0, WHITE);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            return 0;
        }
    }
    else
    {
        DrawTextEx(font, "<", (Vector2){15, 500}, 50, 0, WHITE);
    }

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
        DrawText(fileName, 100, yPosition, 35, WHITE);
        if (CheckCollisionPointRec(mousePoint, (Rectangle){20, yPosition, MeasureText(fileName, 20), 20}) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            strcpy(projectFileName, fileName);
            return 3;
        }
        yPosition += 50;
    }

    static float blinkTimer = 0;
    blinkTimer += GetFrameTime();

    if (blinkTimer >= 0.3f) // Half a second
    {
        blinkTimer = 0;
        showSelectorArrow = !showSelectorArrow; // Toggle visibility
    }

    if (IsKeyPressed(KEY_DOWN))
    {
        if (selectedProject == files.count - 1)
        {
            selectedProject = 0;
        }
        else
        {
            selectedProject++;
        }
        showSelectorArrow = true;
        blinkTimer = 0;
    }
    if (IsKeyPressed(KEY_UP))
    {
        if (selectedProject == 0)
        {
            selectedProject = files.count - 1;
        }
        else
        {
            selectedProject--;
        }
        showSelectorArrow = true;
        blinkTimer = 0;
    }

    if (showSelectorArrow)
    {
        DrawTextEx(font, ">", (Vector2){80, 80 + selectedProject * 50}, 30, 0, (Color){202, 97, 255, 255});
    }

    if (IsKeyPressed(KEY_ENTER))
    {
        strcpy(projectFileName, GetFileName(files.paths[selectedProject]));
        return 3;
    }

    UnloadDirectoryFiles(files);

    return 1;
}

int CreateProject(ProjectOptions PO)
{
    char projectPath[512];

    sprintf(projectPath, "C:\\Users\\user\\Desktop\\RapidEngine\\Projects\\%s", PO.projectName);

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

    sprintf(mainPath, "%s\\%s.c", projectPath, PO.projectName);

    FILE *file = fopen(mainPath, "w");

    if (file == NULL)
    {
        printf("Error creating file!\n");
        return 1;
    }

    printf("File created successfully: %s\n", mainPath);

    fclose(file);

    sprintf(mainPath, "%s\\%s.cg", projectPath, PO.projectName);

    file = fopen(mainPath, "w");

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

int WindowCreateProject(char *projectFileName, Font font)
{
    Rectangle backButton = {1, 0, 65, 1600};
    static Rectangle textBox = {700, 230, 250, 40};
    static char inputText[MAX_PROJECT_NAME] = "";
    static int letterCount = 0;
    static bool isFocused = true;

    static ProjectOptions PO;

    Vector2 mousePoint = GetMousePosition();

    if (IsKeyPressed(KEY_LEFT))
    {
        return 0;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        isFocused = CheckCollisionPointRec(mousePoint, textBox);
    }

    if (isFocused)
    {
        int key = GetCharPressed();
        while (key > 0)
        {
            if ((key >= 32) && (key <= 125) && (letterCount < MAX_PROJECT_NAME))
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
    }

    BeginDrawing();
    ClearBackground((Color){40, 42, 54, 255});

    DrawTopButtons();

    DrawRectangleRec(backButton, CLITERAL(Color){70, 70, 70, 150});

    if (CheckCollisionPointRec(mousePoint, backButton))
    {
        DrawRectangleRec(backButton, CLITERAL(Color){255, 255, 255, 50});
        DrawTextEx(font, "<", (Vector2){10, 490}, 70, 0, WHITE);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            return 0;
        }
    }
    else
    {
        DrawTextEx(font, "<", (Vector2){15, 500}, 50, 0, WHITE);
    }

    DrawRectangleRounded(textBox, 2.0f, 8, LIGHTGRAY);
    DrawRectangleRoundedLinesEx(textBox, 2.0f, 8, 2, isFocused ? WHITE : DARKGRAY);
    const char *subStr;
    if (MeasureTextEx(font, inputText, 30, 0).x > textBox.width)
    {
        int len = strlen(inputText);
        for (int i = 0; i < len; i++)
        {
            subStr = &inputText[i];
            if (MeasureTextEx(font, subStr, 30, 0).x <= textBox.width - MeasureText("_", 30) - 15)
            {
                DrawTextEx(font, subStr, (Vector2){textBox.x + 5, textBox.y + 10}, 30, 0, BLACK);
                break;
            }
        }
    }
    else
    {
        DrawTextEx(font, inputText, (Vector2){textBox.x + 5, textBox.y + 10}, 30, 0, BLACK);
    }

    if (isFocused && MeasureTextEx(font, inputText, 30, 0).x < textBox.width && (GetTime() - (int)GetTime() < 0.5))
    {
        DrawText("_", textBox.x + 10 + MeasureTextEx(font, inputText, 30, 0).x, textBox.y + 10, 30, BLACK);
    }
    else if (isFocused && MeasureTextEx(font, inputText, 30, 0).x > textBox.width && (GetTime() - (int)GetTime() < 0.5))
    {
        DrawText("_", textBox.x + 10 + MeasureTextEx(font, subStr, 30, 0).x, textBox.y + 10, 30, BLACK);
    }

    DrawTextEx(font, "Enter project name:", (Vector2){textBox.x + 10, textBox.y - 30}, 25, 0, WHITE);

    DrawTextEx(font, "2D", (Vector2){700, 330}, 30, 0, WHITE);
    DrawRectangle(750, 330, 30, 30, WHITE);
    DrawRectangleLinesEx((Rectangle){750, 330, 30, 30}, 3, BLACK);
    if (!PO.is3D && CheckCollisionPointRec(mousePoint, (Rectangle){750, 330, 30, 30}))
    {
        DrawX((Vector2){765, 345}, 20, 5, (Color){202, 97, 255, 255});
    }
    else if (!PO.is3D)
    {
        DrawX((Vector2){765, 345}, 15, 3, (Color){202, 97, 255, 255});
    }
    if (CheckCollisionPointRec(mousePoint, (Rectangle){750, 330, 30, 30}))
    {
        DrawRectangle(750, 330, 30, 30, (Color){255, 255, 255, 150});
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            PO.is3D = false;
        }
    }

    DrawTextEx(font, "3D", (Vector2){840, 330}, 30, 0, WHITE);
    DrawRectangle(890, 330, 30, 30, WHITE);
    DrawRectangleLinesEx((Rectangle){890, 330, 30, 30}, 3, BLACK);
    if (PO.is3D && CheckCollisionPointRec(mousePoint, (Rectangle){890, 330, 30, 30}))
    {
        DrawX((Vector2){905, 345}, 20, 5, (Color){202, 97, 255, 255});
    }
    else if (PO.is3D)
    {
        DrawX((Vector2){905, 345}, 15, 3, (Color){202, 97, 255, 255});
    }
    if (CheckCollisionPointRec(mousePoint, (Rectangle){890, 330, 30, 30}))
    {
        DrawRectangle(890, 330, 30, 30, (Color){255, 255, 255, 150});
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            PO.is3D = true;
        }
    }

    //Temporary warning
    if(PO.is3D){
        DrawTextEx(font, "3D mode is currently unavailable", (Vector2){690, 475}, 20, 0, RED);
    }

    if (letterCount > 0 && !PO.is3D)
    {
        DrawRectangleRounded((Rectangle){700, 500, 250, 50}, 2.0f, 8, (Color){202, 97, 255, 255});
        DrawRectangleRoundedLinesEx((Rectangle){700, 500, 250, 50}, 2.0f, 8, 3, WHITE);
        DrawTextEx(font, "Create project", (Vector2){730, 507}, 32, 0, WHITE);
        if(CheckCollisionPointRec(mousePoint, (Rectangle){700, 500, 250, 50})){
            DrawRectangleRounded((Rectangle){700, 500, 250, 50}, 2.0f, 8, (Color){255, 255, 255, 150});
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                strcpy(PO.projectName, inputText);
                CreateProject(PO);
                strcpy(projectFileName, inputText);
                return 3;
            }
        }
    }
    else
    {
        DrawRectangleRounded((Rectangle){700, 500, 250, 50}, 2.0f, 8, DARKGRAY);
        DrawRectangleRoundedLinesEx((Rectangle){700, 500, 250, 50}, 2.0f, 8, 3, BLACK);
        DrawTextEx(font, "Create project", (Vector2){730, 507}, 32, 0, LIGHTGRAY);
        if(CheckCollisionPointRec(mousePoint, (Rectangle){700, 500, 250, 50}) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !PO.is3D){
            DrawRectangleRounded(textBox, 2.0f, 8, RED);
        }
    }

    return 2;
}

char *HandleProjectManager()
{
    Font font = LoadFontEx("fonts/arialbd.ttf", 256, NULL, 0);
    Font fontRE = LoadFontEx("fonts/sonsie.ttf", 256, NULL, 0);

    int windowMode = 0;
    char *projectFileName = malloc(MAX_PROJECT_NAME * sizeof(char));
    projectFileName[0] = '\0';

    while (1)
    {
        Vector2 mousePoint = GetMousePosition();

        if (windowMode == 0)
        {
            windowMode = MainWindow(mousePoint, font, fontRE);
        }
        else if (windowMode == 1)
        {
            windowMode = WindowLoadProject(projectFileName, font);
        }

        else if (windowMode == 2)
        {
            windowMode = WindowCreateProject(projectFileName, font);
        }

        else if (windowMode == 3)
        {
            UnloadFont(font);
            UnloadFont(fontRE);

            return projectFileName;
        }

        EndDrawing();
    }
}