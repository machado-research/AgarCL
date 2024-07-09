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
      explicit ScreenObservation(int num_frames, screen_len width, screen_len height) :
        _num_frames(num_frames), _width(width), _height(height) {
        std::cout << "ScreenObservation constructor called with num_frames: " << num_frames
                  << ", width: " << width << ", height: " << height << std::endl;
        _frame_data = new std::uint8_t[length()];
        clear();
      }

      [[nodiscard]] const std::uint8_t *frame_data() const {
        return _frame_data; }


      [[nodiscard]] std::size_t length() const {
        return  _num_frames * _width * _height * PIXEL_LEN;
      }

      void clear() {
        std::fill(_frame_data, _frame_data + length(), 0);
      }


      std::uint8_t *frame_data(int frame_index) const {
        if (frame_index >= _num_frames)
          throw FBOException("Frame index " + std::to_string(frame_index) + " out of bounds");

        auto data_index = frame_index * _width * _height * PIXEL_LEN;
        return &_frame_data[data_index];
      }


      [[nodiscard]] int num_frames() const { return _num_frames; }


      [[nodiscard]] std::vector<int> shape() const {
        return {_num_frames, _width, _height, PIXEL_LEN};
      }

      [[nodiscard]] std::vector<ssize_t> strides() const {
        return {
          _width * _height * PIXEL_LEN * dtype_size,
                   _height * PIXEL_LEN * dtype_size,
                             PIXEL_LEN * dtype_size,
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
    };

    template<bool renderable>
    class ScreenEnvironment : public BaseEnvironment<renderable> {
      using GameState = GameState<renderable>;
      using Player = Player<renderable>;
      using Cell = Cell<renderable>;
      using Pellet = Pellet<renderable>;
      using Virus = Virus<renderable>;
      using Food = Food<renderable>;
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
        bool respawn,
        screen_len screen_width,
        screen_len screen_height,
        bool reward_type=0
      ):
        Super(num_agents, frames_per_step, arena_size, pellet_regen, num_pellets, num_viruses, num_bots, reward_type),
        _observation(frames_per_step, screen_width, screen_height),
        frame_buffer(std::make_shared<FrameBufferObject>(screen_width, screen_height)),
        renderer(frame_buffer, this->engine_.arena_width(), this->engine_.arena_height())
      {
        if (!frame_buffer) {
          std::cerr << "Error: frame_buffer is null in ScreenEnvironment constructor" << std::endl;
        }
      }

      [[nodiscard]] const ScreenObservation &get_state() {
        return _observation; }
      // [[nodiscard]] const ScreenObservation &get_observations() const { return _observation; }
      [[nodiscard]] screen_len screen_width() const { return frame_buffer->width(); }
      [[nodiscard]] screen_len screen_height() const { return frame_buffer->height(); }

    private:
      ScreenObservation _observation;
      std::shared_ptr<FrameBufferObject> frame_buffer;
      agario::Renderer renderer;

      // stores current frame into buffer containing the next observation
      void _partial_observation(Player &player, int frame_index) override {
        renderer.render_screen(player, this->engine_.game_state());
        void *data = _observation.frame_data(frame_index);
        frame_buffer->copy(data);
      }


      void _partial_observation(int agent_index, int tick_index) override{
        auto &player = this->engine_.player(this->pids_[agent_index]);
        _partial_observation(player, tick_index);
      }

    };

} // namespace agario:env
