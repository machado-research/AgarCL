#pragma once

#include <agario/core/Player.hpp>
#include <agario/bots/Bot.hpp>

#define SHY_RADIUS 25

namespace agario {
  namespace bot {

    template<bool renderable>
    class HungryShyBot : public Bot<renderable> {
      static constexpr agario::color default_color = agario::color::purple;
    public:
      typedef Bot<renderable> Bot;
      typedef agario::Player<renderable> Player;

      HungryShyBot(agario::pid pid, const std::string &name, agario::color color) : Bot(pid, name, color) {}
      HungryShyBot(agario::pid pid, const std::string &name) : HungryShyBot(pid, name, default_color) {}
      explicit HungryShyBot(const std::string &name) : HungryShyBot(-1, name) {}
      explicit HungryShyBot(agario::pid pid) : HungryShyBot(pid, "HungryShyBot") {}

      void take_action(const GameState<renderable> &state) override {
        this->action = agario::action::none; // no splitting or anything

        // check if there are any big players nearby
        for (auto &pair : state.players) {
          Player &other_player = *pair.second;
          if (other_player == *this) continue; // skip self

          // is it nearby?
          auto distance = this->location().distance_to(other_player.location());

          // it is scary?
          /* `this->mass()` is required: Player is a dependent base, so
           * unqualified lookup skips it and instead finds the *type*
           * agario::mass, making `mass()` a cast that yields 0. The
           * comparison was therefore always true and this bot fled from
           * every nearby player, including ones it far outweighed. */
          if (distance < SHY_RADIUS && other_player.mass() > this->mass()) {
            // yes! run (directly) away!
            this->target = this->location() - (other_player.location() - this->location());
            return;
          }
        }

        // no cells are too close for comfort... forage for foods
        this->target = this->nearest_pellet(state);
      }

    };

  }
}
