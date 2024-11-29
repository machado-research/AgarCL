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

#define NUM_GRID_LINES 11

const char* vertex_shader_src =
#include "shaders/_vertex.glsl"
  ;

const char* fragment_shader_src =
#include "shaders/_fragment.glsl"
  ;

namespace agario {

  class Renderer {
  public:
    typedef Player<true> Player;

    explicit Renderer(std::shared_ptr<Canvas> canvas,
                      agario::distance arena_width,
                      agario::distance arena_height) :
      _canvas(std::move(canvas)),
      arena_width(arena_width), arena_height(arena_height),
      shader(), grid(arena_width, arena_height) {
      shader.compile_shaders(vertex_shader_src, fragment_shader_src);
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
      shader.setMat4("projection_transform", perspective_projection(player));
      shader.setMat4("view_transform", view_projection(player));
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
    void render_screen(Player &player, agario::GameState<true> &state) {
      shader.use();

      make_projections(player);

      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      // grid.draw(shader);

      for (auto &pellet : state.pellets)
        pellet.draw(shader, 0);

      for (auto &food : state.foods)
        food.draw(shader, 0);

      for (auto &pair : state.players)
        pair.second->draw(shader, 1);

      for (auto &virus : state.viruses)
        virus.draw(shader, 2);

    }
    void close_program()
    {
      shader.cleanup();
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

    Shader shader;
    agario::Grid<NUM_GRID_LINES> grid;
  };

}
