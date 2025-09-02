#ifndef HITBOX_EDITOR_H
#define HITBOX_EDITOR_H

#include "raylib.h"
#include <stdbool.h>
#include "Nodes.h"

#define SNAP_DIST 10.0f
#define TEST_RADIUS 10.0f

typedef struct {
    Texture2D texture;
    Vector2 position;
    Polygon poly;
    int draggingVerticeIndex;
    Vector2 scale;
} HitboxEditorContext;

HitboxEditorContext InitHitboxEditor(Texture2D tex, Vector2 pos, Vector2 scale);
void AddVertex(HitboxEditorContext *e, Vector2 v);
bool CheckCollisionPolyPolyTest(Polygon *a, Vector2 aOffset, Polygon *b, Vector2 bOffset);
bool UpdateHitboxEditor(HitboxEditorContext *e, Vector2 mouseLocal, GraphContext *graph, int hitboxEditingPinID);
void DrawHitboxEditor(HitboxEditorContext *e, Vector2 mouseLocal);

#endif