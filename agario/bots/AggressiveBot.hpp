#pragma once

#include <agario/core/Player.hpp>
#include <agario/bots/Bot.hpp>

#define AGGRESSIVE_RADIUS 20

namespace agario {
  namespace bot {


    template<bool renderable>
    class AggressiveBot : public Bot<renderable> {
    public:
      using Bot = Bot<renderable>;
      using Player = agario::Player<renderable>;
      using Cell = agario::Cell<renderable>;
      using GameState = GameState<renderable>;

      static constexpr agario::color default_color = agario::color::red;
      AggressiveBot(agario::pid pid, const std::string &name) : AggressiveBot(pid, name, default_color) {}
      explicit AggressiveBot(const std::string &name) : AggressiveBot(-1, name) {}
      explicit AggressiveBot(agario::pid pid) : AggressiveBot(pid, "AggressiveBot") {}

      AggressiveBot(agario::pid pid, const std::string &name, agario::color color)
        : Bot(pid, name, color), targeting(bot::no_player) { }

      void take_action(const GameState &state) override {
        if (this->cells.empty()) return; // dead: nothing to decide

        auto &largest_cell = this->largest_cell();

        // check if there are any wimpy players nearby
        for (Player *player_ptr : this->players_by_pid(state)) {
          Player &player = *player_ptr;
          if (player.pid() == this->pid()) continue; // skip self

          // is it nearby?
          auto distance = this->location().distance_to(player.location());
          if (distance <= AGGRESSIVE_RADIUS) {

            // can I eat it?
            auto edible_mass = this->edible_mass(player, largest_cell);
            if (edible_mass > 0) {
              this->target_player(player, largest_cell);
              return;
            }
          }

        }

        this->chase_pellet(state);
      }

    private:
      agario::pid targeting;
    };

  }
}