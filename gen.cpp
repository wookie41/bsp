#include <stdio.h>
#include <stdint.h>

struct vec2
{
    float x = 0, y = 0;
};


static vec2 mapVertices[400] = { { 100.f, 100.f }, { 700.f, 100.f }, { 700.f, 500.f }, { 100.f, 500.f },
                                 { 500.f, 150.f }, { 550.f, 200.f }, { 500.f, 250.f }, { 450.f, 200.f },
                                 { 200.f, 250.f }, { 250.f, 300.f }, { 200.f, 350.f }, { 150.f, 300.f } };

static uint32_t mapIndices[]{ 0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 8, 9, 9, 10, 10, 11, 11, 8 };

struct MapFileHeader
{
    uint32_t numVertices;
    uint32_t numOfIndices;
};

int main()
{
    MapFileHeader mapFileHeader;
    mapFileHeader.numVertices = sizeof(mapVertices) / sizeof(vec2);
    mapFileHeader.numOfIndices = sizeof(mapIndices) / sizeof(uint32_t);
    FILE* mapFile = fopen("test.map", "wb");
    fwrite(&mapFileHeader, sizeof(MapFileHeader), 1, mapFile);
    fwrite(mapVertices, sizeof(vec2), mapFileHeader.numVertices, mapFile);
    fwrite(mapIndices, sizeof(uint32_t), mapFileHeader.numOfIndices, mapFile);
    fclose(mapFile);
    return 0;
}