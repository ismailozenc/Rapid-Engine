#pragma once

#include "raylib.h"
#include "definitions.h"

typedef struct ProjectOptions
{
    char projectName[MAX_FILE_NAME];
    bool is3D;
} ProjectOptions;

typedef enum{
    PROJECT_MANAGER_WINDOW_MODE_MAIN,
    PROJECT_MANAGER_WINDOW_MODE_LOAD,
    PROJECT_MANAGER_WINDOW_MODE_CREATE,
    PROJECT_MANAGER_WINDOW_MODE_EXIT
}ProjectManagerWindowMode;

char *HandleProjectManager();