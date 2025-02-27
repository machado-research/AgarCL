#pragma once

#include <cassert>

#include <unordered_map>

#include <agario/engine/Engine.hpp>
#include <agario/core/types.hpp>
#include <agario/core/Entities.hpp>
#include <agario/core/Ball.hpp>
#include <agario/bots/bots.hpp>
#include <agario/engine/GameState.hpp>

#include "environment/envs/BaseEnvironment.hpp"

#ifdef RENDERABLE
#include <agario/core/renderables.hpp>
#include <agario/rendering/renderer.hpp>
#include "agario/rendering/FrameBufferObject.hpp"
#endif

#define DEFAULT_GRID_SIZE 128
# define PIXEL_LEN 3
#define f first
#define s second
namespace agario::env {

    using vecpii = std::vector<std::pair<int, int>>;

    class GlobalState {
        public :
            explicit GlobalState(int width, int height, int frame_limit, int last_frame, int team_num )
            : map_width(width),
            map_height(height),
            frame_limit(frame_limit),
            last_frame_count(last_frame),
            team_num(team_num) { }

            void update_last_frame_count(int frame_count) {
                last_frame_count = frame_count;
            }

            // Getters
            const int get_map_width() const {
                return map_width;
            }

            const int get_map_height() const {
                return map_height;
            }

            int get_frame_limit() const {
                return frame_limit;
            }

            const int get_team_num() const {
                return team_num;
            }

            int get_last_frame_count() {
                return last_frame_count;
            }


        private:
            int map_width;
            int map_height;
            int frame_limit;
            int last_frame_count;
            int team_num;
    };

    struct FoodInfo{
        std::pair<int, int> position;
        double radius;
        agario::mass score;
    };

    struct VirusInfo{
        std::pair<int, int> position;
        double radius;
        agario::mass score;
        std::pair< double, double> velocity;
    };

    struct SporeInfo{
        std::pair<int, int> position;
        double radius;
        agario::mass score;
        std::pair< double, double> velocity;
        int owner; //pid
    };


    struct CloneInfo{
        std::pair<int, int> position;
        double radius;
        agario::mass score;
        std::pair< double, double> velocity;
        agario::angle direction;
        int owner; //pid
        int teamId; //teamid
    };

    class PlayerState {
        public:

            PlayerState() = default;
            explicit PlayerState(int player_id,
                             const std::vector<FoodInfo>& food_infos,
                             const std::vector<VirusInfo>& virus_infos,
                             const std::vector<SporeInfo>& spore_infos,
                             const std::vector<CloneInfo>& clone_infos,
                             std::string team_name,
                             double score,
                             bool can_eject,
                             bool can_split)
            : player_id(player_id),
              food_infos(food_infos),
              virus_infos(virus_infos),
              spore_infos(spore_infos),
              clone_infos(clone_infos),
              team_name(team_name),
              score(score),
              can_eject(can_eject),
              can_split(can_split) {}

            // Update functions that replace the entire info vectors
            void update_food_infos(const std::vector<FoodInfo>& infos) {
                food_infos = infos;
            }
            void update_virus_infos(const std::vector<VirusInfo>& infos) {
                virus_infos = infos;
            }
            void update_spore_infos(const std::vector<SporeInfo>& infos) {
                spore_infos = infos;
            }
            void update_clone_infos(const std::vector<CloneInfo>& infos) {
                clone_infos = infos;
            }

            // Alternatively, add single info objects:
            void add_food_info(const FoodInfo& info) {
                food_infos.push_back(info);
            }
            void add_virus_info(const VirusInfo& info) {
                virus_infos.push_back(info);
            }
            void add_spore_info(const SporeInfo& info) {
                spore_infos.push_back(info);
            }
            void add_clone_info(const CloneInfo& info) {
                clone_infos.push_back(info);
            }


            void update_can_eject(bool can_eject) {
                this->can_eject = can_eject;
            }
            void update_can_split(bool can_split) {
                this->can_split = can_split;
            }
            void update_score(double score) {
                this->score = score;
            }

