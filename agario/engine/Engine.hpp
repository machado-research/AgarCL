#pragma once

#include <vector>
#include <cstdlib>
#include <chrono>
#include <algorithm>
#include <sstream>
#include<set>
#include <numeric>
#include<random>
#include "agario/core/Player.hpp"
#include "agario/core/settings.hpp"
#include "agario/core/types.hpp"
#include "agario/core/Entities.hpp"
#include "agario/engine/GameState.hpp"
#include "agario/utils/random.hpp"
#include "agario/utils/collision_detection.hpp"
#include <thread>
#include <chrono>
#include <fstream>
namespace agario {

  class EngineException : public std::runtime_error {
    using runtime_error::runtime_error;
  };

  template<bool renderable>
  class Engine {
  public:
    using Player = Player<renderable>;
    using Cell = Cell<renderable>;
    using Food = Food<renderable>;
    using Pellet = Pellet<renderable>;
    using Virus = Virus<renderable>;
    using GameState = GameState<renderable>;

    agario::GameState<renderable> state;
    std::unordered_map<int, std::vector<std::pair<int, float>>> vec;

    Engine(distance arena_width, distance arena_height,
           int num_pellets = DEFAULT_NUM_PELLETS,
           int num_viruses = DEFAULT_NUM_VIRUSES,
           bool pellet_regen = true) :
      state(agario::GameConfig(arena_width, arena_height, num_pellets, num_viruses, pellet_regen))
    {
      std::srand(std::chrono::system_clock::now().time_since_epoch().count());
    }
    Engine() : Engine(DEFAULT_ARENA_WIDTH, DEFAULT_ARENA_HEIGHT) {}

    /* the number of ticks that have elapsed in the game */
    agario::tick ticks() const { return state.ticks; }
    const typename GameState::PlayerMap &players() const { return state.players; }
    const std::vector<Pellet> &pellets() const { return state.pellets; }
    const std::vector<Food> &foods() const { return state.foods; }
    const std::vector<Virus> &viruses() const { return state.viruses; }
    agario::GameState<renderable> &game_state() { return state; }
    const agario::GameState<renderable> &get_game_state() const { return state; }
    agario::distance arena_width() const { return state.config.arena_width; }
    agario::distance arena_height() const { return state.config.arena_height; }
    int player_count() const { return state.players.size(); }
    int pellet_count() const { return state.pellets.size(); }
    int virus_count() const { return state.viruses.size(); }
    int food_count() const { return state.foods.size(); }
    bool pellet_regen() const { return state.config.pellet_regen; };

    template<typename P>
    agario::pid add_player(const std::string &name = std::string()) {
      auto pid = state.next_pid++;

      std::shared_ptr<P> player = nullptr;
      if (name.empty()) {
        player = std::make_shared<P>(pid);
      } else {
        player = std::make_shared<P>(pid, name);
      }
      auto p = state.players.insert(std::make_pair(pid, player));
      respawn(*player);
      return pid;
    }

    Player &player(agario::pid pid) {
      return const_cast<Player &>(get_player(pid));
    }

    Player &get_player(agario::pid pid) const {
      if (state.players.find(pid) == state.players.end()) {
        std::stringstream ss;
        ss << "Player ID: " << pid << " does not exist.";
        throw EngineException(ss.str());
      }
      return *state.players.at(pid);
    }

    void reset() {
      state.clear();
      initialize_game();
    }

    void initialize_game() {
      add_pellets(state.config.target_num_pellets);
      add_viruses(state.config.target_num_viruses);
      update_vec();
    }

    void respawn(Player &player) {
      player.kill();
      player.add_cell(random_location(agario::radius_conversion(CELL_MIN_SIZE)), CELL_MIN_SIZE);
    }

    agario::Location random_location() {
      return random_location(0);
    }

    agario::Location random_location(agario::distance radius) {
      auto x = random<agario::distance>(arena_width() - 2*radius) + radius; //if it is 0, it will be 0 + radius.
      auto y = random<agario::distance>(arena_height() - 2*radius) + radius;

      return Location(x, y);
    }

    /**
     * Performs a single game tick, moving all entities, performing
     * collision detection and updating the game state accordingly
     * @param elapsed_seconds the amount of time which has elapsed
     * since the previous game tick.
     */
    void tick(const agario::time_delta &elapsed_seconds) {

      for (auto &pair : state.players) {
        auto &player = *pair.second;
        if (!player.dead())
          tick_player(player, elapsed_seconds);
      }
      // to change the hierarchy of: I want pair of player_id and cells: Keep in mind that we will std::move cells
      std::vector<std::pair<agario::pid, Cell>> cells_per_player;

      for(auto &pair : state.players) {
        auto &player = *pair.second;
        sort(player.cells.begin(), player.cells.end());
        for(auto &cell : player.cells) {
          cells_per_player.emplace_back(std::make_pair(player.pid(), std::move(cell)));
        }
      }

      //change
      PrecisionCollisionDetection<renderable> pcd({arena_width(), arena_height()}, 100);
      //send the cells for each player
      auto results = pcd.solve(cells_per_player, cells_per_player);

      for (const auto& result : results) {
        const auto& id = result.first;
        const auto& cells = result.second;
        for (const auto& vect_id : cells) {
          auto &eaten_player = get_player(vect_id.first);
          const auto& cell = vect_id.second;
          auto &player = get_player(cells_per_player[id].first);
          auto& eaten_player_cells = eaten_player.cells;
          auto it = std::lower_bound(player.cells.begin(), player.cells.end(), cells_per_player[id].second.id, [](const Cell& c, int id) {
            return c.id < id;
          });

          if (it != player.cells.end()) {
            it->increment_mass(cell.mass());
          }

          auto eaten_it = std::lower_bound(eaten_player_cells.begin(), eaten_player_cells.end(), cell.id, [](const Cell& c, int id) {
            return c.id < id;
          });

          if (eaten_it != eaten_player_cells.end()) {
            eaten_player_cells.erase(eaten_it);
          }

        }
      }

      // do the collision part here:
      // check_player_collisions();

      cells_per_player.clear();

      move_foods(elapsed_seconds);

      if(state.ticks%300 == 0){
        if (state.config.pellet_regen) {
          add_pellets(state.config.target_num_pellets - state.pellets.size());
        }
        int prev_virus_size = state.viruses.size();
        add_viruses(state.config.target_num_viruses - state.viruses.size());
        if(prev_virus_size - state.viruses.size() != 0)
            update_vec();
      }
      state.ticks++;

    }

