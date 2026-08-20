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

  /**
   * Per-entity color. Geometry and GPU buffers used to live here too, but
   * entities are now drawn by InstancedBatch from shared geometry, so no
   * entity owns GL objects: that removes the per-entity vertex array from
   * every ball (84 B per pellet, 1824 B per virus) and makes the VAO/VBO
   * leak in move-assignment structurally impossible.
   */
  template<unsigned NSides>
  class Circle {
  public:
    GLfloat color[COLOR_LEN];

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
    explicit RenderableBall(Loc &&loc) : Ball(loc), color(agario::random_color()) {}

    RenderableBall(agario::distance x, agario::distance y) :
      RenderableBall(Location(x, y)) {}

    /* All special members are defaulted. These objects no longer own GL
     * handles (geometry and buffers live in the renderer's shared instanced
     * batches), so copies and moves are plain member-wise operations.
     *
     * The previous hand-written move operations existed to transfer VAO/VBO
     * ownership, and were subtly identity-destroying: the move constructor
     * delegated to the location constructor, which mints a fresh Ball id, and
     * the move assignment copied position but never the id. Every std::sort
     * or std::remove_if over renderable cells therefore reassigned or
     * scrambled cell ids while sorting by id - a comparator whose key mutates
     * mid-sort is undefined behaviour, and id-based lookups then hit the
     * wrong cells. Defaulted moves preserve identity. */
    RenderableBall(const RenderableBall &) = default;
    RenderableBall &operator=(const RenderableBall &) = default;
    RenderableBall(RenderableBall &&) noexcept = default;
    RenderableBall &operator=(RenderableBall &&) noexcept = default;

    void set_color(agario::color c) {
      color = c;
      circle.set_color(c);
    }

    ~RenderableBall() override = default;

  protected:
    static constexpr unsigned NVertices = NSides + 2;
    Circle<NSides> circle;
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

    /* defaulted for the same identity-preservation reasons as RenderableBall */
    RenderableMovingBall(const RenderableMovingBall &) = default;
    RenderableMovingBall &operator=(const RenderableMovingBall &) = default;
    RenderableMovingBall(RenderableMovingBall &&) noexcept = default;
    RenderableMovingBall &operator=(RenderableMovingBall &&) noexcept = default;
  };

  template<unsigned NLines>
  class Grid {
  public:
    Grid(distance arena_width, distance arena_height, float z = 0.0) :
      arena_width(arena_width), arena_height(arena_height), z(z), _initialized(false) {}

    void draw(Shader &shader) {
      if (!_initialized) _initialize(); // lazy initialization

      shader.setVec4(shader.loc_color, color[0], color[1], color[2], 1.0);

      glm::mat4 model_matrix(1);
      model_matrix = glm::scale(model_matrix, glm::vec3(arena_width, arena_height, 0));

      shader.setMat4(shader.loc_model, model_matrix);

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
