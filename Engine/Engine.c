#include <stdio.h>
#include <raylib.h>
#include <stdlib.h>
#include <time.h>
#include "CGEditor.h"
#include "ProjectManager.h"
#include "Engine.h"
#include "Interpreter.h"
#include "HitboxEditor.h"

bool STRING_ALLOCATION_FAILURE = false;

Logs InitLogs()
{
    Logs logs;
    logs.count = 0;
    logs.capacity = 100;
    logs.entries = malloc(sizeof(LogEntry) * logs.capacity);
    return logs;
}

void AddToLog(EngineContext *eng, const char *newLine, int level);

void EmergencyExit(EngineContext *eng, CGEditorContext *cgEd, InterpreterContext *intp);

EngineContext InitEngineContext()
{
    EngineContext eng = {0};

    eng.logs = InitLogs();

    eng.screenWidth = GetScreenWidth();
    eng.screenHeight = GetScreenHeight();
    eng.sideBarWidth = eng.screenWidth * 0.2;
    eng.bottomBarHeight = eng.screenHeight * 0.25;

    eng.prevScreenWidth = eng.screenWidth;
    eng.prevScreenHeight = eng.screenHeight;

    eng.viewportWidth = eng.screenWidth - eng.sideBarWidth;
    eng.viewportHeight = eng.screenHeight - eng.bottomBarHeight;

    eng.sideBarMiddleY = (eng.screenHeight - eng.bottomBarHeight) / 2 + 20;

    eng.mousePos = GetMousePosition();

    eng.viewportTex = LoadRenderTexture(eng.screenWidth * 2, eng.screenHeight * 2);
    eng.uiTex = LoadRenderTexture(eng.screenWidth, eng.screenHeight);
    Image tempImg;
    tempImg = LoadImageFromMemory(".png", resize_btn_png, resize_btn_png_len);
    eng.resizeButton = LoadTextureFromImage(tempImg);
    UnloadImage(tempImg);
    tempImg = LoadImageFromMemory(".png", viewport_fullscreen_png, viewport_fullscreen_png_len);
    eng.viewportFullscreenButton = LoadTextureFromImage(tempImg);
    UnloadImage(tempImg);
    tempImg = LoadImageFromMemory(".png", settings_gear_png, settings_gear_png_len);
    eng.settingsGear = LoadTextureFromImage(tempImg);
    UnloadImage(tempImg);
    if (eng.uiTex.id == 0 || eng.viewportTex.id == 0 || eng.resizeButton.id == 0 || eng.viewportFullscreenButton.id == 0)
    {
        AddToLog(&eng, "Failed to load texture{E223}", LOG_LEVEL_ERROR);
        EmergencyExit(&eng, &(CGEditorContext){0}, &(InterpreterContext){0});
    }

    eng.delayFrames = true;
    eng.draggingResizeButtonID = 0;

    eng.font = LoadFontFromMemory(".ttf", arialbd_ttf, arialbd_ttf_len, 128, NULL, 0);
    if (eng.font.texture.id == 0)
    {
        AddToLog(&eng, "Failed to load font{E224}", LOG_LEVEL_ERROR);
        EmergencyExit(&eng, &(CGEditorContext){0}, &(InterpreterContext){0});
    }

    eng.CGFilePath = malloc(MAX_FILE_PATH);
    eng.CGFilePath[0] = '\0';

    eng.hoveredUIElementIndex = -1;

    eng.viewportMode = VIEWPORT_CG_EDITOR;
    eng.isGameRunning = false;

    eng.saveSound = LoadSoundFromWave(LoadWaveFromMemory(".wav", save_wav, save_wav_len));
    if (eng.saveSound.frameCount == 0)
    {
        AddToLog(&eng, "Failed to load audio{E225}", LOG_LEVEL_ERROR);
        EmergencyExit(&eng, &(CGEditorContext){0}, &(InterpreterContext){0});
    }

    eng.isSoundOn = true;

    eng.sideBarHalfSnap = false;

    eng.zoom = 1.0f;

    eng.wasBuilt = false;

    eng.showSaveWarning = 0;
    eng.showSettingsMenu = false;

    eng.varsFilter = 0;

    eng.isGameFullscreen = false;

    eng.isSaveButtonHovered = false;

    eng.isAutoSaveON = false;
    eng.autoSaveTimer = 0.0f;

    eng.fpsLimit = 240;
    eng.shouldShowFPS = false;

    eng.isAnyMenuOpen = false;

    eng.shouldCloseWindow = false;

    return eng;
}

void FreeEngineContext(EngineContext *eng)
{
    if (eng->currentPath)
    {
        free(eng->currentPath);
        eng->currentPath = NULL;
    }

    if (eng->projectPath)
    {
        free(eng->projectPath);
        eng->projectPath = NULL;
    }

    if (eng->CGFilePath)
        free(eng->CGFilePath);

    if (eng->logs.entries)
    {
        free(eng->logs.entries);
        eng->logs.entries = NULL;
    }

    UnloadDirectoryFiles(eng->files);

    UnloadRenderTexture(eng->viewportTex);
    UnloadRenderTexture(eng->uiTex);
    UnloadTexture(eng->resizeButton);
    UnloadTexture(eng->viewportFullscreenButton);
    UnloadTexture(eng->settingsGear);

    UnloadFont(eng->font);

    UnloadSound(eng->saveSound);
}

void AddUIElement(EngineContext *eng, UIElement element)
{
    if (eng->uiElementCount < MAX_UI_ELEMENTS)
    {
        eng->uiElements[eng->uiElementCount++] = element;
    }
    else
    {
        AddToLog(eng, "UIElement limit reached{E212}", LOG_LEVEL_ERROR);
        EmergencyExit(eng, &(CGEditorContext){0}, &(InterpreterContext){0});
    }
}

void AddToLog(EngineContext *eng, const char *newLine, int level)
{
    if (eng->logs.count >= eng->logs.capacity)
    {
        eng->logs.capacity += 100;
        eng->logs.entries = realloc(eng->logs.entries, sizeof(LogEntry) * eng->logs.capacity);
        if (!eng->logs.entries)
        {
            exit(1);
        }
    }

    time_t timestamp = time(NULL);
    struct tm *tm_info = localtime(&timestamp);

    strmac(eng->logs.entries[eng->logs.count].message, MAX_LOG_MESSAGE_SIZE, "%02d:%02d:%02d %s", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, newLine);

    eng->logs.entries[eng->logs.count].level = level;

    eng->logs.count++;
    eng->delayFrames = true;
}

