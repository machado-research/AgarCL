#pragma once

#include <agario/engine/Engine.hpp>
#include <agario/core/types.hpp>
#include <agario/core/Entities.hpp>
#include <agario/core/Ball.hpp>
#include <agario/bots/bots.hpp>
#include "agario/engine/GameState.hpp"

#include "agario/rendering/types.hpp"
#include "agario/rendering/FrameBufferObject.hpp"
#include "agario/rendering/renderer.hpp"

#include "environment/envs/BaseEnvironment.hpp"

#include <iostream>

#define PIXEL_LEN 3

// todo: needs to be converted over to multi-environment

namespace agario::env {

    class ScreenObservation {
    public:
      explicit ScreenObservation(int num_frames, screen_len width, screen_len height, bool agent_view) :
        _num_frames(num_frames), _width(width), _height(height), multi_channel_obs(agent_view) {
        std::cout << "AGENT VIEW: " << agent_view << std::endl;
        std::cout << "ScreenObservation constructor called with num_frames: " << num_frames
                  << ", width: " << width << ", height: " << height << std::endl;
        _frame_data = new std::uint8_t[length()];
        clear();
      }

      [[nodiscard]] const std::uint8_t *frame_data() const {
        return _frame_data; }


      [[nodiscard]] std::size_t length() const {
        return  _num_frames * _width * _height * (PIXEL_LEN + multi_channel_obs);
      }

      void clear() {
        std::fill(_frame_data, _frame_data + length(), 0);
      }


      /* Remaps the rendered RGBA frame into the agent-view channel encoding.
       *
       * Walks whole pixels instead of individual bytes. The byte-wise loop
       * this replaces branched on `i % 4` for every byte and recomputed
       * neighbour offsets with modulo arithmetic; the stride is a compile-time
       * 4 here, so those all fold away. The mutation order is preserved
       * exactly: the three colour channels of a pixel are processed in order
       * (each may write that pixel's alpha), and only then is the alpha branch
       * evaluated - which is what lets it observe the colour channels' writes.
       * Output is byte-identical.
       */
      void post_processing_frame_data(std::uint8_t *&data) {
        const int channels = PIXEL_LEN + multi_channel_obs;
        if (channels != 4) return; // encoding is only defined for RGBA
        const int row = _width * 4;          // bytes per pixel row
        const int n_pixels = _width * _height;

        for (int p = 0; p < n_pixels; ++p) {
          const int b = p * 4;               // base byte of this pixel
          const int a = b + 3;               // its alpha byte

          // colour channels: move a non-background value into alpha, or
          // continue a horizontal grid line from the two preceding pixels
          for (int k = 0; k < 3; ++k) {
            const std::uint8_t v = data[b + k];
            if (v == 0) continue;
            if (v <= 230) {
              data[a] = v;
              data[b + k] = 0;
            } else {
              // virus / enemy / pellet: inherit a grid line from the left
              const int prev_a = b - 1;      // alpha of pixel p-1
              const int prev_prev_a = b - 5; // alpha of pixel p-2
              if (prev_prev_a >= 0 && data[prev_prev_a] <= 30 && data[prev_a] <= 30)
                data[a] = data[prev_a];
            }
          }

          // alpha channel: continue a vertical grid line from directly above
          if (data[a] == 0 || data[a] == 255) {
            const int above_a = a - row;
            const int above_above_a = above_a - row;
            if (above_above_a >= 0 && data[above_a] != 0 && data[above_above_a] <= 30)
              data[a] = data[above_above_a];
            else
              data[a] = 0; // transparent
          }
        }
      }
      std::uint8_t *frame_data(int frame_index) const {

        if (frame_index >= _num_frames)
          throw FBOException("Frame index " + std::to_string(frame_index) + " out of bounds");

        auto data_index = frame_index * _width * _height * (PIXEL_LEN + multi_channel_obs);
        return &_frame_data[data_index];
      }

      [[nodiscard]] int num_frames() const { return _num_frames; }

      [[nodiscard]] std::vector<int> shape() const {
        return {_num_frames, _width, _height, PIXEL_LEN + multi_channel_obs};
      }

      [[nodiscard]] std::vector<ssize_t> strides() const {
        return {
          _width * _height *  (PIXEL_LEN + multi_channel_obs)  * dtype_size,
                   _height *  (PIXEL_LEN + multi_channel_obs)  * dtype_size,
                              (PIXEL_LEN + multi_channel_obs)  * dtype_size,
                                                                 dtype_size
        };
      }

