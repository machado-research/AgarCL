#pragma once

#include <agario/engine/Engine.hpp>
#include <agario/core/types.hpp>
#include <agario/core/Entities.hpp>
#include <agario/core/Ball.hpp>
#include <agario/bots/bots.hpp>
#include "agario/engine/GameState.hpp"
#include <fstream>
#include <dependencies/json.hpp>
#include <tuple>
#include <agario/utils/json.hpp>
// 30 frames per second: the default amount of time between frames of the game
#define DEFAULT_DT (1.0 / 30.0)

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
        int c_death = 0,
        int mode_number = 0,
        bool load_env_snapshot = false
      ):
        num_agents_(num_agents),
        dones_(num_agents),
        engine_(arena_size, arena_size, num_pellets, num_viruses, pellet_regen, mode_number),
        ticks_per_step_(ticks_per_step),
        num_bots_(num_bots),
        reward_type_(reward_type),
        step_dt_(DEFAULT_DT),
        c_death_(c_death),
        is_loading_env_state(load_env_snapshot)
      {
        std::cout <<"Mode Number: " <<  mode_number << std::endl;
        curr_mode_number = mode_number;
        pids_.reserve(num_agents);
        std::cout << "LOADING Snapshot: " << load_env_snapshot << std::endl;
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
      std::vector<std::vector<reward>> step() {
        this->_step_hook(); // allow subclass to set itself up for the step
        is_main_player_respawned = false;
        auto full_mass = masses<float>();
        auto before = full_mass.first;
        auto before_bot = full_mass.second;
        for (int tick = 0; tick < ticks_per_step(); tick++)
          engine_.tick(step_dt_);

        for (int agent = 0; agent < num_agents(); agent++)
          this->_partial_observation(agent, 0);

        // std::cout <<"HELLO::" << curr_mode_number << std::endl;
        if(curr_mode_number == 0)
            repsawn_all_players();

        else if(curr_mode_number > 6){ //other agents and Virus mini-games
          // if any bot dies or me, end the game (dones = true)
          for(auto &pair : this->engine_.state.players){
            auto pid = pair.first;
            auto player = pair.second;
            dones_[0] = player->dead() | is_main_player_respawned;
            if(player->dead()){
              dones_[0] = true; // assuming the first agent is the main agent
              break;
            }
          }
        }
        // reward could be the current mass or the difference in mass from the last step
        auto after = masses<reward>();
        auto rewards = after.first;
        auto rewards_bot = after.second;
        if(reward_type_){
          for (int i = 0; i < num_agents(); ++i)
          {
            rewards[i] -= (before[i] - ((is_main_player_respawned) ? c_death_ : 0));
            rewards_bot[i] -= (before_bot[i] - ((is_main_player_respawned) ? c_death_ : 0));
          }
        }

        return {rewards, rewards_bot};


      }

        /* the mass of each rl-controlled player */
      template<typename T>
      std::pair<std::vector<T>,std::vector<T>> masses() const {
        std::vector<T> masses_;
        std::vector<T> masses_bot;
        masses_.reserve(num_agents());
        masses_.reserve(num_agents());
        for (const auto &[pid, player] : engine_.players()) {
          std::cout << "Player ID: " << pid << " Mass: " << player->mass() << std::endl;
          if (player->is_bot)
          {
              masses_bot.push_back(static_cast<T>(player->mass()));
          }
          else
          {
              masses_.push_back(static_cast<T>(player->mass()));
              if(curr_mode_number == 3 && player->mass() >= max_mass)
              {
                dones_[0] = true; // assuming the first agent is the main agent
                is_main_player_respawned = true;
              }
          }
        }
        return {masses_, masses_bot};
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

        if(this->is_loading_env_state == true)
          return;

        engine_.reset();
        pids_.clear();
        // c_death_ = 0;
        // add players
        for (int i = 0; i < num_agents_; i++) {
          auto name = "agent" + std::to_string(i);
          auto pid = engine_.template add_player<Player>(name);
          engine_.state.main_agent_pid = pid;
          pids_.emplace_back(pid);
          dones_[i] = false;
        }

        if(curr_mode_number == 0)
          add_bots();
        else if(curr_mode_number > 6)
          custom_add_bot(curr_mode_number - 7);
        // the following loop is needed to "initialize" the observation object
        // with the newly reset state so that a call to `get_state` directly
        // after `reset` will return a state representing the fresh beginning

        for (int agent_index = 0; agent_index < num_agents(); agent_index++)
          this->_partial_observation(agent_index, 0);
      }

      [[nodiscard]] std::vector<bool> dones() const { return dones_; }
      [[nodiscard]] int ticks_per_step() const { return ticks_per_step_; }

      virtual void render() {};

      void seed (int s) { engine_.seed(s); seed_ = s; }
      // Save the environment state to a file
      void save_env_state(const std::string &filename) const {
        using json = nlohmann::json;

        // std::ifstream in_file(filename);
        // json agarcl_data = json::parse(in_file);
        json agarcl_data = json::object();

        agarcl_data["num_agents"] = num_agents_;
        agarcl_data["ticks_per_step"] = ticks_per_step_;
        agarcl_data["arena_size"] = static_cast<int>(engine_.arena_width());
        agarcl_data["num_bots"] = num_bots_;
        agarcl_data["reward_type"] = reward_type_;
        agarcl_data["seed"] = seed_;
        agarcl_data["c_death"] = c_death_;
        agarcl_data["mode_number"] = engine_.mode_number;
        agarcl_data["pellet_regen"] = engine_.pellet_regen();
        agarcl_data["pellet_count"] = engine_.pellet_count();

        //Get the data for the player:
        agarcl_data["players"] = json::array();
        for (const auto &[pid, player] : engine_.players()) {
            nlohmann::json player_data;
            player_data["pid"] = pid;
            player_data["name"] = player->name(); // Add the player's name
            player_data["target_x"] = static_cast<float>(player->target.x);
            player_data["target_y"] = static_cast<float>(player->target.y);
            player_data["is_bot"] = player->is_bot;
            player_data["dead"] = player->dead();
            player_data["split_cooldown"] = player->split_cooldown;
            player_data["feed_cooldown"] = player->feed_cooldown;
            player_data["virus_eaten_ticks"] = json::array();

            for (const auto &tick : player->virus_eaten_ticks) {
              player_data["virus_eaten_ticks"].push_back(tick);
            }

            agario::color player_color = player->color();
            player_data["cells"] = json::array();

            for (const auto &cell : player->cells) {
              nlohmann::json cell_data;
              cell_data["id"] = cell.id;
              cell_data["x"] = static_cast<float>(cell.x);
              cell_data["y"] = static_cast<float>(cell.y);
              cell_data["mass"] = cell.mass();
              cell_data["velocity_x"] = static_cast<float>(cell.velocity.dx);
              cell_data["velocity_y"] = static_cast<float>(cell.velocity.dy);
              cell_data["color"] = player_color;
              player_data["cells"].push_back(cell_data);
            }

            player_data["anti_team_decay"] = player->anti_team_decay;
            player_data["elapsed_ticks"] = player->elapsed_ticks;
            player_data["last_decay_tick"] = player->last_decay_tick;
            player_data["food_eaten"] = player->food_eaten;
            player_data["highest_mass"] = player->highest_mass;
            player_data["cells_eaten"] = player->cells_eaten;
            player_data["viruses_eaten"] = player->viruses_eaten;
            player_data["top_position"] = player->top_position;
            agarcl_data["players"].push_back(player_data);
        }

        // Pellets
        agarcl_data["pellets"] = json::array();
        for (const auto &pellet : engine_.state.pellets) {
          nlohmann::json pellet_data;
          pellet_data["x"] = static_cast<float>(pellet.x);
          pellet_data["y"] = static_cast<float>(pellet.y);
          agarcl_data["pellets"].push_back(pellet_data);
        }

        // Viruses
        agarcl_data["viruses"] = json::array();
        for (const auto &virus : engine_.state.viruses) {
          nlohmann::json virus_data;
          virus_data["x"] = static_cast<float>(virus.x);
          virus_data["y"] = static_cast<float>(virus.y);
          virus_data["velocity_x"] = static_cast<float>(virus.velocity.dx);
          virus_data["velocity_y"] = static_cast<float>(virus.velocity.dy);
          virus_data["mass"] = static_cast<float>(virus.mass());
          // virus_data["is_eaten"] = virus.is_eaten;
          agarcl_data["viruses"].push_back(virus_data);
        }

        //Food
        agarcl_data["foods"] = json::array();
        for (const auto &food : engine_.state.foods) {
          nlohmann::json food_data;
          food_data["x"] = static_cast<float>(food.x);
          food_data["y"] = static_cast<float>(food.y);
          food_data["velocity_x"] = static_cast<float>(food.velocity.dx);
          food_data["velocity_y"] = static_cast<float>(food.velocity.dy);
          // food_data["mass"] = static_cast<float>(food.mass());
          agarcl_data["foods"].push_back(food_data);
        }

        // Open the output file for writing
        std::ofstream out_file(filename);
        if (!out_file.is_open()) {
            throw std::runtime_error("Failed to open " + filename + " for writing");
        }

        // Dump pretty‑printed JSON (4-space indent)
        out_file << std::setw(4) << agarcl_data << std::endl;

      }

      void load_env_state(const std::string &filename) {
        this->is_loading_env_state =  true;
        engine_.reset_state();
        pids_.clear();
        pids_.reserve(num_agents_);
        // c_death_ = 0;

        engine_.load_env_state(filename);

        int i = 0;
        for(auto &pair : engine_.state.players){
          auto pid = pair.first;
          auto player = pair.second;
          if(player->is_bot) continue;
          engine_.state.main_agent_pid = pid;
          pids_.emplace_back(pid);
          std::cout << pid << " " << player->name() << std::endl;
          dones_[i] = false;
          i++;
        }

        for (int agent_index = 0; agent_index < num_agents(); agent_index++)
            this->_partial_observation(agent_index, 0);
      }

    protected:
      Engine <renderable> engine_;
      std::vector<agario::pid> pids_;
      mutable std::vector<bool> dones_;
      int c_death_;
      const int num_agents_;
      const int ticks_per_step_;
      const int num_bots_;
      const agario::time_delta step_dt_;
      const bool reward_type_;
      int seed_ = 0;
      int curr_mode_number = 0;
      const int max_mass = 23000;
      mutable bool is_main_player_respawned = false;

/* allows subclass to do something special at the beginning of each step */
      virtual void _step_hook() {};

      /* override this to allow environment to get it's state from
       * intermediate frames between the start and end of a "step" */
      virtual void _partial_observation(int agent_index, int tick_index) {};
      virtual void _partial_observation(Player &player, int tick_index) {};

      bool is_loading_env_state = false;



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

      void custom_add_bot(int index)
      {
        using HungryBot = agario::bot::HungryBot<renderable>;
        using HungryShyBot = agario::bot::HungryShyBot<renderable>;
        using AggressiveBot = agario::bot::AggressiveBot<renderable>;
        using AggressiveShyBot = agario::bot::AggressiveShyBot<renderable>;

        switch (index) {
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


    };

  } // namespace env
} // namespace agario