void EmergencyExit(EngineContext *eng, CGEditorContext *cgEd, InterpreterContext *intp)
{
    time_t timestamp = time(NULL);
    struct tm *tm_info = localtime(&timestamp);

    FILE *logFile = fopen("engine_log.txt", "w");
    if (logFile)
    {
        for (int i = 0; i < eng->logs.count; i++)
        {
            char *level;
            switch (eng->logs.entries[i].level)
            {
            case LOG_LEVEL_NORMAL:
                level = "INFO";
                break;
            case LOG_LEVEL_WARNING:
                level = "WARNING";
                break;
            case LOG_LEVEL_ERROR:
                level = "ERROR";
                break;
            case LOG_LEVEL_SUCCESS:
                level = "SAVE";
                break;
            case LOG_LEVEL_DEBUG:
                level = "DEBUG";
                break;
            default:
                level = "UNKNOWN";
                break;
            }
            fprintf(logFile, "[ENGINE %s] %s\n", level, eng->logs.entries[i].message);
        }

        for (int i = 0; i < cgEd->logMessageCount; i++)
        {
            char *level;
            switch (cgEd->logMessageLevels[i])
            {
            case LOG_LEVEL_NORMAL:
                level = "INFO";
                break;
            case LOG_LEVEL_WARNING:
                level = "WARNING";
                break;
            case LOG_LEVEL_ERROR:
                level = "ERROR";
                break;
            case LOG_LEVEL_SUCCESS:
                level = "SAVE";
                break;
            case LOG_LEVEL_DEBUG:
                level = "DEBUG";
                break;
            default:
                level = "UNKNOWN";
                break;
            }
            fprintf(logFile, "[CGEDITOR %s] %s %s\n", level, TextFormat("%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec), cgEd->logMessages[i]);
        }

        for (int i = 0; i < intp->logMessageCount; i++)
        {
            char *level;
            switch (intp->logMessageLevels[i])
            {
            case LOG_LEVEL_NORMAL:
                level = "INFO";
                break;
            case LOG_LEVEL_WARNING:
                level = "WARNING";
                break;
            case LOG_LEVEL_ERROR:
                level = "ERROR";
                break;
            case LOG_LEVEL_SUCCESS:
                level = "SAVE";
                break;
            case LOG_LEVEL_DEBUG:
                level = "DEBUG";
                break;
            default:
                level = "UNKNOWN";
                break;
            }
            fprintf(logFile, "[INTERPRETER %s] %s %s\n", level, TextFormat("%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec), intp->logMessages[i]);
        }

        fprintf(logFile, "\nTo submit a crash report, please email support@rapidengine.eu");

        fclose(logFile);
    }

    OpenFile("engine_log.txt");

    FreeEngineContext(eng);
    FreeEditorContext(cgEd);
    FreeInterpreterContext(intp);

    free(intp->projectPath);
    intp->projectPath = NULL;

    CloseAudioDevice();

    CloseWindow();

    exit(1);
}

char *SetProjectFolderPath(const char *fileName)
{
    if (!fileName)
    {
        return NULL;
    }

    char cwd[MAX_FILE_PATH];
    if (!GetCWD(cwd, sizeof(cwd)))
    {
        return NULL;
    }

    char *projectPath = malloc(MAX_FILE_PATH);
    if (!projectPath)
    {
        return NULL;
    }

    strmac(projectPath, MAX_FILE_PATH, "%s%cProjects%c%s", cwd, PATH_SEPARATOR, PATH_SEPARATOR, fileName);

    return projectPath;
}

FileType GetFileType(const char *folderPath, const char *fileName)
{
    char fullPath[MAX_FILE_PATH];
    strmac(fullPath, MAX_FILE_PATH, "%s%c%s", folderPath, PATH_SEPARATOR, fileName);

    const char *ext = GetFileExtension(fileName);
    if (!ext || *(ext + 1) == '\0')
    {
        if (DirectoryExists(fullPath))
            return FILE_FOLDER;
        else
            return FILE_OTHER;
    }

    if (strcmp(ext + 1, "cg") == 0){
        return FILE_CG;
    }
    else if (strcmp(ext + 1, "png") == 0 || strcmp(ext + 1, "jpg") == 0 || strcmp(ext + 1, "jpeg") == 0){
        return FILE_IMAGE;
    }

    return FILE_OTHER;
}

void PrepareCGFilePath(EngineContext *eng, const char *projectName)
{
    char cwd[MAX_FILE_PATH];

    if (!GetCWD(cwd, sizeof(cwd)))
    {
        exit(1);
    }

    strmac(eng->CGFilePath, MAX_FILE_PATH, "%s%cProjects%c%s%c%s.cg", cwd, PATH_SEPARATOR, PATH_SEPARATOR, projectName, PATH_SEPARATOR, projectName);

    for (int i = 0; i < eng->files.count; i++)
    {
        if (strcmp(eng->CGFilePath, eng->files.paths[i]) == 0)
        {
            return;
        }
    }

    FILE *f = fopen(eng->CGFilePath, "w");
    if (f)
    {
        fclose(f);
    }
}

int DrawSaveWarning(EngineContext *eng, GraphContext *graph, CGEditorContext *cgEd)
{
    eng->isViewportFocused = false;
    int popupWidth = 500;
    int popupHeight = 150;
    int screenWidth = eng->screenWidth;
    int screenHeight = eng->screenHeight;
    int popupX = (screenWidth - popupWidth) / 2;
    int popupY = (screenHeight - popupHeight) / 2 - 100;

    const char *message = "Save changes before exiting?";
    int textWidth = MeasureTextEx(eng->font, message, 30, 0).x;

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

    DrawTextEx(eng->font, message, (Vector2){popupX + (popupWidth - textWidth) / 2, popupY + 20}, 30, 0, WHITE);

    DrawRectangleRounded(saveBtn, 0.2f, 2, DARKGREEN);
    DrawTextEx(eng->font, "Save", (Vector2){saveBtn.x + 35, saveBtn.y + 4}, 24, 0, WHITE);

    DrawRectangleRounded(closeBtn, 0.2f, 2, (Color){210, 21, 35, 255});
    DrawTextEx(eng->font, "Don't save", (Vector2){closeBtn.x + 18, closeBtn.y + 4}, 24, 0, WHITE);

    DrawRectangleRounded(cancelBtn, 0.2f, 2, (Color){80, 80, 80, 255});
    DrawTextEx(eng->font, "Cancel", (Vector2){cancelBtn.x + 25, cancelBtn.y + 4}, 24, 0, WHITE);

    if (CheckCollisionPointRec(eng->mousePos, saveBtn))
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (SaveGraphToFile(eng->CGFilePath, graph) == 0)
            {
                AddToLog(eng, "Saved successfully{C300}", LOG_LEVEL_SUCCESS);
                cgEd->hasChanged = false;
            }
            else
            {
                AddToLog(eng, "Error saving changes!{C101}", LOG_LEVEL_WARNING);
            }
            return 2;
        }
    }
    else if (CheckCollisionPointRec(eng->mousePos, closeBtn))
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            return 2;
        }
    }
    else if (CheckCollisionPointRec(eng->mousePos, cancelBtn))
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

void DrawSlider(Vector2 pos, bool *value, Vector2 mousePos)
{
    if (CheckCollisionPointRec(mousePos, (Rectangle){pos.x, pos.y, 40, 25}))
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            *value = !*value;
            DrawRectangleRounded((Rectangle){pos.x, pos.y, 40, 24}, 1.0f, 8, (Color){65, 179, 89, 255});
            DrawCircle(pos.x + 20, pos.y + 12, 10, WHITE);
            return;
        }
    }

    if (*value)
    {
        DrawRectangleRounded((Rectangle){pos.x, pos.y, 40, 24}, 1.0f, 8, (Color){0, 128, 0, 255});
        DrawCircle(pos.x + 28, pos.y + 12, 10, WHITE);
    }
    else
    {
        DrawRectangleRounded((Rectangle){pos.x, pos.y, 40, 24}, 1.0f, 8, GRAY);
        DrawCircle(pos.x + 12, pos.y + 12, 10, WHITE);
    }
}

void DrawFPSLimitDropdown(Vector2 pos, int *limit, Vector2 mousePos, Font font)
{
    static bool dropdownOpen = false;
    int fpsOptions[] = {240, 90, 60, 30};
    int fpsCount = sizeof(fpsOptions) / sizeof(fpsOptions[0]);

    float blockHeight = 30;

    Rectangle mainBox = {pos.x, pos.y, 90, blockHeight};
    DrawRectangle(pos.x, pos.y, 90, blockHeight, (Color){60, 60, 60, 255});
    DrawTextEx(font, TextFormat("%d FPS", *limit), (Vector2){pos.x + 14 - 4 * (*limit) / 100, pos.y + 4}, 20, 1, WHITE);
    DrawRectangleLines(pos.x, pos.y, 90, blockHeight, WHITE);

    if (dropdownOpen)
    {
        for (int i = 0; i < fpsCount; i++)
        {
            Rectangle optionBox = {mainBox.x - (i + 1) * 40, mainBox.y, 40, blockHeight};
            DrawRectangle(mainBox.x - (i + 1) * 40 - 2, mainBox.y, 40, blockHeight, (*limit == fpsOptions[i]) ? (Color){0, 128, 0, 255} : (Color){60, 60, 60, 255});
            DrawTextEx(font, TextFormat("%d", fpsOptions[i]), (Vector2){optionBox.x + 10 - 4 * fpsOptions[i] / 100, optionBox.y + 4}, 20, 1, WHITE);

            if (CheckCollisionPointRec(mousePos, optionBox))
            {
                SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    *limit = fpsOptions[i];
                    dropdownOpen = false;
                }
            }
        }
    }

    if (CheckCollisionPointRec(mousePos, mainBox))
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            dropdownOpen = !dropdownOpen;
        }
    }
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        dropdownOpen = false;
    }
}

bool DrawSettingsMenu(EngineContext *eng, InterpreterContext *intp)
{

    DrawRectangle(0, 0, eng->screenWidth, eng->screenHeight, (Color){0, 0, 0, 150});

    static SettingsMode settingsMode = SETTINGS_MODE_ENGINE;

    DrawRectangleRounded((Rectangle){eng->screenWidth / 4, 100, eng->screenWidth * 2 / 4, eng->screenHeight - 200}, 0.08f, 4, (Color){30, 30, 30, 255});
    DrawRectangleRoundedLines((Rectangle){eng->screenWidth / 4, 100, eng->screenWidth * 2 / 4, eng->screenHeight - 200}, 0.08f, 4, WHITE);

    static bool skipClickOnFirstFrame = true;
    DrawLineEx((Vector2){eng->screenWidth * 3 / 4 - 50, 140}, (Vector2){eng->screenWidth * 3 / 4 - 30, 160}, 3, WHITE);
    DrawLineEx((Vector2){eng->screenWidth * 3 / 4 - 50, 160}, (Vector2){eng->screenWidth * 3 / 4 - 30, 140}, 3, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){eng->screenWidth * 3 / 4 - 50, 140, 20, 20}))
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            skipClickOnFirstFrame = true;
            return false;
        }
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(eng->mousePos, (Rectangle){eng->screenWidth / 4, 100, eng->screenWidth * 2 / 4, eng->screenHeight - 200}))
    {
        if (skipClickOnFirstFrame)
        {
            skipClickOnFirstFrame = false;
        }
        else
        {
            skipClickOnFirstFrame = true;
            return false;
        }
    }

    DrawTextEx(eng->font, "Settings", (Vector2){eng->screenWidth / 2 - MeasureTextEx(eng->font, "Settings", 50, 1).x / 2, 130}, 50, 1, WHITE);

    DrawTextEx(eng->font, "Engine", (Vector2){eng->screenWidth / 4 + 30, 300}, 30, 1, settingsMode == SETTINGS_MODE_ENGINE ? WHITE : GRAY);

    if (CheckCollisionPointRec(eng->mousePos, (Rectangle){eng->screenWidth / 4 + 20, 290, MeasureTextEx(eng->font, "eng", 30, 1).x + 20, 50}) && settingsMode != SETTINGS_MODE_ENGINE)
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            settingsMode = SETTINGS_MODE_ENGINE;
        }
    }

    DrawTextEx(eng->font, "Game", (Vector2){eng->screenWidth / 4 + 30, 350}, 30, 1, settingsMode == SETTINGS_MODE_GAME ? WHITE : GRAY);

    if (CheckCollisionPointRec(eng->mousePos, (Rectangle){eng->screenWidth / 4 + 20, 340, MeasureTextEx(eng->font, "Game", 30, 1).x + 20, 50}) && settingsMode != SETTINGS_MODE_GAME)
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            settingsMode = SETTINGS_MODE_GAME;
        }
    }

    DrawTextEx(eng->font, "Keybinds", (Vector2){eng->screenWidth / 4 + 30, 400}, 30, 1, settingsMode == SETTINGS_MODE_KEYBINDS ? WHITE : GRAY);

    if (CheckCollisionPointRec(eng->mousePos, (Rectangle){eng->screenWidth / 4 + 20, 390, MeasureTextEx(eng->font, "Keybinds", 30, 1).x + 20, 50}) && settingsMode != SETTINGS_MODE_KEYBINDS)
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            settingsMode = SETTINGS_MODE_KEYBINDS;
        }
    }

    DrawTextEx(eng->font, "Export", (Vector2){eng->screenWidth / 4 + 30, 450}, 30, 1, settingsMode == SETTINGS_MODE_EXPORT ? WHITE : GRAY);

    if (CheckCollisionPointRec(eng->mousePos, (Rectangle){eng->screenWidth / 4 + 20, 440, MeasureTextEx(eng->font, "Export", 30, 1).x + 20, 50}) && settingsMode != SETTINGS_MODE_EXPORT)
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            settingsMode = SETTINGS_MODE_EXPORT;
        }
    }

    DrawRectangleGradientV(eng->screenWidth / 4 + 180, 300, 2, eng->screenHeight - 400, (Color){100, 100, 100, 255}, (Color){30, 30, 30, 255});

    switch (settingsMode)
    {
    case SETTINGS_MODE_ENGINE:
        DrawTextEx(eng->font, "Sound", (Vector2){eng->screenWidth / 4 + 200, 300}, 28, 1, WHITE);
        DrawSlider((Vector2){eng->screenWidth * 3 / 4 - 70, 305}, &eng->isSoundOn, eng->mousePos);

        DrawTextEx(eng->font, "Auto Save Every 2 Minutes", (Vector2){eng->screenWidth / 4 + 200, 350}, 28, 1, WHITE);
        DrawSlider((Vector2){eng->screenWidth * 3 / 4 - 70, 355}, &eng->isAutoSaveON, eng->mousePos);

        DrawTextEx(eng->font, "FPS Limit", (Vector2){eng->screenWidth / 4 + 200, 400}, 28, 1, WHITE);
        DrawFPSLimitDropdown((Vector2){eng->screenWidth * 3 / 4 - 100, 405}, &eng->fpsLimit, eng->mousePos, eng->font);

        DrawTextEx(eng->font, "Show FPS", (Vector2){eng->screenWidth / 4 + 200, 450}, 28, 1, WHITE);
        DrawSlider((Vector2){eng->screenWidth * 3 / 4 - 70, 455}, &eng->shouldShowFPS, eng->mousePos);
        break;
    case SETTINGS_MODE_GAME:
        DrawTextEx(eng->font, "Infinite Loop Protection", (Vector2){eng->screenWidth / 4 + 200, 300}, 28, 1, WHITE);
        DrawSlider((Vector2){eng->screenWidth * 3 / 4 - 70, 305}, &intp->isInfiniteLoopProtectionOn, eng->mousePos);

        DrawTextEx(eng->font, "Show Hitboxes", (Vector2){eng->screenWidth / 4 + 200, 350}, 28, 1, WHITE);
        DrawSlider((Vector2){eng->screenWidth * 3 / 4 - 70, 355}, &intp->shouldShowHitboxes, eng->mousePos);
        break;
    case SETTINGS_MODE_KEYBINDS:
        break;
    case SETTINGS_MODE_EXPORT:
        break;
    default:
        settingsMode = SETTINGS_MODE_ENGINE;
    }

    return true;
}

