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

            int get_frame_limit() {
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

    class PlayerState {
        public:

            PlayerState() = default;
            explicit PlayerState(int player_id, vecpii food_positions, vecpii virus_positions, vecpii spore_positions, vecpii clone_positions, std::string team_name, double score, bool can_eject, bool can_split ):
            player_id(player_id),
            food_positions(food_positions),
            virus_positions(virus_positions),
            spore_positions(spore_positions),
            clone_positions(clone_positions),
            team_name(team_name),
            score(score),
            can_eject(can_eject),
            can_split(can_split) { }


            void update_can_eject(bool can_eject) {
                this->can_eject = can_eject;
            }

            void update_can_split(bool can_split) {
                this->can_split = can_split;
            }

            void update_score(double score) {
                this->score = score;
            }

            void update_food_positions(vecpii food_positions) {
                this->food_positions = food_positions;
            }

            void update_virus_positions(vecpii virus_positions) {
                this->virus_positions = virus_positions;
            }

            void update_spore_positions(vecpii spore_positions) {
                this->spore_positions = spore_positions;
            }

            void update_clone_positions(vecpii clone_positions) {
                this->clone_positions = clone_positions;
            }


            bool canEject() {
                return can_eject;
            }

            bool canSplit() {
                return can_split;
            }

            double get_score() {
                return score;
            }

            const vecpii& get_spore_positions() {
                return spore_positions;
            }

            const vecpii& get_clone_positions() {
                return clone_positions;
            }

            const vecpii& get_food_positions() {
                return food_positions;
            }

            const vecpii& get_virus_positions() {
                return virus_positions;
            }

            std::string get_team_name() {
                return team_name;
            }

            int get_player_id() {
                return player_id;
            }

        
        private:
            int player_id;
            vecpii food_positions;
            vecpii virus_positions;
            vecpii spore_positions;
            vecpii clone_positions;
            std::string team_name;
            double score;
            bool can_eject;
            bool can_split;        
    };


    class PlayerStates {
        public:
            explicit PlayerStates(std::unordered_map<int, PlayerState> player_states)
            : player_states(player_states) { }

            void update_player_state(int player_id, PlayerState player_state) {
                player_states[player_id] = player_state;
            }

            const std::unordered_map<int, PlayerState>& get_all_player_states() const {
                return player_states;
            }

        private:
            std::unordered_map<int, PlayerState> player_states;
    };

   

    class GoBiggerObservation {

        public:

            using dtype = double;

            explicit GoBiggerObservation(int map_width, int map_height, int frame_limit, int last_frame, int team_num)
            : global_state(map_width, map_height, frame_limit, last_frame, team_num),
            player_states({}) {} // Initialize Player States with empty map

            // Update global state
            void update_global_state(int frame_count) {
                global_state.update_last_frame_count(frame_count);
            }

            // For now num_frames is 1
            int num_frames() const { return 1; }

             // Update a player's state with the given parameters.
            void update_player_state(int player_id,
                                    const vecpii &food_positions,
                                    const vecpii &virus_positions,
                                    const vecpii &spore_positions,
                                    const vecpii &clone_positions,
                                    const std::string &team_name,
                                    double score,
                                    bool can_eject,
                                    bool can_split)
            {
                PlayerState ps(player_id, food_positions, virus_positions, spore_positions, clone_positions,
                            team_name, score, can_eject, can_split);
                player_states.update_player_state(player_id, ps);
            }

            const GlobalState& get_global_state() const {
                return global_state;
            }

            const PlayerStates& get_player_states() const {
                return player_states;
            }

            const PlayerState& get_player_state(int player_id) const {
                const int team_num = global_state.get_team_num();
                assert (player_id < team_num );
                const auto& all_states = player_states.get_all_player_states();
                std::cout << "Player id: " << player_id << std::endl;
                auto it = all_states.find(player_id);
                if (it == all_states.end()) {
                    throw std::out_of_range("Player id not found in player_states mapping");
                }
                return it->second;
            }

            size_t length() const {
                auto s = shape();
                return std::get<0>(s) * std::get<1>(s) * std::get<2>(s);
            }

            // Access to the underlying data (assuming row-major order)
            const dtype* data() const { return observation_data.data(); }
            dtype* data() { return observation_data.data(); }

            // Compute the strides (in bytes) for each dimension
            std::tuple<size_t, size_t, size_t> strides() const {
                auto s = shape();
                // Assuming row-major order:
                size_t frame = std::get<0>(s);
                size_t height = std::get<1>(s);
                size_t width  = std::get<2>(s);
                // For a contiguous array of type dtype, strides are:
                size_t stride0 = height * width * sizeof(dtype);
                size_t stride1 = width * sizeof(dtype);
                size_t stride2 = sizeof(dtype);
                return {stride0, stride1, stride2};
            }

            const std::tuple<int, int, int> shape() const {
                int frames = num_frames(); 
                const int height = global_state.get_map_height();
                const int width = global_state.get_map_width();

                const int team_num = global_state.get_team_num();
                
                std::cout << "Frames: " << frames << " Height: " << height << " Width: " << width << " Team Num: " << team_num << std::endl;
                
                return {frames, height, width};
            }


            // Discuss with mohammad if add_frame is needed.


        private:
            GlobalState global_state;
            PlayerStates player_states;
            std::vector<dtype> observation_data;

        };

template <bool renderable>
    class GoBiggerEnvironment : public BaseEnvironment<renderable> {
        using gameState = GameState<renderable>;
        using Player    = agario::Player<renderable>;
        using Cell      = agario::Cell<renderable>;
        using Pellet    = agario::Pellet<renderable>;
        using Virus     = agario::Virus<renderable>;
        using Food      = agario::Food<renderable>;
        using Super     = BaseEnvironment<renderable>;

    public:
        using dtype = std::uint8_t;
        using observationType = GoBiggerObservation;

        explicit GoBiggerEnvironment(int map_width, int map_height, int frame_limit, int num_agents,
                                     int ticks_per_step, int arena_size, bool pellet_regen, int num_pellets,
                                     int num_viruses, int num_bots, bool reward_type, int c_death = 0,
                                     bool agent_view = false)
            : Super(num_agents, ticks_per_step, arena_size, pellet_regen, num_pellets, num_viruses, num_bots, reward_type),
              observation(map_width, map_height, frame_limit, 0, num_agents),
              last_frame_index(0),
              last_player(nullptr),
              frame_buffer(std::make_shared<FrameBufferObject>(512, 512, false)) {}

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
            // if (frame_index >= 0) // frame skipping
            //   observation.add_frame(player, state, frame_index);

            last_player = &player;
            last_frame_index = frame_index;

            observation.update_global_state(last_frame_index);
        }

        const std::tuple<int, int, int> observation_shape() const {
            return observation.shape();
        }

        const std::vector<GoBiggerObservation> &get_observations() const { return observations; }


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
                renderer->close_program();
                // glfwTerminate();
                // glDeleteProgram(renderer->shader.program);
            #endif
        }

    private:
        int last_frame_index;
        Player *last_player;
        GoBiggerObservation observation;

        std::vector<GoBiggerObservation> observations;



        #ifdef RENDERABLE
            std::unique_ptr<agario::Renderer> renderer;
            std::shared_ptr<FrameBufferObject> frame_buffer;
        #endif
    };

}  // namespace agario::env