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
        // std::cout << "ScreenObservation constructor called with num_frames: " << num_frames
                  // << ", width: " << width << ", height: " << height << std::endl;
        _frame_data = new std::uint8_t[length()];
        // std::cout << "Memory allocated for _frame_data of size: " << length() << std::endl;
        // std::cout << "_frame_data allocated at address: " << static_cast<void*>(_frame_data) << std::endl;
        clear();
        // std::cout << "ScreenObservation constructor done" << std::endl;
      }

      [[nodiscard]] const std::uint8_t *frame_data() const { 
        // std::cout << "frame_data() const called" << std::endl;
        return _frame_data; }


      // create data 
      // [[nodiscard]] const std::uint8_t *data() const { 
      //   std::cout << "data() const called" << std::endl;
      //   return _frame_data; }


      [[nodiscard]] std::size_t length() const {
        return  _width * _height * PIXEL_LEN;
      }

      void clear() {
        // std::cout << "clear() called" << std::endl;
        std::fill(_frame_data, _frame_data + length(), 0);
        // std::cout << "Memory cleared" << std::endl;
      }


      std::uint8_t *frame_data(int frame_index) const {
        // std::cout << "frame_data(" << frame_index << ") called" << std::endl;
        if (frame_index >= _num_frames)
          throw FBOException("Frame index " + std::to_string(frame_index) + " out of bounds");

        auto data_index = frame_index * _width * _height * PIXEL_LEN;
        return &_frame_data[data_index];
      }


      [[nodiscard]] int num_frames() const { return _num_frames; }


      [[nodiscard]] std::vector<int> shape() const {
        //  std::cout << "shape() called" << std::endl;
        return {_num_frames, _width, _height, PIXEL_LEN};
        // return {_width, _height, PIXEL_LEN};
      }

      [[nodiscard]] std::vector<ssize_t> strides() const {
        // std::cout << "strides() called" << std::endl;
        return {
          _width * _height * PIXEL_LEN * dtype_size,
                   _height * PIXEL_LEN * dtype_size,
                             PIXEL_LEN * dtype_size,
                                         dtype_size
        };
        // };
        // return {
        //            _height * PIXEL_LEN * dtype_size,
        //                      PIXEL_LEN * dtype_size,
        //                                  dtype_size
        // }
        // return {num_frames() * 1024 * 1024 * PIXEL_LEN * dtype_size, 
        //                             1024 * 1024 * PIXEL_LEN * dtype_size, 
        //                             1024 * PIXEL_LEN * dtype_size, 
        //                             PIXEL_LEN * dtype_size, 
        //                             dtype_size}
      }

      // void clear() {
      //   std::cout << "clear() called" << std::endl;
      //   std::fill(_frame_data, _frame_data + length(), 0);
      // }

      // void clear() {
      //       std::cout << "clear() called" << std::endl;
      //       std::cout << "_frame_data address: " << static_cast<void*>(_frame_data) << std::endl;
      //       if (_frame_data) {
      //           std::cout << "length() called" << std::endl;
      //           std::size_t len = length();
      //           std::cout << "Clearing memory of size: " << len << std::endl;
      //           std::fill(_frame_data, _frame_data + len, 0);
      //           std::cout << "Memory cleared" << std::endl;
      //       } else {
      //           std::cout << "Error: _frame_data is null" << std::endl;
      //       }
      //   }
            

      // ~ScreenObservation() { std::cout << "ScreenObservation destructor called" << std::endl;
      // delete[] _frame_data; }

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
      using Observation = ScreenObservation;
      using dtype = std::uint8_t; 


      explicit ScreenEnvironment(int num_agents, int frames_per_step, int arena_size, bool pellet_regen,
                                 int num_pellets, int num_viruses, int num_bots,
                                 screen_len screen_width, screen_len screen_height) :
        Super(num_agents, frames_per_step, arena_size, pellet_regen, num_pellets, num_viruses, num_bots),
        _observation(frames_per_step, screen_width, screen_height),
        frame_buffer(std::make_shared<FrameBufferObject>(screen_width, screen_height)),
        renderer(frame_buffer, this->engine_.arena_width(), this->engine_.arena_height()) {

          // std::cout << "ScreenEnvironment constructor called" << std::endl;

          if (!frame_buffer) {
              std::cerr << "Error: frame_buffer is null in ScreenEnvironment constructor" << std::endl;
          }
          // configure_observation(frames_per_step, screen_width, screen_height);
        }


      /* the shape of the observation object(s) */
      // const typename Observation::Shape &observation_shape() const {
      //   assert (_observation.size() > 0);
      //   return _observation[0].shape();
      // }

      // [[nodiscard]] const ScreenObservation &get_state() const { return _observation; }
      // [[nodiscard]] const ScreenObservation get_observations() const { return _observation; }
      // [[nodiscard]] const std::vector<ScreenObservation> &get_observations() const { return _observation; }
      [[nodiscard]] const ScreenObservation &get_observations() const { return _observation; }
      [[nodiscard]] screen_len screen_width() const { return frame_buffer->width(); }
      [[nodiscard]] screen_len screen_height() const { return frame_buffer->height(); }
    
      
      // void configure_observation(int frames_per_step, screen_len screen_width, screen_len screen_height) {
      //   _observation.clear();
      //   for (int i = 0; i < this->num_agents(); i++)
      //     _observation.emplace_back(frames_per_step, screen_width, screen_height );
      // }
      

    private:
      ScreenObservation _observation;
      // std::vector<ScreenObservation> _observation;
      std::shared_ptr<FrameBufferObject> frame_buffer;
      agario::Renderer renderer;

      // stores current frame into buffer containing the next observation
      void _partial_observation(Player &player, int frame_index) override {
        // std::cout << "_partial_observation() called for frame_index: " << frame_index << std::endl;
        renderer.render_screen(player, this->engine_.game_state());
        // void *data = _observation[frame_index].frame_data(frame_index);
        void *data = _observation.frame_data(frame_index);
        frame_buffer->copy(data);
      }

    };

} // namespace agario:env




