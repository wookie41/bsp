#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "GL/glew.h"
#include "SDL2/SDL.h"

#define STB_IMAGE_IMPLEMENTATION

#include "stb/stb_image.h"

#define WH_GL_IMPLEMENTATION
#define WH_FS_IMPLEMENTATION

#include "wh/fs.hpp"
#include "wh/gl.hpp"

static const uint16_t WINDOW_WIDTH = 800;
static const uint16_t WINDOW_HEIGHT = 600;

struct vec2
{
    float x = 0, y = 0;
};

static vec2 mapVertices[400] = { { 100.f, 100.f }, { 700.f, 100.f }, { 700.f, 500.f }, { 100.f, 500.f },
                                 { 500.f, 150.f }, { 550.f, 200.f }, { 500.f, 250.f }, { 450.f, 200.f },
                                 { 200.f, 250.f }, { 250.f, 300.f }, { 200.f, 350.f }, { 150.f, 300.f } };


int mapVerticesCount = 12;

static uint32_t mapIndices[]{ 0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 8, 9, 9, 10, 10, 11, 11, 8 };
int mapElementsCount = sizeof(mapIndices) / sizeof(uint32_t);

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
    uint16_t numElements = 0;
    uint32_t* elements = nullptr;
};

union BSPData {
    BSPChildren* children;
    BSPLines* lines;
};

struct BSPNode
{
    bool isLeaf;
    BSPData data;
    uint32_t splitter[2];
};

char vertPath[] = "basic.vert";
char fragPath[] = "basic.frag";

BSPNode* partitionSpace(BSPLines* lines);
uint16_t pickSplitter(BSPLines* lines);
bool isPointInFront(uint32_t* lineIndices, uint32_t pointIdx);
void render(BSPNode* node);
void renderShape(BSPNode* shape);

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
    initialMapLines->elements = new uint32_t[mapElementsCount];
    initialMapLines->numElements = mapElementsCount;
    memcpy(initialMapLines->elements, mapIndices, sizeof(mapIndices));

    basicShader = wh::compileShader(vertPath, fragPath);
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
    // bool hadNegativeDot = false;
    // bool hadPositiveDot = false;

    // for (uint16_t vertIdx = 0; vertIdx < numVerts; vertIdx += 2)
    // {
    //     uint16_t prevVertIdx = vertIdx + 1;
    //     uint16_t nextVertIdx = vertIdx == (numVerts - 2) ? 0 : vertIdx + 3;

    //     vec2* prevVert = mapVertices + shapeVerts[prevVertIdx];
    //     vec2* currVert = mapVertices + shapeVerts[vertIdx];
    //     vec2* nextVert = mapVertices + shapeVerts[nextVertIdx];

    //     vec2 v1 = { prevVert->x - currVert->x, prevVert->y - currVert->y };
    //     vec2 v2 = { prevVert->x - nextVert->x, prevVert->y - nextVert->y };

    //     float currDot = dot(&v1, &v2);

    //     if (currDot > 0.f)
    //         hadPositiveDot = true;
    //     else if (currDot < 0.f)
    //         hadNegativeDot = true;

    //     if (hadNegativeDot && hadPositiveDot)
    //     {
    //         return false;
    //     }
    // }
    // return true;

    bool hadNegativeX = false, hadPositiveX = false, hadNegativeY = false, hadPositiveY = false;

    vec2* firstLineStart = mapVertices + shapeVerts[0];
    vec2* firstLineEnd = mapVertices + shapeVerts[1];

    vec2 currentDelta = { firstLineEnd->x - firstLineStart->x, firstLineEnd->y - firstLineStart->y };

    for (uint16_t vertIdx = 2; vertIdx < numVerts; vertIdx += 2)
    {
        vec2* lineStart = mapVertices + shapeVerts[vertIdx];
        vec2* lineEnd = mapVertices + shapeVerts[vertIdx + 1];

        vec2 delta = { lineEnd->x - lineStart->x, lineEnd->y - lineStart->y };

        if (delta.x > 0)
        {
            if (currentDelta.x < 0)
            {
                if (hadPositiveX)
                    return false;

                hadNegativeX = true;
            }
            currentDelta.x = delta.x;
        }
        else if (delta.x < 0)
        {
            if (currentDelta.x > 0)
            {
                if (hadNegativeX)
                    return false;

                hadPositiveX = true;
            }
            currentDelta.x = delta.x;
        }

        if (delta.y > 0)
        {
            if (currentDelta.y < 0)
            {
                if (hadPositiveY)
                    return false;

                hadNegativeY = true;
            }
            currentDelta.y = delta.y;
        }
        else if (delta.y < 0)
        {
            if (currentDelta.y > 0)
            {
                if (hadNegativeY)
                    return false;

                hadPositiveY = true;
            }
            currentDelta.y = delta.y;
        }
    }
    return true;
}

