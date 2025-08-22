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
} HitboxEditor;

HitboxEditor InitHitboxEditor(Texture2D tex, Vector2 pos);
void AddVertex(HitboxEditor *e, Vector2 v);
bool CheckCollisionPolyPolyTest(Polygon *a, Vector2 aOffset, Polygon *b, Vector2 bOffset);
void UpdateEditor(HitboxEditor *e, Vector2 mouseLocal);
void DrawEditor(HitboxEditor *e, Vector2 mouseLocal);
Polygon CircleToPoly(Vector2 center, float radius, int sides);

#endif