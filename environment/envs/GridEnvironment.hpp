#pragma once

#include <cassert>

#include <agario/engine/Engine.hpp>
#include <agario/core/types.hpp>
#include <agario/core/Entities.hpp>
#include <agario/core/Ball.hpp>
#include <agario/bots/bots.hpp>
#include <agario/engine/GameState.hpp>

#include "environment/envs/BaseEnvironment.hpp"

#ifdef RENDERABLE
#include <agario/core/renderables.hpp>
#include <agario/rendering/renderer.hpp>
#include "agario/rendering/FrameBufferObject.hpp"
#endif

#define DEFAULT_GRID_SIZE 128
# define PIXEL_LEN 3

namespace agario::env {

    template<typename T, bool renderable>
    class GridObservation {
      using GameState = GameState<renderable>;
      using Player = Player<renderable>;
      using Cell = Cell<renderable>;
      using Pellet = Pellet<renderable>;
      using Virus = Virus<renderable>;
      using Food = Food<renderable>;

      enum calc_type {
        at_least_ = 0,
        total_mass_ = 1,
        min_ = 2,
        max_ = 3
      };

    public:
      using dtype = T;
      using Shape = std::tuple<int, int, int>;
      using Strides = std::tuple<ssize_t, ssize_t, ssize_t>;

      /* construct without configuring. configure() must be called. */
      GridObservation() : data_(nullptr) { }

      /* construct with configuration. configure() need not be called */
      template <typename ...Args>
      explicit GridObservation(Args&&... args) : config_(args...) {
        _make_shapes();
        data_ = new dtype[length()];
        clear_data();
      }

      /* configures the observation for a particular size */
      template <typename ...Args>
      void configure(Args&&... args) {
        config_(args...);

        delete[] data_; // might be nullptr, thats ok

        _make_shapes();
        data_ = new dtype[length()];
        clear_data();
      }

      [[nodiscard]] bool configured() const  { return data_ != nullptr; }

      /* data buffer, mulit-dim array shape and sizes*/
      const dtype *data() const {
        if (!configured())
          throw EnvironmentException("GridObservation was not configured.");
        return data_;
      }

      [[nodiscard]] const Shape &shape() const {
        if (!configured())
          throw EnvironmentException("GridObservation was not configured.");
        return shape_;
      };

      [[nodiscard]] const Strides &strides() const {
        if (!configured())
          throw EnvironmentException("GridObservation was not configured.");
        return strides_;
      }

      /* adds a single frame to the observation at index `frame_index` */
      void add_frame(const Player &player, const GameState &game_state, int frame_index) {
        if (data_ == nullptr)
          throw EnvironmentException("GridObservation was not configured.");

        int channel = channels_per_frame() * frame_index;
        _mark_out_of_bounds(player, channel, game_state.config.arena_width, game_state.config.arena_height);
        if (config_.observe_pellets) {
          channel++;
          _store_entities<Pellet>(game_state.pellets, player, channel, calc_type::at_least_); //at least one_pellet
          channel++;
          _store_entities<Pellet>(game_state.pellets, player, channel, calc_type::total_mass_); //total_number_of_pellets
        }

        if (config_.observe_viruses) {
          channel++;
          _store_entities<Virus>(game_state.viruses, player, channel, calc_type::at_least_); //at least one_virus
          channel++;
          _store_entities<Virus>(game_state.viruses, player, channel, calc_type::total_mass_); //total_number_of_viruses
        }
        if (config_.observe_cells) {
          channel++;
          _store_entities<Cell>(player.cells, player, channel); //just one cell (agent)
        }
        if (config_.observe_others) {
          channel++;
          for (auto &pair : game_state.players) {
            Player &other_player = *pair.second;
            if (other_player.pid() == player.pid()) continue;
            _store_entities<Cell>(other_player.cells, player, channel, calc_type::min_); //min_mass
            _store_entities<Cell>(other_player.cells, player, channel+1, calc_type::max_); //max_mass
          }
        }
      }

