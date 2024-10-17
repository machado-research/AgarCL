#pragma once

#include <agario/engine/Engine.hpp>
#include <agario/core/types.hpp>
#include <agario/core/Entities.hpp>
#include <agario/core/Ball.hpp>
#include <agario/bots/bots.hpp>
#include "agario/engine/GameState.hpp"

#include <tuple>
#include <fstream>

// 30 frames per second: the default amount of time between frames of the game
#define DEFAULT_DT (1.0 / 30)

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
            // std::cout << "Player \"" << player->name() << "\" (pid ";
            // std::cout << pid << ") died." << std::endl;
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
          this->_partial_observation(agent, ticks_per_step() - 1);
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

      void save(const std::string &filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
          throw EnvironmentException("Unable to open file for saving: " + filename);
        }

        file << "num_agents: " << num_agents_ << "\n";
        file << "ticks_per_step: " << ticks_per_step_ << "\n";
        file << "num_bots: " << num_bots_ << "\n";
        file << "reward_type: " << reward_type_ << "\n";

        file << "players:\n";

        for (const auto &[pid, player] : engine_.players()) {
          file << "    target_x: " << player->target.x << "\n";
          file << "    target_y: " << player->target.y << "\n";
          file << "  - pid: " << pid << "\n";
          agario::color player_color = player->color();
          file << "    cells:\n";
          for (const auto &cell : player->cells) {
            file << "      id:  "<< cell.id << "\n";
            file << "      - x: " << cell.x << "\n";
            file << "        y: " << cell.y << "\n";
            file << "        mass: " << cell.mass() << "\n";
            file << "        velocity_x: " << cell.velocity.dx << "\n";
            file << "        velocity_y: " << cell.velocity.dy << "\n";
            file << "        color: " << player_color << "\n";
          }
          file << "    is_bot: " << player->is_bot << "\n";
          file << "    dead: " << player->dead() << "\n";
          file << "    split_cooldown: " << player->split_cooldown << "\n";
          file << "    feed_cooldown: " << player->feed_cooldown << "\n";
          file << "    virus_eaten_ticks: ";
          for (const auto &tick : player->virus_eaten_ticks) {
            file << tick << " ";
          }
          file << "\n";
          file << "    anti_team_decay: " << player->anti_team_decay << "\n";
          file << "    elapsed_ticks: " << player->elapsed_ticks << "\n";
          file << "    last_decay_tick: " << player->last_decay_tick << "\n";
          file << "    food_eaten: " << player->food_eaten << "\n";
          file << "    highest_mass: " << player->highest_mass << "\n";
          file << "    cells_eaten: " << player->cells_eaten << "\n";
          file << "    viruses_eaten: " << player->viruses_eaten << "\n";
          file << "    top_position: " << player->top_position << "\n";
        }

        file << "pellets:\n";
        for (const auto &pellet : engine_.state.pellets) {
          file << "  - x: " << pellet.x << "\n";
          file << "    y: " << pellet.y << "\n";
        }

        file << "viruses:\n";
        for (const auto &virus : engine_.state.viruses) {
          file << "  - x: " << virus.x << "\n";
          file << "    y: " << virus.y << "\n";
        }

        file.close();
      }

      void load(const std::string &filename) {
        engine_.load(filename);
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
