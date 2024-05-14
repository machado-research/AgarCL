#pragma once

#include "agario/rendering/platform.hpp"
#include <GLFW/glfw3.h>

#include "agario/rendering/Canvas.hpp"

#include <exception>

class WindowException : public std::runtime_error {
  using runtime_error::runtime_error;
};

void glfw_error_callback_Window(int error, const char *description) {
  (void) error;
  throw WindowException(description);
}

class Window : public Canvas {
public:

  Window(const std::string &window_name, screen_len screen_width, screen_len screen_height) :
    screen_width(screen_width),
    screen_height(screen_height),
    _destroy(false) {

    glfwSetErrorCallback(glfw_error_callback_Window);

    if (!glfwInit())
      throw WindowException("GLFW initialization failed.");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create a windowed mode window and its OpenGL context
    window = glfwCreateWindow(screen_width, screen_height, window_name.c_str(), nullptr, nullptr);

    if (window == nullptr) {
      glfwTerminate();
      throw WindowException("Window creation failed");
    }

    _destroy = true;

    glfwMakeContextCurrent(window);

    glfwSetWindowUserPointer(window, this);
    glfwSetWindowSizeCallback(window, window_resize_callback);
  }

  Window(int screen_width, int screen_height) : Window("", screen_width, screen_height) {}

  int width() const override { return screen_width; }
  int height() const override { return screen_height; }

  GLFWwindow *pointer() const {
    return window;
  }

  bool should_close() const {
    return glfwWindowShouldClose(window);
  }

  void swap_buffers() const {
    glfwSwapBuffers(window);
  };

  void destroy() {
    if (_destroy)
      glfwDestroyWindow(window);
    _destroy = false;
  }

  ~Window() { destroy(); }

private:

  static void window_resize_callback(GLFWwindow *window, screen_len width, screen_len height) {
    auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
    win->screen_width = width;
    win->screen_height = height;
  }

  GLFWwindow *window;
  screen_len screen_width;
  screen_len screen_height;
  bool _destroy;
};



