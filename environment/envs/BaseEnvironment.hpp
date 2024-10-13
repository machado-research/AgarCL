#pragma once

#include <agario/engine/Engine.hpp>
#include <agario/core/types.hpp>
#include <agario/core/Entities.hpp>
#include <agario/core/Ball.hpp>
#include <agario/bots/bots.hpp>
#include "agario/engine/GameState.hpp"

#include <tuple>

// 60 frames per second: the default amount of time between frames of the game
#define DEFAULT_DT (1.0 / 60)

namespace agario {
  namespace env {

    class EnvironmentException : public std::runtime_error {
      using runtime_error::runtime_error;
    };

    /* represents a full action in the environment */
    class Action {
    public:
      Action(float dx, float dy, action a) : dx(dx), dy(dy), a(a) { }
      float dx, dy; // target relative to player location
      agario::action a; // game-action (i.e. split/feed/none)
    };

    typedef double reward;

    template<bool renderable>
    class BaseEnvironment {
      using Player = agario::Player<renderable>;

    public:

      explicit BaseEnvironment(
        int num_agents,
        int ticks_per_step,
        int arena_size,
        bool pellet_regen,
        int num_pellets,
        int num_viruses,
        int num_bots,
        bool reward_type,
        int c_death = 0
      ):
        num_agents_(num_agents),
        dones_(num_agents),
        engine_(arena_size, arena_size, num_pellets, num_viruses, pellet_regen),
        ticks_per_step_(ticks_per_step),
        num_bots_(num_bots),
        reward_type_(reward_type),
        step_dt_(DEFAULT_DT),
        c_death_(c_death)
      {
        pids_.reserve(num_agents);
        reset();
      }

      virtual void close(){}
      ~BaseEnvironment()=default;
      [[nodiscard]] int num_agents() const { return num_agents_; }

      void repsawn_all_players(){
        for(auto &pair : this->engine_.state.players){
          auto pid = pair.first;
          auto player = pair.second;
          if(player->dead()){
            this->engine_.respawn(*player);
          }
        }
      }

      /**
       * Steps the environment forward by several game frames
       * @return the reward accumulated by the player during those
       * frames, which is equal to the difference in it's mass before
       * and after the step
       */
      std::vector<reward> step() {
        this->_step_hook(); // allow subclass to set itself up for the step

        auto before = masses<float>();
        for (int tick = 0; tick < ticks_per_step(); tick++)
          engine_.tick(step_dt_);

        for (int agent = 0; agent < num_agents(); agent++)
          this->_partial_observation(agent, 0);
        repsawn_all_players();

        // reward could be the current mass or the difference in mass from the last step
        auto rewards = masses<reward>();
        if(reward_type_){
          for (int i = 0; i < num_agents(); ++i)
            rewards[i] -= (before[i] - c_death_);
        }

        return rewards;
      }

        /* the mass of each rl-controlled player */
      template<typename T>
      std::vector<T> masses() const {
        std::vector<T> masses_;
        masses_.reserve(num_agents());
        for (const auto &[pid, player] : engine_.players()) {
          if (player->is_bot) continue;
          masses_.push_back(static_cast<T>(player->mass()));
        }
        return masses_;
      }

      /* take an action for each agent */
      void take_actions(const std::vector<Action> &actions) {
        if (actions.size() != num_agents())
          throw EnvironmentException("Number of actions (" + std::to_string(actions.size())
                                     + ") does not match number of agents (" + std::to_string(num_agents()) + ")");

        for (int i = 0; i < num_agents(); i++)
          take_action(pids_[i], actions[i]);
      }

      /* set the action for a given player `pid` */
      void take_action(agario::pid pid, const Action &action) {
        take_action(pid, action.dx, action.dy, action.a);
      }

      /**
       * Specifies the next action for the agent to take
       * but does not step the game forwards in time. This
       * just specifies what action will be taken by
       * the agent on the next call to step
       * @param dx from -1 to 1 specifying x direction to go in
       * @param dy from -1 to 1 specifying y direction go to in
       * @param action {0, 1, 2} meaning, none, split, feed
       */
      void take_action(agario::pid pid, float dx, float dy, int action) {
        auto &player = engine_.player(pid);

        if (player.dead()) return; // its okay to take action on a dead player

        /* todo: this isn't exactly "calibrated" such such that
         * dx = 1 means move exactly the maximum speed */
        auto target_x = player.x() + dx * 10;
        auto target_y = player.y() + dy * 10;

        player.action = static_cast<agario::action>(action);
        player.target = agario::Location(target_x, target_y);
      }

      /* resets the environment by resetting the game engine. */
      void reset() {
        engine_.reset();
        pids_.clear();
        c_death_ = 0;
        // add players
        for (int i = 0; i < num_agents_; i++) {
          auto name = "agent" + std::to_string(i);
          auto pid = engine_.template add_player<Player>(name);
          pids_.emplace_back(pid);
          dones_[i] = false;
        }

        add_bots();

        // the following loop is needed to "initialize" the observation object
        // with the newly reset state so that a call to `get_state` directly
        // after `reset` will return a state representing the fresh beginning
        for (int frame_index = 0; frame_index < ticks_per_step(); frame_index++)
          for (int agent_index = 0; agent_index < num_agents(); agent_index++)
            this->_partial_observation(agent_index, frame_index);
      }

      [[nodiscard]] std::vector<bool> dones() const { return dones_; }
      [[nodiscard]] int ticks_per_step() const { return ticks_per_step_; }

      virtual void render() {};

      void seed (int s) { engine_.seed(s); }

    protected:
      Engine <renderable> engine_;
      std::vector<agario::pid> pids_;
      std::vector<bool> dones_;
      int c_death_;
      const int num_agents_;
      const int ticks_per_step_;
      const int num_bots_;
      const agario::time_delta step_dt_;
      const bool reward_type_;
      /* allows subclass to do something special at the beginning of each step */
      virtual void _step_hook() {};

      /* override this to allow environment to get it's state from
       * intermediate frames between the start and end of a "step" */
      virtual void _partial_observation(int agent_index, int tick_index) {};
      virtual void _partial_observation(Player &player, int tick_index) {};

    private:
      /* adds the specified number of bots to the game */
      void add_bots() {
        using HungryBot = agario::bot::HungryBot<renderable>;
        using HungryShyBot = agario::bot::HungryShyBot<renderable>;
        using AggressiveBot = agario::bot::AggressiveBot<renderable>;
        using AggressiveShyBot = agario::bot::AggressiveShyBot<renderable>;

        for (int i = 0; i < num_bots_; i++) {
          switch (i % num_bots_) {
                case 0:
                  engine_.template add_player<HungryBot>();
                break;
                case 1:
                  engine_.template add_player<HungryShyBot>();
                break;
                case 2:
                  engine_.template add_player<AggressiveBot>();
                break;
                case 3:
                  engine_.template add_player<AggressiveShyBot>();
                break;
                default:
                  engine_.template add_player<HungryBot>();
                break;
          }
        }
      }

    };

  } // namespace env
} // namespace agario
