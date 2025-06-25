#ifndef PROJECT_MANAGER_H
#define PROJECT_MANAGER_H

#include "raylib.h"
#include "shell_execute.h"

#define MAX_PROJECT_NAME 99

typedef struct ProjectOptions
{
    char projectName[MAX_PROJECT_NAME];
    bool is3D;
} ProjectOptions;

void DrawMovingDotAlongRectangle();

void DrawX(Vector2 center, float size, float thickness, Color color);

void DrawTopButtons();

bool hasCursorMoved(Vector2 lastMousePos);

int MainWindow(Vector2 mousePoint, Font font, Font fontRE);

int WindowLoadProject(char *projectFileName, Font font);

int CreateProject(ProjectOptions PO);

int WindowCreateProject(char *projectFileName, Font font);

char *handleProjectManager();

#endif