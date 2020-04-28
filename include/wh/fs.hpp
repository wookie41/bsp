namespace wh
{
    char* readFile(const char* filePath, bool nullTerminate = false);
} // namespace wh

#ifdef WH_FS_IMPLEMENTATION

namespace wh
{
    char* readFile(const char* filePath, bool nullTerminate)
    {
        FILE* fileToRead = fopen(filePath, "rb");

        fseek(fileToRead, 0, SEEK_END);

        char* buffer = nullptr;
        uint32_t numRead = 0;
        uint32_t fileSize = ftell(fileToRead);

        rewind(fileToRead);

        buffer = (char*)malloc(fileSize + nullTerminate);
        numRead = fread(buffer, 1, fileSize, fileToRead);

        if (numRead != fileSize)
        {
            free(buffer);
            perror("Failed to read file");
            return nullptr;
        }

        if (nullTerminate)
            buffer[numRead] = '\0';

        fclose(fileToRead);
        return buffer;
    }
} // namespace wh

#endif