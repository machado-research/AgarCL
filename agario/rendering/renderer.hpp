#pragma once
#include "glad/glad.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "agario/rendering/platform.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <exception>
#include <vector>
#include <string>
#include <math.h>
#include <optional>

#include "agario/engine/GameState.hpp"
#include <agario/core/Entities.hpp>
#include <agario/core/Player.hpp>

#include "agario/core/renderables.hpp"
#include "agario/rendering/Canvas.hpp"
#include "agario/rendering/shader.hpp"
#include "agario/rendering/instanced_batch.hpp"

#define NUM_GRID_LINES 8

const char* vertex_shader_src =
#include "shaders/_vertex.glsl"
  ;

const char* fragment_shader_src =
#include "shaders/_fragment.glsl"
  ;

/* instanced pipeline: shared unit-circle geometry, per-instance
 * (offset, radius, color) attributes. One draw call per shape class. */
const char* instanced_vertex_shader_src = R"GLSL(
#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec2 offset;
layout (location = 2) in float radius;
layout (location = 3) in vec4 inst_color;

uniform mat4 projection_transform;
uniform mat4 view_transform;

out vec4 frag_color;

void main() {
    vec2 world = position.xy * radius + offset;
    gl_Position = projection_transform * view_transform * vec4(world, 0.0, 1.0);
    frag_color = inst_color;
}
)GLSL";

const char* instanced_fragment_shader_src = R"GLSL(
#version 330 core
in vec4 frag_color;
out vec4 colorF;

void main() {
    colorF = frag_color;
}
)GLSL";

namespace agario {

  class Renderer {
  public:
    typedef Player<true> Player;

    explicit Renderer(std::shared_ptr<Canvas> canvas,
                      agario::distance arena_width,
                      agario::distance arena_height) :
      _canvas(std::move(canvas)),
      arena_width(arena_width), arena_height(arena_height),
      shader(), grid(arena_width, arena_height),
      pellet_batch(PELLET_SIDES), food_batch(FOOD_SIDES),
      cell_batch(CELL_SIDES), virus_batch(VIRUS_SIDES, /*wavy=*/true) {
      shader.compile_shaders(vertex_shader_src, fragment_shader_src);
      inst_shader.compile_shaders(instanced_vertex_shader_src, instanced_fragment_shader_src);
      shader.use();
    }

    explicit Renderer(agario::distance arena_width, agario::distance arena_height) :
      Renderer(nullptr, arena_width, arena_height) {}

    /**
     * converts a screen position to a world position
     * @param player player to calculate position relative to
     * @param xpos screen horizontal position (0 to screen_width - 1)
     * @param ypos screen vertical position (0 to screen_height - 1)
     * @return world location
     */
    agario::Location to_target(Player &player, float xpos, float ypos) {

      // normalized device coordinates (from -1 to 1)
      auto ndc_x = 2 * (xpos / _canvas->width()) - 1;
      auto ndc_y = 1 - 2 * (ypos / _canvas->height());
      auto loc = glm::vec4(ndc_x, ndc_y, 1.0, 1);

      auto perspective = perspective_projection(player);
      auto view = view_projection(player);

      auto world_loc = glm::inverse(perspective * view) * loc;
      auto w = world_loc[3];
      auto x = world_loc[0] / w;
      auto y = world_loc[1] / w;

      return { x, y };
    }

    void make_projections(const Player &player) {
      shader.setMat4(shader.loc_projection, perspective_projection(player));
      shader.setMat4(shader.loc_view, view_projection(player));
    }

    /**
     * The z-coordinate distance away from the playing arena from which to
     * view the game as rendered from the perspective of the given player
     * @param player the player to render the game relative to
     * @return  z-coordinate for the camera positiooning
     */
    GLfloat camera_z(const Player &player) {
      return clamp(100 + player.mass() / 10.0, 100.0, 900.0);
    }

    /**
     * projection matrix for viewing the world
     * from the perspective of the given player
     * @param player player to make projection matrix for
     * @return 4x4 projection matrix
     */
    glm::mat4 perspective_projection(const Player &player) {
      auto angle = glm::radians(45.0f);
      auto znear = 0.1f;
      auto zfar = 1 + camera_z(player);
      return glm::perspective(angle, _canvas->aspect_ratio(), znear, zfar);
    }

    /**
     * the view projection from which the game world is
     * viewed from the perspective of the given player
     * @param player the player to get the view projection relative to
     * @return 4x4 view projection matrix
     */
    glm::mat4 view_projection(const Player &player) {
      return glm::lookAt(
        glm::vec3(player.x(), player.y(), camera_z(player)), // Camera location in World Space
        glm::vec3(player.x(), player.y(), 0), // camera "looks at" location
        glm::vec3(0, 1, 0)  // Head is up (set to 0,-1,0 to look upside-down)
      );
    }

