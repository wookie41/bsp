#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <time.h>

#include "SDL2/SDL.h"
#include "GL/glew.h"

#define STB_IMAGE_IMPLEMENTATION

#include "stb/stb_image.h"

#define WH_GL_IMPLEMENTATION
#define WH_FS_IMPLEMENTATION

#include "wh/fs.hpp"
#include "wh/gl.hpp"

const uint16_t WINDOW_WIDTH = 800;
const uint16_t WINDOW_HEIGHT = 600;

struct vec2
{
    float x = 0, y = 0;
};

static vec2 mapVertices[400] = { { 100.f, 100.f }, { 700.f, 100.f }, { 700.f, 500.f },
                                 { 100.f, 500.f }, { 500.f, 150.f }, { 550.f, 200.f },
                                 { 500.f, 250.f }, { 450.f, 200.f } };

int mapVerticesCount = 16;
static uint32_t mapIndices[]{ 0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4 };

// shapeVerts is a list of mapVertices indicies that make the shape
bool isConvex(uint32_t* shapeVerts, uint16_t numVerts);

struct BSPNode;

struct BSPChildren
{
    BSPNode* frontChild;
    BSPNode* backChild;
};

struct BSPLines
{
    uint16_t numVertices = 0;
    uint32_t* verticesIndecies = nullptr;
};

union BSPData {
    BSPChildren* children;
    BSPLines* lines;
};

struct BSPNode
{
    bool isLeaf;
    BSPData data;
};

BSPNode* partitionSpace(BSPLines* lines);
uint16_t pickSplitter(BSPLines* lines);
bool isPointInFront(uint32_t* lineIndices, uint32_t pointIdx);
void render(BSPNode* node);
void renderShape(BSPLines* shape);

wh::SDLContext context;
GLuint basicShader;
GLuint mapVBO, mapVAO;

int main(int argc, char** argv)
{
    time_t t;
    srand((unsigned)time(&t));

    wh::initSDLContext(&context, WINDOW_WIDTH, WINDOW_HEIGHT);

    glClearColor(0.f, 0.f, 0.f, 1.f);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    // TODO calculate based on window
    GLfloat projectionMatrix[4][4] = { { 2.f / (float)WINDOW_WIDTH, 0, 0, 0 },
                                       { 0, 2.f / WINDOW_HEIGHT, 0, 0 },
                                       { 0, 0, -0.00200019986f, 0 },
                                       { -1, -1, -1.00019991, 1 } };

    SDL_Event event;
    bool isRunning = true;

    BSPLines* initialMapLines = new BSPLines;
    initialMapLines->verticesIndecies = new uint32_t[16];
    initialMapLines->numVertices = mapVerticesCount;
    memcpy(initialMapLines->verticesIndecies, mapIndices, sizeof(mapIndices));

    basicShader = wh::compileShader("basic.vert", "basic.frag");
    glUseProgram(basicShader);
    glUniformMatrix4fv(glGetUniformLocation(basicShader, "projection"), 1, GL_FALSE, (GLfloat*)projectionMatrix);

    glGenVertexArrays(1, &mapVAO);
    glGenBuffers(1, &mapVBO);

    glBindVertexArray(mapVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mapVBO);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    BSPNode* root = partitionSpace(initialMapLines);

    glBufferData(GL_ARRAY_BUFFER, sizeof(mapVertices), mapVertices, GL_DYNAMIC_DRAW);

    glClear(GL_COLOR_BUFFER_BIT);
    render(root);
    SDL_GL_SwapWindow(context.window);

    while (isRunning)
    {
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
                }
            }
        }
    }

    SDL_DestroyWindow(context.window);
    SDL_Quit();
    return 0;
}

inline float dot(vec2 const* v1, vec2 const* v2) { return (v1->x * v2->x) + (v1->y * v2->y); }

bool isConvex(uint32_t* shapeVerts, uint16_t numVerts)
{
    bool hadNegativeDot = false;
    bool hadPositiveDot = false;

    for (uint16_t vertIdx = 0; vertIdx < numVerts; vertIdx += 2)
    {
        uint16_t prevVertIdx = vertIdx == 0 ? numVerts - 2 : vertIdx - 2;
        uint16_t nextVertIdx = vertIdx == vertIdx + 1;

        vec2* prevVert = mapVertices + shapeVerts[prevVertIdx];
        vec2* currVert = mapVertices + shapeVerts[vertIdx];
        vec2* nextVert = mapVertices + shapeVerts[nextVertIdx];

        vec2 v1 = { currVert->x - prevVert->x, currVert->y - prevVert->y };
        vec2 v2 = { currVert->x - nextVert->x, currVert->y - nextVert->y };

        float currDot = dot(&v1, &v2);
        if (currDot > 0.f)
            hadPositiveDot = true;
        else if (currDot < 0.f)
            hadNegativeDot = true;

        if (hadNegativeDot && hadPositiveDot)
        {
            return false;
        }
    }
    return true;
}