      void clear_data() {
        std::fill(data_, data_ + length(), 0);
      }

      /* full length of data array */
      [[nodiscard]] int length() const {
        return std::get<0>(shape_) * std::get<1>(shape_) * std::get<2>(shape_);
      }

      /* the number of frames captured by the observation */
      [[nodiscard]] int num_frames() const { return config_.num_frames; }


      // no copy operations because if you're copying this object then
      // you're probably not using it correctly
      GridObservation(const GridObservation &) = delete; // no copy constructor
      GridObservation &operator=(const GridObservation &) = delete; // no copy assignments

      /* move constructor */
      GridObservation(GridObservation &&obs) noexcept :
        data_(std::move(obs.data_)),
        shape_(std::move(obs.shape_)),
        strides_(std::move(obs.strides_)),
        config_(std::move(obs.config_)) {
        obs.data_ = nullptr;
      };

      /* move assignment */
      GridObservation &operator=(GridObservation &&obs) noexcept {
        data_ = std::move(obs.data_);
        shape_ = std::move(obs.shape_);
        strides_ = std::move(obs.strides_);
        config_ = std::move(obs.config_);
        obs.data_ = nullptr;
        return *this;
      };
      ~GridObservation() { delete[] data_; }

    private:
      dtype *data_;
      Shape shape_;
      Strides strides_;

      /* observation configuration parameters */
      class Configuration {
      public:
        Configuration(int num_frames, int grid_size,
                      bool observe_cells, bool observe_others,
                      bool observe_viruses, bool observe_pellets) :
          num_frames(num_frames), grid_size(grid_size),
          observe_cells(observe_cells), observe_others(observe_others),
          observe_pellets(observe_pellets), observe_viruses(observe_viruses) {}
        int num_frames;
        int grid_size;
        bool observe_pellets;
        bool observe_cells;
        bool observe_viruses;
        bool observe_others;
      };

      Configuration config_;

      /* the number of channels in each frame */
      [[nodiscard]] int channels_per_frame() const {
        // the +1 is for the out-of-bounds channel
        // Pellets: one says if there is a pellet in the cell, the other says the total pellets in the cell.
        // Viruses: one says if there is a virus in the cell, the other says the total viruses in the cell.
        // Observing others: one says if there is a cell in the cell, the other says the maximum in the cell.
        return static_cast<int>(1 + config_.observe_cells + 2*config_.observe_others
                                + 2*config_.observe_viruses + 2*config_.observe_pellets);
      }

      /* creates the shape and strides to represent the multi-dimensional array */
      void _make_shapes() {
        int num_channels = config_.num_frames * channels_per_frame();

        shape_ = {num_channels, config_.grid_size, config_.grid_size};

        auto dtype_size = static_cast<long>(sizeof(dtype));
        strides_ = {
          config_.grid_size * config_.grid_size * dtype_size,
          config_.grid_size * dtype_size,
          dtype_size
        };
      }

      /* stores the given entities in the data array at the given `channel` */
      template<typename U>
      void _store_entities(const std::vector<U> &entities, const Player &player, int channel, calc_type calc = calc_type::total_mass_) {
        float view_size = _view_size(player);

        int grid_x, grid_y;
        for (auto &entity : entities) {
          _world_to_grid(player, entity.location(), view_size, grid_x, grid_y);

          int index = _index(channel, grid_x, grid_y);
          if (_inside_grid(grid_x, grid_y)) {
             if(calc == calc_type::at_least_)
              data_[index] = entity.mass();
             else if(calc == calc_type::total_mass_)
              data_[index] += entity.mass();
             else if(calc == calc_type::max_)
                data_[index] = std::max(static_cast<int>(data_[index]), static_cast<int>(entity.mass()));
             else
                data_[index] = (data_[index] == 0 ? static_cast<int>(entity.mass()) : std::min(static_cast<int>(data_[index]), static_cast<int>(entity.mass())));
        }
      }
    }

