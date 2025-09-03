#ifndef HITBOX_EDITOR_H
#define HITBOX_EDITOR_H

#include "raylib.h"
#include <stdbool.h>
#include "Nodes.h"

typedef struct {
    Texture2D texture;
    Vector2 position;
    Polygon poly;
    int draggingVerticeIndex;
    Vector2 scale;
} HitboxEditorContext;

HitboxEditorContext InitHitboxEditor(Texture2D tex, Vector2 pos, Vector2 scale);
bool UpdateHitboxEditor(HitboxEditorContext *e, Vector2 mouseLocal, GraphContext *graph, int hitboxEditingPinID);
void DrawHitboxEditor(HitboxEditorContext *e, Vector2 mouseLocal);

#endif