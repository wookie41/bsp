#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <string.h>
#include <math.h>
#include <winsock.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include <cstdint>

struct vec2
{
    float x = 0, y = 0;
};

struct BSPNode;

struct BSPChildren
{
    BSPNode* frontChild;
    BSPNode* backChild;
};

struct BSPLines
{
    uint16_t numIndices = 0;
    uint32_t* verticesIndecies = nullptr;
};

union BSPData {
    BSPChildren* children;
    BSPLines* lines;
};

struct BSPNode
{
    bool isLeaf;
    uint32_t splitter[2];
    BSPData data;
};

struct Map
{
    uint32_t numVertices;
    uint32_t numOfIndices;
    uint32_t maxVertices;
    vec2* vertices;
};

static char manual[] = "Usage: bsp <input-map-file> <output-map-file>";

BSPNode* partitionSpace(Map* map, BSPLines* lines);
uint16_t pickSplitter(Map* map, BSPLines* lines);
bool isPointInFront(Map* map, uint32_t* lineIndices, uint32_t pointIdx);
void memPack(BSPLines* lines);
void grow(Map* map);
void printNode(Map* map, BSPNode* node);
bool isConvex(Map* map, uint32_t* shapeVerts, uint16_t numVerts);
void writeToFile(BSPNode* node, FILE* file);

/*
map-file-format <binary>
HEADER START
<uint32> numberOfVertices
<uint32> numberOfIndices (numberOfLineSegments * 2, as as segment consists of two points)
HEADER END
(numberOfVertices) <float(s)>
(numberOfIndices) <uint32(s)>
*/

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        puts(manual);
        return 1;
    }

    char* inputFilePath = argv[1];
    char* outputFilePath = argv[2];

    FILE* inputFile = fopen(inputFilePath, "rb");

    Map map;
    fread(&map, sizeof(uint64_t), 1, inputFile);
    map.maxVertices = map.numVertices + (map.numVertices / 2);
    map.vertices = new vec2[map.maxVertices];

    fread(map.vertices, sizeof(vec2), map.numVertices, inputFile);

    BSPLines* initialMapLines = new BSPLines;
    initialMapLines->numIndices = map.numOfIndices;
    initialMapLines->verticesIndecies = new uint32_t[initialMapLines->numIndices];
    fread(initialMapLines->verticesIndecies, sizeof(uint32_t), initialMapLines->numIndices, inputFile);

    fclose(inputFile);
    BSPNode* root = partitionSpace(&map, initialMapLines);

    FILE* outputFile = fopen(outputFilePath, "wb+");
    fwrite(&map.numVertices, sizeof(uint32_t), 1, outputFile);

    fwrite(map.vertices, sizeof(vec2), map.numVertices, outputFile);
    writeToFile(root, outputFile);

    fclose(outputFile);
}

inline float dot(vec2 const* v1, vec2 const* v2) { return (v1->x * v2->x) + (v1->y * v2->y); }

