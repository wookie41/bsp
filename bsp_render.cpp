#define FRONT_CHILD(node) ((BSPNode*)(((uint8_t*)node) + node->data.children.frontChildOffset))
#define BACK_CHILD(node) ((BSPNode*)(((uint8_t*)node) + node->data.children.backChildOffset))
#define RAD(angle) ((angle)*M_PI / 180.0)

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <fstream>

#include "GL/glew.h"
#include "SDL2/SDL.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_DS_IMPLEMENTATION

#include "stb_ds.h"
#include "stb/stb_image.h"

#define WH_GL_IMPLEMENTATION
#define WH_FS_IMPLEMENTATION

#include "wh/fs.hpp"
#include "wh/gl.hpp"

#include <cstdint>

#define LEFT_CHILD(node) (((uint32*)node) + 10)
#define LEFT_CHILD(node) (((uint32*)node) + 10)

static const uint16_t WINDOW_WIDTH = 800;
static const uint16_t WINDOW_HEIGHT = 800;

static const float WINDOW_WIDTH_F = 800.f;
static const float WINDOW_HEIGHT_F = 800.f;
static const float WALL_DIVIDE_CONST = 30000.f;


struct vec2
{
    float x = 0, y = 0;
};

struct vec3
{
    float x = 0, y = 0, z = 0;
};

void randomColor(vec3* color);

char vertPath[] = "basic.vert";
char fragPath[] = "basic.frag";

wh::SDLContext context;
GLuint basicShader;
GLuint quadVAO, quadVBO;
GLuint positionsBuffer, indicesBuffer, colorsBuffer;

#pragma pack(push, 1)

struct BSPLines
{
    vec3 wallColor;
    uint16_t numElements = 0;
    uint32_t elements[];
};

struct BSPNode;

struct BSPChildren
{
    uint32_t splitter[2];
    uint32_t frontChildOffset;
    uint32_t backChildOffset;
};

union BSPData {
    BSPChildren children;
    BSPLines lines;
};

struct BSPNode
{
    bool isLeaf;
    BSPData data;
};
#pragma pack(pop)

struct Map
{
    uint32_t numVertices;
    vec2* vertices;
    BSPNode* root;
};

struct Player
{
    float fov;
    float angle;
    float viewDistance;
    float focalLength;
    vec2 pos;
};

bool isPointInFront(Map* map, uint32_t* lineIndices, vec2* pos);

static vec3 outputPPM[WINDOW_WIDTH][WINDOW_HEIGHT];

bool render(Map* map, BSPNode* node, Player* player);
inline float cross(vec2* a, vec2* b);


// position + texCoords
static float quadData[] = {
    -1.0f, 1.0f, 0.0f, 0.0f, -1.0f, -1.0f, 1.0f, 0.0f,
    1.0f,  1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 1.0f,
};

int main(int argc, char** argv)
{
    time_t t;
    srand((unsigned)time(&t));

    wh::SDLContext context;
    wh::initSDLContext(&context, WINDOW_WIDTH, WINDOW_HEIGHT);

    glClearColor(0.f, 0.f, 0.f, 1.f);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    SDL_Event event;
    bool isRunning = true;

    Map map;
    FILE* mapFile = fopen(argv[1], "rb");

    size_t r = fread(&map, sizeof(uint32_t), 1, mapFile);
    map.vertices = new vec2[map.numVertices];
    r = fread(map.vertices, sizeof(vec2), map.numVertices, mapFile);

    size_t currentPos = ftell(mapFile);
    fseek(mapFile, 0, SEEK_END);
    size_t treeSize = ftell(mapFile) - currentPos;
    fseek(mapFile, currentPos, SEEK_SET);

    map.root = (BSPNode*)malloc(treeSize);

    fread(map.root, treeSize, 1, mapFile);
    fclose(mapFile);


    Player player;
    player.fov = 90.f;
    player.viewDistance = 1000.f;
    player.pos = { 125.f, 125.f };
    player.angle = 90.f;
    player.focalLength = 300.f;


    GLuint basicShader = wh::compileShader(vertPath, fragPath);
    glUseProgram(basicShader);

    GLuint quadVBO, quadVAO;
    glGenVertexArrays(1, &quadVAO);
    glBindVertexArray(quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadData), quadData, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(sizeof(float) * 2));

    GLuint sceneTexture;
    glGenTextures(1, &sceneTexture);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGB, GL_FLOAT, outputPPM);
    glGenerateMipmap(GL_TEXTURE_2D);
    glUniform1i(glGetUniformLocation(basicShader, "tex"), 0);

    float dt = 1 / 60.f;
    while (isRunning)
    {

        memset(outputPPM, 0, sizeof(outputPPM));
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_KEYDOWN:

                switch (event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    isRunning = false;
                    break;

                case SDLK_w:
                    player.pos.y += (500.f * dt);
                    break;


                case SDLK_s:
                    player.pos.y -= (500.f * dt);
                    break;


                case SDLK_a:
                    player.pos.x += (500.f * dt);
                    break;


                case SDLK_d:
                    player.pos.x -= (500.f * dt);
                    break;


                case SDLK_LEFT:
                    player.angle += (100.f * dt);
                    break;

                case SDLK_RIGHT:
                    player.angle -= (100.f * dt);
                    break;
                }
            }
        }

        render(&map, map.root, &player);

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GL_RGB, GL_FLOAT, outputPPM);

        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        SDL_GL_SwapWindow(context.window);
    }

    SDL_DestroyWindow(context.window);
    SDL_Quit();
    return 0;
}

