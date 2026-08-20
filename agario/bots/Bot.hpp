#pragma once

#include <cstdint>
#include <limits>


#include <agario/engine/GameState.hpp>
#include <agario/core/Player.hpp>

#define NO_PLAYER (-1)


namespace agario {
  namespace bot {

    /* agario::pid is unsigned, so this sentinel is the maximum pid value.
     * Bots constructed without an explicit pid carry it until the engine
     * assigns one in add_player(). */
    static constexpr agario::pid no_player = NO_PLAYER;

    template<bool renderable>
    class Bot : public agario::Player<renderable> {

      using Player = agario::Player<renderable> ;
      using Cell = agario::Cell<renderable>;
      using GameState = agario::GameState<renderable>;

      static constexpr agario::color default_color = agario::color::yellow;

    public:
      Bot(agario::pid pid, const std::string &name, agario::color color) : Player(pid, name, color) {this->is_bot = true;}
      Bot(agario::pid pid, const std::string &name) : Bot(pid, name, default_color) {}
      explicit Bot(const std::string &name) : Bot(no_player, name) {}
      explicit Bot(agario::pid pid) : Bot(pid, "Bot") {}

    protected:

      void chase_pellet(const GameState &state) {
        this->action = agario::action::none;
        this->target = this->nearest_pellet(state);
      }

      /* Players ordered by pid.
       *
       * state.players is an unordered_map, and every bot scan below takes the
       * *first* qualifying player and returns. Iterating the map directly
       * therefore made "which player do I flee from / attack" depend on hash
       * bucket layout, which varies with standard library implementation and
       * insertion history - so a seeded episode was not reproducible across
       * machines. Iterating in pid order keeps the same first-match rule while
       * making "first" well defined. The buffer is a member, so the ordering
       * costs no allocation after the first call. */
      const std::vector<Player *> &players_by_pid(const GameState &state) const {
        sorted_players_.clear();
        sorted_players_.reserve(state.players.size());
        for (auto &pair : state.players)
          sorted_players_.push_back(pair.second.get());
        std::sort(sorted_players_.begin(), sorted_players_.end(),
                  [](const Player *a, const Player *b) { return a->pid() < b->pid(); });
        return sorted_players_;
      }

      /* weighted average of the cells that we can eat from this player */
      void target_player (const Player &player, const Cell &largest_cell) {
        agario::mass edible = 0;
        agario::Location target;
        for (auto &cell : player.cells) {
          if (largest_cell.can_eat(cell)) {
            target += cell.mass() * cell.location();
            edible += cell.mass();
          }
        }
        /* Nothing edible: dividing by zero mass would yield NaN coordinates,
         * which propagate into movement and observations (and
         * static_cast<int>(NaN) is undefined behaviour). Callers currently
         * pre-check edible_mass > 0, so this is a guard rather than a
         * behaviour change. */
        if (edible == 0) return;
        auto ds = (target / edible) - this->location();
        this->target = this->location() + 3 * ds;
      }

      /* The largest cell that belongs to the bot.
       * Requires a live bot: callers must check cells before calling, which
       * every take_action() now does. Previously .at() would throw
       * std::out_of_range on a dead bot, and an exception escaping through the
       * pybind layer would end the episode. */
      const Cell& largest_cell () const {
        std::size_t largest = 0;
        for (std::size_t i = 1; i < this->cells.size(); ++i) {
          if (this->cells[i].mass() > this->cells[largest].mass())
            largest = i;
        }
        return this->cells[largest];
      }

      /* reused ordering buffer for players_by_pid (mutable: the scans that
       * need it are const) */
      mutable std::vector<Player *> sorted_players_;

      agario::mass edible_mass (const Player &player, const Cell &largest_cell) const {
        agario::mass mass = 0;
        for (auto &cell : player.cells) {
          if (largest_cell.can_eat(cell))
            mass += cell.mass();
        }
        return mass;
      }


      /* A wander target used when there is nothing to forage. Derived from the
       * bot's pid and the tick counter rather than std::rand(): the global C
       * RNG is process-wide shared state, so drawing from it here made a
       * seeded episode non-reproducible (and impossible to seed per
       * environment when several share a process). */
      agario::Location wander_target(const GameState &state) const {
        const std::uint32_t w = static_cast<std::uint32_t>(state.config.arena_width);
        const std::uint32_t h = static_cast<std::uint32_t>(state.config.arena_height);
        if (w == 0 || h == 0) return agario::Location(0, 0); // degenerate arena
        std::uint32_t s = static_cast<std::uint32_t>(this->pid()) * 2654435761u
                        + static_cast<std::uint32_t>(state.ticks) * 40503u;
        s ^= s >> 15; s *= 2246822519u; s ^= s >> 13;
        return agario::Location(static_cast<agario::distance>(s % w),
                                static_cast<agario::distance>((s / (w ? w : 1)) % h));
      }

      /* location of the nearest pellet */
      agario::Location nearest_pellet(const GameState &state) const {
        if (state.pellets.empty())
          return wander_target(state);


        /* The bot's own centroid is loop-invariant but was recomputed for
         * every pellet, and location() is two O(cells) reductions that each
         * call mass() - with ~1000 pellets that dominated the tick. Distances
         * are also compared squared, which removes ~1000 sqrt calls; ordering
         * and the 0.01 cutoff are unchanged, since both are monotonic in the
         * squared value. */
        const agario::Location me = this->location();
        const float me_x = static_cast<float>(me.x);
        const float me_y = static_cast<float>(me.y);
        constexpr float min_sq = 0.01f * 0.01f;

        agario::Location target;
        bool found = false;
        float best_sq = std::numeric_limits<float>::max();

        for (const auto &pellet : state.pellets) {
          const float dx = static_cast<float>(pellet.x) - me_x;
          const float dy = static_cast<float>(pellet.y) - me_y;
          const float d_sq = dx * dx + dy * dy;
          if (d_sq < best_sq && d_sq > min_sq) {
            target = pellet.location();
            best_sq = d_sq;
            found = true;
          }
        }

        /* Every pellet sits on top of the bot (or the list is otherwise
         * unusable): wander instead. This previously fell through with an
         * unset target and sent the bot to world origin - the guard it used
         * was unreachable, since the loop only ever assigned a distance
         * greater than the cutoff. */
        if (!found) return wander_target(state);

        return target;
      }


    };
  }
}