bool isConvex(Map* map, uint32_t* shapeVerts, uint16_t numVerts)
{
    bool hadNegativeX = false, hadPositiveX = false, hadNegativeY = false, hadPositiveY = false;

    vec2* firstLineStart = map->vertices + shapeVerts[0];
    vec2* firstLineEnd = map->vertices + shapeVerts[1];

    vec2 currentDelta = { firstLineEnd->x - firstLineStart->x, firstLineEnd->y - firstLineStart->y };

    for (uint16_t vertIdx = 2; vertIdx < numVerts; vertIdx += 2)
    {
        vec2* lineStart = map->vertices + shapeVerts[vertIdx];
        vec2* lineEnd = map->vertices + shapeVerts[vertIdx + 1];

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


BSPNode* partitionSpace(Map* map, BSPLines* lines)
{
    BSPNode* node = new BSPNode;
    node->isLeaf = isConvex(map, lines->verticesIndecies, lines->numIndices);

    if (node->isLeaf)
    {
        node->data.lines = lines;
        return node;
    }

    uint16_t splitterIdx = pickSplitter(map, lines);

    BSPLines* front = new BSPLines;
    BSPLines* back = new BSPLines;

    front->verticesIndecies = new uint32_t[lines->numIndices];
    back->verticesIndecies = new uint32_t[lines->numIndices];

    BSPLines splitter;
    splitter.verticesIndecies = lines->verticesIndecies + splitterIdx;
    splitter.numIndices = 2;

    node->splitter[0] = lines->verticesIndecies[splitterIdx];
    node->splitter[1] = lines->verticesIndecies[splitterIdx + 1];

    for (uint16_t lineIdx = 0; lineIdx < lines->numIndices; lineIdx += 2)
    {
        uint32_t lineStartVert = lines->verticesIndecies[lineIdx];
        uint32_t lineEndVert = lines->verticesIndecies[lineIdx + 1];

        bool lineStartsInFront = isPointInFront(map, &lines->verticesIndecies[splitterIdx], lineStartVert);
        bool lineEndsInFront = isPointInFront(map, &lines->verticesIndecies[splitterIdx], lineEndVert);

        if (lineIdx == splitterIdx)
        {
            continue;
        }

        if (lineStartVert == node->splitter[1])
        {
            BSPLines* side = lineEndsInFront ? front : back;
            side->verticesIndecies[side->numIndices++] = lineStartVert;
            side->verticesIndecies[side->numIndices++] = lineEndVert;
            continue;
        }

        if (lineEndVert == node->splitter[0])
        {
            BSPLines* side = lineStartsInFront ? front : back;
            side->verticesIndecies[side->numIndices++] = lineStartVert;
            side->verticesIndecies[side->numIndices++] = lineEndVert;
            continue;
        }

        if (lineStartsInFront == lineEndsInFront)
        {
            BSPLines* side = lineStartsInFront ? front : back;
            side->verticesIndecies[side->numIndices++] = lineStartVert;
            side->verticesIndecies[side->numIndices++] = lineEndVert;
            continue;
        }

        // the lines intersect

        vec2* lineStart = map->vertices + lineStartVert;
        vec2* lineEnd = map->vertices + lineEndVert;

        vec2* splitterStart = map->vertices + node->splitter[0];
        vec2* splitterEnd = map->vertices + node->splitter[1];

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

        uint32_t intersectionVertex = map->numVertices++;
        if (intersectionVertex >= map->maxVertices)
            grow(map);

        map->vertices[intersectionVertex] = { intersectionX, intersectionY };

        {
            BSPLines* f = lineStartsInFront ? front : back;
            BSPLines* b = lineStartsInFront ? back : front;

            f->verticesIndecies[f->numIndices++] = lineStartVert;
            f->verticesIndecies[f->numIndices++] = intersectionVertex;

            b->verticesIndecies[b->numIndices++] = intersectionVertex;
            b->verticesIndecies[b->numIndices++] = lineEndVert;
        }
    }

    back->verticesIndecies[back->numIndices++] = node->splitter[0];
    back->verticesIndecies[back->numIndices++] = node->splitter[1];


    memPack(front);
    memPack(back);

    delete[] lines->verticesIndecies;
    delete lines;

    node->data.children = new BSPChildren;
    node->data.children->frontChild = partitionSpace(map, front);
    node->data.children->backChild = partitionSpace(map, back);

    return node;
}

uint16_t pickSplitter(Map* map, BSPLines* lines)
{
    vec2 middle;
    for (uint32_t pointIdx = 0; pointIdx < lines->numIndices; pointIdx += 2)
    {
        middle.x += map->vertices[lines->verticesIndecies[pointIdx]].x;
        middle.y += map->vertices[lines->verticesIndecies[pointIdx]].y;
    }

    middle.x /= (lines->numIndices / 2);
    middle.y /= (lines->numIndices / 2);

    float minDistance = FLT_MAX;
    uint32_t closestPointIdx = 0;

    for (uint32_t pointIdx = 0; pointIdx < lines->numIndices; pointIdx += 2)
    {
        vec2 toMiddleVec{ middle.x - map->vertices[lines->verticesIndecies[pointIdx]].x,
                          middle.y - map->vertices[lines->verticesIndecies[pointIdx]].y };

        float distance = sqrtf((toMiddleVec.x * toMiddleVec.x) + (toMiddleVec.y * toMiddleVec.y));
        if (distance < minDistance)
        {
            minDistance = distance;
            closestPointIdx = pointIdx;
        }
    }

    return closestPointIdx;
}

bool isPointInFront(Map* map, uint32_t* lineIndices, uint32_t pointIdx)
{
    vec2* lineStart = map->vertices + lineIndices[0];
    vec2* lineEnd = map->vertices + lineIndices[1];

    vec2* point = map->vertices + pointIdx;

    vec2 pointVec{ point->x - lineStart->x, point->y - lineStart->y };
    vec2 lineVec{ lineEnd->x - lineStart->x, lineEnd->y - lineStart->y };

    return lineVec.x * pointVec.y > lineVec.y * pointVec.x;
}

void memPack(BSPLines* lines)
{
    uint32_t* tmp = new uint32_t[lines->numIndices];
    memcpy(tmp, lines->verticesIndecies, sizeof(uint32_t) * lines->numIndices);
    delete[] lines->verticesIndecies;
    lines->verticesIndecies = tmp;
}

void grow(Map* map)
{
    vec2* biggerMap = new vec2[map->numVertices + (map->numVertices / 2)];
    memcpy(biggerMap, map->vertices, sizeof(vec2) * map->numVertices);
    delete[] map->vertices;
    map->vertices = biggerMap;
}

void writeToFile(BSPNode* node, FILE* file)
{
    static uint32_t innerNodeSize = sizeof(bool) + (sizeof(uint32_t) * 4);
    BSPNode** queue = NULL;
    arrput(queue, node);

    while (arrlenu(queue))
    {
        uint32_t currLevelSize = 0;
        for (uint32_t nodeIdx = 0; nodeIdx < arrlenu(queue); ++nodeIdx)
        {
            if (queue[nodeIdx]->isLeaf)
            {
                currLevelSize += (sizeof(bool) + sizeof(uint16_t) + (sizeof(float) * 3) +
                                  (sizeof(uint32_t) * queue[nodeIdx]->data.lines->numIndices));
            }
            else
            {
                currLevelSize += innerNodeSize;
            }
        }

        BSPNode* current = queue[0];
        arrdel(queue, 0);
        fwrite(&current->isLeaf, sizeof(bool), 1, file);

        if (current->isLeaf)
        {
            float color[3];
            for (uint32_t channelIdx = 0; channelIdx < 3; ++channelIdx)
            {
                color[channelIdx] = ((float)(rand() % 255)) / 255.f;
            }

            fwrite(color, sizeof(color), 1, file);
            fwrite(&current->data.lines->numIndices, sizeof(uint16_t), 1, file);
            fwrite(current->data.lines->verticesIndecies, sizeof(uint32_t),
                   current->data.lines->numIndices, file);
        }
        else
        {
            fwrite(current->splitter, sizeof(uint32_t), 2, file);
            fwrite(&currLevelSize, sizeof(uint32_t), 1, file); // offset to front child

            if (current->data.children->frontChild->isLeaf)
            {
                currLevelSize +=
                (sizeof(bool) + sizeof(uint16_t) + (sizeof(float) * 3) +
                 (sizeof(uint32_t) * current->data.children->frontChild->data.lines->numIndices));
            }
            else
            {
                currLevelSize += innerNodeSize;
            }

            fwrite(&currLevelSize, sizeof(uint32_t), 1, file); // offset to back child

            arrput(queue, current->data.children->frontChild);
            arrput(queue, current->data.children->backChild);
        }
    }
}