    /**
     * renders a single frame of the game from the perspective
     * of the given player.
     * @param player player to reneder the game for
     * @param state current state of the game
     */
    void multi_channel_render_screen(Player &player, agario::GameState<true> &state) {
      shader.use();

      make_projections(player);

      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      grid.draw(shader);

      // fixed channel-encoding colors (same values as the old type-based draw)
      pellet_batch.clear();
      for (auto &pellet : state.pellets)
        pellet_batch.add(pellet.x, pellet.y, pellet.radius(), 1.0f, 0.0f, 0.0f);

      food_batch.clear();
      for (auto &food : state.foods)
        food_batch.add(food.x, food.y, food.radius(), 1.0f, 0.0f, 0.0f);

      // main agent first, other players after (they overwrite on overlap,
      // matching the previous draw order)
      cell_batch.clear();
      auto main_it = state.players.find(state.main_agent_pid);
      if (main_it != state.players.end())
        for (auto &cell : main_it->second->cells)
          cell_batch.add(cell.x, cell.y, cell.radius(), 0.9f, 0.0f, 0.0f);
      for (auto &pair : state.players) {
        if (pair.first == state.main_agent_pid) continue;
        for (auto &cell : pair.second->cells)
          cell_batch.add(cell.x, cell.y, cell.radius(), 0.0f, 1.0f, 0.0f);
      }

      virus_batch.clear();
      for (auto &virus : state.viruses)
        virus_batch.add(virus.x, virus.y, virus.radius(), 0.0f, 0.0f, 1.0f);

      draw_batches(player);
    }

/**
     * renders a single frame of the game from the perspective
     * of the given player.
     * @param player player to reneder the game for
     * @param state current state of the game
     */
    void render_screen(Player &player, agario::GameState<true> &state) {
      shader.use();

      make_projections(player);

      glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      grid.draw(shader);

      pellet_batch.clear();
      for (auto &pellet : state.pellets)
        add_colored(pellet_batch, pellet.x, pellet.y, pellet.radius(), pellet.color);

      food_batch.clear();
      for (auto &food : state.foods)
        add_colored(food_batch, food.x, food.y, food.radius(), food.color);

      cell_batch.clear();
      for (auto &pair : state.players)
        for (auto &cell : pair.second->cells)
          add_colored(cell_batch, cell.x, cell.y, cell.radius(), cell.color);

      virus_batch.clear();
      for (auto &virus : state.viruses)
        add_colored(virus_batch, virus.x, virus.y, virus.radius(), virus.color);

      draw_batches(player);
    }

    void close_program()
    {
      shader.cleanup();
      inst_shader.cleanup();
    }
    /**
     * Sets the canvas to render to
     * @param canvas pointer to a canvas to render to
     */
    void set_canvas(std::shared_ptr<Canvas> canvas) {
      _canvas = std::move(canvas);
    }

    ~Renderer() {
      // glfwTerminate();
    }

  private:
    std::shared_ptr<Canvas> _canvas;

    agario::distance arena_width;
    agario::distance arena_height;

    Shader shader;      // grid lines (model-matrix pipeline)
    Shader inst_shader; // instanced entities
    agario::Grid<NUM_GRID_LINES> grid;

    InstancedBatch pellet_batch;
    InstancedBatch food_batch;
    InstancedBatch cell_batch;
    InstancedBatch virus_batch;

    /* stage an instance colored via the agario palette */
    void add_colored(InstancedBatch &batch, float x, float y,
                     float radius, agario::color c) {
      const float *rgb;
      switch (c) {
        case agario::color::red:    rgb = red_color;    break;
        case agario::color::blue:   rgb = blue_color;   break;
        case agario::color::green:  rgb = green_color;  break;
        case agario::color::orange: rgb = orange_color; break;
        case agario::color::purple: rgb = purple_color; break;
        case agario::color::yellow: rgb = yellow_color; break;
        default:                    rgb = black_color;  break;
      }
      batch.add(x, y, radius, rgb[0], rgb[1], rgb[2]);
    }

    /* upload + draw all staged instances: one draw call per shape class,
     * in the same category paint order as the old per-entity path */
    void draw_batches(const Player &player) {
      inst_shader.use();
      inst_shader.setMat4(inst_shader.loc_projection, perspective_projection(player));
      inst_shader.setMat4(inst_shader.loc_view, view_projection(player));

      pellet_batch.flush();
      food_batch.flush();
      cell_batch.flush();
      virus_batch.flush();

      glBindVertexArray(0);
      shader.use(); // restore the default program for the next grid draw
    }
  };

}
