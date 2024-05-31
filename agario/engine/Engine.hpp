#pragma once

#include <vector>
#include <cstdlib>
#include <chrono>
#include <algorithm>
#include <sstream>
#include<set>

#include "agario/core/Player.hpp"
#include "agario/core/settings.hpp"
#include "agario/core/types.hpp"
#include "agario/core/Entities.hpp"
#include "agario/engine/GameState.hpp"

namespace agario {

  class EngineException : public std::runtime_error {
    using runtime_error::runtime_error;
  };

  template<bool renderable>
  class Engine {
  std::vector<std::pair<agario::distance, agario::distance>> entity_locations;
  public:
    using Player = Player<renderable>;
    using Cell = Cell<renderable>;
    using Food = Food<renderable>;
    using Pellet = Pellet<renderable>;
    using Virus = Virus<renderable>;
    using GameState = GameState<renderable>;

    Engine(distance arena_width, distance arena_height,
           int num_pellets = DEFAULT_NUM_PELLETS,
           int num_viruses = DEFAULT_NUM_VIRUSES,
           bool pellet_regen = true) :
      state(arena_width, arena_height),
      _num_pellets(num_pellets), _num_virus(num_viruses),
      _pellet_regen(pellet_regen),
      next_pid(0) {
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
    agario::distance arena_width() const { return state.arena_width; }
    agario::distance arena_height() const { return state.arena_height; }
    int player_count() const { return state.players.size(); }
    int pellet_count() const { return state.pellets.size(); }
    int virus_count() const { return state.viruses.size(); }
    int food_count() const { return state.foods.size(); }
    bool pellet_regen() const { return _pellet_regen; };

    template<typename P>
    agario::pid add_player(const std::string &name = std::string()) {
      auto pid = next_pid++;

      std::shared_ptr<P> player = nullptr;
      if (name.empty()) {
        player = std::make_shared<P>(pid);
      } else {
        player = std::make_shared<P>(pid, name);
      }

      auto p = state.players.insert(std::make_pair(pid, player));
      _respawn(*player);
      return pid;
    }

    Player &player(agario::pid pid) {
      return const_cast<Player &>(get_player(pid));
    }

    const Player &get_player(agario::pid pid) const {
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
      add_pellets(_num_pellets);
      add_viruses(_num_virus);
    }

    void respawn(agario::pid pid) { _respawn(player(pid)); }

    agario::Location random_location() {
      auto x = random<agario::distance>(arena_width());
      auto y = random<agario::distance>(arena_height());
      
      return Location(x, y);
    }

    bool is_location_free(agario::distance x, agario::distance y, agario::distance radius) {

      for (auto &entity_loc : entity_locations) {
        auto entity_x = entity_loc.first;
        auto entity_y = entity_loc.second;

        agario::distance dx = x - entity_x;
        agario::distance dy = y - entity_y;
        agario::distance dis = std::sqrt(dx * dx + dy * dy);
        if(dis <= 2*radius)
          return false;
      }
      entity_locations.push_back(std::make_pair(x, y));
      return true;
    }
    agario::Location random_location(agario::distance radius) {
      auto mx_value = arena_width() - 2*radius;
      auto my_value = arena_height() - 2*radius;
      
      agario::distance x, y;
    do{
      x = random<agario::distance>(mx_value) + radius; //if it is 0, it will be 0 + radius. 
      y = random<agario::distance>(my_value) + radius;
    }while(!is_location_free(x, y,radius));

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

      check_player_collisions();

      move_foods(elapsed_seconds);

      if (_pellet_regen) {
        add_pellets(_num_pellets - state.pellets.size());
      }
      add_viruses(_num_virus - state.viruses.size());
      state.ticks++;
    }

    void seed(unsigned s) { std::srand(s); }

    Engine(const Engine &) = delete; // no copy constructor
    Engine &operator=(const Engine &) = delete; // no copy assignments
    Engine(Engine &&) = delete; // no move constructor
    Engine &operator=(Engine &&) = delete; // no move assignment

  private:

    agario::GameState<renderable> state;

    agario::pid next_pid;
    int _num_pellets, _num_virus, _pellet_regen;

    /**
     * Resets a player to the starting position
     * @param pid player ID of the player to reset
     */
    void _respawn(Player &player) {
      player.kill();
      player.add_cell(random_location(), CELL_MIN_SIZE);
    }
    


    void add_pellets(int n) {
      agario::distance pellet_radius = agario::radius_conversion(PELLET_MASS);
      
      for (int p = 0; p < n; p++)
        state.pellets.emplace_back(random_location(pellet_radius));
    }

    void add_viruses(int n) {
      agario::distance virus_radius = agario::radius_conversion(VIRUS_MASS);
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

      if (ticks() % 10 == 0)
        player.take_action(state);

      move_player(player, elapsed_seconds);

      std::vector<Cell> created_cells; // list of new cells that will be created
      int create_limit = PLAYER_CELL_LIMIT - player.cells.size();

      bool can_eat_virus = ((player.cells.size() >= NUM_CELLS_TO_SPLIT) & (player.get_max_mass_cell() >= MIN_CELL_SPLIT_MASS));

      for (Cell &cell : player.cells) {

        eat_pellets(cell);
        eat_food(cell);
        check_virus_collisions(cell, created_cells, create_limit, can_eat_virus);
      }

      create_limit -= created_cells.size();

      maybe_emit_food(player);
      maybe_split(player, created_cells, create_limit);

      // add any cells that were created
      player.add_cells(created_cells);
      created_cells.erase(created_cells.begin(), created_cells.end());

      recombine_cells(player);
    }

    /**
     * Moves all of the cells of the given player by an amount proportional
     * to the elapsed time since the last tick, given by elapsed_seconds
     * @param player the player to move
     * @param elapsed_seconds time since the last game tick
     */
    void move_player(Player &player, const agario::time_delta &elapsed_seconds) {
      auto dt = elapsed_seconds.count();
      agario::mass best_mass_cell = 0; 
      for (auto &cell : player.cells) {
        cell.velocity.dx = 3 * (player.target.x - cell.x);
        cell.velocity.dy = 3 * (player.target.y - cell.y);
        best_mass_cell = std::max(best_mass_cell, cell.mass());
        // clip speed
        auto speed_limit = max_speed(cell.mass());
        cell.velocity.clamp_speed(0, speed_limit);

        cell.move(dt);
        cell.splitting_velocity.decelerate(SPLIT_DECELERATION, dt);

        check_boundary_collisions(cell);
      }
      player.set_max_mass_cell(best_mass_cell);
      // make sure not to move two of players own cells into one another
      check_player_self_collisions(player);
    }

    void move_foods(const agario::time_delta &elapsed_seconds) {
      auto dt = elapsed_seconds.count();

      for (auto &food : state.foods) {
        if (food.velocity.magnitude() == 0) continue;

        food.decelerate(FOOD_DECEL, dt);
        food.move(dt);

        check_boundary_collisions(food);
      }
    }

    /**
     * Constrains the location of `ball` to be inside the boundaries
     * of the arena
     * @param ball the ball to keep inside the arena
     */
    void check_boundary_collisions(Ball &ball) {
      ball.x = clamp<agario::distance>(ball.x, 0, arena_width());
      ball.y = clamp<agario::distance>(ball.y, 0, arena_height());
    }

    /**
     * Moves all of `player`'s cells apart slightly such that
     * cells which aren't eligible for recombining don't overlap
     * with other cells of the same player.
     */
    void check_player_self_collisions(Player &player) {
      for (auto it = player.cells.begin(); it != player.cells.end(); ++it) {
        for (auto it2 = std::next(it); it2 != player.cells.end(); it2++) {

          // only allow collisions if both are eligible for recombining
          if (it->can_recombine() && it2->can_recombine())
            continue;

          Cell &cell_a = *it;
          Cell &cell_b = *it2;
          if (cell_a.touches(cell_b))
            prevent_overlap(cell_a, cell_b);
        }
      }
    }

    /**
     * moves `cell_a` and `cell_b` apart slightly
     * such that they cannot be overlapping
     * @param cell_a first cell to move apart
     * @param cell_b second cell to move apart
     */
    void prevent_overlap(Cell &cell_a, Cell &cell_b) {

      auto dx = cell_b.x - cell_a.x;
      auto dy = cell_b.y - cell_a.y;

      auto dist = sqrt(dx * dx + dy * dy);
      auto target_dist = cell_a.radius() + cell_b.radius();

      if (dist > target_dist) return; // aren't overlapping

      auto x_ratio = dx / (std::abs(dx) + std::abs(dy));
      auto y_ratio = dy / (std::abs(dx) + std::abs(dy));

      cell_b.x += (target_dist - dist) * x_ratio / 2;
      cell_b.y += (target_dist - dist) * y_ratio / 2;

      cell_a.x -= (target_dist - dist) * x_ratio / 2;
      cell_a.y -= (target_dist - dist) * y_ratio / 2;
    }

    /**
     * checks for collisions between the given cell and
     * all of the pellets in the game, removing those pellets
     * from the game which the cell eats
     * @param cell the cell which is doing the eating
     */
    void eat_pellets(Cell &cell) {
      auto prev_size = pellet_count();

      state.pellets.erase(
        std::remove_if(state.pellets.begin(), state.pellets.end(),
                       [&](const Pellet &pellet) {
                         return cell.can_eat(pellet) && cell.collides_with(pellet);
                       }),
        state.pellets.end());
      auto num_eaten = prev_size - pellet_count();
      cell.increment_mass(num_eaten * PELLET_MASS);
    }

    void eat_food(Cell &cell) {
      if (cell.mass() < FOOD_MASS) return;
      auto prev_size = food_count();

      state.foods.erase(
        std::remove_if(state.foods.begin(), state.foods.end(),
                       [&](const Food &pellet) {
                         return cell.can_eat(pellet) && cell.collides_with(pellet);
                       }),
        state.foods.end());
      auto num_eaten = prev_size - food_count();
      cell.increment_mass(num_eaten * FOOD_MASS);
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

    void player_split(Player &player, std::vector<Cell> &created_cells, int create_limit) {
      if (create_limit == 0)
        return;

      int num_splits = 0;
      for (Cell &cell : player.cells) {

        if (cell.mass() < CELL_SPLIT_MINIMUM)
          continue;

        agario::mass split_mass = cell.mass() / 2;
        auto remaining_mass = cell.mass() - split_mass;

        cell.set_mass(remaining_mass);

        auto dir = (player.target - cell.location()).normed();
        auto loc = cell.location() + dir * cell.radius();
        Velocity vel(dir * split_speed(split_mass));

        // todo: add constructor that takes splitting velocity (and color)
        Cell new_cell(loc, vel, split_mass);
        new_cell.splitting_velocity = vel;

        cell.reset_recombine_timer();
        new_cell.reset_recombine_timer();

        created_cells.emplace_back(std::move(new_cell));

        // stop splitting when we've created enough cells
        if (++num_splits == create_limit)
          return;
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
        eat_others(p1, cell);
      for (auto &cell : p1.cells)
        eat_others(p2, cell);
    }

    /**
     * Checks if `cell` collides with and can eat any of `player`'s
     * cells. Updates the mas of `cell` and removes cells from
     * `player` if any are eaten.
     * todo: update this so that removals are O(1) making this
     * section O(n) rather tha O(n^2)
     */
    void eat_others(Player &player, Cell &cell) {

      agario::mass original_mass = player.mass();

      // remove all the cells that we eat
      player.cells.erase(
        std::remove_if(player.cells.begin(), player.cells.end(),
                       [&](const Cell &other_cell) {
                         return cell.collides_with(other_cell) && cell.can_eat(other_cell);
                       }),
        player.cells.end());

      agario::mass gained_mass = original_mass - player.mass();
      cell.increment_mass(gained_mass);
    }

    void recombine_cells(Player &player) {
      for (auto it = player.cells.begin(); it != player.cells.end(); ++it) {
        if (!it->can_recombine()) continue;

        Cell &cell = *it;

        for (auto it2 = std::next(it); it2 != player.cells.end();) {
          Cell &other = *it2;
          if (other.can_recombine() && cell.collides_with(other)) {
            cell.increment_mass(other.mass());
            it2 = player.cells.erase(it2);
          } else {
            ++it2;
          }
        }
      }
    }

    void check_virus_collisions(Cell &cell, std::vector<Cell> &created_cells, int create_limit, bool can_eat_virus) {
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
          return; // only collide once
        } else ++it;
      }
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
      return CELL_MAX_SPEED / std::sqrt((float) mass);
    }

    template<typename T>
    T random(T min, T max) {
      return (max - min) * (static_cast<T>(rand()) / static_cast<T>(RAND_MAX)) + min;
    }

    template<typename T>
    T random(T max) { return random<T>(0, max); }
  };

}

