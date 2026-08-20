#pragma once

#include "glad/glad.h"
#include <glm/glm.hpp>

#include <cmath>
#include <vector>

namespace agario {

  /**
   * Draws all circles of one shape class (pellet / food / cell / virus)
   * with a single glDrawArraysInstanced call.
   *
   * The unit-circle geometry is uploaded once and shared by every
   * instance; per-entity (x, y, radius, rgba) live in a streaming
   * instance buffer refilled each frame. This replaces the previous
   * design of one VAO + VBO + ~7 GL calls per entity per frame.
   */
  class InstancedBatch {
  public:
    static constexpr int floats_per_instance = 7; // x, y, radius, r, g, b, a

    /* `wavy` reproduces the virus border from oVirus::_create_vertices */
    explicit InstancedBatch(unsigned n_sides, bool wavy = false)
      : n_sides_(n_sides), n_vertices_(n_sides + 2), wavy_(wavy) {}

    InstancedBatch(const InstancedBatch &) = delete;
    InstancedBatch &operator=(const InstancedBatch &) = delete;

    ~InstancedBatch() {
      if (initialized_) {
        glDeleteVertexArrays(1, &vao_);
        glDeleteBuffers(1, &vbo_geom_);
        glDeleteBuffers(1, &vbo_inst_);
      }
    }

    void clear() { staging_.clear(); }

    void add(float x, float y, float radius,
             float r, float g, float b, float a = 1.0f) {
      staging_.insert(staging_.end(), {x, y, radius, r, g, b, a});
    }

    [[nodiscard]] std::size_t count() const {
      return staging_.size() / floats_per_instance;
    }

    /* uploads the staged instances and issues the single draw call */
    void flush() {
      if (staging_.empty()) return;
      if (!initialized_) initialize();

      glBindBuffer(GL_ARRAY_BUFFER, vbo_inst_);
      // orphan + upload: avoids syncing with the previous frame's draw
      glBufferData(GL_ARRAY_BUFFER,
                   staging_.size() * sizeof(GLfloat),
                   staging_.data(), GL_STREAM_DRAW);

      glBindVertexArray(vao_);
      glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, n_vertices_,
                            static_cast<GLsizei>(count()));
    }

  private:
    const unsigned n_sides_;
    const unsigned n_vertices_;
    const bool wavy_;

    GLuint vao_ = 0, vbo_geom_ = 0, vbo_inst_ = 0;
    bool initialized_ = false;
    std::vector<GLfloat> staging_;

    void initialize() {
      std::vector<GLfloat> verts(3 * n_vertices_, 0.0f);
      for (unsigned i = 1; i < n_vertices_; i++) {
        // same formulas as RenderableBall / oVirus vertex generation
        float radius = wavy_ ? 1.0f + std::sin(30 * M_PI * i / n_sides_) / 15.0f : 1.0f;
        verts[i * 3]     = radius * std::cos(i * 2 * M_PI / n_sides_);
        verts[i * 3 + 1] = radius * std::sin(i * 2 * M_PI / n_sides_);
      }

      glGenVertexArrays(1, &vao_);
      glGenBuffers(1, &vbo_geom_);
      glGenBuffers(1, &vbo_inst_);

      glBindVertexArray(vao_);

      // shared unit-circle geometry (attribute 0)
      glBindBuffer(GL_ARRAY_BUFFER, vbo_geom_);
      glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(GLfloat),
                   verts.data(), GL_STATIC_DRAW);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
      glEnableVertexAttribArray(0);

      // per-instance attributes (1: offset, 2: radius, 3: color)
      constexpr GLsizei stride = floats_per_instance * sizeof(GLfloat);
      glBindBuffer(GL_ARRAY_BUFFER, vbo_inst_);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *) 0);
      glEnableVertexAttribArray(1);
      glVertexAttribDivisor(1, 1);
      glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void *) (2 * sizeof(GLfloat)));
      glEnableVertexAttribArray(2);
      glVertexAttribDivisor(2, 1);
      glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void *) (3 * sizeof(GLfloat)));
      glEnableVertexAttribArray(3);
      glVertexAttribDivisor(3, 1);

      glBindVertexArray(0);
      initialized_ = true;
    }
  };

}
