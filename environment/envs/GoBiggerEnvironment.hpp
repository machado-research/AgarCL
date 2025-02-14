#pragma once

#include <cassert>

#include <algorithms>
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


        private:
            int map_width;
            int map_height;
            int frame_limit;
            int last_frame_count;
            int team_num;
    };

    class PlayerState {
        public:
            explicit PlayerState(int player_id, vecpii food_positions, vecpii thorn_positions, vecpii spore_positions, vecpii clone_positions, string team_name, double score, bool can_eject, bool can_split ):
            player_id(player_id),
            food_positions(food_positions),
            thorn_positions(thorn_positions),
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

            void update_thorn_positions(vecpii thorn_positions) {
                this->thorn_positions = thorn_positions;
            }

            void update_spore_positions(vecpii spore_positions) {
                this->spore_positions = spore_positions;
            }

            void update_clone_positions(vecpii clone_positions) {
                this->clone_positions = clone_positions;
            }
        
        private:
            int player_id;
            vecpii food_positions;
            vecpii thorn_positions;
            vecpii spore_positions;
            vecpii clone_positions;
            string team_name;
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

        private:
            std::unordered_map<int, PlayerState> player_states;
    };

   

    class GoBiggerObservation {

        public:
            explicit GoBiggerObservation(int map_width, int map_height, int frame_limit, int team_num)
            : global_state(map_width, map_height, frame_limit, team_num) {}

            // Update global state
            void update_global_state(int frame_count) {
                global_state.update_last_frame_count(frame_count);
            }

       
             // Update a player's state with the given parameters.
            void update_player_state(int player_id,
                                    const vecpii &food_positions,
                                    const vecpii &thorn_positions,
                                    const vecpii &spore_positions,
                                    const vecpii &clone_positions,
                                    const string &team_name,
                                    double score,
                                    bool can_eject,
                                    bool can_split)
            {
                PlayerState ps(player_id, food_positions, thorn_positions, spore_positions, clone_positions,
                            team_name, score, can_eject, can_split);
                player_states.update_player_state(player_id, ps);
            }

            const GlobalState& get_global_state() const {
                return global_state;
            }

            const PlayerStates& get_player_states() const {
                return player_states;
            }


        private:
            GlobalState global_state;
            PlayerStates player_states;
        };
}