      /* marks out-of-bounds locations on the given `channel` */
      void _mark_out_of_bounds(const Player &player, int channel,
                               agario::distance arena_width, agario::distance arena_height) {
        float view_size = _view_size(player);

        int centering = config_.grid_size / 2;
        for (int i = 0; i < config_.grid_size; i++)
          for (int j = 0; j < config_.grid_size; j++) {

            auto loc = _grid_to_world(player, view_size, i, j);
            int index = _index(channel, i, j);
            bool in_bounds = _in_bounds(loc, arena_width, arena_height);
            data_[index] = in_bounds ? 0 : -1;
          }
      }

      /* determines what the view size should be, based on the player's mass */
      float _view_size(const Player &player) const {
        // todo: make this "consistent" with the renderer's view (somewhat tough)
        return agario::clamp<float>(2 * player.mass(), 100, 300);
      }

      /* converts world-coordinates to grid-coordinates */
      void _world_to_grid(const Player &player, const Location &loc,
                          float view_size, int &grid_x, int &grid_y) const {

        float centering = config_.grid_size / 2.0;

        auto diff_x = loc.x - player.x();
        auto diff_y = loc.y - player.y();

        grid_x = static_cast<int>(config_.grid_size * diff_x / view_size + centering);
        grid_y = static_cast<int>(config_.grid_size * diff_y / view_size + centering);
      }

      /* converts grid-coordinates to world-coordinates */
      Location _grid_to_world(const Player &player, float view_size, int grid_x, int grid_y) const {
        float centering = config_.grid_size / 2.0;

        float x_diff = static_cast<float>(grid_x) - centering;
        float y_diff = static_cast<float>(grid_y) - centering;

        float dx = x_diff * view_size / config_.grid_size;
        float dy = y_diff * view_size / config_.grid_size;
        return player.location() + Location(dx, dy);
      }

      /* the index of a given channel, x, y grid-coordinate in the `_data` array */
      [[nodiscard]] int _index(int channel, int grid_x, int grid_y) const {
        int channel_stride = config_.grid_size * config_.grid_size;
        int x_stride = config_.grid_size;
        int y_stride = 1;
        return channel_stride * channel + x_stride * grid_x + y_stride * grid_y;
      }

      /* determines whether the given x, y grid-coordinates, are within the grid */
      [[nodiscard]] bool _inside_grid(int grid_x, int grid_y) const {
        return 0 <= grid_x && grid_x < config_.grid_size && 0 <= grid_y && grid_y < config_.grid_size;
      }

      bool _in_bounds(const Location &loc, distance arena_width, distance arena_height) {
        return 0 <= loc.x && loc.x < arena_width && 0 <= loc.y && loc.y < arena_height;
      }
    };