            // Getters
            bool canEject() const { return can_eject; }
            bool canSplit() const { return can_split; }
            double get_score() const { return score; }
            const std::vector<FoodInfo>& get_food_infos() const { return food_infos; }
            const std::vector<VirusInfo>& get_virus_infos() const { return virus_infos; }
            const std::vector<SporeInfo>& get_spore_infos() const { return spore_infos; }
            const std::vector<CloneInfo>& get_clone_infos() const { return clone_infos; }
            std::string get_team_name() const { return team_name; }
            int get_player_id() const { return player_id; }

        
        private:
            int player_id;
            std::vector<FoodInfo> food_infos;
            std::vector<VirusInfo> virus_infos;
            std::vector<SporeInfo> spore_infos;
            std::vector<CloneInfo> clone_infos;
            std::string team_name;
            double score;
            bool can_eject;
            bool can_split;        
    };


    class PlayerStates {
        public:
            explicit PlayerStates(std::unordered_map<int, PlayerState> player_states)
            : player_states(std::move( player_states ) ) { }

            void update_player_state(int player_id, PlayerState player_state) {
                player_states[player_id] = player_state;
            }

            PlayerState get_player_state(int player_id) {
                auto it = player_states.find(player_id);
                if (it == player_states.end()) {
                    throw std::out_of_range("Player id not found in player_states mapping");
                }
                return it->second;
            }

            const std::unordered_map<int, PlayerState>& get_all_player_states() const {
                return player_states;
            }

            void clear() {
                player_states.clear();
            }

        private:
            std::unordered_map<int, PlayerState> player_states;
    };

   
//-----------------------------------------------
// GoBiggerObservation
//-----------------------------------------------
    template<bool R>
    class GoBiggerObservation {
        using dtype = double;

    private:
        GlobalState global_state;
        PlayerStates player_states;
        int no_frames;
        std::vector<dtype> observation_data;

    public:
        using GameState = GameState<R>;
        using Player    = Player<R>;
        using Cell      = Cell<R>;
        using Pellet    = Pellet<R>;
        using Virus     = Virus<R>;
        using Food      = Food<R>;

        class Configuration {
        public:
            Configuration(int num_frames, int grid_size,
                        bool observe_cells, bool observe_others,
                        bool observe_viruses, bool observe_pellets)
            : num_frames(num_frames),
                grid_size(grid_size),
                observe_cells(observe_cells),
                observe_others(observe_others),
                observe_pellets(observe_pellets),
                observe_viruses(observe_viruses) {}

            int num_frames;
            int grid_size;
            bool observe_pellets;
            bool observe_cells;
            bool observe_viruses;
            bool observe_others;
        };

        Configuration config_;

        enum calc_type {
            at_least_   = 0,
            total_mass_ = 1,
            min_        = 2,
            max_        = 3
        };

        // MUST ALWAYS CALL CONGUFIGURE AFTER CONSTRUCTION
        explicit GoBiggerObservation(int map_width, int map_height,
                                    int frame_limit, int last_frame,
                                    int team_num)
        : global_state(map_width, map_height, frame_limit, last_frame, team_num),
            player_states(std::unordered_map<int, PlayerState>{}),
            no_frames(0),
            config_(1, DEFAULT_GRID_SIZE, true, true, true, true)  {} // to change to use actual config args

        // Add a constructor that uses the configuration
        template <typename ...Args>
        void configure(Args&&... args) {
            // Store config
            config_ = Configuration(std::forward<Args>(args)...);

            // Clear states & data if needed
            player_states.clear();
            observation_data.clear();
        }

        [[nodiscard]] bool _inside_grid(int gx, int gy) const {
            return (0 <= gx && gx < config_.grid_size &&
                    0 <= gy && gy < config_.grid_size);
        }

        bool _in_bounds(const Location &loc, distance w, distance h) {
            return (0 <= loc.x && loc.x < w &&
                    0 <= loc.y && loc.y < h);
        }

        void update_global_state(int frame_count) {
            global_state.update_last_frame_count(frame_count);
        }

        int num_frames() const { return no_frames; }

        void update_player_state(int player_id,
                                const std::vector<FoodInfo> &food_infos,
                                const std::vector<VirusInfo> &virus_infos,
                                const std::vector<SporeInfo> &spore_infos,
                                const std::vector<CloneInfo> &clone_infos,
                                const std::string &team_name,
                                double score,
                                bool can_eject,
                                bool can_split)
        {
            PlayerState ps(player_id,
                        food_infos,
                        virus_infos,
                        spore_infos,
                        clone_infos,
                        team_name,
                        score,
                        can_eject,
                        can_split);
            player_states.update_player_state(player_id, ps);
        }

