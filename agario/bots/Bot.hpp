#pragma once

#include <agario/engine/GameState.hpp>
#include <agario/core/Player.hpp>

#define NO_PLAYER (-1)


namespace agario {
  namespace bot {

    static constexpr agario::pid no_player = NO_PLAYER;

    template<bool renderable>
    class Bot : public agario::Player<renderable> {

      using Player = agario::Player<renderable> ;
      using Cell = agario::Cell<renderable>;
      using GameState = agario::GameState<renderable>;

      static constexpr agario::color default_color = agario::color::yellow;

    public:
      Bot(agario::pid pid, const std::string &name, agario::color color) : Player(pid, name, color) {this->is_bot = true;}
      Bot(agario::pid pid, const std::string &name) : Bot(pid, name, default_color)  {this->is_bot = true;}
      explicit Bot(const std::string &name) : Bot(-1, name) {this->is_bot = true;}
      explicit Bot(agario::pid pid) : Bot(pid, "Bot") {this->is_bot = true;}

    protected:

      void chase_pellet(const GameState &state) {
        this->action = agario::action::none;
        this->target = this->nearest_pellet(state);
      }

      agario::pid find_target (const GameState &state, const Cell &largest_cell, agario::distance radius) const {

        agario::pid target = bot::no_player;
        agario::mass target_mass = 0;

        for (auto &pair : state.players) {
          auto &player = *pair.second;
          auto proximity = this->location().distance_to(player.location());
          if (proximity < radius) {
            auto mass = this->edible_mass(player, largest_cell);
            if (target == bot::no_player || mass > target_mass) {
              target = player.pid();
              target_mass = mass;
            }
          }
        }
        return target;
      }

      /* weighted average of the cells that we can eat from this player */
      void target_player (const Player &player, const Cell &largest_cell) {
        agario::mass mass = 0;
        agario::Location target;
        for (auto &cell : player.cells) {
          if (largest_cell.can_eat(cell)) {
            target += cell.mass() * cell.location();
            mass += cell.mass();
          }
        }
        auto ds = (target / mass) - this->location();
        this->target = this->location() + 3 * ds;
      }

      /* the largest cell that belongs to the bot */
      const Cell& largest_cell () const {
        int largest = 0;
        for (int i = 0; i < this->cells.size(); ++i) {
          if (i == 0 || this->cells[i].mass() > this->cells[largest].mass()) {
            largest = i;
          }
        }
        return this->cells.at(largest);
        // return this->cells[0];
      }

      agario::mass edible_mass (const Player &player, const Cell &largest_cell) const {
        agario::mass mass = 0;
        for (auto &cell : player.cells) {
          if (largest_cell.can_eat(cell))
            mass += cell.mass();
        }
        return mass;
      }


      /* location of the nearest pellet */
      agario::Location nearest_pellet(const GameState &state) const {
        if (state.pellets.empty()) {
          return agario::Location(
          std::rand() % static_cast<int>(state.config.arena_width),
          std::rand() % static_cast<int>(state.config.arena_height));
        }

        // 1/10 chance to pick a random pellet
        // if (std::rand() % 10 == 0) {
        //   const auto &random_pellet = state.pellets[std::rand() % state.pellets.size()];
        //   if (random_pellet.location().distance_to(this->location()) < 25) {
        //         return agario::Location((std::rand() + static_cast<int>(random_pellet.location().x)) % static_cast<int>(state.config.arena_width),
        //         (std::rand() + static_cast<int>(random_pellet.location().y)) % static_cast<int>(state.config.arena_height));
        //   }
        //   return random_pellet.location();
        // }

        agario::Location target;
        distance min_distance = agario::distance::max();

        for (const auto &pellet : state.pellets) {
          distance dist = pellet.location().distance_to(this->location());
          if (dist < min_distance && dist > 0.01) {
              target = pellet.location();
              min_distance = dist;
          }
        }

        // If the nearest pellet is at the same location as the bot, adjust the target slightly
        if (min_distance < 0.01) {
          target += agario::Location( target.x +
          std::rand() % static_cast<int>(state.config.arena_width),
          target.y +
          std::rand() % static_cast<int>(state.config.arena_height));
        }

        return target;
      }

      // agario::Location nearest_pellet (const GameState &state) const {
      //   if(state.pellets.empty()) {
      //       return agario::Location(std::rand() % static_cast<int>(state.config.arena_width), std::rand() % static_cast<int>(state.config.arena_height));
      //   }
      //   distance min_distance = agario::distance::max();
      //   agario::Location target;

      //   if(state.pellets.size() %10 == 0)
      //   {
      //       std::srand(std::chrono::system_clock::now().time_since_epoch().count());
      //       auto &pellet = state.pellets[std::rand() % state.pellets.size()];
      //       if (pellet.location().distance_to(this->location()) == 0) {
      //           target = pellet.location() + agario::Location(std::rand() % static_cast<int>(state.config.arena_width), std::rand() % static_cast<int>(state.config.arena_height));

      //       }
      //       else
      //         target = pellet.location();
      //   }
      //   else {
      //       for (auto &pellet : state.pellets) {
      //         distance dist = pellet.location().distance_to(this->location());
      //         if (dist < min_distance) {
      //           target = pellet.location();
      //           min_distance = dist;
      //         }
      //       }
      //       if (target.location().distance_to(this->location()) == 0) {
      //         target = pellet.location() + agario::Location(std::rand() % static_cast<int>(state.config.arena_width), std::rand() % static_cast<int>(state.config.arena_height));

      //     }
      //   }
      //   return target;
      // }
      /* location of the nearest food */
      agario::Location nearest_food (const GameState &state) const {
        distance min_distance = agario::distance::max();
        agario::Location target;
        for (auto &food : state.foods) {
          distance dist = food.location().distance_to(this->location());
          if (dist < min_distance) {
            target = food.location();
            min_distance = dist;
          }
        }
        return target;
      }

    };
  }
}
