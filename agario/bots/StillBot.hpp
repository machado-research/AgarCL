#pragma once

#include <agario/core/Player.hpp>
#include <agario/bots/Bot.hpp>

namespace agario {
  namespace bot {

    template<bool renderable>
    class StillBot : public Bot<renderable> {
    public:
      typedef Bot<renderable> Bot;

      StillBot(agario::pid pid, const std::string &name, agario::color color) : Bot(pid, name, color) {
        this->kill();
        agario::mass agent_mass = 5000; // mass of the agent
        agario::Location loc = agario::Location(70,70);
        this->add_cell(loc, agent_mass);
      }
      StillBot(agario::pid pid, const std::string &name) : StillBot(pid, name, agario::color::blue) {}
      explicit StillBot(const std::string &name) : StillBot(-1, name) {}
      explicit StillBot(agario::pid pid) : StillBot(pid, "StillBot") {}

      void take_action(const GameState <renderable> &state) override {
        this->action = agario::action::none;
        // this->target = this->location(); // stay still
        this->target = agario::Location(rand()%200, this->y());
        // this->target = this->nearest_pellet(state);
      }

    };


  }
}
