#pragma once

#ifdef __LINUX__
  #include <GL/glut.h>
  #include <GL/gl.h>
  #include <GL/glu.h>
#endif

class FBOException : public std::runtime_error {
  // using runtime_error::runtime_error;
  public:
    using std::runtime_error::runtime_error;

    FBOException(const std::string &message) : std::runtime_error(message) {
        std::cout << "FBOException: " << message << std::endl;
    }

    FBOException(const char *message) : std::runtime_error(message) {
        std::cout << "FBOException: " << message << std::endl;
    }
};

#ifdef USE_EGL
#include <EGL/egl.h>
void exception_on_egl_error(std::string error) {
  auto eglstatus = eglGetError();
  if (eglstatus != EGL_SUCCESS) {
    std::string es("Unknown EGL error");
    switch (eglstatus) {
      case EGL_NOT_INITIALIZED: es = "EGL_NOT_INITIALIZED"; break;
      case EGL_BAD_ACCESS: es = "EGL_BAD_ACCESS"; break;
      case EGL_BAD_ALLOC: es = "EGL_BAD_ALLOC"; break;
      case EGL_BAD_ATTRIBUTE: es = "EGL_BAD_ATTRIBUTE"; break;
      case EGL_BAD_CONTEXT: es = "EGL_BAD_CONTEXT"; break;
      case EGL_BAD_CONFIG: es = "EGL_BAD_CONFIG"; break;
      case EGL_BAD_CURRENT_SURFACE: es = "EGL_BAD_CURRENT_SURFACE"; break;
      case EGL_BAD_DISPLAY: es = "EGL_BAD_DISPLAY"; break;
      case EGL_BAD_SURFACE: es = "EGL_BAD_SURFACE"; break;
      case EGL_BAD_MATCH: es = "EGL_BAD_MATCH"; break;
      case EGL_BAD_PARAMETER: es = "EGL_BAD_PARAMETER"; break;
      case EGL_BAD_NATIVE_PIXMAP: es = "EGL_BAD_NATIVE_PIXMAP"; break;
      case EGL_BAD_NATIVE_WINDOW: es = "EGL_BAD_NATIVE_WINDOW"; break;
      case EGL_CONTEXT_LOST: es = "EGL_CONTEXT_LOST"; break;
      default: es = "Unknown EGL error"; break;
    }
    throw FBOException("EGL Error in " + error + ' ' + std::to_string(eglstatus) + ", " + es);
  }
}
#endif
void exception_on_gl_error(std::string error){
  auto glstatus = glGetError();
    if (glstatus != GL_NO_ERROR)
    {
      std::string es((const char *)gluErrorString(glstatus));
      throw FBOException("GL Error in copy "+ error + ' '+ std::to_string(glstatus)+ ", "+ es);
      }
}
