#pragma once

#include "agario/rendering/platform.hpp"
#include <GLFW/glfw3.h>
#include "agario/rendering/Canvas.hpp"
#include "agario/rendering/utils.hpp"
#include <iostream>

#ifdef USE_EGL

#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>

// void assertEGLError(const std::string& msg) {
// 	EGLint error = eglGetError();

// 	if (error != EGL_SUCCESS) {
// 		stringstream s;
// 		s << "EGL error 0x" << std::hex << error << " at " << msg;
// 		throw runtime_error(s.str());
// 	}
// }

static const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
};

static const int pbufferWidth = 96;
static const int pbufferHeight = 96;

static EGLint pbufferAttribs[] = {
      EGL_WIDTH, pbufferWidth,
      EGL_HEIGHT, pbufferHeight,
      EGL_NONE,
};
#endif



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
    pbufferAttribs[1] = _width;
    pbufferAttribs[3] = _height;
#ifdef USE_EGL
     _initialize_egl();
     _check_egl_context_creation();
#else
    _initialize_context();
    _check_context_creation();
#endif
    _initialize_frame_buffers();
  }

  int width() const override { return _width; }
  int height() const override { return _height; }

  void show() const {
#ifdef USE_EGL
    // For EGL, there is no direct equivalent to showing a window since it's off-screen rendering.
    // std::cout << "EGL context is active, but no window to show." << std::endl;
#else
    glfwShowWindow(window);
#endif
  }

  void hide() const {
#ifdef USE_EGL
    // For EGL, there is no direct equivalent to hiding a window since it's off-screen rendering.
    // std::cout << "EGL context is active, but no window to hide." << std::endl;
#else
    glfwHideWindow(window);
#endif
  }

  void copy(void *data) {
    glReadBuffer(GL_BACK);

#ifdef USE_EGL
    exception_on_egl_error("ReadBuffer");
#else
    exception_on_gl_error("ReadBuffer");
#endif
    // glReadPixels(_width / 2, _height / 2, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, data);
    glReadPixels(0, 0, _width, _height, GL_RGBA, GL_UNSIGNED_BYTE, data); // for rgb_array render mode

#ifdef USE_EGL
    exception_on_egl_error("ReadPixels");
#else
    exception_on_gl_error("ReadPixels");
#endif
  }


  void swap_buffers() const {
  #ifdef USE_EGL
    eglSwapBuffers(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW));
  #else
    glfwSwapBuffers(window);
  #endif
  }

  ~FrameBufferObject() override {
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo_color);
    glDeleteRenderbuffers(1, &rbo_depth);
    #ifdef USE_EGL
      eglDestroySurface(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW));
      eglDestroyContext(eglGetCurrentDisplay(), eglGetCurrentContext());
      eglTerminate(eglGetCurrentDisplay());
    #else
      glfwDestroyWindow(window);
      glfwTerminate();
    #endif
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
    } else {
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
#ifdef USE_EGL
    exception_on_egl_error("After BindBuffer");
#else
    exception_on_gl_error("After BindBuffer");
#endif
  }

  void _check_context_creation() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
      throw FBOException("Failed to initialize GLAD");
    } else {
      std::cout << "GLAD with ok" << std::endl;
    }
  }

#ifdef USE_EGL
  void _check_egl_context_creation() {
    if (!gladLoadGLLoader((GLADloadproc)eglGetProcAddress)) {
      std::cerr << "Error: " << eglGetError() << std::endl;
      throw FBOException("Failed to initialize GLAD with EGL");
    } else {
      std::cout << "GLAD with EGL ok" << std::endl;
    }
  }
#endif

#ifdef USE_EGL
  void _initialize_egl() {
    EGLDisplay eglDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    EGLint major, minor;
    eglInitialize(eglDpy, &major, &minor);

    // 2. Select an appropriate configuration
    EGLint numConfigs;
    EGLConfig eglCfg;
    eglChooseConfig(eglDpy, configAttribs, &eglCfg, 1, &numConfigs);

    // 3. Create a surface
    EGLSurface eglSurf = eglCreatePbufferSurface(eglDpy, eglCfg, pbufferAttribs);

    // 4. Bind the API
    eglBindAPI(EGL_OPENGL_API);

    // 5. Create a context and make it current
    EGLContext eglCtx = eglCreateContext(eglDpy, eglCfg, EGL_NO_CONTEXT, NULL);

    eglMakeCurrent(eglDpy, eglSurf, eglSurf, eglCtx);
  }
#endif

};