    void update_vec()
    {
       for (int id = 0; id < state.viruses.size(); id++) {
                const auto& node = state.viruses[id];
                int row_id = get_row(node.x, state.config.arena_width);
                vec[row_id].emplace_back(std::make_pair(id, node.y));
            }
            for (auto& val : vec) {
                std::sort(val.second.begin(), val.second.end(), [](const auto& a, const auto& b) {
                    return a.second < b.second;
                });
            }
    }

    void seed(unsigned s) {
      this->state.rng.seed(s);
      std::srand(s);
    }

    Engine(const Engine &) = delete; // no copy constructor
    Engine &operator=(const Engine &) = delete; // no copy assignments
    Engine(Engine &&) = delete; // no move constructor
    Engine &operator=(Engine &&) = delete; // no move assignment

  private:
    void add_pellets(int n) {
      agario::distance pellet_radius = agario::radius_conversion(PELLET_MASS);
        for (int p = 0; p < n; p++)
          state.pellets.emplace_back(random_location(pellet_radius));
    }

    void add_viruses(int n) {
      agario::distance virus_radius = agario::radius_conversion(VIRUS_INITIAL_MASS);
      int mx_num_viruses = std::min(arena_height(), arena_width())/virus_radius;
        for (int v = 0; v < n; v++)
          state.viruses.emplace_back(random_location(virus_radius));

    }

    /**
     * "ticks" the given player, which involves moving the player's cells and checking
     * for collisions between the player and all other entities in the arena
     * Also performs any player actions (i.e. splitting or feeling) decrements
     * the cooldown timers on the player actions
     * @param player the player to tick
     * @param elapsed_seconds the amount of (game) time since the last game tick
     */
    void tick_player(Player &player, const agario::time_delta &elapsed_seconds) {
      player.elapsed_ticks += 1;

      if (ticks() % 10 == 0)
        player.take_action(state);

      move_player(player, elapsed_seconds);

      int prev_player_cells = player.cells.size();

      std::vector<Cell> created_cells; // list of new cells that will be created
      int create_limit = PLAYER_CELL_LIMIT - prev_player_cells;

      bool can_eat_virus = ((player.cells.size() >= NUM_CELLS_TO_SPLIT));

      // check_pellets_collisions(state.pellets, player);
      player.highest_mass = std::max(player.highest_mass, player.mass());
      optimized_eat_pellets(player.cells);
      for (Cell &cell : player.cells) {
        can_eat_virus &= cell.mass() >= MIN_CELL_SPLIT_MASS;
        may_be_auto_split(cell, created_cells, create_limit, player.cells.size(), player.target);
        // player.food_eaten +=eat_pellets(cell);
        player.food_eaten +=eat_food(cell);

      // if (check_virus_collisions(cell, created_cells, create_limit, can_eat_virus)) {
      //     player.virus_eaten_ticks.emplace_back(player.elapsed_ticks);
      //     player.viruses_eaten++;
      //   }
        if(virus_Opt_Collision(cell, player, create_limit, created_cells, can_eat_virus)){
          player.virus_eaten_ticks.emplace_back(player.elapsed_ticks);
          player.viruses_eaten++;
        }
      }

      create_limit -= created_cells.size();
      maybe_emit_food(player);
      maybe_split(player, created_cells, create_limit);
      // player.colorize_cells(prev_player_cells);

      // add any cells that were created
      player.add_cells(created_cells);
      created_cells.erase(created_cells.begin(), created_cells.end());

      recombine_cells(player);

      // some actions do not need to happen every tick
      // these will be executed once per second
      if (player.elapsed_ticks % 60 == 0) {
        maybe_activate_anti_team(player);
        mass_decay(player);
      }
    }

    /**
     * Anti-team is triggered by hitting 3 viruses or more in a row in 1 minute. Mass will start to decay slightly faster than usual after hitting 2 viruses.
     * The more subsequent viruses hit, the faster the rate of mass decay.
     * @param cell the cell to check for anti-team activation
     * @param player the player to check for anti-team activation
     */


