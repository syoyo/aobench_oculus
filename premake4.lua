-- Oculus
newoption {
   trigger     = "oculus-path",
   description = "Path to Oculus SDK.",
   value       = "PATH"
}

sources = {
   "main.cc",
   "trackball.cc",
   }

-- premake4.lua
solution "AOBenchOculusSolution"
   configurations { "Release", "Debug" }

   if (os.is("windows")) then
      platforms { "native", "x32", "x64" }
   else
      platforms { "native", "x32", "x64" }
   end

   -- A project defines one build target
   project "AOBenchOculus"
      kind "ConsoleApp"
      language "C++"
      files { sources }

      -- MacOSX. Guess we use gcc.
      configuration { "macosx", "gmake" }

         -- Oculus
         if _OPTIONS["oculus-path"] then
            buildoptions { "-fno-rtti" }
            includedirs { _OPTIONS["oculus-path"] .. "/Include" }
            linkoptions { _OPTIONS["oculus-path"] .. "/Lib/MacOS/Debug/libovr.a" }
         end

         links { "GLUT.framework", "OpenGL.framework", "IOKit.framework", "Cocoa.framework" }

      -- Linux specific
      configuration {"linux", "gmake"}
         defines { '__STDC_CONSTANT_MACROS', '__STDC_LIMIT_MACROS' } -- c99

         linkoptions { "-pthread" }

         defines { 'ENABLE_SDL' }
         defines { '_LARGEFILE_SOURCE', '_FILE_OFFSET_BITS=64' }
         buildoptions { "`sdl-config --cflags`" }
         linkoptions { "`sdl-config --libs`" }

      configuration "Debug"
         defines { "DEBUG" } -- -DDEBUG
         flags { "Symbols" }
         targetname "aobench_oculus_debug"

      configuration "Release"
         -- defines { "NDEBUG" } -- -NDEBUG
         flags { "Symbols", "Optimize" }
         targetname "aobench_oculus"
