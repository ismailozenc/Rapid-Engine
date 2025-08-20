#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include "HitboxEditor.h"

#define MAX_VERTICES 64
#define SNAP_DIST 10.0f
#define TEST_RADIUS 10.0f

HitboxEditor InitHitboxEditor(Texture2D tex, Vector2 pos)
{
    HitboxEditor e = {0};
    e.texture = tex;
    e.position = pos;
    e.poly.count = 0;
    e.poly.closed = false;
    return e;
}

void AddVertex(HitboxEditor *e, Vector2 v)
{
    if (e->poly.count < MAX_VERTICES && !e->poly.closed)
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

bool CheckCollisionPolyPolyTest(Polygon *a, Vector2 aOffset, Polygon *b, Vector2 bOffset) {
    for (int i = 0; i < a->count; i++) {
        Vector2 a1 = { a->vertices[i].x + aOffset.x, a->vertices[i].y + aOffset.y };
        Vector2 a2 = { a->vertices[(i + 1) % a->count].x + aOffset.x, a->vertices[(i + 1) % a->count].y + aOffset.y };

        for (int j = 0; j < b->count; j++) {
            Vector2 b1 = { b->vertices[j].x + bOffset.x, b->vertices[j].y + bOffset.y };
            Vector2 b2 = { b->vertices[(j + 1) % b->count].x + bOffset.x, b->vertices[(j + 1) % b->count].y + bOffset.y };

            if (CheckCollisionLines(a1, a2, b1, b2, NULL)) return true;
        }
    }
    return false;
}

void UpdateEditor(HitboxEditor *e, Vector2 mouseLocal)
{
    if (e->poly.closed)
        return;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        Vector2 mouse = mouseLocal;
        Vector2 rel = {mouse.x - e->position.x - e->texture.width / 2, mouse.y - e->position.y - e->texture.height / 2};

        if (e->poly.count > 2)
        {
            Vector2 firstScreen = {
                e->poly.vertices[0].x + e->position.x + e->texture.width / 2,
                e->poly.vertices[0].y + e->position.y + e->texture.height / 2};
            if (IsNear(mouse, firstScreen, SNAP_DIST))
            {
                e->poly.closed = true;
                return;
            }
        }

        AddVertex(e, rel);
    }
}

void DrawEditor(HitboxEditor *e, Vector2 mouseLocal)
{
    ClearBackground(BLACK);

    DrawTexture(e->texture, e->position.x, e->position.y, WHITE);

    for (int i = 0; i < e->poly.count; i++)
    {
        Vector2 p = {
            e->position.x + e->poly.vertices[i].x + e->texture.width / 2,
            e->position.y + e->poly.vertices[i].y + e->texture.height / 2
        };
        DrawCircle(p.x, p.y, 4, BLUE);

        if (i > 0)
        {
            Vector2 prev = {
                e->position.x + e->poly.vertices[i - 1].x + e->texture.width / 2,
                e->position.y + e->poly.vertices[i - 1].y + e->texture.height / 2
            };
            DrawLineEx(prev, p, 2, RED);
        }
    }

    if (e->poly.closed && e->poly.count > 2)
    {
        Vector2 first = {
            e->position.x + e->poly.vertices[0].x + e->texture.width / 2,
            e->position.y + e->poly.vertices[0].y + e->texture.height / 2
        };
        Vector2 last = {
            e->position.x + e->poly.vertices[e->poly.count - 1].x + e->texture.width / 2,
            e->position.y + e->poly.vertices[e->poly.count - 1].y + e->texture.height / 2
        };
        DrawLineEx(last, first, 2, RED);
    }
    else if (e->poly.count > 0)
    {
        Vector2 last = {
            e->position.x + e->poly.vertices[e->poly.count - 1].x + e->texture.width / 2,
            e->position.y + e->poly.vertices[e->poly.count - 1].y + e->texture.height / 2
        };
        
        DrawLineEx(last, mouseLocal, 1, GRAY);

        if (e->poly.count >= 3)
        {
            Vector2 first = {
                e->position.x + e->poly.vertices[0].x + e->texture.width / 2,
                e->position.y + e->poly.vertices[0].y + e->texture.height / 2
            };
            if (IsNear(mouseLocal, first, SNAP_DIST))
            {
                DrawCircleLines(first.x, first.y, SNAP_DIST, GREEN);
            }
        }
    }
}

Polygon CircleToPoly(Vector2 center, float radius, int sides) {
    Polygon poly = {0};
    poly.count = sides;
    poly.closed = true;

    for (int i = 0; i < sides; i++) {
        float angle = (2 * PI * i) / sides;
        poly.vertices[i].x = center.x + cosf(angle) * radius;
        poly.vertices[i].y = center.y + sinf(angle) * radius;
    }

    return poly;
}

/*int main()
{
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1000, 1000, "Sprite Collision Editor Test");
    SetTargetFPS(1000);

    Texture2D tex = LoadTexture("donkeyKong.png");
    HitboxEditor editor = InitEditor(tex, (Vector2){(GetScreenWidth() - tex.width) / 2.0f, (GetScreenHeight() - tex.height) / 2.0f});

    while (!WindowShouldClose())
    {
        UpdateEditor(&editor);

        Vector2 mouse = GetMousePosition();
        Color background = RAYWHITE;

        Polygon cursorPoly = CircleToPoly(mouse, TEST_RADIUS, 16);

        if (editor.poly.closed)
        {
            if (CheckCollisionPolyPoly(&cursorPoly, (Vector2){0,0}, &editor.poly, editor.position)){ //a
                background = (Color){200, 255, 200, 255};
            }
        }

        BeginDrawing();
        DrawEditor(&editor, background);

        if (editor.poly.closed)
        {
            DrawCircle(mouse.x, mouse.y, TEST_RADIUS, Fade(BLUE, 0.5f));
        }

        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE) && editor.poly.closed){break;}

        DrawFPS(10, 10);
    }

    UnloadTexture(tex);
    CloseWindow();
    return 0;
}*/