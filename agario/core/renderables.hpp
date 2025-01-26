#pragma once
#define GL_SILENCE_DEPRECATION

#include"glad/glad.h"
#include <GLFW/glfw3.h>
#include "agario/rendering/platform.hpp"

#include "agario/core/color.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <agario/core/Ball.hpp>
#include <agario/rendering/shader.hpp>

#define COLOR_LEN 3

namespace agario {

  class RenderingException : public std::runtime_error {
    using runtime_error::runtime_error;
  };

  template<unsigned NSides>
  class Circle {
  public:
    GLfloat verts[3 * (NSides + 2)];
    GLfloat color[COLOR_LEN];
    GLuint vao; // vertex attribute object
    GLuint vbo; // vertex buffer object (gpu memory)

    void set_color(agario::color c) {
      GLfloat *color_array;
      switch (c) {
        case agario::color::red:
          color_array = red_color;
          break;
        case agario::color::blue:
          color_array = blue_color;
          break;
        case agario::color::green:
          color_array = green_color;
          break;
        case agario::color::orange:
          color_array = orange_color;
          break;
        case agario::color::purple:
          color_array = purple_color;
          break;
        case agario::color::yellow:
          color_array = yellow_color;
          break;
        default:
          throw RenderingException("Not a color");
      }
      std::copy(color_array, color_array + COLOR_LEN, color);
    }
  };

  template<unsigned NSides>
  class RenderableBall : virtual public Ball {
  public:
    using Ball::Ball;
    agario::color color;

    template<typename Loc>
    explicit RenderableBall(Loc &&loc) : Ball(loc), color(agario::random_color()),
                                         _initialized(false) {}

    RenderableBall(agario::distance x, agario::distance y) :
      RenderableBall(Location(x, y)) {}

    // move constructor
    RenderableBall(RenderableBall &&rb) noexcept : RenderableBall(rb.location()) {
      if (rb._initialized) {
        _initialized = true;
        circle = rb.circle;
      }
      color = rb.color;
      rb._initialized = false;
    }

    // move assignment
    RenderableBall &operator=(RenderableBall &&rb) noexcept {
      x = rb.x;
      y = rb.y;
      if (rb._initialized) {
        _initialized = true;
        circle = rb.circle;
      }
      color = rb.color;
      rb._initialized = false;
      return *this;
    }

    // copy constructor and assignment operator
    RenderableBall(const RenderableBall &rbm) = delete;

    RenderableBall &operator=(const RenderableBall &rmb) = delete;

    void set_color(agario::color c) {
      color = c;
      circle.set_color(c);
    }

    void draw(Shader &shader, int type) {
      if (!_initialized) _initialize();

      if(type == 0) // pellets
        shader.setVec4("color", 1.00f, 0.00f, 0.00f, 1.0);
      else if(type == 1) //Players
        shader.setVec4("color", 0.00f, 1.00f, 0.00f, 1.0);
      else if(type == 2) //Viruses
        shader.setVec4("color", 0.00f, 0.00f, 1.00f, 1.0);
      else if(type == 3) //Main Player
        shader.setVec4("color", 0.9f, 0.0f, 0.0f, 1.0);


      // world location
      auto location = glm::vec3(x, y, 0);
      glm::mat4 position_transform(1);
      position_transform = glm::translate(position_transform, location);

      // scaling
      glm::mat4 scale_transform(1);
      scale_transform = glm::scale(scale_transform, glm::vec3(radius(), radius(), 0));

      shader.setMat4("model_transform", position_transform * scale_transform);

      // draw them!
      glBindVertexArray(circle.vao);
      glDrawArrays(GL_TRIANGLE_FAN, 0, NVertices);
      glBindVertexArray(0);
    }

    void draw(Shader &shader) {
      if (!_initialized) _initialize();

      shader.setVec4("color", circle.color[0], circle.color[1], circle.color[2], 1.0);

      // world location
      auto location = glm::vec3(x, y, 0);
      glm::mat4 position_transform(1);
      position_transform = glm::translate(position_transform, location);

      // scaling
      glm::mat4 scale_transform(1);
      scale_transform = glm::scale(scale_transform, glm::vec3(radius(), radius(), 0));

      shader.setMat4("model_transform", position_transform * scale_transform);

      // draw them!
      glBindVertexArray(circle.vao);
      glDrawArrays(GL_TRIANGLE_FAN, 0, NVertices);
      glBindVertexArray(0);
    }

    ~RenderableBall() override {
      // If you get a "function not found" compilation error
      // right here its probably because you didn't link OpenGL
      if (_initialized) {
        glDeleteVertexArrays(1, &circle.vao);
        glDeleteBuffers(1, &circle.vbo);
      }
    }

  protected:
    static constexpr unsigned NVertices = NSides + 2;
    Circle<NSides> circle;
    bool _initialized;