    class FrameObservation {
    public:
      explicit FrameObservation(int num_frames, screen_len width, screen_len height) :
        _num_frames(num_frames), _width(width), _height(height) {
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

        [[nodiscard]] std::vector<int> frame_shape() const {
          return {_num_frames, _width, _height, PIXEL_LEN};
        }

        [[nodiscard]] std::vector<ssize_t> frame_strides() const {
          return {
            _width * _height * PIXEL_LEN * dtype_size,
                    _height * PIXEL_LEN * dtype_size,
                              PIXEL_LEN * dtype_size,
                                          dtype_size
          };
        }

        ~FrameObservation() {
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


    template<typename T, bool renderable>
    class GridEnvironment : public BaseEnvironment<renderable> {
      using Player = agario::Player<renderable>;
      using GridObservation = GridObservation<T, renderable>;
      using Super = BaseEnvironment<renderable>;

    public:
      using dtype = T;
      using Observation = GridObservation;

      explicit GridEnvironment(int num_agents, int ticks_per_step, int arena_size, bool pellet_regen,
                               int num_pellets, int num_viruses, int num_bots, bool reward_type, int c_death) :
        Super(num_agents, ticks_per_step, arena_size, pellet_regen,
              num_pellets, num_viruses, num_bots,reward_type),
        frame_observation(ticks_per_step, 512, 512),
        frame_buffer(std::make_shared<FrameBufferObject>(512, 512, false)) {

#ifdef RENDERABLE
        renderer = std::make_unique<agario::Renderer>(frame_buffer,
                                                      this->engine_.arena_width(),
                                                      this->engine_.arena_height());

#endif
      }

      /* Configures the observation types that will be returned. */
      template <typename ...Config>
      void configure_observation(Config&&... config) {
        observations.clear();
        for (int i = 0; i < this->num_agents(); i++)
          observations.emplace_back(config...);
      }

      /* the shape of the observation object(s) */
      const typename Observation::Shape &observation_shape() const {
        assert (observations.size() > 0);
        return observations[0].shape();
      }

      /**
       * Returns the current state of the world without advancing through time
       * @return An Observation object containing all of the
       * locations of every entity in the current state of the game world
       */
      const std::vector<Observation> &get_observations() const { return observations; }

      /* since we reuse the observation's data buffer for each step,
       * we need to have the data cleared at the beginning of each step */
      void _step_hook() override {
        for (auto &observation : observations)
          observation.clear_data();
      }

      /* allows for intermediate grid frames to be stored in the GridObservation */
      void _partial_observation(int agent_index, int tick_index) override {

        assert(agent_index < this->num_agents());
        assert(tick_index < this->ticks_per_step());

          auto &player = this->engine_.player(this->pids_[agent_index]);
          this-> c_death_ = 0;

          Observation &observation = observations[agent_index];


          auto &state = this->engine_.game_state();
          // we store in the observation the last `num_frames` frames between each step
          int frame_index = tick_index - (this->ticks_per_step() - observation.num_frames());
          if (frame_index >= 0) // frame skipping
            observation.add_frame(player, state, frame_index);


        last_player = &player;
        last_frame_index = frame_index;
      }
      void render() override {
#ifdef RENDERABLE

      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glViewport(0, 0, frame_buffer->width(), frame_buffer->height());
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      for (auto &pid: this->pids_) {
        auto &player = this->engine_.player(pid);
        renderer->render_screen(player, this->engine_.game_state());
      }

      glfwPollEvents();
      frame_buffer -> swap_buffers();
      frame_buffer -> show();
#endif
      }

    FrameObservation& get_frame() {
      save_frames();
      return frame_observation;
    }

    std::tuple<int, int, int, int> frame_observation_shape() const {
        std::vector<int> shape_vec = frame_observation.frame_shape();
        return std::make_tuple(shape_vec[0], shape_vec[1], shape_vec[2], shape_vec[3]);
      }

      void save_frames() {
#ifdef RENDERABLE
      if (last_player != nullptr) {
        for (int frame_index = 0; frame_index < frame_observation.num_frames(); ++frame_index) {
            renderer->render_screen(*last_player, this->engine_.game_state());
            void *data = frame_observation.frame_data(frame_index);
            frame_buffer->copy(data);
        }
      }
#endif
      }

       void close() override {
#ifdef RENDERABLE
      renderer->close_program();
      // glfwTerminate();
      // glDeleteProgram(renderer->shader.program);
#endif
    }
    virtual ~GridEnvironment() {
#ifdef RENDERABLE
#endif
    }

    private:
      std::vector<Observation> observations;
      FrameObservation frame_observation;
      int last_frame_index = 0;  // Store the last frame index
      Player* last_player = nullptr;  // Store the last processed player

#ifdef RENDERABLE
      std::unique_ptr<agario::Renderer> renderer;
      std::shared_ptr<FrameBufferObject> frame_buffer;
#endif
    };

} // namespace agario::env