void CountingSortByLayer(EngineContext *eng)
{
    int **elements = malloc(MAX_LAYER_COUNT * sizeof(int *));
    int *layerCount = calloc(MAX_LAYER_COUNT, sizeof(int));
    for (int i = 0; i < MAX_LAYER_COUNT; i++)
    {
        elements[i] = malloc(eng->uiElementCount * sizeof(int));
    }

    for (int i = 0; i < eng->uiElementCount; i++)
    {
        if (eng->uiElements[i].layer < MAX_LAYER_COUNT)
        {
            elements[eng->uiElements[i].layer][layerCount[eng->uiElements[i].layer]] = i;
            layerCount[eng->uiElements[i].layer]++;
        }
    }

    UIElement *sorted = malloc(sizeof(UIElement) * eng->uiElementCount);
    int sortedCount = 0;

    for (int i = 0; i < MAX_LAYER_COUNT; i++)
    {
        for (int j = 0; j < layerCount[i]; j++)
        {
            sorted[sortedCount++] = eng->uiElements[elements[i][j]];
        }
    }

    memcpy(eng->uiElements, sorted, sizeof(UIElement) * sortedCount);
    free(sorted);
    free(layerCount);

    for (int i = 0; i < MAX_LAYER_COUNT; i++)
    {
        free(elements[i]);
    }
    free(elements);
}

void DrawUIElements(EngineContext *eng, GraphContext *graph, CGEditorContext *cgEd, InterpreterContext *intp, RuntimeGraphContext *runtimeGraph)
{
    eng->isSaveButtonHovered = false;
    char temp[256];
    if (eng->hoveredUIElementIndex != -1 && !eng->isAnyMenuOpen)
    {
        switch (eng->uiElements[eng->hoveredUIElementIndex].type)
        {
        case UI_ACTION_NO_COLLISION_ACTION:
            break;
        case UI_ACTION_SAVE_CG:
            eng->isSaveButtonHovered = true;
            if (eng->viewportMode != VIEWPORT_CG_EDITOR)
            {
                break;
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if (eng->isSoundOn)
                {
                    PlaySound(eng->saveSound);
                }
                if (SaveGraphToFile(eng->CGFilePath, graph) == 0)
                {
                    cgEd->hasChanged = false;
                    AddToLog(eng, "Saved successfully{C300}", LOG_LEVEL_SUCCESS);
                }
                else
                    AddToLog(eng, "Error saving changes!{C101}", LOG_LEVEL_WARNING);
            }
            break;
        case UI_ACTION_STOP_GAME:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                eng->viewportMode = VIEWPORT_CG_EDITOR;
                cgEd->isFirstFrame = true;
                eng->isGameRunning = false;
                eng->wasBuilt = false;
                FreeInterpreterContext(intp);
            }
            break;
        case UI_ACTION_RUN_GAME:
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if (cgEd->hasChanged)
                {
                    AddToLog(eng, "Project not saved!{I102}", LOG_LEVEL_WARNING);
                    break;
                }
                else if (!eng->wasBuilt)
                {
                    AddToLog(eng, "Project has not been built{I103}", LOG_LEVEL_WARNING);
                    break;
                }
                eng->viewportMode = VIEWPORT_GAME_SCREEN;
                eng->isGameRunning = true;
                intp->isFirstFrame = true;
            }
            break;
        case UI_ACTION_BUILD_GRAPH:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (cgEd->hasChanged)
                {
                    AddToLog(eng, "Project not saved!{I102}", LOG_LEVEL_WARNING);
                    break;
                }
                *runtimeGraph = ConvertToRuntimeGraph(graph, intp);
                intp->runtimeGraph = runtimeGraph;
                if (intp->buildErrorOccured)
                {
                    EmergencyExit(eng, cgEd, intp);
                }
                eng->delayFrames = true;
                if (runtimeGraph != NULL)
                {
                    AddToLog(eng, "Build successful{I300}", LOG_LEVEL_SUCCESS);
                    eng->wasBuilt = true;
                }
                else
                {
                    AddToLog(eng, "Build failed{I100}", LOG_LEVEL_WARNING);
                }
            }
            break;
        case UI_ACTION_BACK_FILEPATH:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                char *lastSlash = strrchr(eng->currentPath, PATH_SEPARATOR);
                if (lastSlash && lastSlash != eng->currentPath)
                {
                    *lastSlash = '\0';
                }

                UnloadDirectoryFiles(eng->files);
                eng->files = LoadDirectoryFilesEx(eng->currentPath, NULL, false);
                if (!eng->files.paths || eng->files.count < 0)
                {
                    AddToLog(eng, "Error loading files{E201}", LOG_LEVEL_ERROR);
                    EmergencyExit(eng, cgEd, intp);
                }
                eng->uiElementCount = 0;
                eng->delayFrames = true;
                return;
            }
            break;
        case UI_ACTION_REFRESH_FILES:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                UnloadDirectoryFiles(eng->files);
                eng->files = LoadDirectoryFilesEx(eng->currentPath, NULL, false);
                if (!eng->files.paths || eng->files.count < 0)
                {
                    AddToLog(eng, "Error loading files{E201}", LOG_LEVEL_ERROR);
                    EmergencyExit(eng, cgEd, intp);
                }
            }
            break;
        case UI_ACTION_CLOSE_WINDOW:
            eng->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (cgEd->hasChanged)
                {
                    eng->showSaveWarning = true;
                }
                else
                {
                    eng->shouldCloseWindow = true;
                    break;
                }
            }
            break;
        case UI_ACTION_MINIMIZE_WINDOW:
            eng->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                MinimizeWindow();
            }
            break;
        case UI_ACTION_OPEN_SETTINGS:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                eng->showSettingsMenu = true;
            }
            eng->isViewportFocused = false;
            break;
        case UI_ACTION_RESIZE_BOTTOM_BAR:
            eng->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && eng->draggingResizeButtonID == 0)
            {
                eng->draggingResizeButtonID = 1;
            }
            break;
        case UI_ACTION_RESIZE_SIDE_BAR:
            eng->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && eng->draggingResizeButtonID == 0)
            {
                eng->draggingResizeButtonID = 2;
            }
            break;
        case UI_ACTION_RESIZE_SIDE_BAR_MIDDLE:
            eng->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && eng->draggingResizeButtonID == 0)
            {
                eng->draggingResizeButtonID = 3;
            }
            break;
        case UI_ACTION_OPEN_FILE:
            char tooltipText[256];
            strmac(tooltipText, MAX_FILE_TOOLTIP_SIZE, "File: %s\nSize: %d bytes", GetFileName(eng->uiElements[eng->hoveredUIElementIndex].name), GetFileLength(eng->uiElements[eng->hoveredUIElementIndex].name));
            Rectangle tooltipRect = {eng->uiElements[eng->hoveredUIElementIndex].rect.pos.x + 10, eng->uiElements[eng->hoveredUIElementIndex].rect.pos.y - 61, MeasureTextEx(eng->font, tooltipText, 20, 0).x + 20, 60};
            AddUIElement(eng, (UIElement){
                                     .name = "FileTooltip",
                                     .shape = UIRectangle,
                                     .type = UI_ACTION_NO_COLLISION_ACTION,
                                     .rect = {.pos = {tooltipRect.x, tooltipRect.y}, .recSize = {tooltipRect.width, tooltipRect.height}, .roundness = 0, .roundSegments = 0},
                                     .color = DARKGRAY,
                                     .layer = 1,
                                     .text = {.string = "", .textPos = {tooltipRect.x + 10, tooltipRect.y + 10}, .textSize = 20, .textSpacing = 0, .textColor = WHITE}});
            strmac(eng->uiElements[eng->uiElementCount - 1].text.string, MAX_FILE_TOOLTIP_SIZE, "%s", tooltipText);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                double currentTime = GetTime();
                static double lastClickTime = 0;

                if (currentTime - lastClickTime <= doubleClickThreshold)
                {
                    if (GetFileType(eng->currentPath, GetFileName(eng->uiElements[eng->hoveredUIElementIndex].name)) == FILE_CG)
                    {
                        char openedFileName[MAX_FILE_NAME];
                        strmac(openedFileName, MAX_FILE_NAME, "%s", eng->uiElements[eng->hoveredUIElementIndex].text.string);
                        openedFileName[strlen(eng->uiElements[eng->hoveredUIElementIndex].text.string) - 3] = '\0';

                        *cgEd = InitEditorContext();
                        *graph = InitGraphContext();

                        PrepareCGFilePath(eng, openedFileName);

                        LoadGraphFromFile(eng->CGFilePath, graph);

                        cgEd->graph = graph;

                        eng->viewportMode = VIEWPORT_CG_EDITOR;
                    }
                    else if (GetFileType(eng->currentPath, GetFileName(eng->uiElements[eng->hoveredUIElementIndex].name)) != FILE_FOLDER)
                    {
                        OpenFile(TextFormat("%s%c%s", eng->currentPath, PATH_SEPARATOR, eng->uiElements[eng->hoveredUIElementIndex].text.string));
                    }
                    else
                    {
                        strmac(eng->currentPath, MAX_FILE_PATH, "%s%c%s", eng->currentPath, PATH_SEPARATOR, eng->uiElements[eng->hoveredUIElementIndex].text.string);

                        UnloadDirectoryFiles(eng->files);
                        eng->files = LoadDirectoryFilesEx(eng->currentPath, NULL, false);
                        if (!eng->files.paths || eng->files.count < 0)
                        {
                            AddToLog(eng, "Error loading files{E201}", LOG_LEVEL_ERROR);
                            EmergencyExit(eng, cgEd, intp);
                        }
                    }
                }
                lastClickTime = currentTime;
            }

            break;

        case UI_ACTION_VAR_TOOLTIP_RUNTIME:
            strmac(temp, MAX_VARIABLE_TOOLTIP_SIZE, "%s %s = %s", ValueTypeToString(intp->values[eng->uiElements[eng->hoveredUIElementIndex].valueIndex].type), intp->values[eng->uiElements[eng->hoveredUIElementIndex].valueIndex].name, ValueToString(intp->values[eng->uiElements[eng->hoveredUIElementIndex].valueIndex]));
            AddUIElement(eng, (UIElement){
                                     .name = "VarTooltip",
                                     .shape = UIRectangle,
                                     .type = UI_ACTION_NO_COLLISION_ACTION,
                                     .rect = {.pos = {eng->sideBarWidth, eng->uiElements[eng->hoveredUIElementIndex].rect.pos.y}, .recSize = {MeasureTextEx(eng->font, temp, 20, 0).x + 20, 40}, .roundness = 0.4f, .roundSegments = 8},
                                     .color = DARKGRAY,
                                     .layer = 1,
                                     .text = {.textPos = {eng->sideBarWidth + 10, eng->uiElements[eng->hoveredUIElementIndex].rect.pos.y + 10}, .textSize = 20, .textSpacing = 0, .textColor = WHITE}});
            strmac(eng->uiElements[eng->uiElementCount - 1].text.string, MAX_VARIABLE_TOOLTIP_SIZE, "%s", temp);
            break;

        case UI_ACTION_CHANGE_VARS_FILTER:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                eng->varsFilter++;
                if (eng->varsFilter > 5)
                {
                    eng->varsFilter = 0;
                }
            }
            break;
        case UI_ACTION_FULLSCREEN_BUTTON_VIEWPORT:
            eng->isViewportFocused = false;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                eng->isGameFullscreen = true;
            }
        }

        if (eng->uiElements[eng->hoveredUIElementIndex].shape == 0)
        {
            AddUIElement(eng, (UIElement){
                                     .name = "HoverBlink",
                                     .shape = UIRectangle,
                                     .type = UI_ACTION_NO_COLLISION_ACTION,
                                     .rect = {.pos = {eng->uiElements[eng->hoveredUIElementIndex].rect.pos.x, eng->uiElements[eng->hoveredUIElementIndex].rect.pos.y}, .recSize = {eng->uiElements[eng->hoveredUIElementIndex].rect.recSize.x, eng->uiElements[eng->hoveredUIElementIndex].rect.recSize.y}, .roundness = eng->uiElements[eng->hoveredUIElementIndex].rect.roundness, .roundSegments = eng->uiElements[eng->hoveredUIElementIndex].rect.roundSegments},
                                     .color = eng->uiElements[eng->hoveredUIElementIndex].rect.hoverColor,
                                     .layer = 99});
        }
    }

    for (int i = 0; i < eng->uiElementCount; i++)
    {
        UIElement *el = &eng->uiElements[i];
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
            DrawTextEx(eng->font, el->text.string, el->text.textPos, el->text.textSize, el->text.textSpacing, el->text.textColor);
        }
    }
}