    void maybe_activate_anti_team(Player &player) {
      auto fall_off_time = player.elapsed_ticks - (60 * ANTI_TEAM_ACTIVATION_TIME);

      // in-place delete ticks that are older than ANTI_TEAM_ACTIVATION_TIME
      player.virus_eaten_ticks.erase(
        std::remove_if(
          player.virus_eaten_ticks.begin(),
          player.virus_eaten_ticks.end(),
          [fall_off_time](int tick) { return tick < fall_off_time; }
        ),
        player.virus_eaten_ticks.end()
      );


      auto n_eaten = player.virus_eaten_ticks.size();
      if (n_eaten == 0) {
        return;
      }

      player.anti_team_decay = std::pow(1.1, n_eaten - 1);
    }

    /**
     * Reducing the mass of the cell of a player after a couple of seconds (DECAY_FOR_NUM_SECONDS)
     * @param cell the cell to check for decay
     * @param player the player
     */
    void mass_decay(Player &player) {
      auto ticks_since_decay = player.elapsed_ticks - player.last_decay_tick;
      if(ticks_since_decay >= 60 * DECAY_FOR_NUM_SECONDS) {
        for (auto &cell : player.cells) {
          cell.mass_decay(player.anti_team_decay);
        }

        player.last_decay_tick = player.elapsed_ticks;
      }
    }

    /**
     * Enforce the cell of a player to be splitted if it exceeds the maximum mass in the game.
     * @param cell the cell to check for splitting
     * @param created_cells the list of cells that will be created
     * @param create_limit the maximum number of cells that can be created
     */
    void may_be_auto_split(Cell &cell, std::vector<Cell>&created_cells, int create_limit, int num_cells, Location player_target) {

      if(cell.mass() >= MAX_MASS_IN_THE_GAME)
      {
        if(num_cells < PLAYER_CELL_LIMIT)
          cell_split(cell, created_cells, create_limit, player_target);
        else
          cell.set_mass(NEW_MASS_IF_NO_SPLIT); // if the player has reached the limit, the cell will be set to the new mass
      }
    }

    /**
     * Moves all of the cells of the given player by an amount proportional
     * to the elapsed time since the last tick, given by elapsed_seconds
     * @param player the player to move
     * @param elapsed_seconds time since the last game tick
     */
    void move_player(Player &player, const agario::time_delta &elapsed_seconds) {

      //check whether the player target is out of arena or not

      auto dt = elapsed_seconds.count();
      agario::mass smallest_mass_cell = std::numeric_limits<agario::mass>::max();

      for (auto &cell : player.cells) {
        cell.velocity.dx = 3 * (player.target.x - cell.x);
        cell.velocity.dy = 3 * (player.target.y - cell.y);
        smallest_mass_cell = std::min(smallest_mass_cell, cell.mass());
        // clip speed
        auto speed_limit = max_speed(cell.mass());
        cell.velocity.clamp_speed(0, speed_limit);
        cell.move(dt);
        cell.splitting_velocity.decelerate(SPLIT_DECELERATION, dt);
        check_boundary_collisions(cell);
      }
      player.set_min_mass_cell(smallest_mass_cell);
      // make sure not to move two of players own cells into one another
      check_player_self_collisions(player, elapsed_seconds);
    }

    void move_foods(const agario::time_delta &elapsed_seconds) {
      auto dt = elapsed_seconds.count();

      for (auto food_it = state.foods.begin() ; food_it != state.foods.end(); ) {
        if (food_it->velocity.magnitude() == 0) {
          food_it++;
          continue;
        }

        Velocity food_vel = food_it->velocity;
        food_it->decelerate(FOOD_DECEL, dt);
        food_it->move(dt);

        check_boundary_collisions(*food_it);

        bool hit_virus = maybe_hit_virus(*food_it, food_vel, elapsed_seconds);

        if(hit_virus) {
            if(state.foods.size() > 1)
              std::swap(*food_it, state.foods.back());
            state.foods.pop_back();
          } else
              ++food_it;
      }
    }

    /*
    * Check for collisions between the foods and viruses in the game
    */
    bool maybe_hit_virus(const Food &food, const Velocity &food_vel, const agario::time_delta &elapsed_seconds) {
      auto dt = elapsed_seconds.count();
      for (auto &virus : state.viruses) {

        if (food.collides_with(virus)) {
            if(virus.get_num_food_hits() >= NUMBER_OF_FOOD_HITS) {
              // Return the virus to its original mass.
              virus.set_num_food_hits(0);
              virus.set_mass(VIRUS_INITIAL_MASS);

              // For the new virus take the food direction and location with VIRUSS NORMAL MASS.
              Velocity vel = food_vel;
              Virus new_virus(Location(virus.x,virus.y), vel);
              new_virus.move(dt*10);
              check_boundary_collisions(new_virus);
              new_virus.set_mass(VIRUS_INITIAL_MASS);
              state.viruses.emplace_back(std::move(new_virus));
            } else {

              virus.set_num_food_hits(virus.get_num_food_hits() + 1);
              virus.set_mass(virus.mass() + FOOD_MASS);
            }
            return true;

        }
      }
      return false;
    }


    /**
     * Constrains the location of `ball` to be inside the boundaries
     * of the arena
     * @param ball the ball to keep inside the arena
     */
    void check_boundary_collisions(Ball &ball) {
      ball.x = clamp<agario::distance>(ball.x, ball.radius(), arena_width()-ball.radius());
      ball.y = clamp<agario::distance>(ball.y, ball.radius(), arena_height()-ball.radius());
    }


