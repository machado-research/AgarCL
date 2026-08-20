#pragma once

#include <agario/core/Player.hpp>
#include <agario/bots/Bot.hpp>

#define AGGRESSIVE_RADIUS 20

namespace agario {
  namespace bot {


    template<bool renderable>
    class AggressiveShyBot : public Bot<renderable> {
    public:
      using Bot = Bot<renderable>;
      using Player = agario::Player<renderable>;
      using Cell = agario::Cell<renderable>;
      using GameState = GameState<renderable>;

      static constexpr agario::color default_color = agario::color::orange;
      AggressiveShyBot(agario::pid pid, const std::string &name) : AggressiveShyBot(pid, name, default_color) {}
      explicit AggressiveShyBot(const std::string &name) : AggressiveShyBot(-1, name) {}
      explicit AggressiveShyBot(agario::pid pid) : AggressiveShyBot(pid, "AggressiveShyBot") {}

      AggressiveShyBot(agario::pid pid, const std::string &name, agario::color color)
        : Bot(pid, name, color), targeting(bot::no_player) { }

      void take_action(const GameState &state) override {

        // check if there are any big players nearby
        for (auto &pair : state.players) {
          Player &other_player = *pair.second;
          if (other_player == *this) continue; // skip self

          // is it nearby?
          auto distance = this->location().distance_to(other_player.location());

          // it is scary?
          /* `this->mass()` is required: Player is a dependent base, so
           * unqualified lookup skips it and finds the *type* agario::mass
           * instead, making `mass()` a cast that yields 0. The comparison was
           * always true, so this bot fled from any nearby player and the
           * aggressive branch below was unreachable whenever one was in
           * range. */
          if (distance < SHY_RADIUS && other_player.mass() > this->mass()) {
            // yes! run (directly) away!
            this->target = this->location() - (other_player.location() - this->location());
            return;
          }
        }

        auto &largest_cell = this->largest_cell();

        // check if there are any wimpy players nearby
        for (auto &pair : state.players) {
          auto &player = *pair.second;
          if (player == *this) continue; // skip self

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