BSPNode* partitionSpace(BSPLines* lines)
{
    BSPNode* node = new BSPNode;
    node->isLeaf = isConvex(lines->elements, lines->numElements);

    if (node->isLeaf)
    {
        node->data.lines = lines;
        return node;
    }

    uint16_t splitterIdx = pickSplitter(lines);

    BSPLines* front = new BSPLines;
    BSPLines* back = new BSPLines;

    front->elements = new uint32_t[lines->numElements];
    back->elements = new uint32_t[lines->numElements];

    BSPLines splitter;
    splitter.elements = lines->elements + splitterIdx;
    splitter.numElements = 2;

    node->splitter[0] = lines->elements[splitterIdx];
    node->splitter[1] = lines->elements[splitterIdx + 1];

    for (uint16_t lineIdx = 0; lineIdx < lines->numElements; lineIdx += 2)
    {
        uint32_t lineStartVert = lines->elements[lineIdx];
        uint32_t lineEndVert = lines->elements[lineIdx + 1];

        bool lineStartsInFront = isPointInFront(&lines->elements[splitterIdx], lineStartVert);
        bool lineEndsInFront = isPointInFront(&lines->elements[splitterIdx], lineEndVert);

        if (lineIdx == splitterIdx)
        {
            continue;
        }

        if (lineStartVert == node->splitter[1])
        {
            BSPLines* side = lineEndsInFront ? front : back;
            side->elements[side->numElements++] = lineStartVert;
            side->elements[side->numElements++] = lineEndVert;
            continue;
        }

        if (lineEndVert == node->splitter[0])
        {
            BSPLines* side = lineStartsInFront ? front : back;
            side->elements[side->numElements++] = lineStartVert;
            side->elements[side->numElements++] = lineEndVert;
            continue;
        }

        if (lineStartsInFront == lineEndsInFront)
        {
            BSPLines* side = lineStartsInFront ? front : back;
            side->elements[side->numElements++] = lineStartVert;
            side->elements[side->numElements++] = lineEndVert;
            continue;
        }

        // the lines intersect

        vec2* lineStart = &mapVertices[lineStartVert];
        vec2* lineEnd = &mapVertices[lineEndVert];

        vec2* splitterStart = &mapVertices[node->splitter[0]];
        vec2* splitterEnd = &mapVertices[node->splitter[1]];

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

            intersectionX =
            ((splitterStart->y - (splitterSlope * splitterStart->x)) - (lineStart->y - (lineSlope * lineStart->x))) /
            (lineSlope - splitterSlope);
        }

        float intersectionY = (slope * (intersectionX - start->x)) + start->y;

        uint32_t intersectionVertex = mapVerticesCount++;
        mapVertices[intersectionVertex] = { intersectionX, intersectionY };

        {
            BSPLines* f = lineStartsInFront ? front : back;
            BSPLines* b = lineStartsInFront ? back : front;

            f->elements[f->numElements++] = lineStartVert;
            f->elements[f->numElements++] = intersectionVertex;

            b->elements[b->numElements++] = intersectionVertex;
            b->elements[b->numElements++] = lineEndVert;
        }
    }

    delete lines->elements;
    delete lines;

    node->data.children = new BSPChildren;
    node->data.children->frontChild = partitionSpace(front);
    node->data.children->backChild = partitionSpace(back);

    return node;
}

uint16_t pickSplitter(BSPLines* lines)
{
    vec2 middle;
    for (uint32_t pointIdx = 0; pointIdx < lines->numElements; pointIdx += 2)
    {
        middle.x += mapVertices[lines->elements[pointIdx]].x + mapVertices[lines->elements[pointIdx + 1]].x;
        middle.y += mapVertices[lines->elements[pointIdx]].y + mapVertices[lines->elements[pointIdx + 1]].y;
    }

    middle.x /= (lines->numElements / 2);
    middle.y /= (lines->numElements / 2);

    float minDistance = FLT_MAX;
    uint32_t closestPointIdx = 0;

    for (uint32_t pointIdx = 0; pointIdx < lines->numElements; pointIdx += 2)
    {
        vec2 lineMiddle{ mapVertices[lines->elements[pointIdx]].x + mapVertices[lines->elements[pointIdx + 1]].x,
                         mapVertices[lines->elements[pointIdx]].y + mapVertices[lines->elements[pointIdx + 1]].y };

        vec2 toMiddleVec{ middle.x - lineMiddle.x, middle.y - lineMiddle.y };

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

    return lineVec.x * pointVec.y > lineVec.y * pointVec.x;
}

void render(BSPNode* node)
{
    if (!node->isLeaf)
    {
        render(node->data.children->frontChild);
        render(node->data.children->backChild);
        glDrawElements(GL_LINES, 2, GL_UNSIGNED_INT, node->splitter);
        return;
    }

    renderShape(node);
}

void renderShape(BSPNode* node)
{
    float color[4];
    for (uint32_t channelIdx = 0; channelIdx < 3; ++channelIdx)
    {
        color[channelIdx] = ((float)(rand() % 255)) / 255.f;
    }

    color[3] = 1.f;

    glUniform4fv(glGetUniformLocation(basicShader, "PointColor"), 1, color);
    glDrawElements(GL_LINES, node->data.lines->numElements, GL_UNSIGNED_INT, node->data.lines->elements);
}