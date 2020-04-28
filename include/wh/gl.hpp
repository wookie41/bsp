#include <cstdint>
#include "GL/glew.h"

struct SDLContext;

namespace wh
{
    GLuint compileShader(char* vertexShaderPath, char* fragmentShaderPath);
    int initSDLContext(SDLContext* context, uint16_t windowWidth, uint16_t windowHeight);
} // namespace wh


#ifdef WH_GL_IMPLEMENTATION

namespace wh
{
    struct SDLContext
    {
        SDL_Window* window;
        SDL_GLContext glContext;
    };

    int initSDLContext(SDLContext* context, uint16_t windowWidth, uint16_t windowHeight)
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            printf("Failed to init SDL2 video module: %s\n", SDL_GetError());
            return -1;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        context->window =
        SDL_CreateWindow("BSP", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth,
                         windowHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);

        if (context->window == nullptr)
        {
            printf("Failed to create window: %s\n", SDL_GetError());
            return 1;
        }

        context->glContext = SDL_GL_CreateContext(context->window);
        SDL_GL_MakeCurrent(context->window, context->glContext);

        GLenum glewInitResult = glewInit();
        if (glewInitResult != GLEW_OK)
        {
            printf("Failed to initialize GLEW: %s\n", glewGetErrorString(glewInitResult));
            return 1;
        }
    }

    GLuint compileShader(char* vertexShaderPath, char* fragmentShaderPath)
    {
        GLuint shaderProgramID = glCreateProgram();
        GLuint vertexShader = 0, fragmentShader = 0;

        vertexShader = glCreateShader(GL_VERTEX_SHADER);
        fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

        char* vertexSource = readFile(vertexShaderPath, true);
        char* fragmentSource = readFile(fragmentShaderPath, true);

        GLint success;
        static char infoLog[1024];

        glShaderSource(vertexShader, 1, &vertexSource, NULL);
        glCompileShader(vertexShader);

        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(vertexShader, 1024, NULL, infoLog);
            printf("Failed to shader %s: %s", vertexShaderPath, infoLog);
        }
        else
        {
            glAttachShader(shaderProgramID, vertexShader);
        }

        glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
        glCompileShader(fragmentShader);

        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(fragmentShader, 1024, NULL, infoLog);
            printf("Failed to shader %s: %s", fragmentShaderPath, infoLog);
        }
        else
        {
            glAttachShader(shaderProgramID, fragmentShader);
        }

        glLinkProgram(shaderProgramID);
        glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shaderProgramID, 1024, NULL, infoLog);
            printf("Failed to link shader program: %s", infoLog);
            glDeleteShader(shaderProgramID);
            shaderProgramID = 0;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return shaderProgramID;
    }
} // namespace wh

#endif