    void _initialize() {
      _create_vertices();

      circle.set_color(color);

      glGenVertexArrays(1, &circle.vao);
      glGenBuffers(1, &circle.vbo);

      glBindVertexArray(circle.vao);
      glBindBuffer(GL_ARRAY_BUFFER, circle.vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(circle.verts), circle.verts, GL_STREAM_DRAW);

      // Position attribute
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
      glEnableVertexAttribArray(0);

      _initialized = true;
    }

    virtual void _create_vertices() {
      circle.verts[0] = 0;
      circle.verts[1] = 0;
      circle.verts[2] = 0;
      for (unsigned i = 1; i < NVertices; i++) {
        circle.verts[i * 3] = cos(i * 2 * M_PI / NSides);
        circle.verts[i * 3 + 1] = sin(i * 2 * M_PI / NSides);
        circle.verts[i * 3 + 2] = 0;
      }
    }
  };

  template<unsigned NSides>
  class RenderableMovingBall : public RenderableBall<NSides>, public MovingBall {
  public:

    // inherit move constructor from RenderableBall
    using RenderableBall<NSides>::RenderableBall;

    template<typename Loc, typename Vel>
    RenderableMovingBall(Loc &&loc, Vel &&vel) : Ball(loc),
                                                 RenderableBall<NSides>(loc),
                                                 MovingBall(loc, vel) {}

    template<typename Loc>
    explicit RenderableMovingBall(Loc &&loc) : RenderableMovingBall(loc, Velocity()) {}

    // move constructor
    RenderableMovingBall(RenderableMovingBall &&rmb) noexcept :
      RenderableMovingBall(rmb.location(), rmb.velocity) {
      if (rmb._initialized) {
        this->_initialized = true;
        this->circle = rmb.circle;
      }
      this->color = rmb.color;
      rmb._initialized = false;
    }

    // move assignment
    RenderableMovingBall &operator=(RenderableMovingBall &&rmb) noexcept {
      x = rmb.x;
      y = rmb.y;
      velocity = rmb.velocity;
      if (rmb._initialized) {
        this->_initialized = true;
        this->circle = rmb.circle;
      }
      this->color = rmb.color;
      rmb._initialized = false;
      return *this;
    }

  };

  template<unsigned NLines>
  class Grid {
  public:
    Grid(distance arena_width, distance arena_height, float z = 0.0) :
      arena_width(arena_width), arena_height(arena_height), z(z), _initialized(false) {}

    void draw(Shader &shader) {
      if (!_initialized) _initialize(); // lazy initialization

      shader.setVec4("color", color[0], color[1], color[2], 1.0);

      glm::mat4 model_matrix(1);
      model_matrix = glm::scale(model_matrix, glm::vec3(arena_width, arena_height, 0));

      shader.setMat4("model_transform", model_matrix);

      // do the actual drawing
      glBindVertexArray(vao);
      glDrawArrays(GL_LINES, 0, NumVertices);
      glBindVertexArray(0);
    }

    ~Grid() {
      if (_initialized) {
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
      }
    }

  private:
    static constexpr unsigned NumVertices = 2 * 3 * 2 * NLines;

    distance arena_width;
    distance arena_height;
    GLfloat z;
    GLfloat color[COLOR_LEN];

    GLuint vao;
    GLuint vbo;
    GLfloat vertices[NumVertices];
    bool _initialized;

    void _initialize() {
      _create_vertices();

      color[0] = 0.1;
      color[1] = 0.0;
      color[2] = 0.0;

      glGenVertexArrays(1, &vao);
      glGenBuffers(1, &vbo);

      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

      // Position attribute
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
      glEnableVertexAttribArray(0);

      _initialized = true;
    }

    void _create_vertices() {
      _create_horiz_verts(&vertices[3 * 2 * NLines]);
      _create_vertical_verts(&vertices[0]);
    }

    void _create_vertical_verts(GLfloat verts[]) {
      GLfloat spacing = 1.0 / (NLines - 1);
      for (unsigned i = 0; i < NLines; i++) {
        GLfloat x = i * spacing;
        verts[6 * i] = x;
        verts[6 * i + 1] = 0;
        verts[6 * i + 2] = z;

        verts[6 * i + 3] = x;
        verts[6 * i + 4] = 1;
        verts[6 * i + 5] = z;
      }
    }

    void _create_horiz_verts(GLfloat verts[]) {
      GLfloat spacing = 1.0 / (NLines - 1);
      for (unsigned i = 0; i < NLines; i++) {
        GLfloat y = i * spacing;
        verts[6 * i] = 0;
        verts[6 * i + 1] = y;
        verts[6 * i + 2] = z;

        verts[6 * i + 3] = 1;
        verts[6 * i + 4] = y;
        verts[6 * i + 5] = z;
      }
    }
  };

}