    void avoid_static_overlap(Cell &cell_a, Cell& cell_b)
    {
      auto mass_a = cell_a.mass();
      auto mass_b = cell_b.mass();

      auto dx = cell_b.x - cell_a.x;
      auto dy = cell_b.y - cell_a.y;

      auto dist = sqrt(dx * dx + dy * dy);
      auto target_dist = cell_a.radius() + cell_b.radius();

      if (dist > target_dist) {
        return; // cells are not overlapping
      }

      auto x_ratio = dx / (std::abs(dx) + std::abs(dy));
      auto y_ratio = dy / (std::abs(dx) + std::abs(dy));

      auto depth = target_dist - dist;

      std::pair<float,float> cell_a_ratio = {0.5, 0.5}, cell_b_ratio = {0.5, 0.5};

      auto check_border = [&](Cell &cell, std::pair<float,float> &cell_ratio)
      {
        if(cell.x == cell.radius() || cell.x == this->arena_width() - cell.radius())
        {
          cell_ratio.first = 1.0;
          cell.velocity.dx = 0;
        }
        if(cell.y == cell.radius() || cell.y == this->arena_height() - cell.radius())
        {
          cell_ratio.second = 1.0;
          cell.velocity.dy = 0;
        }
      };

      check_border(cell_a,cell_a_ratio);
      check_border(cell_b, cell_b_ratio);

      cell_a.x -= x_ratio * depth*cell_a_ratio.first;
      cell_a.y -= y_ratio * depth*cell_a_ratio.second;

      cell_b.x += x_ratio * depth*cell_b_ratio.first;
      cell_b.y += y_ratio * depth*cell_b_ratio.second;

    }

    int get_row(float x, int border, int precision=100) {
              return static_cast<int>(x / border * precision);
    }

    bool virus_Opt_Collision(Cell& cell, Player& player, int create_limit, std::vector<Cell>& created_cells, bool can_eat_virus) {
            // update the vec
            // the query_list
            // for(auto &cell : player.cells) {
                float left = cell.x - cell.radius();
                float right = cell.x + cell.radius();
                int top = get_row(left,state.config.arena_width);
                int bottom = get_row(right, state.config.arena_width);
                for (int i = top; i <= bottom; i++) {
                    if (vec.find(i) == vec.end()) continue;
                    int l = vec[i].size();
                    int start_pos = 0;
                    for (int j = 10; j >= 0; j--) {
                        if (start_pos + (1 << j) < l && vec[i][start_pos + (1 << j)].second < left) {
                            start_pos += (1 << j);
                        }
                    }
                    for (int j = start_pos; j < l; j++) {
                      if(vec[i][j].first >= state.viruses.size())continue;
                      auto & virus = state.viruses[vec[i][j].first];
                        if(cell.can_eat(virus) && cell.collides_with(virus)) {
                            if(can_eat_virus)
                              cell.increment_mass(virus.mass());
                            else
                              disrupt(cell, virus, created_cells, create_limit);

                              std::swap(virus, state.viruses.back()); // O(1) removal
                              state.viruses.pop_back();
                              return true;
                        }
                    }

                }
            // }
            return false;
    }
    /**
     * Moves all of `player`'s cells apart slightly such that
     * cells which aren't eligible for recombining don't overlap
     * with other cells of the same player.
     */
    // NEED TO OPTIMIZE THIS FUNCTION: BIG BIG BIG O(n^2) for each player, so it is O(M*N^2): N is the number of cells and M is the number of Players
    // What if we can make it O(M*N*log(N)), or Much better O(M*N)? How?
    // One option left to try is to use the quadtree data structure to store the cells of each player, and then check the cells that are close to each other: Should try after the cleanup.
    void check_player_self_collisions(Player &player, const agario::time_delta &elapsed_seconds) {
    bool overlap = false;
    for(int iter = 0 ; iter < 5 ;iter++){
      overlap = false;
     // Insert all cells into the spatial map
      // Define the size of each cell in the spatial map
      const int   cell_size = 100;
      const int   mx_cells = 256; //16*16
      std::unordered_map<int, std::vector<Cell*>> spatial_map;

      // Insert all cells into the spatial map
      for (Cell &cell : player.cells) {
        int cell_x = static_cast<int>(cell.x / cell_size);
        int cell_y = static_cast<int>(cell.y / cell_size);
        int cell_key = cell_y * mx_cells + cell_x; // Create a unique key for each cell
        spatial_map[cell_key].push_back(&cell);
      }

      // Check for collisions using the spatial map
      for (Cell &cell : player.cells) {
        int cell_x = static_cast<int>(cell.x / cell_size);
        int cell_y = static_cast<int>(cell.y / cell_size);

        // Check the neighboring cells
        for (int dx = -1; dx <= 1; dx++) {
          for (int dy = -1; dy <= 1; dy++) {
            int neighbor_key = (cell_y + dy) * mx_cells + (cell_x + dx);
            if (spatial_map.find(neighbor_key) != spatial_map.end()) {
              for (Cell* other_cell : spatial_map[neighbor_key]) {
                if (&cell == other_cell) continue; // Skip self

                // only allow collisions if both are eligible for recombining
                if (cell.can_recombine() && other_cell->can_recombine()) continue;

                if (cell.touches(*other_cell)) {
                  overlap = true;
                  prevent_overlap(cell, *other_cell, elapsed_seconds, player.target);
                }
              }
            }
          }
        }
      }
      if(!overlap)
        break;
    }

    if(overlap)
    {
      for (int idx_a = 0; idx_a < player.cells.size(); idx_a++) {
        for (int idx_b = idx_a + 1; idx_b < player.cells.size(); idx_b++) {

          Cell &cell_a = player.cells[idx_a];
          Cell &cell_b = player.cells[idx_b];
          if (cell_a.touches(cell_b))
            avoid_static_overlap(cell_a, cell_b);
        }
      }
    }

  }

