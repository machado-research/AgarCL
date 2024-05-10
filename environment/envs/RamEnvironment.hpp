#pragma once

#include <agario/engine/Engine.hpp>
#include <agario/core/types.hpp>
#include <agario/core/Entities.hpp>
#include <agario/core/Ball.hpp>
#include <agario/bots/bots.hpp>
#include <agario/engine/GameState.hpp>
#include <agario/core/settings.hpp>

#include <environment/envs/BaseEnvironment.hpp>

#include <tuple>

#define DEFAULT_NUM_FOODS 10

#ifdef RENDERABLE

#include "agario/core/renderables.hpp"
#include "agario/rendering/window.hpp"
#include "agario/rendering/renderer.hpp"

#endif

namespace agario {
  namespace env {

    template<bool renderable, int cell_limit, int num_foods>
    class RamObservation {
      using GameState = GameState<renderable>;
      using Player = Player<renderable>;
      using Cell = Cell<renderable>;
      using Pellet = Pellet<renderable>;
      using Virus = Virus<renderable>;
      using Food = Food<renderable>;

    public:
      using dtype = float;
      using Shape = std::tuple<int>;
      using Strides = std::tuple<ssize_t>;

      /* Construct a ram observation from the perspective of `player` for the
       * given game state `game_state`.
       */
      explicit RamObservation(const Player &player, const GameState &game_state,
                              int num_pellets, int num_viruses):
                              num_pellets(num_pellets), num_viruses(num_viruses){
        _make_shapes(game_state);
        _data = new dtype[length()];
      }

      void clear_data() {
        std::fill(_data, _data + length(), 0);
      }

      /* captures the state of the game as a "RAM" observation */
      void capture_ram(const Player &player, const GameState &game_state) {
        clear_data();
        _data[0] = game_state.ticks;
        _data[1] = game_state.arena_width;
        _data[2] = game_state.arena_height;

        int index = 3;
        index = _store_player(player, index);

        // store each player
        for (auto &pair : game_state.players) {
          if (*pair.second != player)
            index = _store_player(*pair.second, index);
        }

        index = _store_entities<Pellet>(game_state.pellets, index, num_pellets);
        index = _store_entities< Virus>(game_state.viruses, index, num_viruses);
        index = _store_entities<  Food>(game_state.foods,   index,   num_foods);
      }

      /* data buffer, mulit-dim array shape and sizes*/
      [[nodiscard]] const dtype *data() const { return _data; }
      [[nodiscard]] const Shape &shape() const { return _shape; }
      [[nodiscard]] const Shape & strides() const { return _strides; }

      /* full length of data array */
      [[nodiscard]] int length() const { return std::get<0>(shape()); }

      RamObservation(const RamObservation &) = delete; // no copy constructor
      RamObservation &operator=(const RamObservation &) = delete; // no copy assignments

      /* move constructor */
      RamObservation(RamObservation &&obs) noexcept : _data(std::move(obs._data)),
                                                      _shape(std::move(obs._shape)),
                                                      _strides(std::move(obs._strides)) {
        obs._data = nullptr;
      };

      /* move assignment */
      RamObservation &operator=(RamObservation &&obs) noexcept {
        _data = std::move(obs._data);
        _shape = std::move(obs._shape);
        _strides = std::move(obs._strides);
        obs._data = nullptr;
        return *this;
      };

      ~RamObservation() { delete[] _data; }

    private:
      dtype *_data;
      Shape _shape;
      Shape _strides;
      int num_pellets, num_viruses;

      /* crates the shape and strides to represent the multi-dimensional array */
      void _make_shapes(const GameState &state) {
        auto length = 1 + 2; // ticks, arena_width, arena_height
        length += 5 * state.players.size();
        length += 2 * num_pellets;
        length += 2 * num_viruses;
        length += 2 * num_foods; // not state.foods.size()

        _shape = { length };
        _strides = {(long) sizeof(dtype)};
      }

      /* stores the player's cell data into the data array */
      int _store_player(const Player &player, int start_index) {
        int cell_count = std::min<int>(cell_limit, player.cells.size());
        for (int i = 0; i < cell_count; i++) {
          auto &cell = player.cells[i];
          int index = start_index + 5 * i;

          _data[index + 0] = cell.mass();
          _data[index + 1] = cell.x;
          _data[index + 2] = cell.y;
          _data[index + 3] = cell.velocity.dx;
          _data[index + 4] = cell.velocity.dy;
        }
        return start_index + cell_count;
      }

      /* store the given entities in the data array at layer */
      template<typename U>
      int _store_entities(const std::vector<U> &entities, int start_index, int n) {

        int num_stored = 0;
        for (auto &entity : entities) {
          auto index = start_index + 2 * num_stored;

          _data[index + 0] = entity.x;
          _data[index + 1] = entity.y;

          if (++num_stored == n)
            break;
        }
        return start_index + 2 * n;
      }

    };

    template<bool renderable>
    class RamEnvironment : public BaseEnvironment<renderable> {
      using Player = agario::Player<renderable>;
      static constexpr int cell_limit = PLAYER_CELL_LIMIT;
      static constexpr int num_foods = DEFAULT_NUM_FOODS;

    public:
      using Observation = RamObservation<renderable, cell_limit, num_foods>;
      using dtype = typename Observation::dtype;

      typedef BaseEnvironment <renderable> Super;
      explicit RamEnvironment(int num_agents, int ticks_per_step, int arena_size, bool pellet_regen,
                              int num_pellets, int num_viruses, int num_bots) :
        Super(num_agents, ticks_per_step, arena_size, pellet_regen, num_pellets, num_viruses, num_bots),
        num_pellets(num_pellets), num_viruses(num_viruses) {
        /* could use "if constexpr" from C++17 but meh */
#ifdef RENDERABLE
        window = std::make_shared<Window>("Agar.io Environment", 512, 512);
        renderer = std::make_unique<agario::Renderer>(window,
                                                      this->engine_.arena_width(),
                                                      this->engine_.arena_height());
#endif
      }

      /* returns the length of the observation data  */
      typename Observation::Shape observation_shape() const {
        auto length = 1 + 2; // ticks, arena_width, arena_height
        length += 5 * (1 + this->num_bots_);
        length += 2 * num_pellets;
        length += 2 * num_viruses;
        length += 2 * num_foods;
        return { length };
      }

      /**
       * Returns the current state of the world (without advancing through time)
       * @return An Observation object representing the "RAM" of the game
       */
      const std::vector<Observation> &get_observations() const {
        return observations;
      }

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

        // only capture the RAM after the final tick of the step
        if (tick_index != this->ticks_per_step() - 1)
          return;

        auto &player = this->engine_.player(this->pids_[agent_index]);
        if (player.dead()) return;

        Observation &observation = observations[agent_index];
        auto &state = this->engine_.game_state();

        observation.capture_ram(player, state);
      }

      void render() override {
#ifdef RENDERABLE
        for (auto &pid: this->pids_) {
        auto &player = this->engine_.player(pid);
        renderer->render_screen(player, this->engine_.game_state());
      }

        glfwPollEvents();
        window->swap_buffers();
#endif
      }

    private:
      std::vector<Observation> observations;
      int num_pellets, num_viruses;

#ifdef RENDERABLE
      std::unique_ptr<agario::Renderer> renderer;
      std::shared_ptr<Window> window;
#endif
    };

  } // namespace env
} // namespace agario
