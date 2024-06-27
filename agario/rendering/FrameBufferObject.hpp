#pragma once

#include "agario/rendering/platform.hpp"
#include <GLFW/glfw3.h>
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "agario/rendering/Canvas.hpp"
#include "agario/rendering/utils.hpp"
#include <iostream>


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

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
      throw FBOException("GLFW initialization failed.");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
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
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
      throw FBOException("Failed to initialize GLAD" );
    }else{
      std::cout << "GLAD ok " << std::endl;
    }
    
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

    exception_on_gl_error("Before BindBuffer");

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    exception_on_gl_error("After BindBuffer");

  }

  int width() const override { return _width; }
  int height() const override { return _height; }

  void show() const { glfwShowWindow(window); }
  void hide() const { glfwHideWindow(window); }

  void copy(void *data) {
    glReadBuffer(GL_BACK);

    exception_on_gl_error("ReadBuffer");
    
    glReadPixels(_width / 2, _height / 2, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, data);
    // glReadPixels(0, 0, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, data);

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
};