bool render(Map* map, BSPNode* node, Player* player)
{
    if (node->isLeaf)
    {
        uint32_t* lines = node->data.lines.elements;
        for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
        {
            for (uint32_t line = 0; line < node->data.lines.numElements; line += 2)
            {
                float progress = ((float)x) / WINDOW_WIDTH_F;
                float degrees = player->angle + (player->fov / 2) - (progress * player->fov);
                float rayAngle = RAD(degrees);

                vec2* lineStart = map->vertices + lines[line];
                vec2* lineEnd = map->vertices + lines[line + 1];

                float raySin = sinf(rayAngle);
                float rayCos = cosf(rayAngle);

                float playerSin = sinf(RAD(player->angle));
                float playerCos = cosf(RAD(player->angle));

                vec2& rayStart = player->pos;
                vec2 rayEnd{ rayStart.x + (player->viewDistance * rayCos),
                             rayStart.y + (player->viewDistance * raySin) };

                vec2 r = { rayEnd.x - rayStart.x, rayEnd.y - rayStart.y };
                vec2 s = { lineEnd->x - lineStart->x, lineEnd->y - lineStart->y };

                float SCrossR = cross(&s, &r);
                float u = (cross(&rayStart, &r) - cross(lineStart, &r)) / SCrossR;
                float t = -(cross(lineStart, &s) - cross(&rayStart, &s)) / SCrossR;

                if (u < 0.f || u > 1.f || t < 0.f || t > 1.f)
                    continue; // no intersection

                vec2 intersectionPoint = { rayStart.x + (r.x * t), rayStart.y + (r.y * t) };
                vec2 toIntersection = { intersectionPoint.x - rayStart.x,
                                        intersectionPoint.y - rayStart.y };

                float distanceToWall = (toIntersection.x * playerCos) + (toIntersection.y * playerSin);

                int persp = (WALL_DIVIDE_CONST / distanceToWall) / 2;
                int drawStart = (WINDOW_HEIGHT / 2) - persp;
                int drawEnd = (WINDOW_HEIGHT / 2) + persp;

                drawEnd = drawEnd > WINDOW_HEIGHT ? WINDOW_HEIGHT : drawEnd;
                drawStart = drawStart < 0 ? 0 : drawStart;

                for (uint32_t y = drawStart; y < drawEnd; ++y)
                {
                    if (outputPPM[x][y].x > 0)
                        continue; // TODO cheap trick not to overdraw, think about it
                    outputPPM[x][y] = node->data.lines.wallColor;
                }
                break;
            }
            // TODO RENDER BACK
        }
        return true;
    }

    uint32_t* splitter = node->data.children.splitter;

    BSPNode* front = FRONT_CHILD(node);
    BSPNode* back = BACK_CHILD(node);

    if (!isPointInFront(map, splitter, &player->pos))
    {
        BSPNode* tmp = front;
        front = back;
        back = tmp;
    }

    if (render(map, front, player))
    {
        render(map, back, player);
        return true;
    }

    return false;
}

// checking which side are we on, using the 2D cross product
// if cross(v, u) > 0 it means that we're in front of the line and < 0 mean we're behind
bool isPointInFront(Map* map, uint32_t* lineIndices, vec2* point)
{
    vec2* lineStart = map->vertices + lineIndices[0];
    vec2* lineEnd = map->vertices + lineIndices[1];

    vec2 pointVec{ point->x - lineStart->x, point->y - lineStart->y };
    vec2 lineVec{ lineEnd->x - lineStart->x, lineEnd->y - lineStart->y };

    return lineVec.x * pointVec.y > lineVec.y * pointVec.x;
}

float cross(vec2* a, vec2* b) { return (a->x * b->y) - (b->x * a->y); }