#pragma once

#include "agario/rendering/platform.hpp"
#include <GLFW/glfw3.h>
#include "agario/rendering/Canvas.hpp"
#include "agario/rendering/utils.hpp"
#include <iostream>

// #ifdef USE_EGL
#include <EGL/egl.h>
static const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
};

static const int pbufferWidth = 9;
static const int pbufferHeight = 9;

static const EGLint pbufferAttribs[] = {
      EGL_WIDTH, pbufferWidth,
      EGL_HEIGHT, pbufferHeight,
      EGL_NONE,
};
// #endif



void glfw_error_callback(int error, const char *description) {
  static_cast<void>(error);
  throw FBOException(description);
}

class FrameBufferObject : public Canvas {
public:

  static constexpr GLenum target = GL_RENDERBUFFER;

  FrameBufferObject(screen_len width, screen_len height) :
    _width(width), _height(height),
    fbo(0), rbo_depth(0), rbo_color(0),
    window(nullptr) {
// #ifdef USE_EGL
     _initialize_egl();
     std::cout <<"DONE\n";
// #else
    // _initialize_context();
// #endif
    _check_context_creation();
    _initialize_frame_buffers();
  }

  int width() const override { return _width; }
  int height() const override { return _height; }

  void show() const { glfwShowWindow(window); }
  void hide() const { glfwHideWindow(window); }

  void copy(void *data) {
    glReadBuffer(GL_BACK);

    exception_on_gl_error("ReadBuffer");

    // glReadPixels(_width / 2, _height / 2, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, data);
    glReadPixels(0, 0, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, data); // for rgb_array render mode

    exception_on_gl_error("ReadPixels");
    }


  void swap_buffers() const { glfwSwapBuffers(window); }

  ~FrameBufferObject() override {
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo_color);
    glDeleteRenderbuffers(1, &rbo_depth);
    glfwDestroyWindow(window);
    glfwTerminate();
  }

private:
  const screen_len _width;
  const screen_len _height;

  GLuint fbo;
  GLuint rbo_depth;
  GLuint rbo_color;

  GLFWwindow *window;

  void _initialize_context() {
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
      throw FBOException("GLFW initialization failed.");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // GLFW_FALSE - window is not visible by default, GLFW_TRUE - window is visible by default
    window = glfwCreateWindow(_width, _height, "", nullptr, nullptr);

    if (window == nullptr) {
      glfwTerminate();
      throw FBOException("Off-screen window creation failed");
    }

    glfwHideWindow(window);
    glfwMakeContextCurrent(window);

    if (!glfwGetCurrentContext()) {
      throw FBOException("Failed to make context current");
    }else{
      std::cout << "glfwGetCurrentContext ok " << std::endl;
    }
  }

  void _initialize_frame_buffers() {
    // Frame Buffer Object
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

    // Color
    glGenRenderbuffers(1, &rbo_color);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, _width, _height);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo_color);

    // Depth
    glGenRenderbuffers(1, &rbo_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, _width, _height);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);

    auto fbo_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fbo_status == GL_FRAMEBUFFER_UNSUPPORTED)
      throw FBOException("Not supported by OpenGL driver " + std::to_string(fbo_status));

    if (fbo_status != GL_FRAMEBUFFER_COMPLETE)
      throw FBOException("Framebuffer not complete");

    auto glstatus = glGetError();
    if (glstatus != GL_NO_ERROR)
      throw FBOException("GL Error: " + std::to_string(glstatus));

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    exception_on_gl_error("After BindBuffer");
  }

  void _check_context_creation()
  {
    if (!gladLoadGLLoader((GLADloadproc)eglGetProcAddress)) {
      throw FBOException("Failed to initialize GLAD with EGL");
    } else {
      std::cout << "GLAD with EGL ok" << std::endl;
    }
  }


  void _initialize_egl() {
    EGLDisplay eglDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    EGLint major, minor;

    eglInitialize(eglDpy, &major, &minor);

    // 2. Select an appropriate configuration
    EGLint numConfigs;
    EGLConfig eglCfg;

    eglChooseConfig(eglDpy, configAttribs, &eglCfg, 1, &numConfigs);

    // 3. Create a surface
    EGLSurface eglSurf = eglCreatePbufferSurface(eglDpy, eglCfg,
                                                 pbufferAttribs);

    // 4. Bind the API
    eglBindAPI(EGL_OPENGL_API);

    // 5. Create a context and make it current
    EGLContext eglCtx = eglCreateContext(eglDpy, eglCfg, EGL_NO_CONTEXT,
                                         NULL);

    eglMakeCurrent(eglDpy, eglSurf, eglSurf, eglCtx);


  }

};