        const GlobalState& get_global_state()   const { return global_state; }
        const PlayerStates& get_player_states() const { return player_states; }

        const PlayerState& get_player_state(int player_id) const {
            int tnum = global_state.get_team_num();
            assert(player_id < tnum && "Player ID >= team num!");
            const auto& m = player_states.get_all_player_states();
            auto it = m.find(player_id);
            if (it == m.end()) {
                throw std::out_of_range("Player id not found in player_states mapping");
            }
            return it->second;
        }

        size_t length() const {
            auto [f, h, w] = shape();
            return static_cast<size_t>(f * h * w);
        }

        const dtype* data() const { return observation_data.data(); }
        dtype* data()             { return observation_data.data(); }

        const std::tuple<int,int,int> shape() const {
            int frames = num_frames();
            int height = global_state.get_map_height();
            int width  = global_state.get_map_width();
            return {frames, height, width};
        }

        // Determine how large a 'view' should be, based on the player's mass
        // todo: Need to find what formula they use
        float _view_size(const Player &player) const {
            return agario::clamp<float>(2 * player.mass(), 100, 300);
        }

        void _world_to_grid(const Player &player, const Location &loc,
                            float view_size, int &gx, int &gy) const
        {
            float centering = static_cast<float>(config_.grid_size) / 2.f;
            float diff_x = loc.x - player.x();
            float diff_y = loc.y - player.y();

            gx = static_cast<int>((config_.grid_size * diff_x / view_size) + centering);
            gy = static_cast<int>((config_.grid_size * diff_y / view_size) + centering);
        }

        Location _grid_to_world(const Player &player, float view_size,
                                int gx, int gy) const
        {
            float centering = static_cast<float>(config_.grid_size) / 2.f;
            float dx = (gx - centering) * (view_size / config_.grid_size);
            float dy = (gy - centering) * (view_size / config_.grid_size);
            return player.location() + Location(dx, dy);
        }

        template<typename U>
        void _store_entities(const std::vector<U> &entities,
                            const Player &player,
                            PlayerState &ps,
                            int pid,
                            int channel,
                            calc_type ctype = calc_type::total_mass_)
        {
            float view_size = _view_size(player);

            int grid_x = 0, grid_y = 0;
            for (auto &entity : entities) {
                _world_to_grid(player, entity.location(), view_size, grid_x, grid_y);

                if (_inside_grid(grid_x, grid_y)) {
                    if constexpr (std::is_same_v<U, Pellet>) {
                        FoodInfo info = {
                            {grid_x, grid_y},
                            entity.radius(),
                            entity.mass()
                        };
                        ps.add_food_info(info);
                    }
                    else if constexpr (std::is_same_v<U, Virus>) {
                        VirusInfo info = {
                            {grid_x, grid_y},
                            entity.radius(),
                            entity.mass(),
                            std::make_pair(0,0)
                        };
                        ps.add_virus_info(info);
                    }
                    else if constexpr (std::is_same_v<U, Food>) {
                        SporeInfo info = {
                            {grid_x, grid_y},
                            entity.radius(),
                            entity.mass(),
                            std::make_pair(0,0),
                            player.pid()
                        };
                        ps.add_spore_info(info);
                    }
                    else if constexpr (std::is_same_v<U, Cell>) {
                        CloneInfo info = {
                            {grid_x, grid_y},
                            entity.radius(),
                            entity.mass(),
                            std::make_pair( entity.get_velocity().dx, entity.get_velocity().dy ),
                            entity.get_velocity().direction(),
                            player.pid(),
                            0 //player.teamId()
                        };
                        ps.add_clone_info(info);
                    }
                    else {
                        throw std::runtime_error("Unknown entity type in _store_entities");
                    }
                    // commit updated player state
                    player_states.update_player_state(pid, ps);
                }
            }
        }

