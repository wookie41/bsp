-- premake5.lua
workspace "bsp"
   configurations { "Debug", "Release" }

project "bsp"
   kind "ConsoleApp"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"

   libdirs { "libs/glew/x86", "libs/SDL2/x86" }
   links { "glew32", "opengl32", "SDL2", "SDL2main" }

   files { "bsp.cpp"}

   includedirs { "include" }