      ~ScreenObservation() {
        if (_frame_data) {
          delete[] _frame_data;
        } else {
          std::cout << "Error: _frame_data is null in destructor" << std::endl;
        }
      }

    private:
      int _num_frames;
      const int _width;
      const int _height;
      static constexpr ssize_t dtype_size = sizeof(std::uint8_t);
      std::uint8_t *_frame_data;
      bool multi_channel_obs;
    };

    template<bool renderable>
    class ScreenEnvironment : public BaseEnvironment<renderable> {
      using GameState = GameState<renderable>;
      using Player = Player<renderable>;
      using Cell = Cell<renderable>;
      using Pellet = Pellet<renderable>;
      using Virus = Virus<renderable>;
      using Food = Food<renderable>;
      bool multi_channel_obs;
    public:
      using Super = BaseEnvironment<renderable>;
      using dtype = std::uint8_t;

      explicit ScreenEnvironment(
        int num_agents,
        int frames_per_step,
        int arena_size,
        bool pellet_regen,
        int num_pellets,
        int num_viruses,
        int num_bots,
        bool reward_type,
        int c_death,
        int mode_number,
        bool load_env_snapshot,
        screen_len screen_width,
        screen_len screen_height,
        bool agent_view
      ):
        Super(num_agents, frames_per_step, arena_size, pellet_regen, num_pellets, num_viruses, num_bots, reward_type, c_death, mode_number, load_env_snapshot),
        _observation(1, screen_width, screen_height, agent_view),
        frame_buffer(std::make_shared<FrameBufferObject>(screen_width, screen_height, agent_view)),
        renderer(frame_buffer, this->engine_.arena_width(), this->engine_.arena_height()),
        multi_channel_obs(agent_view)
      {
        multi_channel_obs = agent_view;
        if (!frame_buffer) {
          std::cerr << "Error: frame_buffer is null in ScreenEnvironment constructor" << std::endl;
        }

      }

      [[nodiscard]] const ScreenObservation &get_state() {
        return _observation; }
      [[nodiscard]] screen_len screen_width() const { return frame_buffer->width(); }
      [[nodiscard]] screen_len screen_height() const { return frame_buffer->height(); }

      std::tuple<int, int, int, int> observation_shape() const {
        std::vector<int> shape_vec = _observation.shape();
        return std::make_tuple(shape_vec[0], shape_vec[1], shape_vec[2], shape_vec[3]);
      }

      void render() override {
        frame_buffer->unbind_capture(); // present to the window, not the FBO
        glViewport(0, 0, screen_width(), screen_height());
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (auto &pid: this->pids_) {
          auto &player = this->engine_.player(pid);
          render_frame(player);
        }

        glfwPollEvents();
        frame_buffer -> swap_buffers();
        frame_buffer -> show();
      }

      void close() override {
        renderer.close_program();
      }

    private:
      ScreenObservation _observation;
      std::shared_ptr<FrameBufferObject> frame_buffer;
      agario::Renderer renderer;

      void multi_channel_render_frame(Player &player) {
        renderer.multi_channel_render_screen(player, this->engine_.game_state());
      }

      void render_frame(Player &player) {
        renderer.render_screen(player, this->engine_.game_state());
      }

      // stores current frame into buffer containing the next observation
      void _partial_observation(Player &player, int frame_index) override {
        frame_buffer->bind_for_capture(); // render offscreen, not to the hidden window
        if(multi_channel_obs == true)
        {
          multi_channel_render_frame(player);
          void *data = _observation.frame_data(frame_index);
          frame_buffer->copy(data, true);
          auto *frame_data_ptr = static_cast<std::uint8_t *>(data);
          _observation.post_processing_frame_data(frame_data_ptr);
        }
        else
        {
          render_frame(player);
          void *data = _observation.frame_data(frame_index);
          frame_buffer->copy(data, false);
        }
      }


      void _partial_observation(int agent_index, int frame_index) override{
        auto &player = this->engine_.player(this->pids_[agent_index]);
        _partial_observation(player, frame_index);
        if (player.dead())
        {
          this->engine_.respawn(player);
          // this->dones_[agent_index] = true;
          this->is_main_player_respawned = true;
          // return;
        }
      }
    };

} // namespace agario:env
