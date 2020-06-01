-- premake5.lua
workspace "bsp"
   configurations { "Debug", "Release" }

project "bsp_creator"
   kind "ConsoleApp"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"

   files { "bsp_creator.cpp"}

project "bsp_render"
   kind "ConsoleApp"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"

   libdirs { "libs/glew/x86", "libs/SDL2/x86" }
   links { "glew32", "opengl32", "SDL2", "SDL2main" }

   files { "bsp_render.cpp"}

   includedirs { "include" }

project "bsp_gen"
   kind "ConsoleApp"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"

   files { "gen.cpp"}