void BuildUITexture(EngineContext *eng, GraphContext *graph, CGEditorContext *cgEd, InterpreterContext *intp, RuntimeGraphContext *runtimeGraph)
{
    eng->uiElementCount = 0;

    BeginTextureMode(eng->uiTex);
    ClearBackground((Color){255, 255, 255, 0});

    if (eng->screenWidth > eng->screenHeight && eng->screenWidth > 1000)
    {
        AddUIElement(eng, (UIElement){
                                 .name = "SideBarVars",
                                 .shape = UIRectangle,
                                 .type = UI_ACTION_NO_COLLISION_ACTION,
                                 .rect = {.pos = {0, 0}, .recSize = {eng->sideBarWidth, eng->sideBarMiddleY}, .roundness = 0.0f, .roundSegments = 0},
                                 .color = (Color){28, 28, 28, 255},
                                 .layer = 0,
                             });

        AddUIElement(eng, (UIElement){
                                 .name = "SideBarLog",
                                 .shape = UIRectangle,
                                 .type = UI_ACTION_NO_COLLISION_ACTION,
                                 .rect = {.pos = {0, eng->sideBarMiddleY}, .recSize = {eng->sideBarWidth, eng->screenHeight - eng->bottomBarHeight}, .roundness = 0.0f, .roundSegments = 0},
                                 .color = (Color){15, 15, 15, 255},
                                 .layer = 0,
                             });

        AddUIElement(eng, (UIElement){
                                 .name = "SideBarMiddleLine",
                                 .shape = UILine,
                                 .type = UI_ACTION_NO_COLLISION_ACTION,
                                 .line = {.startPos = {eng->sideBarWidth, 0}, .engPos = {eng->sideBarWidth, eng->screenHeight - eng->bottomBarHeight}, .thickness = 2},
                                 .color = WHITE,
                                 .layer = 0,
                             });

        AddUIElement(eng, (UIElement){
                                 .name = "SideBarFromViewportDividerLine",
                                 .shape = UILine,
                                 .type = UI_ACTION_NO_COLLISION_ACTION,
                                 .line = {.startPos = {0, eng->sideBarMiddleY}, .engPos = {eng->sideBarWidth, eng->sideBarMiddleY}, .thickness = 2},
                                 .color = WHITE,
                                 .layer = 0,
                             });

        Vector2 saveButtonPos = {
            eng->sideBarHalfSnap ? eng->sideBarWidth - 70 : eng->sideBarWidth - 145,
            eng->sideBarHalfSnap ? eng->sideBarMiddleY + 60 : eng->sideBarMiddleY + 15};

        AddUIElement(eng, (UIElement){
                                 .name = "SaveButton",
                                 .shape = UIRectangle,
                                 .type = UI_ACTION_SAVE_CG,
                                 .rect = {.pos = saveButtonPos, .recSize = {64, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = (eng->viewportMode == VIEWPORT_CG_EDITOR ? Fade(WHITE, 0.6f) : (Color){0, 0, 0, 0})},
                                 .color = (Color){70, 70, 70, 200},
                                 .layer = 1,
                                 .text = {.textPos = {cgEd->hasChanged ? saveButtonPos.x + 5 : saveButtonPos.x + 8, saveButtonPos.y + 5}, .textSize = 20, .textSpacing = 2, .textColor = (eng->viewportMode == VIEWPORT_CG_EDITOR ? WHITE : GRAY)},
                             });
        if (cgEd->hasChanged)
        {
            strmac(eng->uiElements[eng->uiElementCount - 1].text.string, MAX_FILE_PATH, "Save*");
        }
        else
        {
            strmac(eng->uiElements[eng->uiElementCount - 1].text.string, MAX_FILE_PATH, "Save");
        }

        if (eng->viewportMode == VIEWPORT_GAME_SCREEN)
        {
            AddUIElement(eng, (UIElement){
                                     .name = "StopButton",
                                     .shape = UIRectangle,
                                     .type = UI_ACTION_STOP_GAME,
                                     .rect = {.pos = {eng->sideBarWidth - 70, eng->sideBarMiddleY + 15}, .recSize = {64, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                     .color = RED,
                                     .layer = 1,
                                     .text = {.string = "Stop", .textPos = {eng->sideBarWidth - 62, eng->sideBarMiddleY + 20}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                                 });
        }
        else if (eng->wasBuilt)
        {
            AddUIElement(eng, (UIElement){
                                     .name = "RunButton",
                                     .shape = UIRectangle,
                                     .type = UI_ACTION_RUN_GAME,
                                     .rect = {.pos = {eng->sideBarWidth - 70, eng->sideBarMiddleY + 15}, .recSize = {64, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                     .color = DARKGREEN,
                                     .layer = 1,
                                     .text = {.string = "Run", .textPos = {eng->sideBarWidth - 56, eng->sideBarMiddleY + 20}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                                 });
        }
        else
        {
            AddUIElement(eng, (UIElement){
                                     .name = "BuildButton",
                                     .shape = UIRectangle,
                                     .type = UI_ACTION_BUILD_GRAPH,
                                     .rect = {.pos = {eng->sideBarWidth - 70, eng->sideBarMiddleY + 15}, .recSize = {64, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                     .color = (Color){70, 70, 70, 200},
                                     .layer = 1,
                                     .text = {.string = "Build", .textPos = {eng->sideBarWidth - 64, eng->sideBarMiddleY + 20}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                                 });
        }

        int logY = eng->screenHeight - eng->bottomBarHeight - 30;
        char cutMessage[256];
        for (int i = eng->logs.count - 1; i >= 0 && logY > eng->sideBarMiddleY + 60 + eng->sideBarHalfSnap * 40; i--)
        {
            const char *msgNoTimestamp = eng->logs.entries[i].message + 9;

            char finalMsg[256];
            strmac(finalMsg, MAX_LOG_MESSAGE_SIZE, "%s", eng->logs.entries[i].message);
            if(eng->logs.entries[i].level != LOG_LEVEL_DEBUG){
                finalMsg[strlen(finalMsg) - 6] = '\0';
            }

            int repeatCount = 1;
            while (i - repeatCount >= 0)
            {
                const char *prevMsgNoTimestamp = eng->logs.entries[i - repeatCount].message + 9;
                if (strcmp(msgNoTimestamp, prevMsgNoTimestamp) != 0)
                    break;
                repeatCount++;
            }

            if (repeatCount > 1)
            {
                strmac(finalMsg, MAX_LOG_MESSAGE_SIZE, "[x%d] %s", repeatCount, eng->logs.entries[i].message);
                i -= (repeatCount - 1);
            }

            int j;
            for (j = 0; j < (int)strlen(finalMsg); j++)
            {
                char temp[256];
                strmac(temp, MAX_LOG_MESSAGE_SIZE, "%.*s", j, finalMsg);
                temp[j] = '\0';

                if (MeasureTextEx(eng->font, temp, 20, 2).x < eng->sideBarWidth - 25)
                {
                    continue;
                }
                else
                {
                    break;
                }
            }
            strmac(cutMessage, MAX_LOG_MESSAGE_SIZE, "%.*s", j, finalMsg);

            Color logColor;
            switch (eng->logs.entries[i].level)
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
            case LOG_LEVEL_SUCCESS:
                logColor = GREEN;
                break;
            default:
                logColor = WHITE;
                break;
            }

            AddUIElement(eng, (UIElement){
                                     .name = "LogText",
                                     .shape = UIText,
                                     .type = UI_ACTION_NO_COLLISION_ACTION,
                                     .text = {.textPos = {10, logY}, .textSize = 20, .textSpacing = 2, .textColor = logColor},
                                     .layer = 0});

            strmac(eng->uiElements[eng->uiElementCount - 1].text.string, MAX_LOG_MESSAGE_SIZE, cutMessage);

            logY -= 25;
        }

        if (eng->sideBarMiddleY > 45)
        {
            AddUIElement(eng, (UIElement){
                                     .name = "VarsFilterShowText",
                                     .shape = UIText,
                                     .type = UI_ACTION_NO_COLLISION_ACTION,
                                     .text = {.string = "Show:", .textPos = {eng->sideBarWidth - 155, 20}, .textSize = 20, .textSpacing = 2, .textColor = WHITE},
                                     .layer = 0});
            char varsFilterText[10];
            Color varFilterColor;
            switch (eng->varsFilter)
            {
            case VAR_FILTER_ALL:
                strmac(varsFilterText, 10, "All");
                varFilterColor = RAYWHITE;
                break;
            case VAR_FILTER_NUMBERS:
                strmac(varsFilterText, 10, "Nums");
                varFilterColor = (Color){24, 119, 149, 255};
                break;
            case VAR_FILTER_STRINGS:
                strmac(varsFilterText, 10, "Strings");
                varFilterColor = (Color){180, 178, 40, 255};
                break;
            case VAR_FILTER_BOOLS:
                strmac(varsFilterText, 10, "Bools");
                varFilterColor = (Color){27, 64, 121, 255};
                break;
            case VAR_FILTER_COLORS:
                strmac(varsFilterText, 10, "Colors");
                varFilterColor = (Color){217, 3, 104, 255};
                break;
            case VAR_FILTER_SPRITES:
                strmac(varsFilterText, 10, "Sprites");
                varFilterColor = (Color){3, 206, 164, 255};
                break;
            default:
                eng->varsFilter = 0;
                strmac(varsFilterText, 10, "All");
                varFilterColor = RAYWHITE;
                break;
            }
            AddUIElement(eng, (UIElement){
                                     .name = "VarsFilterButton",
                                     .shape = UIRectangle,
                                     .type = UI_ACTION_CHANGE_VARS_FILTER,
                                     .rect = {.pos = {eng->sideBarWidth - 85, 15}, .recSize = {78, 30}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                     .color = (Color){70, 70, 70, 200},
                                     .layer = 1,
                                     .text = {.textPos = {eng->sideBarWidth - 85 + (78 - MeasureTextEx(eng->font, varsFilterText, 20, 1).x) / 2, 20}, .textSize = 20, .textSpacing = 1, .textColor = varFilterColor},
                                 });
            strmac(eng->uiElements[eng->uiElementCount - 1].text.string, 10, varsFilterText);
        }

        int varsY = 60;
        for (int i = 0; i < (eng->isGameRunning ? intp->valueCount : graph->variablesCount) && varsY < eng->sideBarMiddleY - 40; i++)
        {
            if (eng->isGameRunning)
            {
                if (!intp->values[i].isVariable)
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
            strmac(cutMessage, MAX_VARIABLE_NAME_SIZE, "%s", eng->isGameRunning ? intp->values[i].name : graph->variables[i]);
            switch (eng->isGameRunning ? intp->values[i].type : graph->variableTypes[i])
            {
            case VAL_NUMBER:
            case NODE_CREATE_NUMBER:
                varColor = (Color){24, 119, 149, 255};
                if (eng->varsFilter != VAR_FILTER_NUMBERS && eng->varsFilter != VAR_FILTER_ALL)
                {
                    continue;
                }
                break;
            case VAL_STRING:
            case NODE_CREATE_STRING:
                varColor = (Color){180, 178, 40, 255};
                if (eng->varsFilter != VAR_FILTER_STRINGS && eng->varsFilter != VAR_FILTER_ALL)
                {
                    continue;
                }
                break;
            case VAL_BOOL:
            case NODE_CREATE_BOOL:
                varColor = (Color){27, 64, 121, 255};
                if (eng->varsFilter != VAR_FILTER_BOOLS && eng->varsFilter != VAR_FILTER_ALL)
                {
                    continue;
                }
                break;
            case VAL_COLOR:
            case NODE_CREATE_COLOR:
                varColor = (Color){217, 3, 104, 255};
                if (eng->varsFilter != VAR_FILTER_COLORS && eng->varsFilter != VAR_FILTER_ALL)
                {
                    continue;
                }
                break;
            case VAL_SPRITE:
            case NODE_CREATE_SPRITE:
                varColor = (Color){3, 206, 164, 255};
                if (eng->varsFilter != VAR_FILTER_SPRITES && eng->varsFilter != VAR_FILTER_ALL)
                {
                    continue;
                }
                break;
            default:
                varColor = LIGHTGRAY;
            }

            AddUIElement(eng, (UIElement){
                                     .name = "Variable Background",
                                     .shape = UIRectangle,
                                     .type = eng->isGameRunning ? UI_ACTION_VAR_TOOLTIP_RUNTIME : UI_ACTION_NO_COLLISION_ACTION,
                                     .rect = {.pos = {15, varsY - 5}, .recSize = {eng->sideBarWidth - 25, 35}, .roundness = 0.6f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                     .color = (Color){59, 59, 59, 255},
                                     .layer = 1,
                                     .valueIndex = i});

            float dotsWidth = MeasureTextEx(eng->font, "...", 24, 2).x;
            int j;
            bool wasCut = false;
            for (j = 1; j <= strlen(cutMessage); j++)
            {
                char temp[256];
                strmac(temp, MAX_VARIABLE_NAME_SIZE, "%.*s", j, cutMessage);

                float textWidth = MeasureTextEx(eng->font, temp, 24, 2).x;
                if (textWidth + dotsWidth < eng->sideBarWidth - 80)
                    continue;

                wasCut = true;
                j--;
                break;
            }

            bool textHidden = false;

            if (wasCut && j < 252){
                strmac(cutMessage, MAX_VARIABLE_NAME_SIZE, "%s...", cutMessage);
            }
            if (wasCut && j == 0)
            {
                cutMessage[0] = '\0';
                textHidden = true;
            }

            AddUIElement(eng, (UIElement){
                                     .name = "Variable",
                                     .shape = UICircle,
                                     .type = UI_ACTION_NO_COLLISION_ACTION,
                                     .circle = {.center = (Vector2){textHidden ? eng->sideBarWidth / 2 + 3 : eng->sideBarWidth - 25, varsY + 14}, .radius = 8},
                                     .color = varColor,
                                     .text = {.textPos = {20, varsY}, .textSize = 24, .textSpacing = 2, .textColor = WHITE},
                                     .layer = 2});

            strmac(eng->uiElements[eng->uiElementCount - 1].text.string, MAX_VARIABLE_NAME_SIZE, "%s", cutMessage);
            varsY += 40;
        }
    }

    AddUIElement(eng, (UIElement){
                             .name = "BottomBar",
                             .shape = UIRectangle,
                             .type = UI_ACTION_NO_COLLISION_ACTION,
                             .rect = {.pos = {0, eng->screenHeight - eng->bottomBarHeight}, .recSize = {eng->screenWidth, eng->bottomBarHeight}, .roundness = 0.0f, .roundSegments = 0},
                             .color = (Color){28, 28, 28, 255},
                             .layer = 0,
                         });

    AddUIElement(eng, (UIElement){
                             .name = "BottomBarFromViewportDividerLine",
                             .shape = UILine,
                             .type = UI_ACTION_NO_COLLISION_ACTION,
                             .line = {.startPos = {0, eng->screenHeight - eng->bottomBarHeight}, .engPos = {eng->screenWidth, eng->screenHeight - eng->bottomBarHeight}, .thickness = 2},
                             .color = WHITE,
                             .layer = 0,
                         });

    AddUIElement(eng, (UIElement){
                             .name = "BackButton",
                             .shape = UIRectangle,
                             .type = UI_ACTION_BACK_FILEPATH,
                             .rect = {.pos = {30, eng->screenHeight - eng->bottomBarHeight + 10}, .recSize = {65, 30}, .roundness = 0, .roundSegments = 0, .hoverColor = Fade(WHITE, 0.6f)},
                             .color = (Color){70, 70, 70, 150},
                             .layer = 1,
                             .text = {.string = "Back", .textPos = {35, eng->screenHeight - eng->bottomBarHeight + 12}, .textSize = 25, .textSpacing = 0, .textColor = WHITE}});

    AddUIElement(eng, (UIElement){
                             .name = "RefreshButton",
                             .shape = UIRectangle,
                             .type = UI_ACTION_REFRESH_FILES,
                             .rect = {.pos = {110, eng->screenHeight - eng->bottomBarHeight + 10}, .recSize = {100, 30}, .roundness = 0, .roundSegments = 0, .hoverColor = Fade(WHITE, 0.6f)},
                             .color = (Color){70, 70, 70, 150},
                             .layer = 1,
                             .text = {.string = "Refresh", .textPos = {119, eng->screenHeight - eng->bottomBarHeight + 12}, .textSize = 25, .textSpacing = 0, .textColor = WHITE}});

    AddUIElement(eng, (UIElement){
                             .name = "CurrentPath",
                             .shape = UIText,
                             .type = UI_ACTION_NO_COLLISION_ACTION,
                             .color = (Color){0, 0, 0, 0},
                             .layer = 0,
                             .text = {.string = "", .textPos = {230, eng->screenHeight - eng->bottomBarHeight + 15}, .textSize = 22, .textSpacing = 2, .textColor = WHITE}});
    strmac(eng->uiElements[eng->uiElementCount - 1].text.string, MAX_FILE_PATH, "%s", eng->currentPath);

    int xOffset = 50;
    int yOffset = eng->screenHeight - eng->bottomBarHeight + 70;

    for (int i = 0; i < eng->files.count; i++)
    {
        if (i > MAX_UI_ELEMENTS / 2)
        {
            break;
        }

        const char *fileName = GetFileName(eng->files.paths[i]);

        if (fileName[0] == '.')
        {
            continue;
        }

        Color fileOutlineColor;
        Color fileTextColor;

        switch (GetFileType(eng->currentPath, fileName))
        {
        case FILE_FOLDER:
            fileOutlineColor = (Color){205, 205, 50, 200};
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
            AddToLog(eng, "Out of bounds enum{O201}", LOG_LEVEL_ERROR);
            fileOutlineColor = (Color){160, 160, 160, 255};
            fileTextColor = (Color){220, 220, 220, 255};
            break;
        }

        char buff[MAX_FILE_NAME];
        strmac(buff, MAX_FILE_NAME, "%s", fileName);

        if (MeasureTextEx(eng->font, fileName, 25, 0).x > 135)
        {
            const char *ext = GetFileExtension(fileName);
            int extLen = ext ? strlen(ext) : 0;
            int maxLen = strlen(buff) - extLen;

            for (int j = maxLen - 1; j >= 0; j--)
            {
                buff[j] = '\0';
                if (MeasureText(buff, 25) < 115)
                {
                    if (j > 3)
                    {
                        buff[j - 3] = '\0';
                        strmac(buff, MAX_FILE_NAME, "%s...", buff);
                        if (ext)
                        {
                            strmac(buff, MAX_FILE_NAME, "%s%s", buff, ext);
                        }
                    }
                    break;
                }
            }
        }

        AddUIElement(eng, (UIElement){
                                 .name = "FileOutline",
                                 .shape = UIRectangle,
                                 .type = UI_ACTION_NO_COLLISION_ACTION,
                                 .rect = {.pos = {xOffset - 1, yOffset - 1}, .recSize = {152, 62}, .roundness = 0.5f, .roundSegments = 8},
                                 .color = fileOutlineColor,
                                 .layer = 0});

        AddUIElement(eng, (UIElement){
                                 .name = "File",
                                 .shape = UIRectangle,
                                 .type = UI_ACTION_OPEN_FILE,
                                 .rect = {.pos = {xOffset, yOffset}, .recSize = {150, 60}, .roundness = 0.5f, .roundSegments = 8, .hoverColor = Fade(WHITE, 0.6f)},
                                 .color = (Color){40, 40, 40, 255},
                                 .layer = 1,
                                 .text = {.string = "", .textPos = {xOffset + 10, yOffset + 16}, .textSize = 25, .textSpacing = 0, .textColor = fileTextColor}});
        strmac(eng->uiElements[eng->uiElementCount - 1].name, MAX_FILE_PATH, "%s", eng->files.paths[i]);
        strmac(eng->uiElements[eng->uiElementCount - 1].text.string, MAX_FILE_PATH, "%s", buff);

        xOffset += 200;
        if (xOffset + 100 >= eng->screenWidth)
        {
            xOffset = 50;
            yOffset += 120;
        }
    }

    AddUIElement(eng, (UIElement){
                             .name = "TopBarClose",
                             .shape = UIRectangle,
                             .type = UI_ACTION_CLOSE_WINDOW,
                             .rect = {.pos = {eng->screenWidth - 50, 0}, .recSize = {50, 50}, .roundness = 0.0f, .roundSegments = 0, .hoverColor = RED},
                             .color = (Color){0, 0, 0, 0},
                             .layer = 1,
                         });
    AddUIElement(eng, (UIElement){
                             .name = "TopBarMinimize",
                             .shape = UIRectangle,
                             .type = UI_ACTION_MINIMIZE_WINDOW,
                             .rect = {.pos = {eng->screenWidth - 100, 0}, .recSize = {50, 50}, .roundness = 0.0f, .roundSegments = 0, .hoverColor = GRAY},
                             .color = (Color){0, 0, 0, 0},
                             .layer = 1,
                         });
    AddUIElement(eng, (UIElement){
                             .name = "TopBarSettings",
                             .shape = UIRectangle,
                             .type = UI_ACTION_OPEN_SETTINGS,
                             .rect = {.pos = {eng->screenWidth - 150, 0}, .recSize = {50, 50}, .roundness = 0.0f, .roundSegments = 0, .hoverColor = GRAY},
                             .color = (Color){0, 0, 0, 0},
                             .layer = 1,
                         });

    AddUIElement(eng, (UIElement){
                             .name = "BottomBarResizeButton",
                             .shape = UICircle,
                             .type = UI_ACTION_RESIZE_BOTTOM_BAR,
                             .circle = {.center = (Vector2){eng->screenWidth / 2, eng->screenHeight - eng->bottomBarHeight}, .radius = 10},
                             .color = (Color){255, 255, 255, 1},
                             .layer = 1,
                         });
    AddUIElement(eng, (UIElement){
                             .name = "SideBarResizeButton",
                             .shape = UICircle,
                             .type = UI_ACTION_RESIZE_SIDE_BAR,
                             .circle = {.center = (Vector2){eng->sideBarWidth, (eng->screenHeight - eng->bottomBarHeight) / 2}, .radius = 10},
                             .color = (Color){255, 255, 255, 1},
                             .layer = 1,
                         });
    AddUIElement(eng, (UIElement){
                             .name = "SideBarMiddleResizeButton",
                             .shape = UICircle,
                             .type = UI_ACTION_RESIZE_SIDE_BAR_MIDDLE,
                             .circle = {.center = (Vector2){eng->sideBarWidth / 2, eng->sideBarMiddleY}, .radius = 10},
                             .color = (Color){255, 255, 255, 1},
                             .layer = 1,
                         });

    if (eng->isGameRunning)
    {
        AddUIElement(eng, (UIElement){
                                 .name = "ViewportFullscreenButton",
                                 .shape = UIRectangle,
                                 .type = UI_ACTION_FULLSCREEN_BUTTON_VIEWPORT,
                                 .rect = {.pos = {eng->sideBarWidth + 8, 10}, .recSize = {50, 50}, .roundness = 0.2f, .roundSegments = 8, .hoverColor = GRAY},
                                 .color = (Color){60, 60, 60, 255},
                                 .layer = 1,
                             });
    }

    CountingSortByLayer(eng);

    DrawUIElements(eng, graph, cgEd, intp, runtimeGraph);

    // special symbols and textures
    DrawRectangleLinesEx((Rectangle){0, 0, eng->screenWidth, eng->screenHeight}, 4.0f, WHITE);

    DrawLineEx((Vector2){eng->screenWidth - 35, 15}, (Vector2){eng->screenWidth - 15, 35}, 2, WHITE);
    DrawLineEx((Vector2){eng->screenWidth - 35, 35}, (Vector2){eng->screenWidth - 15, 15}, 2, WHITE);

    DrawLineEx((Vector2){eng->screenWidth - 85, 25}, (Vector2){eng->screenWidth - 65, 25}, 2, WHITE);

    Rectangle src = {0, 0, eng->settingsGear.width, eng->settingsGear.height};
    Rectangle dst = {eng->screenWidth - 140, 12, 30, 30};
    Vector2 origin = {0, 0};
    DrawTexturePro(eng->settingsGear, src, dst, origin, 0.0f, WHITE);

    DrawTexture(eng->resizeButton, eng->screenWidth / 2 - 10, eng->screenHeight - eng->bottomBarHeight - 10, WHITE);
    DrawTexturePro(eng->resizeButton, (Rectangle){0, 0, 20, 20}, (Rectangle){eng->sideBarWidth, (eng->screenHeight - eng->bottomBarHeight) / 2, 20, 20}, (Vector2){10, 10}, 90.0f, WHITE);
    if (eng->sideBarWidth > 150)
    {
        DrawTexture(eng->resizeButton, eng->sideBarWidth / 2 - 10, eng->sideBarMiddleY - 10, WHITE);
    }

    if (eng->isGameRunning)
    {
        DrawTexturePro(eng->viewportFullscreenButton, (Rectangle){0, 0, eng->viewportFullscreenButton.width, eng->viewportFullscreenButton.height}, (Rectangle){eng->sideBarWidth + 8, 10, 50, 50}, (Vector2){0, 0}, 0, WHITE);
    }

    EndTextureMode();
}

bool HandleUICollisions(EngineContext *eng, GraphContext *graph, InterpreterContext *intp, CGEditorContext *cgEd, RuntimeGraphContext *runtimeGraph)
{
    if (eng->uiElementCount == 0)
    {
        eng->hoveredUIElementIndex = 0;
        return true;
    }
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S) && !IsKeyDown(KEY_LEFT_SHIFT) && eng->viewportMode == VIEWPORT_CG_EDITOR)
    {
        if (eng->isSoundOn)
        {
            PlaySound(eng->saveSound);
        }
        if (SaveGraphToFile(eng->CGFilePath, graph) == 0)
        {
            cgEd->hasChanged = false;
            AddToLog(eng, "Saved successfully{C300}", LOG_LEVEL_SUCCESS);
        }
        else
        {
            AddToLog(eng, "Error saving changes!{C101}", LOG_LEVEL_WARNING);
        }
    }
    else if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_R))
    {
        if (cgEd->hasChanged)
        {
            AddToLog(eng, "Project not saved!{I102}", LOG_LEVEL_WARNING);
        }
        else if (!eng->wasBuilt)
        {
            AddToLog(eng, "Project has not been built!{I103}", LOG_LEVEL_WARNING);
        }
        else
        {
            eng->viewportMode = VIEWPORT_GAME_SCREEN;
            eng->isGameRunning = true;
            intp->isFirstFrame = true;
        }
    }
    else if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_E))
    {
        cgEd->delayFrames = true;
        eng->delayFrames = true;
        eng->viewportMode = VIEWPORT_CG_EDITOR;
        cgEd->isFirstFrame = true;
        eng->isGameRunning = false;
        eng->wasBuilt = false;
        eng->isGameFullscreen = false;
        FreeInterpreterContext(intp);
    }
    else if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_B))
    {
        if (cgEd->hasChanged)
        {
            AddToLog(eng, "Project not saved!{I102}", LOG_LEVEL_WARNING);
        }
        else
        {
            *runtimeGraph = ConvertToRuntimeGraph(graph, intp);
            intp->runtimeGraph = runtimeGraph;
            if (intp->buildErrorOccured)
            {
                EmergencyExit(eng, cgEd, intp);
            }
            eng->delayFrames = true;
            if (runtimeGraph != NULL)
            {
                AddToLog(eng, "Build successful{I300}", LOG_LEVEL_NORMAL);
                eng->wasBuilt = true;
            }
            else
            {
                AddToLog(eng, "Build failed{I100}", LOG_LEVEL_ERROR);
            }
        }
    }
    else if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S) && IsKeyDown(KEY_LEFT_SHIFT))
    {
        cgEd->delayFrames = true;
        eng->delayFrames = true;
        eng->viewportMode = VIEWPORT_CG_EDITOR;
        cgEd->isFirstFrame = true;
        eng->isGameRunning = false;
        eng->wasBuilt = false;
        eng->isGameFullscreen = false;
        FreeInterpreterContext(intp);
    }
    else if (IsKeyPressed(KEY_ESCAPE))
    {
        eng->isGameFullscreen = false;
    }

    if (eng->draggingResizeButtonID != 0)
    {
        if (IsMouseButtonUp(MOUSE_LEFT_BUTTON))
        {
            eng->draggingResizeButtonID = 0;
        }

        eng->hasResizedBar = true;

        switch (eng->draggingResizeButtonID)
        {
        case 0:
            break;
        case 1:
            eng->bottomBarHeight -= GetMouseDelta().y;
            if (eng->bottomBarHeight <= 5)
            {
                eng->bottomBarHeight = 5;
            }
            else if (eng->bottomBarHeight >= 3 * eng->screenHeight / 4)
            {
                eng->bottomBarHeight = 3 * eng->screenHeight / 4;
            }
            else
            {
                eng->sideBarMiddleY += GetMouseDelta().y / 2;
            }
            break;
        case 2:
            eng->sideBarWidth += GetMouseDelta().x;

            if (eng->sideBarWidth < 160 && GetMouseDelta().x < 0)
            {
                eng->sideBarWidth = 80;
                eng->sideBarHalfSnap = true;
            }
            else if (eng->sideBarWidth > 110)
            {
                eng->sideBarHalfSnap = false;
                if (eng->sideBarWidth >= 3 * eng->screenWidth / 4)
                {
                    eng->sideBarWidth = 3 * eng->screenWidth / 4;
                }
            }
            break;
        case 3:
            eng->sideBarMiddleY += GetMouseDelta().y;
            break;
        default:
            break;
        }

        if (eng->sideBarMiddleY >= eng->screenHeight - eng->bottomBarHeight - 60 - eng->sideBarHalfSnap * 40)
        {
            eng->sideBarMiddleY = eng->screenHeight - eng->bottomBarHeight - 60 - eng->sideBarHalfSnap * 40;
        }
        else if (eng->sideBarMiddleY <= 5)
        {
            eng->sideBarMiddleY = 5;
        }
    }

    for (int i = 0; i < eng->uiElementCount; i++)
    {
        if (eng->uiElements[i].layer != 0)
        {
            if (eng->uiElements[i].shape == UIRectangle && CheckCollisionPointRec(eng->mousePos, (Rectangle){eng->uiElements[i].rect.pos.x, eng->uiElements[i].rect.pos.y, eng->uiElements[i].rect.recSize.x, eng->uiElements[i].rect.recSize.y}))
            {
                eng->hoveredUIElementIndex = i;
                return true;
            }
            else if (eng->uiElements[i].shape == UICircle && CheckCollisionPointCircle(eng->mousePos, eng->uiElements[i].circle.center, eng->uiElements[i].circle.radius))
            {
                eng->hoveredUIElementIndex = i;
                return true;
            }
        }
    }

    eng->hoveredUIElementIndex = -1;

    return false;
}

void ContextChangePerFrame(EngineContext *eng)
{
    eng->mousePos = GetMousePosition();
    eng->isViewportFocused = CheckCollisionPointRec(eng->mousePos, (Rectangle){eng->sideBarWidth, 0, eng->screenWidth - eng->sideBarWidth, eng->screenHeight - eng->bottomBarHeight});

    eng->screenWidth = GetScreenWidth();
    eng->screenHeight = GetScreenHeight();

    if (eng->prevScreenWidth != eng->screenWidth || eng->prevScreenHeight != eng->screenHeight || eng->hasResizedBar)
    {
        eng->prevScreenWidth = eng->screenWidth;
        eng->prevScreenHeight = eng->screenHeight;
        eng->viewportWidth = eng->screenWidth - eng->sideBarWidth;
        eng->viewportHeight = eng->screenHeight - eng->bottomBarHeight;
        eng->hasResizedBar = false;
        eng->delayFrames = true;
    }
}

int GetEngineMouseCursor(EngineContext *eng, CGEditorContext *cgEd)
{
    if (eng->isAnyMenuOpen)
    {
        return MOUSE_CURSOR_ARROW;
    }

    if (eng->draggingResizeButtonID == 1 || eng->draggingResizeButtonID == 3)
    {
        return MOUSE_CURSOR_RESIZE_NS;
    }
    else if (eng->draggingResizeButtonID == 2)
    {
        return MOUSE_CURSOR_RESIZE_EW;
    }

    if (eng->isViewportFocused)
    {
        switch (eng->viewportMode)
        {
        case VIEWPORT_CG_EDITOR:
            return cgEd->cursor;
        case VIEWPORT_GAME_SCREEN:
            // return intp->cursor;
            return MOUSE_CURSOR_ARROW;
        case VIEWPORT_HITBOX_EDITOR:
            return MOUSE_CURSOR_CROSSHAIR;
        default:
            eng->viewportMode = VIEWPORT_CG_EDITOR;
            return cgEd->cursor;
        }
    }

    if (eng->isSaveButtonHovered && eng->viewportMode != VIEWPORT_CG_EDITOR)
    {
        return MOUSE_CURSOR_NOT_ALLOWED;
    }

    if (eng->hoveredUIElementIndex != -1)
    {
        return MOUSE_CURSOR_POINTING_HAND;
    }

    return MOUSE_CURSOR_ARROW;
}

int GetEngineFPS(EngineContext *eng, CGEditorContext *cgEd, InterpreterContext *intp)
{
    int fps;

    if (eng->isViewportFocused)
    {
        switch (eng->viewportMode)
        {
        case VIEWPORT_CG_EDITOR:
            fps = cgEd->fps;
            break;
        case VIEWPORT_GAME_SCREEN:
            fps = intp->fps;
            break;
        case VIEWPORT_HITBOX_EDITOR:
            fps = 60;
            break;
        default:
            fps = 60;
            eng->viewportMode = VIEWPORT_CG_EDITOR;
            break;
        }
    }
    else
    {
        fps = eng->fps;
    }

    if (fps > eng->fpsLimit)
    {
        fps = eng->fpsLimit;
    }

    return fps;
}

void SetEngineZoom(EngineContext *eng, CGEditorContext *cgEd)
{
    if (eng->viewportMode != VIEWPORT_CG_EDITOR) // zoom only for CG cgEd
    {
        eng->zoom = 1.0f;
        cgEd->zoom = 1.0f;
        return;
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0 && eng->isViewportFocused && !cgEd->menuOpen)
    {
        cgEd->delayFrames = true;

        float zoom = eng->zoom;

        if (wheel > 0 && zoom < 1.5f)
        {
            eng->zoom = zoom + 0.25f;
        }

        if (wheel < 0 && zoom > 0.5f)
        {
            eng->zoom = zoom - 0.25f;
        }

        cgEd->zoom = eng->zoom;
    }
}

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1600, 1000, "RapidEngine");
    SetTargetFPS(140);
    SetExitKey(KEY_NULL);
    char fileName[MAX_FILE_NAME];
    strmac(fileName, MAX_FILE_NAME, "%s", developerMode ? "Tetris" : HandleProjectManager());

    SetTargetFPS(60);

    MaximizeWindow();

    InitAudioDevice();

    EngineContext eng = InitEngineContext();
    CGEditorContext cgEd = InitEditorContext();
    GraphContext graph = InitGraphContext();
    InterpreterContext intp = InitInterpreterContext();
    RuntimeGraphContext runtimeGraph = {0};

    eng.currentPath = SetProjectFolderPath(fileName);

    eng.files = LoadDirectoryFilesEx(eng.currentPath, NULL, false);
    if (!eng.files.paths || eng.files.count <= 0)
    {
        AddToLog(&eng, "Error loading files{E201}", LOG_LEVEL_ERROR);
        EmergencyExit(&eng, &cgEd, &intp);
    }

    PrepareCGFilePath(&eng, fileName);

    intp.projectPath = strmac(NULL, MAX_FILE_PATH, "%s", eng.currentPath);
    eng.projectPath = strmac(NULL, MAX_FILE_PATH, "%s", eng.currentPath);

    if (!LoadGraphFromFile(eng.CGFilePath, &graph))
    {
        AddToLog(&eng, "Failed to load CoreGraph file! Continuing with empty graph{C223}", LOG_LEVEL_ERROR);
        eng.CGFilePath[0] = '\0';
    }
    cgEd.graph = &graph;

    AddToLog(&eng, "All resources loaded. Welcome!{E000}", LOG_LEVEL_NORMAL);

    while (!WindowShouldClose())
    {
        if (!IsWindowReady())
        {
            EmergencyExit(&eng, &cgEd, &intp);
        }

        if(STRING_ALLOCATION_FAILURE){
            AddToLog(&eng, "String allocation failed{O200}", LOG_LEVEL_ERROR);
            EmergencyExit(&eng, &cgEd, &intp);
        }

        ContextChangePerFrame(&eng);

        int prevHoveredUIIndex = eng.hoveredUIElementIndex;
        eng.isAnyMenuOpen = eng.showSaveWarning == 1 || eng.showSettingsMenu;

        if (HandleUICollisions(&eng, &graph, &intp, &cgEd, &runtimeGraph) && !eng.isGameFullscreen)
        {
            if (((prevHoveredUIIndex != eng.hoveredUIElementIndex || IsMouseButtonDown(MOUSE_LEFT_BUTTON)) && eng.showSaveWarning != 1 && eng.showSettingsMenu == false) || eng.delayFrames)
            {
                BuildUITexture(&eng, &graph, &cgEd, &intp, &runtimeGraph);
                eng.fps = 140;
            }
            eng.delayFrames = true;
        }
        else if (eng.delayFrames && !eng.isGameFullscreen)
        {
            BuildUITexture(&eng, &graph, &cgEd, &intp, &runtimeGraph);
            eng.fps = 60;
            eng.delayFrames = false;
        }

        SetMouseCursor(GetEngineMouseCursor(&eng, &cgEd));

        SetTargetFPS(GetEngineFPS(&eng, &cgEd, &intp));

        SetEngineZoom(&eng, &cgEd);

        Vector2 mouseInViewportTex = (Vector2){(eng.mousePos.x - eng.sideBarWidth) / eng.zoom + (eng.viewportTex.texture.width - (eng.isGameFullscreen ? eng.screenWidth : eng.viewportWidth / eng.zoom)) / 2.0f, eng.mousePos.y / eng.zoom + (eng.viewportTex.texture.height - (eng.isGameFullscreen ? eng.screenHeight : eng.viewportHeight / eng.zoom)) / 2.0f};

        Rectangle viewportRecInViewportTex = (Rectangle){
            (eng.viewportTex.texture.width - (eng.isGameFullscreen ? eng.screenWidth : eng.viewportWidth)) / 2.0f,
            (eng.viewportTex.texture.height - (eng.isGameFullscreen ? eng.screenHeight : eng.viewportHeight)) / 2.0f,
            eng.screenWidth - (eng.isGameFullscreen ? 0 : eng.sideBarWidth),
            eng.screenHeight - (eng.isGameFullscreen ? 0 : eng.bottomBarHeight)};

        if (eng.showSaveWarning == 1 || eng.showSettingsMenu)
        {
            eng.isViewportFocused = false;
        }

        BeginDrawing();
        ClearBackground(BLACK);

        switch (eng.viewportMode)
        {
        case VIEWPORT_CG_EDITOR:
        {
            static bool isSecondFrame = false;
            if (cgEd.isFirstFrame)
            {
                isSecondFrame = true;
                cgEd.isFirstFrame = false;
                break;
            }
            if (eng.CGFilePath[0] != '\0' && (eng.isViewportFocused || isSecondFrame))
            {
                cgEd.viewportBoundary = viewportRecInViewportTex;
                HandleEditor(&cgEd, &graph, &eng.viewportTex, mouseInViewportTex, eng.draggingResizeButtonID != 0, isSecondFrame);
            }
            if (isSecondFrame)
            {
                isSecondFrame = false;
            }

            if (eng.isAutoSaveON)
            {
                eng.autoSaveTimer += GetFrameTime();

                if (eng.autoSaveTimer >= 120.0f)
                {
                    if (SaveGraphToFile(eng.CGFilePath, &graph) == 0)
                    {
                        cgEd.hasChanged = false;
                        AddToLog(&eng, "Auto-saved successfully{C301}", LOG_LEVEL_SUCCESS);
                    }
                    else
                    {
                        AddToLog(&eng, "Error saving changes!{C101}", LOG_LEVEL_WARNING);
                    }
                    eng.autoSaveTimer = 0.0f;
                }
            }

            if (cgEd.newLogMessage)
            {
                for (int i = 0; i < cgEd.logMessageCount; i++)
                    AddToLog(&eng, cgEd.logMessages[i], cgEd.logMessageLevels[i]);

                cgEd.newLogMessage = false;
                cgEd.logMessageCount = 0;
                eng.delayFrames = true;
            }
            if (cgEd.engineDelayFrames)
            {
                cgEd.engineDelayFrames = false;
                eng.delayFrames = true;
            }
            if (cgEd.hasChangedInLastFrame)
            {
                eng.delayFrames = true;
                cgEd.hasChangedInLastFrame = false;
                eng.wasBuilt = false;
            }
            if (cgEd.shouldOpenHitboxEditor)
            {
                cgEd.shouldOpenHitboxEditor = false;
                eng.viewportMode = VIEWPORT_HITBOX_EDITOR;
            }

            break;
        }
        case VIEWPORT_GAME_SCREEN:
        {
            if (IsKeyPressed(KEY_P))
            {
                intp.isPaused = !intp.isPaused;
            }
            BeginTextureMode(eng.viewportTex);
            ClearBackground(BLACK);

            eng.isGameRunning = HandleGameScreen(&intp, &runtimeGraph, mouseInViewportTex, viewportRecInViewportTex);

            if (!eng.isGameRunning)
            {
                eng.viewportMode = VIEWPORT_CG_EDITOR;
                cgEd.isFirstFrame = true;
                eng.wasBuilt = false;
                FreeInterpreterContext(&intp);
            }
            if (intp.newLogMessage)
            {
                for (int i = 0; i < intp.logMessageCount; i++)
                    AddToLog(&eng, intp.logMessages[i], intp.logMessageLevels[i]);

                intp.newLogMessage = false;
                intp.logMessageCount = 0;
                eng.delayFrames = true;
            }
            EndTextureMode();

            break;
        }
        case VIEWPORT_HITBOX_EDITOR:
        {
            static HitboxEditorContext hbEd = {0};

            if (hbEd.texture.id == 0)
            {
                eng.delayFrames = true;
                char path[MAX_FILE_PATH];
                strmac(path, MAX_FILE_PATH, "%s%c%s", eng.projectPath, PATH_SEPARATOR, cgEd.hitboxEditorFileName);

                Image img = LoadImage(path);
                if (img.data == NULL)
                {
                    AddToLog(&eng, "Invalid texture file name{H200}", LOG_LEVEL_ERROR);
                    eng.viewportMode = VIEWPORT_CG_EDITOR;
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

                    float scaleX = (float)newWidth / (float)img.width;
                    float scaleY = (float)newHeight / (float)img.height;

                    ImageResize(&img, newWidth, newHeight);

                    Texture2D tex = LoadTextureFromImage(img);
                    UnloadImage(img);

                    Vector2 texPos = (Vector2){
                        eng.viewportTex.texture.width / 2.0f,
                        eng.viewportTex.texture.height / 2.0f};

                    hbEd = InitHitboxEditor(tex, texPos, (Vector2){scaleX, scaleY});

                    for (int i = 0; i < graph.pinCount; i++)
                    {
                        if (graph.pins[i].id == cgEd.hitboxEditingPinID)
                        {
                            hbEd.poly = graph.pins[i].hitbox;
                        }
                    }

                    for (int i = 0; i < hbEd.poly.count; i++)
                    {
                        hbEd.poly.vertices[i].x *= hbEd.scale.x;
                        hbEd.poly.vertices[i].y *= hbEd.scale.y;
                    }
                }
            }

            if (eng.isViewportFocused)
            {
                if (!UpdateHitboxEditor(&hbEd, mouseInViewportTex, &graph, cgEd.hitboxEditingPinID))
                {
                    eng.viewportMode = VIEWPORT_CG_EDITOR;
                    eng.delayFrames = true;
                    break;
                }
            }

            BeginTextureMode(eng.viewportTex);
            DrawHitboxEditor(&hbEd, mouseInViewportTex);
            DrawTextEx(eng.font, "ESC - Save & Exit Hitbox Editor", (Vector2){viewportRecInViewportTex.x + 30, viewportRecInViewportTex.y + 30}, 30, 1, GRAY);
            DrawTextEx(eng.font, "R - reset hitbox", (Vector2){viewportRecInViewportTex.x + 30, viewportRecInViewportTex.y + 70}, 30, 1, GRAY);
            EndTextureMode();

            break;
        }
        default:
        {
            AddToLog(&eng, "Out of bounds enum{O201}", LOG_ERROR);
            eng.viewportMode = VIEWPORT_CG_EDITOR;
        }
        }

        DrawTexturePro(eng.viewportTex.texture,
                       (Rectangle){(eng.viewportTex.texture.width - (eng.isGameFullscreen ? eng.screenWidth : eng.viewportWidth / eng.zoom)) / 2.0f,
                                   (eng.viewportTex.texture.height - (eng.isGameFullscreen ? eng.screenHeight : eng.viewportHeight / eng.zoom)) / 2.0f,
                                   eng.isGameFullscreen ? eng.screenWidth : eng.viewportWidth / eng.zoom,
                                   -(eng.isGameFullscreen ? eng.screenHeight : eng.viewportHeight / eng.zoom)},
                       (Rectangle){eng.isGameFullscreen ? 0 : eng.sideBarWidth,
                                   0,
                                   eng.screenWidth - (eng.isGameFullscreen ? 0 : eng.sideBarWidth),
                                   eng.screenHeight - (eng.isGameFullscreen ? 0 : eng.bottomBarHeight)},
                       (Vector2){0, 0}, 0.0f, WHITE);

        if (eng.uiTex.texture.id != 0 && !eng.isGameFullscreen)
        {
            DrawTextureRec(eng.uiTex.texture, (Rectangle){0, 0, eng.uiTex.texture.width, -eng.uiTex.texture.height}, (Vector2){0, 0}, WHITE);
        }

        if (eng.viewportMode == VIEWPORT_CG_EDITOR)
        {
            DrawTextEx(GetFontDefault(), "CoreGraph", (Vector2){eng.sideBarWidth + 20, 30}, 40, 4, Fade(WHITE, 0.2f));
            DrawTextEx(GetFontDefault(), "TM", (Vector2){eng.sideBarWidth + 230, 20}, 15, 1, Fade(WHITE, 0.2f));
        }

        if (eng.showSaveWarning == 1)
        {
            eng.showSaveWarning = DrawSaveWarning(&eng, &graph, &cgEd);
            if (eng.showSaveWarning == 2)
            {
                eng.shouldCloseWindow = true;
            }
        }
        else if (eng.showSettingsMenu)
        {
            eng.showSettingsMenu = DrawSettingsMenu(&eng, &intp);
        }

        if (eng.shouldShowFPS)
        {
            DrawTextEx(eng.font, TextFormat("%d FPS", GetFPS()), (Vector2){eng.screenWidth / 2, 10}, 40, 1, RED);
        }

        EndDrawing();

        if (eng.shouldCloseWindow)
        {
            break;
        }
    }

    FreeEngineContext(&eng);
    FreeEditorContext(&cgEd);
    FreeInterpreterContext(&intp);

    free(intp.projectPath);
    intp.projectPath = NULL;

    CloseAudioDevice();

    CloseWindow();

    return 0;
}