BSPNode* partitionSpace(BSPLines* lines)
{
    BSPNode* node = new BSPNode;
    node->isLeaf = isConvex(lines->verticesIndecies, lines->numVertices);

    if (node->isLeaf)
    {
        node->data.lines = lines;
        return node;
    }

    uint16_t splitterIdx = pickSplitter(lines);

    BSPLines* front = new BSPLines;
    BSPLines* back = new BSPLines;

    front->verticesIndecies = new uint32_t[lines->numVertices];
    back->verticesIndecies = new uint32_t[lines->numVertices];

    BSPLines splitter;
    splitter.verticesIndecies = lines->verticesIndecies + splitterIdx;
    splitter.numVertices = 2;

    for (uint16_t lineIdx = 0; lineIdx < lines->numVertices; lineIdx += 2)
    {
        bool lineStartsInFront =
        isPointInFront(&lines->verticesIndecies[splitterIdx], lines->verticesIndecies[lineIdx]);
        bool lineEndsInFront =
        isPointInFront(&lines->verticesIndecies[splitterIdx], lines->verticesIndecies[lineIdx + 1]);

        vec2* lineStart = &mapVertices[lines->verticesIndecies[lineIdx]];
        vec2* lineEnd = &mapVertices[lines->verticesIndecies[lineIdx + 1]];

        if (lineStartsInFront == lineEndsInFront)
        {
            BSPLines* side = lineStartsInFront ? front : back;
            side->verticesIndecies[side->numVertices++] = lines->verticesIndecies[lineIdx];
            side->verticesIndecies[side->numVertices++] = lines->verticesIndecies[lineIdx + 1];
        }
        else
        {
            vec2* splitterStart = &mapVertices[lines->verticesIndecies[splitterIdx]];
            vec2* splitterEnd = &mapVertices[lines->verticesIndecies[splitterIdx + 1]];

            vec2* lineStart = &mapVertices[lines->verticesIndecies[lineIdx]];
            vec2* lineEnd = &mapVertices[lines->verticesIndecies[lineIdx + 1]];

            float splitterRise = splitterEnd->y - splitterStart->y;
            float splitterRun = splitterEnd->x - splitterStart->x;
            float splitterSlope = splitterRise / splitterRun;

            float lineRise = lineEnd->y - lineStart->y;
            float lineRun = lineEnd->x - lineStart->x;
            float lineSlope = lineRise / lineRun;

            float slope;
            vec2* start;

            float intersectionX;
            if (splitterRun == 0.f)
            {
                slope = lineSlope;
                start = lineStart;
                intersectionX = splitterStart->x;
            }
            else if (lineRun == 0.f)
            {
                slope = splitterSlope;
                start = splitterStart;
                intersectionX = lineStart->x;
            }
            else
            {
                slope = splitterSlope;
                start = splitterStart;

                intersectionX = ((splitterStart->y - (splitterSlope * splitterStart->x)) -
                                 (lineStart->y - (lineSlope * lineStart->x))) /
                                (lineSlope - splitterSlope);
            }

            float intersectionY = (slope * (intersectionX - start->x)) + start->y;

            uint32_t intersectionPointIdx = mapVerticesCount++;
            mapVertices[intersectionPointIdx] = { intersectionX, intersectionY };

            {
                BSPLines* f = lineStartsInFront ? front : back;
                BSPLines* b = lineStartsInFront ? back : front;

                f->verticesIndecies[f->numVertices++] = lines->verticesIndecies[lineIdx];
                f->verticesIndecies[f->numVertices++] = intersectionPointIdx;

                b->verticesIndecies[f->numVertices++] = intersectionPointIdx;
                b->verticesIndecies[f->numVertices++] = lines->verticesIndecies[lineIdx + 1];
            }
        }
    }

    delete lines->verticesIndecies;
    delete lines;

    node->data.children = new BSPChildren;
    node->data.children->frontChild = partitionSpace(front);
    node->data.children->backChild = partitionSpace(back);

    return node;
}


//TODO this is not the best way to pick a splitter, we can think of soemthing better
uint16_t pickSplitter(BSPLines* lines)
{
    vec2 middle;
    for (uint32_t pointIdx = 0; pointIdx < lines->numVertices; pointIdx += 2)
    {
        middle.x += mapVertices[lines->verticesIndecies[pointIdx]].x;
        middle.y += mapVertices[lines->verticesIndecies[pointIdx]].y;
    }

    middle.x /= (lines->numVertices / 2);
    middle.y /= (lines->numVertices / 2);

    float minDistance = FLT_MAX;
    uint32_t closestPointIdx = 0;

    for (uint32_t pointIdx = 0; pointIdx < lines->numVertices; pointIdx += 2)
    {
        vec2 toMiddleVec{ middle.x - mapVertices[lines->verticesIndecies[pointIdx]].x,
                          middle.y - mapVertices[lines->verticesIndecies[pointIdx]].y };

        float distance = sqrtf((toMiddleVec.x * toMiddleVec.x) + (toMiddleVec.y * toMiddleVec.y));
        if (distance < minDistance)
        {
            minDistance = distance;
            closestPointIdx = pointIdx;
        }
    }

    return closestPointIdx;
}

bool isPointInFront(uint32_t* lineIndices, uint32_t pointIdx)
{
    vec2* lineStart = &mapVertices[lineIndices[0]];
    vec2* lineEnd = &mapVertices[lineIndices[1]];

    vec2* point = &mapVertices[pointIdx];

    vec2 pointVec{ point->x - lineStart->x, point->y - lineStart->y };
    vec2 lineVec{ lineEnd->x - lineStart->x, lineEnd->y - lineStart->y };

    return lineVec.x * pointVec.y >= lineVec.y * pointVec.x;
}

void render(BSPNode* node)
{
    if (!node->isLeaf)
    {
        render(node->data.children->frontChild);
        render(node->data.children->backChild);
        return;
    }

    renderShape(node->data.lines);
}

void renderShape(BSPLines* shape)
{
    float color[4];
    for (uint32_t channelIdx = 0; channelIdx < 4; ++channelIdx)
    {
        color[channelIdx] = ((float)(rand() % 255)) / 255.f;
    }

    glUniform4fv(glGetUniformLocation(basicShader, "color"), 1, color);
    glDrawElements(GL_LINES, shape->numVertices, GL_UNSIGNED_INT, shape->verticesIndecies);
}