    /**
     * Moves `cell_a` and `cell_b` apart slightly
     * such that they cannot be overlapping
     * @param cell_a first cell to move apart
     * @param cell_b second cell to move apart
     * @param player_target the target location of the player
     */
    void separate_cells(Cell& cell_a, Cell& cell_b, const Location& player_target) {
      auto mass_a = cell_a.mass();
      auto mass_b = cell_b.mass();

      auto dx = cell_b.x - cell_a.x;
      auto dy = cell_b.y - cell_a.y;

      auto dist = sqrt(dx * dx + dy * dy);
      auto target_dist = cell_a.radius() + cell_b.radius();

      if (dist > target_dist) {
        return; // cells are not overlapping
      }

      auto x_ratio = dx / (std::abs(dx) + std::abs(dy));
      auto y_ratio = dy / (std::abs(dx) + std::abs(dy));

      auto diff_a = (player_target - cell_a.location()).norm_sqr();
      auto diff_b = (player_target - cell_b.location()).norm_sqr();

      auto depth = target_dist - dist;

      short sign_direction_1 = (cell_a.mass() < cell_b.mass() ? 1 : -1);
      short sign_direction_2 = (diff_a >= diff_b ? 1 : -1);

      // If both directions have the same sign, apply the idea that less mass should move
      short sign_direction = (sign_direction_1 == sign_direction_2 ? sign_direction_2 : 0);

      Cell& temp_cell = (cell_a.mass() < cell_b.mass() ? cell_a : cell_b);

      if (dx >= 0) {
        temp_cell.x -= x_ratio * depth * sign_direction;
        if (dy >= 0) {
          temp_cell.y -= y_ratio * depth * sign_direction;
        } else {
          temp_cell.y += y_ratio * depth * sign_direction;
        }
      } else {
        temp_cell.x += x_ratio * depth * sign_direction;
        if (dy >= 0) {
          temp_cell.y -= y_ratio * depth * sign_direction;
        } else {
          temp_cell.y += y_ratio * depth * sign_direction;
        }
      }
    }


    /**
     * change the dx and dy of both `cell_a` and `cell_b` apart slightly
     * such that they cannot be overlapping
     * @param cell_a first cell to move apart
     * @param cell_b second cell to move apart
     */
    void prevent_overlap(Cell &cell_a, Cell &cell_b, const agario::time_delta &elapsed_seconds, const Location &player_target) {
      auto dx = cell_b.x - cell_a.x;
      auto dy = cell_b.y - cell_a.y;
      auto dist = sqrt(dx * dx + dy * dy);
      auto target_dist = cell_a.radius() + cell_b.radius();
      auto dt = elapsed_seconds.count();

      if (dist > target_dist) return; // aren't overlapping

      cell_a.x -= (cell_a.velocity.dx+cell_a.splitting_velocity.dx) * dt;
      cell_a.y -= (cell_a.velocity.dy+cell_a.splitting_velocity.dy) * dt;

      cell_b.x -= (cell_b.velocity.dx+cell_b.splitting_velocity.dx) * dt;
      cell_b.y -= (cell_b.velocity.dy+cell_b.splitting_velocity.dy) * dt;

      elastic_collision_between_balls(cell_a, cell_b, dx, dy, dist);

      cell_a.move(dt);
      cell_b.move(dt);

      if(cell_a.touches(cell_b))
      {
        if(std::abs(static_cast<int>(cell_a.mass() - cell_b.mass())) <= 10)
          avoid_static_overlap(cell_a, cell_b);
        else
          separate_cells(cell_a, cell_b, player_target);
      }
    }

    /**
     * moves `cell_a` and `cell_b` apart slightly. Preserving the Kinetic Energy and the momentum
     */
    void elastic_collision_between_balls(Cell &cell_a, Cell &cell_b, const float &dx, const float &dy, const float &dist)
    {

      //Calculate the norm vector
      auto nx = dx / dist;
      auto ny = dy / dist;

      //Calculate the tangent vector
      auto tx = -ny;
      auto ty = nx;


      //Calculate the dot product of the velocity vector and the normal vector
      auto dpNorm1 = cell_a.velocity.dx * nx + cell_a.velocity.dy * ny;
      auto dpNorm2 = cell_b.velocity.dx * nx + cell_b.velocity.dy * ny;

      //Calculate the dot product of the velocity vector and the tangent vector
      auto dpTan1 = cell_a.velocity.dx * tx + cell_a.velocity.dy * ty;
      auto dpTan2 = cell_b.velocity.dx * tx + cell_b.velocity.dy * ty;


      //Calculate the mass
      int m1 = cell_a.mass();
      int m2 = cell_b.mass();

      //Calculate the velocity of the 1st and 2nd object in the normal direction
      auto v1 = (dpNorm1 * (m1 - m2) + 2.0f * m2 * dpNorm2) / (m1 + m2);
      auto v2 = (dpNorm2 * (m2 - m1) + 2.0f * m1 * dpNorm1) / (m1 + m2);
      float factor_a = 1.0 , factor_b = 1.0;

      if(cell_a.mass() < cell_b.mass()) {
        cell_a.velocity.dx = (tx * dpTan1 + nx * v1);
        cell_a.velocity.dy = (ty * dpTan1 + ny * v1);
      }
      else if(cell_a.mass() > cell_b.mass()) {
        cell_b.velocity.dx = (tx * dpTan2 + nx * v2);
        cell_b.velocity.dy = (ty * dpTan2 + ny * v2);
      }
      else{
        cell_a.velocity.dx = (tx * dpTan1 + nx * v1);
        cell_a.velocity.dy = (ty * dpTan1 + ny * v1);

        cell_b.velocity.dx = (tx * dpTan2 + nx * v2);
        cell_b.velocity.dy = (ty * dpTan2 + ny * v2);
      }
    }