        inline void add_frame(const Player &ply,
                            const GameState &game_state,
                            int frame_index)
        {
            if (observation_data.empty()) {
                throw EnvironmentException("GoBiggerObservation was not configured.");
            }

            update_global_state(frame_index);
            no_frames++;

            // Example channel usage
            int channel = 0;

            for (auto const &[pid, pl] : game_state.players) {
                auto pstate = player_states.get_player_state(pid);


                _store_entities<Virus>(game_state.viruses, *pl, pstate,
                       pid, channel, calc_type::total_mass_);
                _store_entities<Pellet>(game_state.pellets, *pl, pstate,
                          pid, channel, calc_type::total_mass_);
                _store_entities<Food>(game_state.foods, *pl, pstate,
                        pid, channel, calc_type::total_mass_);
                _store_entities<Cell>(pl->cells, *pl, pstate,
                        pid, channel, calc_type::total_mass_);

            }
        }
    };

template <bool renderable>
    class GoBiggerEnvironment : public BaseEnvironment<renderable> {
        using gameState = agario::GameState<renderable>;
        using Player    = agario::Player<renderable>;
        using Cell      = agario::Cell<renderable>;
        using Pellet    = agario::Pellet<renderable>;
        using Virus     = agario::Virus<renderable>;
        using Food      = agario::Food<renderable>;
        using Super     = BaseEnvironment<renderable>;

    public:
        using dtype = std::uint8_t;
        using observationType = GoBiggerObservation<renderable>;

        explicit GoBiggerEnvironment(int map_width, int map_height, int frame_limit, int num_agents,
                                     int ticks_per_step, int arena_size, bool pellet_regen, int num_pellets,
                                     int num_viruses, int num_bots, bool reward_type, int c_death = 0,
                                     bool agent_view = false)
            : Super(num_agents, ticks_per_step, arena_size, pellet_regen, num_pellets, num_viruses, num_bots, reward_type),
              observation(map_width, map_height, frame_limit, 0, num_agents),
              last_frame_index(0),
              last_player(nullptr),
              frame_buffer(std::make_shared<FrameBufferObject>(512, 512, false)) {
                observations.push_back(observation);
              }

        
        template <typename ...Config>
        void configure_observation(Config&&... config) {
            observations.clear();
            for (int i = 0; i < this->num_agents(); i++)
            {
                GoBiggerObservation<renderable> obs(
                    observation.get_global_state().get_map_width(),
                    observation.get_global_state().get_map_height(),
                    observation.get_global_state().get_frame_limit(),
                    0,  // For example, last_frame set to 0
                observation.get_global_state().get_team_num()
                );
                // Now configure the observation with the provided parameters.
                obs.configure(std::forward<Config>(config)...);
                // Add the configured observation to the vector.
                observations.push_back(obs);
            }
        }


        void _partial_observation(int agent_index, int tick_index) override {
            // For now, every agent is in their own team (so no collaboration)
            assert(agent_index < this->num_agents());
            assert(tick_index < this->ticks_per_step());

            auto &player = this->engine_.player(this->pids_[agent_index]);
            this->c_death_ = 0;

            // Get player state from the observation.
            // PlayerState player_state = observation.get_player_state(player.pid());

            auto &state = this->engine_.game_state();
            // We store in the observation the last `num_frames` frames between each step.
            int frame_index = tick_index - (this->ticks_per_step() - observation.num_frames());
            if (frame_index >= 0) // frame skipping
              observation.add_frame(player, state, frame_index);

            last_player = &player;
            last_frame_index = frame_index;

            observation.update_global_state(last_frame_index);
        }

        const std::tuple<int, int, int> observation_shape() const {
            return observation.shape();
        }

        const std::vector<observationType> &get_observations() const { return observations; }


        void render() override {
            #ifdef RENDERABLE
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, frame_buffer->width(), frame_buffer->height());
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                for (auto &pid: this->pids_) {
                    auto &player = this->engine_.player(pid);
                    renderer->render_screen(player, this->engine_.game_state());
                }

                glfwPollEvents();
                frame_buffer -> swap_buffers();
                frame_buffer -> show();
            #endif
        }

        void close() override {
            #ifdef RENDERABLE
                if (renderer) {
                    renderer->close_program();
                }
                else {
                    std::cerr << "Error: renderer is null in GoBiggerEnvironment and RENDERABLE is TRUE" << std::endl;
                }
                // glfwTerminate();
                // glDeleteProgram(renderer->shader.program);
            #endif
        }

    private:
        int last_frame_index;
        Player *last_player;
        observationType observation;

        std::vector<observationType> observations;



        #ifdef RENDERABLE
            std::unique_ptr<agario::Renderer> renderer;
            std::shared_ptr<FrameBufferObject> frame_buffer;
        #endif
    };

};  // namespace agario::env