#pragma once

#include <stdlib.h>
#include <stdio.h>
#include "raylib.h"
#include "local_config.h"
#include "resources/resources.h"

#if DEVELOPER_MODE
static bool developerMode = true;
#else
static bool developerMode = false;
#endif

#define MAX_FILE_NAME 256
#define MAX_FILE_PATH 2048

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#include <direct.h>
#define MAKE_DIR(path) _mkdir(path)
#define GetCWD _getcwd

void* __stdcall ShellExecuteA(void* hwnd, const char* lpOperation, const char* lpFile, const char* lpParameters, const char* lpDirectory, int nShowCmd);

static inline void OpenFile(const char* filePath) {
    ShellExecuteA(NULL, "open", filePath, NULL, NULL, 1);
}

#elif __APPLE__
#define PATH_SEPARATOR '/'
#include <sys/stat.h>
#include <sys/types.h>
#define MAKE_DIR(path) mkdir(path, 0755)
#define GetCWD getcwd

static inline void OpenFile(const char* filePath) {
    char command[1024];
    snprintf(command, sizeof(command), "open \"%s\"", filePath);
    system(command);
}

#elif __unix__
#define PATH_SEPARATOR '/'
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#define MAKE_DIR(path) mkdir(path, 0755)
#define GetCWD getcwd

static inline void OpenFile(const char* filePath) {
    char command[1024];
    snprintf(command, sizeof(command), "xdg-open \"%s\"", filePath);
    system(command);
}

#else
#error "Rapid Engine supports only Windows, macOS, and Unix-like systems"
#endif

#define MAX_LOG_MESSAGES 32

#define MAX_HITBOX_VERTICES 64

typedef struct {
    Vector2 vertices[MAX_HITBOX_VERTICES];
    int count;
    bool isClosed;
} Polygon;

typedef enum
{
    LOG_LEVEL_NORMAL,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_SAVE,
    LOG_LEVEL_DEBUG
}LogLevel;