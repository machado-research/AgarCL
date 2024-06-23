#pragma once

#define GL_SILENCE_DEPRECATION

#ifdef __APPLE__
  #define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED

  #ifdef __arm__  // Check for ARM processor
      #include <GL/gl.h>
      #include <GL/glu.h>
  #else
      #include <OpenGL/gl.h>
      #include <OpenGL/glu.h>
  #endif
#endif

#ifdef _WIN32
  #include <windows.h>
#endif

#ifdef __linux__ 
  #include <GL/gl.h>
  #include <GL/glu.h>
#endif
