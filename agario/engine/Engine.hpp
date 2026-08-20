#pragma once

#include <vector>
#include <cstdlib>
#include <chrono>
#include <algorithm>
#include <sstream>
#include<set>
#include <unordered_set>
#include <numeric>
#include <fstream>
#include<random>
#include "agario/core/Player.hpp"
#include "agario/core/settings.hpp"
#include "agario/core/types.hpp"
#include "agario/core/Entities.hpp"
#include "agario/engine/GameState.hpp"
#include "agario/utils/random.hpp"
#include "agario/utils/collision_detection.hpp"
#include "agario/utils/json.hpp"
#include <agario/bots/bots.hpp>
#include <thread>
#include <chrono>
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

    Engine(distance arena_width, distance arena_height,
           int num_pellets = DEFAULT_NUM_PELLETS,
           int num_viruses = DEFAULT_NUM_VIRUSES,
           bool pellet_regen = true,
          int mode_number = 0) :
      state(agario::GameConfig(arena_width, arena_height, num_pellets, num_viruses, pellet_regen))
    {
      set_mode(mode_number);
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
    void set_mode_number(const int mode) { mode_number = mode; }

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
      pellet_grid_ready_ = false; // pellet set rebuilt from scratch
      initialize_game();
    }

    void reset_state() {
      state.clear();
      pellet_grid_ready_ = false; // pellet set rebuilt from scratch
      state.ticks = 0;
      state.next_pid = 0;
      state.main_agent_pid = -1;
      /* Deliberately does NOT reseed. This used to do
       *   state.rng.seed(std::random_device{}());
       * which silently discarded a seed set through seed(), so a
       * seed() / reset() / step() sequence was not reproducible. Reseeding is
       * the caller's decision, via seed(). */
    }

    void initialize_game() {
      if(is_squared_pellets_ == true)
        create_squared_pellets(state.config.target_num_pellets);
      else
        add_pellets(state.config.target_num_pellets);
      add_viruses(state.config.target_num_viruses);
    }

    void respawn(Player &player) {
      player.kill();
      int player_mass = std::max(CELL_MIN_SIZE, agent_mass); //agent_mass is the mass of the agent.
      if (!state.pellets.empty()) {
       if(is_squared_pellets_ == true){
        auto random_index = 0;
        auto loc = state.pellets[random_index].location();
        loc.x += 2*agario::radius_conversion(CELL_MIN_SIZE);
        loc.y += 2*agario::radius_conversion(CELL_MIN_SIZE);
        loc.x = std::min(loc.x, arena_width() - agario::radius_conversion(CELL_MIN_SIZE));
        loc.y = std::min(loc.y, arena_height() - agario::radius_conversion(CELL_MIN_SIZE));
        player.add_cell(loc, player_mass);
       }
       else
       player.add_cell(random_location(agario::radius_conversion(CELL_MIN_SIZE)), player_mass);
      } else {
        player.add_cell(random_location(agario::radius_conversion(CELL_MIN_SIZE)), player_mass);
      }
    }

    agario::Location random_location() {
      return random_location(0);
    }

    agario::Location random_location(agario::distance radius) {
      auto x = random<agario::distance>(arena_width() - 2*radius) + radius; //if it is 0, it will be 0 + radius.
      auto y = random<agario::distance>(arena_height() - 2*radius) + radius;

      return Location(x, y);
    }

    /* locates a cell by id in a vector sorted by id, or nullptr.
     * std::lower_bound returns the first element *not less than* the key, so
     * on a miss it points at a valid but different cell; the result must be
     * checked against the key before use. */
    static Cell *find_cell_by_id(std::vector<Cell> &cells, int id) {
      auto it = std::lower_bound(cells.begin(), cells.end(), id,
                                 [](const Cell &c, int key) { return c.id < key; });
      if (it != cells.end() && it->id == id) return &(*it);
      return nullptr;
    }

    /**
     * Resolves cell-eats-cell collisions between different players.
     *
     * Detection still uses the row-binned PrecisionCollisionDetection
     * heuristic; only the bookkeeping around it changed:
     *
     *  - Cells are copied into the snapshot, not std::move'd out of the live
     *    player vectors. The originals stayed in those vectors and were
     *    subsequently indexed and mutated after being moved from.
     *  - The gallery is a separate vector. solve() moves entries out of the
     *    gallery, and passing the same vector as query and gallery corrupted
     *    entries that were still pending as queries.
     *  - The snapshot is ordered by cell id, and results are applied in
     *    ascending query order. Both the snapshot (built by iterating an
     *    unordered_map of players) and the results map are hash-ordered, so
     *    the order in which mass transfers were applied - and therefore which
     *    cell won a contested victim - varied between runs and between
     *    standard library implementations, breaking seeded reproducibility.
     *  - Cells are looked up by verified id rather than trusting lower_bound,
     *    which credited mass to the wrong cell and erased an unrelated one.
     *  - A cell can be eaten at most once per tick, and a cell that has been
     *    eaten cannot go on to eat: previously two predators could both
     *    consume the same victim and each gain its full mass.
     *  - Removal is deferred to a single compaction pass per player. Erasing
     *    inside the loop invalidated the iterators and references held for
     *    later iterations and destroyed the sorted-order precondition that
     *    the binary searches depend on.
     */
    void players_collision()
    {
      cells_snapshot_.clear();
      for (auto &pair : state.players) {
        auto &player = *pair.second;
        std::sort(player.cells.begin(), player.cells.end()); // by id
        for (const auto &cell : player.cells)
          cells_snapshot_.emplace_back(player.pid(), cell);
      }
      if (cells_snapshot_.size() < 2) return;

      // canonical, hash-independent order
      std::sort(cells_snapshot_.begin(), cells_snapshot_.end(),
                [](const std::pair<agario::pid, Cell> &a,
                   const std::pair<agario::pid, Cell> &b) {
                  return a.second.id < b.second.id;
                });

      gallery_ = cells_snapshot_; // assignment reuses capacity across ticks

      PrecisionCollisionDetection<renderable> pcd({arena_width(), arena_height()}, 100);
      auto results = pcd.solve(cells_snapshot_, gallery_);
      if (results.empty()) return;

      eaten_cell_ids_.clear();

      for (size_t qi = 0; qi < cells_snapshot_.size(); qi++) {
        auto res_it = results.find(static_cast<int>(qi));
        if (res_it == results.end()) continue;

        const agario::pid eater_pid = cells_snapshot_[qi].first;
        const int eater_cell_id = cells_snapshot_[qi].second.id;

        // a cell already consumed this tick cannot eat
        if (eaten_cell_ids_.find(eater_cell_id) != eaten_cell_ids_.end()) continue;

        auto &eater_player = get_player(eater_pid);
        Cell *eater_cell = find_cell_by_id(eater_player.cells, eater_cell_id);
        if (eater_cell == nullptr) continue;

        for (const auto &entry : res_it->second) {
          const agario::pid victim_pid = entry.first;
          const int victim_id = entry.second.id;
          if (victim_id == eater_cell_id) continue;      // never eat itself
          if (!eaten_cell_ids_.insert(victim_id).second) continue; // already eaten

          auto &victim_player = get_player(victim_pid);
          Cell *victim = find_cell_by_id(victim_player.cells, victim_id);
          if (victim == nullptr) continue;

          eater_cell->increment_mass(victim->mass());
          eater_player.cells_eaten++;
        }
      }

      if (eaten_cell_ids_.empty()) return;

      for (auto &pair : state.players) {
        auto &cells = pair.second->cells;
        cells.erase(std::remove_if(cells.begin(), cells.end(),
                      [this](const Cell &c) {
                        return eaten_cell_ids_.find(c.id) != eaten_cell_ids_.end();
                      }),
                    cells.end());
      }
    }

    /**
     * Performs a single game tick, moving all entities, performing
     * collision detection and updating the game state accordingly
     * @param elapsed_seconds the amount of time which has elapsed
     * since the previous game tick.
     */
    void tick(const agario::time_delta &elapsed_seconds) {
      // pellets never move: the spatial grid persists across ticks and is
      // rebuilt only after the pellet set changes wholesale (reset/load)
      if (!pellet_grid_ready_)
        rebuild_pellet_grid();
      std::vector<int> pellets_to_remove;
      std::vector<int> viruses_to_remove;
      for (auto &pair : state.players) {
        auto &player = *pair.second;
        if (!player.dead())
          tick_player(player, elapsed_seconds, pellets_to_remove, viruses_to_remove);
      }

      // remove pellets that have been eaten
      remove_pellets(pellets_to_remove);
      remove_viruses(viruses_to_remove);

      players_collision();

      move_foods(elapsed_seconds);

      /* Regeneration requires both the mode to allow it and the caller to ask
       * for it. `regen_pellets` is the mode's policy (modes 1, 2 and 5
       * deliberately disable it); state.config.pellet_regen is the
       * constructor/Python argument, which used to be stored and never read,
       * so pellet_regen=false had no effect. Requiring both keeps every
       * shipped mode config behaving as before (they all pass true) while
       * making the argument meaningful. */
      if(regen_pellets && state.config.pellet_regen){
        if(state.ticks%120 == 0){ //every 6 seconds
          // top up to the target counts; guard against a surplus, since
          // food-fed viruses can push the count past the target and the
          // unsigned difference would wrap to a huge value
          const long pellet_deficit =
            static_cast<long>(state.config.target_num_pellets) - static_cast<long>(state.pellets.size());
          if (pellet_deficit > 0)
            add_pellets(static_cast<int>(pellet_deficit));

          const long virus_deficit =
            static_cast<long>(state.config.target_num_viruses) - static_cast<long>(state.viruses.size());
          if (virus_deficit > 0)
            add_viruses(static_cast<int>(virus_deficit));
        }
      }
      state.ticks++;

    }

    void seed(unsigned s) {
      this->state.rng.seed(s);
      std::srand(s);
    }

    void load_env_state(const std::string &filename) {
      using json = nlohmann::json;
      using HungryBot = agario::bot::HungryBot<renderable>;
      using HungryShyBot = agario::bot::HungryShyBot<renderable>;
      using AggressiveBot = agario::bot::AggressiveBot<renderable>;
      using AggressiveShyBot = agario::bot::AggressiveShyBot<renderable>;
      // Open the input file for reading
      std::ifstream in_file(filename);
      if (!in_file.is_open()) {
        throw std::runtime_error("Failed to open " + filename + " for reading");
      }

      // Parse the JSON data
      json agarcl_data;
      in_file >> agarcl_data;

      set_mode_number(agarcl_data["mode_number"]);

      // Load players
      state.players.clear();
      for (const auto &player_data : agarcl_data["players"]) {
        // auto pid = player_data["pid"].get<agario::pid>();
        // if (state.players.find(pid) != state.players.end()) {
        //   throw EngineException("Duplicate Player ID: " + std::to_string(pid));
        // }
        auto name = player_data["name"].get<std::string>();

        agario::pid pid_added;
        if(name == "HungryBot")
          pid_added = this->template add_player<HungryBot>(name);
        else if(name == "HungryShyBot")
          pid_added = this->template add_player<HungryShyBot>(name);
        else if(name == "AggressiveBot")
          pid_added = this->template add_player<AggressiveBot>(name);
        else if(name == "AggressiveShyBot")
          pid_added = this->template add_player<AggressiveShyBot>(name);
        else
          pid_added = this->template add_player<Player>(name);


        auto &player = this->player(pid_added);
        player.cells.clear();
        player.target.x = player_data["target_x"];
        player.target.y = player_data["target_y"];
        player.is_bot = player_data["is_bot"];
        player.split_cooldown = player_data["split_cooldown"];
        player.feed_cooldown = player_data["feed_cooldown"];
        player.anti_team_decay = player_data["anti_team_decay"];
        player.elapsed_ticks = player_data["elapsed_ticks"];
        player.last_decay_tick = player_data["last_decay_tick"];
        player.food_eaten = player_data["food_eaten"];
        player.highest_mass = player_data["highest_mass"];
        player.cells_eaten = player_data["cells_eaten"];
        player.viruses_eaten = player_data["viruses_eaten"];
        player.top_position = player_data["top_position"];

        for (const auto &tick : player_data["virus_eaten_ticks"]) {
          player.virus_eaten_ticks.push_back(tick);
        }

        for (const auto &cell_data : player_data["cells"]) {
          agario::Location loc(cell_data["x"].get<float>(), cell_data["y"].get<float>());
          agario::Velocity vel(static_cast<agario::distance>(cell_data["velocity_x"].get<float>()),
           static_cast<agario::distance>(cell_data["velocity_y"].get<float>()));
          Cell cell(std::move(loc), std::move(vel), cell_data["mass"].get<float>());
          cell.id = cell_data["id"].get<int>();
          player.cells.push_back(std::move(cell));
        }
      }

      // Load pellets
      pellet_grid_ready_ = false; // pellet set replaced by the snapshot
      state.pellets.clear();
      for (const auto &pellet_data : agarcl_data["pellets"]) {
        state.pellets.emplace_back(Location(static_cast<numWrapper<float, _distance>>(pellet_data["x"]),
                                            static_cast<numWrapper<float, _distance>>(pellet_data["y"])));
      }

      // Load viruses
      state.viruses.clear();
        for (const auto &virus_data : agarcl_data["viruses"]) {
          Virus virus(Location(static_cast<numWrapper<float, _distance>>(virus_data["x"]),
                   static_cast<numWrapper<float, _distance>>(virus_data["y"])));
          virus.velocity.dx = static_cast<float>(virus_data["velocity_x"]);
          virus.velocity.dy = static_cast<float>(virus_data["velocity_y"]);
          virus.set_mass(static_cast<float>(virus_data["mass"]));
          state.viruses.emplace_back(std::move(virus));
        }

        // Load foods
        state.foods.clear();
        for (const auto &food_data : agarcl_data["foods"]) {
          Location loc(static_cast<numWrapper<float, _distance>>(food_data["x"]),
          static_cast<numWrapper<float, _distance>>(food_data["y"]));
          Velocity vel(static_cast<numWrapper<float, _distance>>(food_data["velocity_x"]),
          static_cast<numWrapper<float, _distance>>(food_data["velocity_y"]));
          Food food(loc, vel);
          state.foods.emplace_back(std::move(food));
        }
        // Reset ticks
        state.ticks = 0;
        seed(agarcl_data["seed"]);
      }

    Engine(const Engine &) = delete; // no copy constructor
    Engine &operator=(const Engine &) = delete; // no copy assignments
    Engine(Engine &&) = delete; // no move constructor
    Engine &operator=(Engine &&) = delete; // no move assignment
    int mode_number = 0;
  private:
    /* ---- persistent pellet spatial grid ----
     * Pellets never move, so the grid is built once and maintained
     * incrementally: appends when pellets regenerate, index fix-ups on
     * swap-and-pop removal, and a full rebuild only when the pellet set is
     * reconstructed wholesale (reset, snapshot load, squared-pellet init).
     * Buckets store (x, y, idx) so the hot scan walks small contiguous
     * entries instead of dereferencing into scattered Pellet objects. */
    struct PelletEntry { float x, y; int idx; };
    static constexpr int PELLET_GRID_SHIFT = 5; // 32-unit buckets
    std::vector<std::vector<PelletEntry>> pellets_grid;
    int pellets_grid_width = 0;
    int pellets_grid_height = 0;
    bool pellet_grid_ready_ = false;

    // per-tick marker so a pellet can be eaten (credited + removed) at most once
    std::vector<char> pellet_eaten_;
    // players_collision scratch, reused across ticks to avoid reallocating
    std::vector<std::pair<agario::pid, Cell>> cells_snapshot_;
    std::vector<std::pair<agario::pid, Cell>> gallery_;
    std::unordered_set<int> eaten_cell_ids_;

    bool mass_decay_ = true;
    bool is_squared_pellets_ = false;
    int agent_mass = 25;
    bool regen_pellets = true;

    void set_mode(int mode) {
      switch (mode) {
      case 0:
        mass_decay_ = true;
        is_squared_pellets_ = false;
        regen_pellets = true;
        agent_mass = 25;
        break;
      case 1:
        mass_decay_ = false;
        is_squared_pellets_ = true;
        regen_pellets = false;
        agent_mass = 25;
        break;
      case 2:
        mass_decay_ = true;
        is_squared_pellets_ = true;
        regen_pellets = false;
        agent_mass = 25;
        break;
      case 3:
        mass_decay_ = false;
        is_squared_pellets_ = false; // random
        regen_pellets = true;
        agent_mass = 25;
        break;
      case 4:
        mass_decay_ = true;
        is_squared_pellets_ = false;
        regen_pellets = true;
        agent_mass = 25;
        break;
      case 5:
        set_mode(2);
        agent_mass = 1000;
        break;
      case 6:
        set_mode(4);
        agent_mass = 1000;
        break;
      case 7:
      case 8:
      case 9:
      case 10:
        set_mode(4);
        break;
      default:
        throw EngineException("Invalid mode number");
      }
    }

    void add_pellets(int n)
    {
      agario::distance pellet_radius = agario::radius_conversion(PELLET_MASS);
      for (int p = 0; p < n; p++) {
        state.pellets.emplace_back(random_location(pellet_radius));
        if (pellet_grid_ready_) // regeneration path: index the new pellet
          pellet_grid_insert(static_cast<int>(state.pellets.size()) - 1);
      }
      if (pellet_eaten_.size() < state.pellets.size())
        pellet_eaten_.resize(state.pellets.size(), 0);
    }

    void create_squared_pellets(int n) {
      pellet_grid_ready_ = false; // bulk insert; grid rebuilt lazily on first tick

      // std::random_device rd;
      // std::mt19937 gen(rd());
      // std::uniform_real_distribution<> dis(0.8, 2);

      agario::distance square_size = std::min(arena_height(), arena_width()) / 2; // Randomized size of the square
      agario::distance spacing = 1; // Space between pellets
      int points_per_side = static_cast<int>(square_size / spacing); // Points per side of the square

      agario::distance center_x = arena_width() / 2;
      agario::distance center_y = arena_height() / 2;
      agario::distance half_square_size = square_size / 2;

      // Place points along the top side
      for (int i = 0; i < points_per_side; ++i) {
          auto top_x = center_x - half_square_size + i * spacing;
          auto top_y = center_y - half_square_size;
          if (top_x >= 0 && top_x <= arena_width() && top_y >= 0 && top_y <= arena_height()) {
          state.pellets.emplace_back(Location(top_x, top_y));
          }
      }

      // Place points along the right side
      for (int i = 0; i < points_per_side; ++i) {
          auto right_x = center_x + half_square_size;
          auto right_y = center_y - half_square_size + i * spacing;
          if (right_x >= 0 && right_x <= arena_width() && right_y >= 0 && right_y <= arena_height()) {
          state.pellets.emplace_back(Location(right_x, right_y));
          }
      }

      // Place points along the bottom side
      for (int i = 0; i < points_per_side; ++i) {
          auto bottom_x = center_x + half_square_size - i * spacing;
          auto bottom_y = center_y + half_square_size;
          if (bottom_x >= 0 && bottom_x <= arena_width() && bottom_y >= 0 && bottom_y <= arena_height()) {
          state.pellets.emplace_back(Location(bottom_x, bottom_y));
          }
      }

      // Place points along the left side
      for (int i = 0; i < points_per_side; ++i) {
          auto left_x = center_x - half_square_size;
          auto left_y = center_y + half_square_size - i * spacing;
          if (left_x >= 0 && left_x <= arena_width() && left_y >= 0 && left_y <= arena_height()) {
          state.pellets.emplace_back(Location(left_x, left_y));
          }
      }
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
    void tick_player(Player &player, const agario::time_delta &elapsed_seconds, std::vector<int>&pellets_to_remove, std::vector<int>& viruses_to_remove) {
      player.elapsed_ticks += 1;

      if (ticks() % 10 == 0)
        player.take_action(state);

      move_player(player, elapsed_seconds);

      int prev_player_cells = player.cells.size();

      std::vector<Cell> created_cells; // list of new cells that will be created
      int create_limit = PLAYER_CELL_LIMIT - prev_player_cells;

      bool can_eat_virus = ((player.cells.size() >= NUM_CELLS_TO_SPLIT));


      if(optimized_check_virus_collisions(player.cells, created_cells, create_limit, can_eat_virus, viruses_to_remove)){
        player.virus_eaten_ticks.emplace_back(player.elapsed_ticks);
        player.viruses_eaten++;
      }
      int before = pellets_to_remove.size();
      get_pellets_to_remove_and_increment_cells(player.cells, pellets_to_remove);
      player.food_eaten  += pellets_to_remove.size() - before;
      player.highest_mass = std::max(player.highest_mass, player.mass());

      for (Cell &cell : player.cells) {
        can_eat_virus &= cell.mass() >= MIN_CELL_SPLIT_MASS;
        may_be_auto_split(cell, created_cells, create_limit, player.cells.size(), player.target);
        player.food_eaten +=eat_food(cell);
      }
      create_limit -= created_cells.size();
      maybe_emit_food(player);
      maybe_split(player, created_cells, create_limit);

      // add any cells that were created
      player.add_cells(created_cells, !state.config.multi_channel_observation);
      // created_cells.erase(created_cells.begin(), created_cells.end());

      recombine_cells(player);

      // some actions do not need to happen every tick
      // these will be executed once per second
      if (mass_decay_ == true && player.elapsed_ticks % 60 == 0) {
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
            ball.x = std::max(static_cast<agario::distance>(0.0), clamp<agario::distance>(ball.x, ball.radius(), arena_width()-ball.radius()));
            ball.y = std::max(static_cast<agario::distance>(0.0), clamp<agario::distance>(ball.y, ball.radius(), arena_height()-ball.radius()));
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

      check_boundary_collisions(cell_a);
      check_boundary_collisions(cell_b);

    }

    int get_row(float x, int border, int precision=100) {
              return static_cast<int>(x / border * precision);
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
      for (int idx_a = 0; idx_a < player.cells.size(); idx_a++) {
        for (int idx_b = idx_a + 1; idx_b < player.cells.size(); idx_b++) {
          Cell &cell_a = player.cells[idx_a];
          Cell &cell_b = player.cells[idx_b];
          if (cell_a.touches(cell_b)) {
            overlap = true;
            prevent_overlap(cell_a, cell_b, elapsed_seconds, player.target);
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

      // Ensure cells remain within the arena boundaries
      check_boundary_collisions(cell_a);
      check_boundary_collisions(cell_b);
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

    /* NOTE: pellet eating goes through get_pellets_to_remove_and_increment_cells
     * + remove_pellets, which keep the persistent pellet grid in sync. Any new
     * code path that mutates state.pellets must either maintain the grid or
     * set pellet_grid_ready_ = false. (The old erase-based eat_pellets was
     * removed for exactly that reason.) */

    int pellet_bucket_of(float x, float y) const {
      int bx = static_cast<int>(x) >> PELLET_GRID_SHIFT;
      int by = static_cast<int>(y) >> PELLET_GRID_SHIFT;
      bx = agario::clamp(bx, 0, pellets_grid_width - 1);
      by = agario::clamp(by, 0, pellets_grid_height - 1);
      return by * pellets_grid_width + bx;
    }

    void pellet_grid_insert(int idx) {
      const Pellet &p = state.pellets[idx];
      pellets_grid[pellet_bucket_of(p.x, p.y)].push_back(
          {static_cast<float>(p.x), static_cast<float>(p.y), idx});
    }

    void rebuild_pellet_grid() {
      pellets_grid_width  = (static_cast<int>(state.config.arena_width)  >> PELLET_GRID_SHIFT) + 1;
      pellets_grid_height = (static_cast<int>(state.config.arena_height) >> PELLET_GRID_SHIFT) + 1;
      pellets_grid.assign(static_cast<size_t>(pellets_grid_width) * pellets_grid_height, {});
      for (int i = 0; i < static_cast<int>(state.pellets.size()); ++i)
        pellet_grid_insert(i);
      if (pellet_eaten_.size() < state.pellets.size())
        pellet_eaten_.resize(state.pellets.size(), 0);
      pellet_grid_ready_ = true;
    }

    /* removes the grid entry carrying pellet index `idx` from `bucket` */
    void pellet_grid_erase_entry(int bucket, int idx) {
      auto &b = pellets_grid[bucket];
      for (size_t i = 0; i < b.size(); ++i)
        if (b[i].idx == idx) { b[i] = b.back(); b.pop_back(); return; }
    }

    /* rewrites the entry whose pellet moved from old_idx to new_idx */
    void pellet_grid_reindex_entry(int bucket, int old_idx, int new_idx) {
      for (auto &e : pellets_grid[bucket])
        if (e.idx == old_idx) { e.idx = new_idx; return; }
    }

    /* Finds pellets eaten by `cells` and credits their mass.
     *
     * Predicate: the previous code tested can_eat(pellet) &&
     * collides_with(pellet); for pellets both reduce exactly to
     * dist^2 <= cell_radius^2 (can_eat is mass > 1.1, always true with a
     * minimum cell mass of 25; collides_with ranges max(cell_r, pellet_r),
     * and pellet_r ~= 0.56 is below the smallest cell radius ~= 2.82).
     * Testing that inline avoids two virtual calls and a pow() per pellet.
     *
     * The scanned bucket neighborhood is derived from the cell's radius, so
     * cells wider than a bucket remain correct. The reach is refreshed on
     * each hit because eating grows the cell mid-scan, matching the old
     * behavior of testing collides_with against the current radius. */
    void get_pellets_to_remove_and_increment_cells(std::vector<Cell>& cells,
                                                   std::vector<int>& pellets_to_remove) {
      for (auto &cell : cells) {
        const float cx = static_cast<float>(cell.x);
        const float cy = static_cast<float>(cell.y);
        float reach = static_cast<float>(cell.radius());
        float reach_sq = reach * reach;

        const int bx = static_cast<int>(cx) >> PELLET_GRID_SHIFT;
        const int by = static_cast<int>(cy) >> PELLET_GRID_SHIFT;
        // +1 bucket of slack also covers the radius growth from mid-scan eating
        const int nr = (static_cast<int>(reach) >> PELLET_GRID_SHIFT) + 1;

        const int x_lo = std::max(bx - nr, 0);
        const int x_hi = std::min(bx + nr, pellets_grid_width - 1);
        const int y_lo = std::max(by - nr, 0);
        const int y_hi = std::min(by + nr, pellets_grid_height - 1);

        for (int ny = y_lo; ny <= y_hi; ++ny)
          for (int nx = x_lo; nx <= x_hi; ++nx)
            for (const PelletEntry &e : pellets_grid[ny * pellets_grid_width + nx]) {
              const float dx = e.x - cx;
              const float dy = e.y - cy;
              if (dx * dx + dy * dy <= reach_sq && !pellet_eaten_[e.idx]) {
                pellet_eaten_[e.idx] = 1;
                pellets_to_remove.push_back(e.idx);
                cell.increment_mass(PELLET_MASS);
                reach = static_cast<float>(cell.radius()); // grows as it eats
                reach_sq = reach * reach;
              }
            }
      }
    }

    /* O(1) swap-and-pop removal. Indices are deduplicated and applied in
     * descending order: each removal only disturbs positions at or above the
     * removed index, so smaller pending indices still refer to the pellets
     * that were actually detected as eaten. */
    void remove_pellets(std::vector<int>& pellets_to_remove) {
      std::sort(pellets_to_remove.begin(), pellets_to_remove.end(),
                [](int a, int b) { return a > b; });
      pellets_to_remove.erase(
        std::unique(pellets_to_remove.begin(), pellets_to_remove.end()),
        pellets_to_remove.end());

      for (int idx : pellets_to_remove) {
        if (idx < 0 || static_cast<size_t>(idx) >= state.pellets.size())
          continue; // defensive: stale index
        pellet_eaten_[idx] = 0; // reset mask for the next tick (O(eaten), not O(all))

        // keep the persistent grid in sync: drop the dying pellet's entry,
        // and re-point the entry of the pellet that swap-pop moves into idx
        const Pellet &dying = state.pellets[idx];
        pellet_grid_erase_entry(pellet_bucket_of(dying.x, dying.y), idx);

        const int last = static_cast<int>(state.pellets.size()) - 1;
        if (idx != last) {
          const Pellet &moved = state.pellets[last];
          pellet_grid_reindex_entry(pellet_bucket_of(moved.x, moved.y), last, idx);
          std::swap(state.pellets[idx], state.pellets.back());
        }
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
      if (cell.mass() < CELL_SPLIT_MINIMUM || cell.mass() < 2 * CELL_MIN_SIZE)
        return false;

      agario::mass split_mass = cell.mass() / 2;
      auto remaining_mass = cell.mass() - split_mass;

      cell.set_mass(remaining_mass);

      auto dir = (player_target - cell.location()).normed();
      auto loc = cell.location() + dir * cell.radius();
      loc.x = std::max(static_cast<agario::distance>(0.0), clamp<agario::distance>(loc.x, cell.radius(), arena_width() - cell.radius()));
      loc.y = std::max(static_cast<agario::distance>(0.0), clamp<agario::distance>(loc.y, cell.radius(), arena_height() - cell.radius()));

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
          if (other.can_recombine() && cell.touches(other)) {
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

    /* Rules recap for virus encounters:
     *   1. A player split into many cells (>= NUM_CELLS_TO_SPLIT) with a
     *      sufficiently large cell *consumes* the virus, gaining its mass.
     *   2. A fully-shaped (unsplit) large cell is *disrupted*: popped into
     *      multiple cells.
     *
     * Checks the player's cells against every virus directly.
     *
     * This used to consult a spatial grid (25-unit buckets, 3x3 neighborhood
     * scan). That guarantees seeing viruses only ~25 units from the cell's
     * center, but collides_with() ranges as far as max(cell_radius,
     * virus_radius) - so any cell with radius > 25 (mass > ~2000) could
     * overlap a virus beyond the scanned neighborhood and pass through it
     * undetected. A direct scan is exact for every cell size, and with the
     * typical handful of viruses it is also cheaper than building and
     * tearing down a width*height/625-bucket grid every tick (1,600 buckets
     * at arena 1000, for ~10 viruses). Same collision predicate, same
     * first-hit-per-player semantics as before. */
    bool optimized_check_virus_collisions(std::vector<Cell> &cells, std::vector<Cell> &created_cells, int create_limit, bool can_eat_virus, std::vector<int>& viruses_to_remove) {
      for (Cell &cell : cells) {
        for (int virus_idx = 0; virus_idx < static_cast<int>(state.viruses.size()); ++virus_idx) {
          Virus &virus = state.viruses[virus_idx];
          if (cell.can_eat(virus) && cell.collides_with(virus)) {
            if (can_eat_virus)
              cell.increment_mass(virus.mass());
            else
              disrupt(cell, virus, created_cells, create_limit);
            viruses_to_remove.push_back(virus_idx);
            return true; // only collide once
          }
        }
      }
      return false;
    }
    /* same descending-order swap-and-pop scheme as remove_pellets */
    void remove_viruses(std::vector<int>& viruses_to_remove) {
      std::sort(viruses_to_remove.begin(), viruses_to_remove.end(),
                [](int a, int b) { return a > b; });
      viruses_to_remove.erase(
        std::unique(viruses_to_remove.begin(), viruses_to_remove.end()),
        viruses_to_remove.end());

      for (int idx : viruses_to_remove) {
        if (idx < 0 || static_cast<size_t>(idx) >= state.viruses.size())
          continue; // defensive: stale index
        if (static_cast<size_t>(idx) != state.viruses.size() - 1)
          std::swap(state.viruses[idx], state.viruses.back());
        state.viruses.pop_back();
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

      // fan the popped cells evenly around the parent's heading. `theta` was
      // previously added to an offset that already included direction(), so
      // the heading was counted twice.
      agario::angle theta = cell.velocity.direction();
      for (int c = 0; c < num_new_cells; c++) {
        auto vel = Velocity(theta + static_cast<agario::angle>(2 * M_PI * c / num_new_cells),
                            max_speed(CELL_POP_SIZE));
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

    agario::Location random_circle_point(agario::distance radius) {
      agario::angle theta = random<agario::angle>(0, 2 * M_PI);
      agario::distance x = radius * std::cos(theta) + radius;
      agario::distance y = radius * std::sin(theta) + radius;
      return agario::Location(x, y);
    }

  };

}