    /**
     * checks for collisions between the given cell and
     * all of the pellets in the game, removing those pellets
     * from the game which the cell eats
     * @param cell the cell which is doing the eating
     */
    int eat_pellets(Cell &cell) {
      auto prev_size = pellet_count();

      state.pellets.erase(
        std::remove_if(state.pellets.begin(), state.pellets.end(),
                       [&](const Pellet &pellet) {
                         return cell.can_eat(pellet) && cell.collides_with(pellet);
                       }),
        state.pellets.end());

      auto num_eaten = prev_size - pellet_count();
      cell.increment_mass(num_eaten * PELLET_MASS);

      return num_eaten;
    }

    void optimized_eat_pellets(std::vector<Cell> &cells) {

      std::sort(cells.begin(), cells.end(), [](const Cell &a, const Cell &b) {
        if(a.x == b.x)
          return a.y < b.y;
        return a.x < b.x;
      });

      std::sort(state.pellets.begin(), state.pellets.end(), [](const Pellet &a, const Pellet &b) {
        if(a.x == b.x)
          return a.y < b.y;
        return a.x < b.x;
      });

      // Use two pointers to find collisions
      int cell_idx = 0;
      int pellet_idx = 0;
      std::vector<int> pellets_to_remove;

      while (cell_idx < cells.size() && pellet_idx < state.pellets.size()) {
        Cell &cell = cells[cell_idx];
        Pellet &pellet = state.pellets[pellet_idx];

        if (cell.can_eat(pellet) && cell.collides_with(pellet)) {
          cell.increment_mass(PELLET_MASS);
          pellets_to_remove.push_back(pellet_idx);
          ++pellet_idx;
        } else if (cell.x + cell.radius() < pellet.x - pellet.radius()) {
          ++cell_idx; // Move to the next cell
        } else {
          ++pellet_idx; // Move to the next pellet
        }
      }

     for(int pos = pellets_to_remove.size()-1; pos >= 0; pos--)
      {
          int cur_pos = pellets_to_remove[pos];
          if(cur_pos <  state.pellets.size()-1 && state.pellets.size() > 1)
            std::swap(state.pellets[cur_pos], state.pellets.back());
          state.pellets.pop_back();
      }
    }

    int eat_food(Cell &cell) {
      if (cell.mass() < FOOD_MASS) return 0;
      auto prev_size = food_count();

      state.foods.erase(
        std::remove_if(state.foods.begin(), state.foods.end(),
                       [&](const Food &pellet) {
                         return cell.can_eat(pellet) && cell.collides_with(pellet);
                       }),
        state.foods.end());
      auto num_eaten = prev_size - food_count();
      cell.increment_mass(num_eaten * FOOD_MASS);

      return num_eaten;
    }

    void emit_foods(Player &player) {

      // emit one pellet from each sufficiently large cell
      for (Cell &cell : player.cells) {

        // not big enough to emit pellet
        if (cell.mass() < CELL_MIN_SIZE + FOOD_MASS) continue;

        auto dir = (player.target - cell.location()).normed();
        Location loc = cell.location() + dir * cell.radius();

        Velocity vel(dir * FOOD_SPEED);
        Food food(loc, vel);

        state.foods.emplace_back(std::move(food));
        cell.increment_mass(-food.mass());
      }
    }

    void maybe_emit_food(Player &player) {
      if (player.feed_cooldown > 0)
        player.feed_cooldown -= 1;

      if (player.action == agario::action::feed && player.feed_cooldown == 0) {
        emit_foods(player);
        player.feed_cooldown = 10;
      }
    }

    void maybe_split(Player &player, std::vector<Cell> &created_cells, int create_limit) {
      if (player.split_cooldown > 0)
        player.split_cooldown -= 1;

      if (player.action == agario::action::split && player.split_cooldown == 0) {
        player_split(player, created_cells, create_limit);
        player.split_cooldown = 30;
      }
    }


    bool cell_split(Cell &cell, std::vector<Cell> &created_cells, int create_limit, Location &player_target)
    {
      if (cell.mass() < CELL_SPLIT_MINIMUM)
        return false;

      agario::mass split_mass = cell.mass() / 2;
      auto remaining_mass = cell.mass() - split_mass;

      cell.set_mass(remaining_mass);

      auto dir = (player_target - cell.location()).normed();
      auto loc = cell.location() + dir * cell.radius();
      Velocity vel(dir * split_speed(split_mass));

      // todo: add constructor that takes splitting velocity (and color)
      Cell new_cell(loc, vel, split_mass);
      new_cell.splitting_velocity = vel;

      cell.reset_recombine_timer();
      new_cell.reset_recombine_timer();

      created_cells.emplace_back(std::move(new_cell));
      return true;
    }

