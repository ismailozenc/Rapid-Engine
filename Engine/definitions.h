#pragma once

#include "raylib.h"

#define MAX_FILE_NAME 256
#define MAX_FILE_PATH 2048

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

#define MAX_LOG_MESSAGES 32

#define MAX_HITBOX_VERTICES 64

typedef struct {
    Vector2 vertices[MAX_HITBOX_VERTICES];
    int count;
    bool isClosed;
} Polygon;