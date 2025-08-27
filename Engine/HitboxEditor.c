#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include "HitboxEditor.h"

#define MAX_VERTICES 64
#define SNAP_DIST 10.0f
#define TEST_RADIUS 10.0f

HitboxEditorContext InitHitboxEditor(Texture2D tex, Vector2 pos)
{
    HitboxEditorContext e = {0};
    e.texture = tex;
    e.position = pos;
    e.poly.count = 0;
    e.poly.isClosed = false;
    e.draggingVerticeIndex = -1;
    return e;
}

void AddVertex(HitboxEditorContext *e, Vector2 v)
{
    if (e->poly.count < MAX_VERTICES && !e->poly.isClosed)
    {
        e->poly.vertices[e->poly.count++] = v;
    }
}

static bool IsNear(Vector2 a, Vector2 b, float dist)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return (dx * dx + dy * dy) < (dist * dist);
}

void UpdateEditor(HitboxEditorContext *e, Vector2 mouseLocal)
{
    if (e->poly.isClosed)
    {

        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            e->draggingVerticeIndex = -1;
            return;
        }

        if (e->draggingVerticeIndex == -1)
        {
            for (int i = 0; i < e->poly.count; i++)
            {
                Vector2 vertice = {
                    (e->position.x - e->texture.width / 2) + e->poly.vertices[i].x,
                    (e->position.y - e->texture.height / 2) + e->poly.vertices[i].y};

                if (IsNear(mouseLocal, vertice, SNAP_DIST))
                {
                    e->draggingVerticeIndex = i;
                }
            }

            if (e->draggingVerticeIndex == -1)
            {
                return;
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
                mouseLocal.x - (e->position.x - e->texture.width / 2),
                mouseLocal.y - (e->position.y - e->texture.height / 2)};

            if (e->poly.count > 2)
            {
                Vector2 firstScreen = {
                    (e->position.x - e->texture.width / 2) + e->poly.vertices[0].x,
                    (e->position.y - e->texture.height / 2) + e->poly.vertices[0].y};
                if (IsNear(mouseLocal, firstScreen, SNAP_DIST))
                {
                    e->poly.isClosed = true;
                    return;
                }
            }

            AddVertex(e, rel);
        }
    }
}

void DrawEditor(HitboxEditorContext *e, Vector2 mouseLocal)
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
            (e->position.x - e->texture.width / 2) + e->poly.vertices[i].x,
            (e->position.y - e->texture.height / 2) + e->poly.vertices[i].y};
        DrawCircle(p.x, p.y, 4, GREEN);

        if (i > 0)
        {
            Vector2 prev = {
                (e->position.x - e->texture.width / 2) + e->poly.vertices[i - 1].x,
                (e->position.y - e->texture.height / 2) + e->poly.vertices[i - 1].y};
            DrawLineEx(prev, p, 2, WHITE);
        }
    }

    if (e->poly.isClosed && e->poly.count > 2)
    {
        Vector2 first = {
            (e->position.x - e->texture.width / 2) + e->poly.vertices[0].x,
            (e->position.y - e->texture.height / 2) + e->poly.vertices[0].y};
        Vector2 last = {
            (e->position.x - e->texture.width / 2) + e->poly.vertices[e->poly.count - 1].x,
            (e->position.y - e->texture.height / 2) + e->poly.vertices[e->poly.count - 1].y};
        DrawLineEx(last, first, 2, WHITE);

        for (int i = 0; i < e->poly.count; i++)
        {
            Vector2 vertice = {
                (e->position.x - e->texture.width / 2) + e->poly.vertices[i].x,
                (e->position.y - e->texture.height / 2) + e->poly.vertices[i].y};

            if (IsNear(mouseLocal, vertice, SNAP_DIST))
            {
                DrawCircleLines(vertice.x, vertice.y, SNAP_DIST, GREEN);
            }
        }
    }
    else if (e->poly.count > 0)
    {
        Vector2 last = {
            (e->position.x - e->texture.width / 2) + e->poly.vertices[e->poly.count - 1].x,
            (e->position.y - e->texture.height / 2) + e->poly.vertices[e->poly.count - 1].y};

        DrawLineEx(last, mouseLocal, 1, GRAY);

        if (e->poly.count >= 3)
        {
            Vector2 first = {
                (e->position.x - e->texture.width / 2) + e->poly.vertices[0].x,
                (e->position.y - e->texture.height / 2) + e->poly.vertices[0].y};
            if (IsNear(mouseLocal, first, SNAP_DIST))
            {
                DrawCircleLines(first.x, first.y, SNAP_DIST, GREEN);
            }
        }
    }
}

Polygon CircleToPoly(Vector2 center, float radius, int sides)
{
    Polygon poly = {0};
    poly.count = sides;
    poly.isClosed = true;

    for (int i = 0; i < sides; i++)
    {
        float angle = (2 * PI * i) / sides;
        poly.vertices[i].x = center.x + cosf(angle) * radius;
        poly.vertices[i].y = center.y + sinf(angle) * radius;
    }

    return poly;
}