    void player_split(Player &player, std::vector<Cell> &created_cells, int create_limit) {
      if (create_limit == 0)
        return;

      int num_splits = 0;
      for (Cell &cell : player.cells) {
        bool is_splitted = cell_split(cell, created_cells, create_limit, player.target);
        if(is_splitted) {
            if (++num_splits == create_limit)
              return;
        }
      }
    }

    /**
     * Checks all pairs of players for collisions that result
     * in one cell eating another. Updates the corresponding lists
     * of cells in each player to reflect any collisions.
     */
    void check_player_collisions() {
      for (auto p1_it = state.players.begin(); p1_it != state.players.end(); ++p1_it)
        for (auto p2_it = std::next(p1_it); p2_it != state.players.end(); ++p2_it)
          check_players_collisions(*p1_it->second, *p2_it->second);
    }

    /**
     * Checks cell-cell collisions between players `p1` and `p2`
     * and does consumptions/removal of cells that collide
     * @param p1 The first players
     * @param p2 The second player
     */
    void check_players_collisions(Player &p1, Player &p2) {
      for (auto &cell : p2.cells)
        p2.cells_eaten += eat_others(p1, cell);
      for (auto &cell : p1.cells)
        p1.cells_eaten +=eat_others(p2, cell);
    }

    /**
     * Checks if `cell` collides with and can eat any of `player`'s
     * cells. Updates the mas of `cell` and removes cells from
     * `player` if any are eaten.
     * todo: update this so that removals are O(1) making this
     * section O(n) rather tha O(n^2)
     */
    int eat_others(Player &player, Cell &cell) {

      agario::mass original_mass = player.mass();
      int          original_size = player.cells.size();
      // remove all the cells that we eat
      player.cells.erase(
        std::remove_if(player.cells.begin(), player.cells.end(),
                       [&](const Cell &other_cell) {
                         return cell.collides_with(other_cell) && cell.can_eat(other_cell);
                       }),
        player.cells.end());

      agario::mass gained_mass = original_mass - player.mass();
      cell.increment_mass(gained_mass);

      int eaten_cells = original_size - player.cells.size();
      return eaten_cells;

    }

    void recombine_cells(Player &player) {

      for (auto it = player.cells.begin(); it != player.cells.end(); ++it) {
        if (!it->can_recombine()) continue;

        Cell &cell = *it;

        for (auto it2 = std::next(it); it2 != player.cells.end();) {
          Cell &other = *it2;
          if (other.can_recombine() && cell.collides_with(other)) {
            cell.increment_mass(other.mass());
            // swap the cell to the end and pop it off
            std::swap(*it2, player.cells.back());
            player.cells.pop_back();
          } else {
            ++it2;
          }
        }
      }
    }

    bool check_virus_collisions(Cell &cell, std::vector<Cell> &created_cells, int create_limit, bool can_eat_virus) {
      for (auto it = state.viruses.begin(); it != state.viruses.end();) {
        Virus &virus = *it;

        if (cell.can_eat(virus) && cell.collides_with(virus)) {
          /*
          We have two options:
                  1: if I am within the time of being splitted (Not yet recombined) and I am trying to eat another virus, good. Eat it!
                  Note that You can consume viruses if you are split into 16 cells. One of them has to be at least 130 in mass
                  (or 10% larger than the virus) to consume the viruses. You gain 100 mass from each virus you eat.
                  2: If I am fully shaped and I am trying to eat a virus, then I will be splitted into multiple cells.

          */
          if(can_eat_virus)
            cell.increment_mass(virus.mass());
          else
            disrupt(cell, virus, created_cells, create_limit);

          std::swap(*it, state.viruses.back()); // O(1) removal
          state.viruses.pop_back();
          return true; // only collide once
        } else ++it;
      }
      return false;
    }

    /* called when `cell` collides with `virus` and is popped/disrupted.
     * The new cells that are created are added to `created_cells */
    void disrupt(Cell &cell, Virus &virus, std::vector<Cell> &created_cells, int create_limit) {
      agario::mass total_mass = cell.mass(); // mass to conserve

      // reduce the cell by roughly this ratio CELL_POP_REDUCTION, making sure the
      // amount removes is divisible by CELL_POP_SIZE
      cell.reduce_mass_by_factor(CELL_POP_REDUCTION);
      cell.increment_mass((total_mass - cell.mass()) % CELL_POP_SIZE);

      agario::mass pop_mass = total_mass - cell.mass(); // mass conservation
      int num_new_cells = div_round_up<agario::mass>(pop_mass, CELL_POP_SIZE); //just ceil(POP_MASS, cell_pop_size)

      // limit the number of cells created to the cell-creation limit
      num_new_cells = std::min<int>(num_new_cells, create_limit);

      agario::mass remaining_mass = pop_mass;

      agario::angle theta = cell.velocity.direction();
      for (int c = 0; c < num_new_cells; c++) {
        agario::angle dvel_angle = cell.velocity.direction() + (2 * M_PI * c / num_new_cells);

        auto vel = Velocity(theta + dvel_angle, max_speed(CELL_POP_SIZE));
        auto new_cell_mass = std::min<mass>(remaining_mass, CELL_POP_SIZE);

        auto loc = virus.location();
        Cell new_cell(loc, cell.velocity, new_cell_mass);
        new_cell.splitting_velocity = vel;
        new_cell.reset_recombine_timer();
        created_cells.emplace_back(std::move(new_cell));
        remaining_mass -= new_cell_mass;
      }
      cell.reset_recombine_timer();
    }

