#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include "HitboxEditor.h"

#define MAX_VERTICES 64
#define SNAP_DIST 10.0f
#define TEST_RADIUS 10.0f

HitboxEditorContext InitHitboxEditor(Texture2D tex, Vector2 pos, Vector2 scale)
{
    HitboxEditorContext e = {0};
    e.texture = tex;
    e.position = pos;
    e.poly.count = 0;
    e.poly.isClosed = false;
    e.draggingVerticeIndex = -1;
    e.scale = scale;
    return e;
}

static bool IsNear(Vector2 a, Vector2 b, float dist)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return (dx * dx + dy * dy) < (dist * dist);
}

bool UpdateHitboxEditor(HitboxEditorContext *e, Vector2 mouseLocal, GraphContext *graph, int hitboxEditingPinID)
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (e->poly.isClosed)
        {
            for (int i = 0; i < e->poly.count; i++)
            {
                e->poly.vertices[i].x /= e->scale.x;
                e->poly.vertices[i].y /= e->scale.y;
            }
            for (int i = 0; i < graph->pinCount; i++)
            {
                if (graph->pins[i].id == hitboxEditingPinID)
                {
                    graph->pins[i].hitbox = e->poly;
                }
            }
            UnloadTexture(e->texture);
            e->texture.id = 0;
        }
        return false;
    }
    if (IsKeyPressed(KEY_R))
    {
        e->poly.count = 0;
        e->poly.isClosed = false;
    }
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z))
    {
        if (e->poly.count != 0)
        {
            e->poly.count--;
            e->poly.isClosed = false;
        }
    }

    if (e->poly.isClosed)
    {

        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            e->draggingVerticeIndex = -1;
            return true;
        }

        if (e->draggingVerticeIndex == -1)
        {
            for (int i = 0; i < e->poly.count; i++)
            {
                Vector2 vertice = {
                    e->position.x + e->poly.vertices[i].x,
                    e->position.y + e->poly.vertices[i].y};

                if (IsNear(mouseLocal, vertice, SNAP_DIST))
                {
                    e->draggingVerticeIndex = i;
                }
            }

            if (e->draggingVerticeIndex == -1)
            {
                return true;
            }
        }

        e->poly.vertices[e->draggingVerticeIndex].x += GetMouseDelta().x;
        e->poly.vertices[e->draggingVerticeIndex].y += GetMouseDelta().y;
    }
    else
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 rel = {
                mouseLocal.x - e->position.x,
                mouseLocal.y - e->position.y};

            if (e->poly.count > 2)
            {
                Vector2 firstScreen = {
                    e->position.x + e->poly.vertices[0].x,
                    e->position.y + e->poly.vertices[0].y};
                if (IsNear(mouseLocal, firstScreen, SNAP_DIST))
                {
                    e->poly.isClosed = true;
                    return true;
                }
            }

            if (e->poly.count < MAX_VERTICES && !e->poly.isClosed)
            {
                e->poly.vertices[e->poly.count++] = rel;
            }
        }
    }

    return true;
}

void DrawHitboxEditor(HitboxEditorContext *e, Vector2 mouseLocal)
{
    ClearBackground((Color){80, 0, 90, 100});

    DrawTexture(
        e->texture,
        (int)(e->position.x - e->texture.width / 2),
        (int)(e->position.y - e->texture.height / 2),
        WHITE);

    for (int i = 0; i < e->poly.count; i++)
    {
        Vector2 p = {
            e->position.x + e->poly.vertices[i].x,
            e->position.y + e->poly.vertices[i].y};
        DrawCircle(p.x, p.y, 4, GREEN);

        if (i > 0)
        {
            Vector2 prev = {
                e->position.x + e->poly.vertices[i - 1].x,
                e->position.y + e->poly.vertices[i - 1].y};
            DrawLineEx(prev, p, 2, WHITE);
        }
    }

    if (e->poly.isClosed && e->poly.count > 2)
    {
        Vector2 first = {
            e->position.x + e->poly.vertices[0].x,
            e->position.y + e->poly.vertices[0].y};
        Vector2 last = {
            e->position.x + e->poly.vertices[e->poly.count - 1].x,
            e->position.y + e->poly.vertices[e->poly.count - 1].y};
        DrawLineEx(last, first, 2, WHITE);

        for (int i = 0; i < e->poly.count; i++)
        {
            Vector2 vertice = {
                e->position.x + e->poly.vertices[i].x,
                e->position.y + e->poly.vertices[i].y};

            if (IsNear(mouseLocal, vertice, SNAP_DIST))
            {
                DrawCircleLines(vertice.x, vertice.y, SNAP_DIST, GREEN);
            }
        }
    }
    else if (e->poly.count > 0)
    {
        Vector2 last = {
            e->position.x + e->poly.vertices[e->poly.count - 1].x,
            e->position.y + e->poly.vertices[e->poly.count - 1].y};

        DrawLineEx(last, mouseLocal, 1, GRAY);

        if (e->poly.count >= 3)
        {
            Vector2 first = {
                e->position.x + e->poly.vertices[0].x,
                e->position.y + e->poly.vertices[0].y};
            if (IsNear(mouseLocal, first, SNAP_DIST))
            {
                DrawCircleLines(first.x, first.y, SNAP_DIST, GREEN);
            }
        }
    }
}