    float split_speed(agario::mass mass) {
      return clamp(3 * (std::pow(max_speed(mass), 1.2)), 20.0, 130.0);
    }

    float max_speed(agario::mass mass) {
      return CELL_MAX_SPEED / std::pow(mass, 0.439);
    }

    template<typename T>
    T random(T min, T max) {
      uniform_distribution<T> dist(min, max);
      return dist(this->state.rng);
    }

    template<typename T>
    T random(T max) { return random<T>(0, max); }
  public:
    void load(const std::string &filename) {
      std::ifstream file(filename);
      std::cout <<"LOADING the state of Environment...........\n";
      if (!file.is_open()) {
        throw EngineException("Unable to open file for loading: " + filename);
      }

      std::string line;
      while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        if (line.find("num_agents:") != std::string::npos) {
          int num_agents;
          iss >> key >> num_agents;
        } else if (line.find("ticks_per_step:") != std::string::npos) {
          int ticks_per_step;
          iss >> key >> ticks_per_step;
        } else if (line.find("num_bots:") != std::string::npos) {
          int num_bots;
          iss >> key >> num_bots;
        } else if (line.find("reward_type:") != std::string::npos) {
          std::string reward_type;
          iss >> key >> reward_type;
        } else if (line.find("players:") != std::string::npos) {
          float target_x, target_y;
          std::getline(file, line);
          iss = std::istringstream(line);
          iss >> key >> target_x;
          std::getline(file, line);
          iss = std::istringstream(line);
          iss >> key >> target_y;
          while (std::getline(file, line) && line.find("  - pid:") != std::string::npos) {
            agario::pid pid;
            iss = std::istringstream(line);
            iss >> key >> pid;
            auto player = std::make_shared<Player>(pid);
            state.players[pid] = player;
            player->target = Location(target_x, target_y);
            while (std::getline(file, line) && line.find("    cells:") == std::string::npos) {
              if (line.find("    target_x:") != std::string::npos) {
                iss = std::istringstream(line);
                float target_x;
                iss >> key >> target_x;
              } else if (line.find("    target_y:") != std::string::npos) {
                iss = std::istringstream(line);
                float target_y;
                iss >> key >> target_y;
              } else if (line.find("    is_bot:") != std::string::npos) {
                iss = std::istringstream(line);
                iss >> key >> player->is_bot;
              } else if (line.find("    dead:") != std::string::npos) {
                iss = std::istringstream(line);
                int dead;
                iss >> key >> dead;
              } else if (line.find("    split_cooldown:") != std::string::npos) {
                iss = std::istringstream(line);
                iss >> key >> player->split_cooldown;
              } else if (line.find("    feed_cooldown:") != std::string::npos) {
                iss = std::istringstream(line);
                iss >> key >> player->feed_cooldown;
              } else if (line.find("    virus_eaten_ticks:") != std::string::npos) {
                iss = std::istringstream(line);
                iss >> key;
                int tick;
                while (iss >> tick) {
                  player->virus_eaten_ticks.push_back(tick);
                }
              } else if (line.find("    anti_team_decay:") != std::string::npos) {
                iss = std::istringstream(line);
                iss >> key >> player->anti_team_decay;
              } else if (line.find("    elapsed_ticks:") != std::string::npos) {
                iss = std::istringstream(line);
                iss >> key >> player->elapsed_ticks;
              } else if (line.find("    last_decay_tick:") != std::string::npos) {
                iss = std::istringstream(line);
                iss >> key >> player->last_decay_tick;
              } else if (line.find("    food_eaten:") != std::string::npos) {
                iss = std::istringstream(line);
                iss >> key >> player->food_eaten;
              } else if (line.find("    highest_mass:") != std::string::npos) {
                iss = std::istringstream(line);
                iss >> key >> player->highest_mass;
              } else if (line.find("    cells_eaten:") != std::string::npos) {
                iss = std::istringstream(line);
                iss >> key >> player->cells_eaten;
              } else if (line.find("    viruses_eaten:") != std::string::npos) {
                iss = std::istringstream(line);
                iss >> key >> player->viruses_eaten;
              } else if (line.find("    top_position:") != std::string::npos) {
                iss = std::istringstream(line);
                iss >> key >> player->top_position;
              }
            }

            while (std::getline(file, line) && line.find("      id:") != std::string::npos) {
                iss = std::istringstream(line);
                int id ;
                iss >> key >> id;
                float x,y;
                if (std::getline(file, line) && line.find("x:") != std::string::npos) {
                    std::istringstream iss(line);
                    std::string temp;
                    char colon;
                    iss >> temp >> colon >> colon >> x;
                }
                std::getline(file, line);
                iss = std::istringstream(line);
                iss >> key >> y;

                std::getline(file, line);
                iss = std::istringstream(line);
                agario::mass mass;
                iss >> key >> mass;


                std::getline(file, line);
                iss = std::istringstream(line);
                float dx, dy;
                iss >> key >> dx;
                std::getline(file, line);
                iss = std::istringstream(line);
                iss >> key >> dy;

                Velocity vel;
                Cell cell(Location(x, y), vel, mass);
                cell.id = id;
                cell.velocity.dx = dx;
                cell.velocity.dy = dy;
                player->cells.push_back(std::move(cell));
            }
          }
        }
      }
      file.close();
    }
  };
} // namespace agario
