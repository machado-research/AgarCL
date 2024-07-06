#pragma once

#include "agario/core/Ball.hpp"
#include "agario/core/Entities.hpp"
#include "agario/core/Player.hpp"

#include <vector>
#include <unordered_map>
#include <iomanip>
#include <memory>
#include <random>

namespace agario {
  class GameConfig {
    public:
      const agario::distance arena_width, arena_height;

      const size_t target_num_pellets;
      const size_t target_num_viruses;
      const bool pellet_regen;

      explicit GameConfig(
        agario::distance w,
        agario::distance h,
        size_t num_pellets,
        size_t num_viruses,
        bool pellet_regen
      ):
        arena_width(w),
        arena_height(h),
        target_num_pellets(num_pellets),
        target_num_viruses(num_viruses),
        pellet_regen(pellet_regen)
      {}
  };

  template<bool renderable>
  class GameState {
  public:
    using PlayerMap = std::unordered_map<agario::pid, std::shared_ptr<agario::Player<renderable>>>;

    PlayerMap players;
    std::vector<agario::Pellet<renderable>> pellets;
    std::vector<agario::Food<renderable>> foods;
    std::vector<agario::Virus<renderable>> viruses;

    std::mt19937_64 rng;
    agario::tick ticks = 0;
    agario::pid next_pid = 0;

    agario::GameConfig config;

    explicit GameState (const agario::GameConfig &c) :
      config(c),
      rng(std::random_device{}())
    { }

    void clear() {
      players.clear();
      pellets.clear();
      foods.clear();
      viruses.clear();
      ticks = 0;
    }
  };

  /* prints out a list of players sorted by mass (i.e. the leaderboard) */
  template<bool r>
  std::ostream &operator<<(std::ostream &os, const GameState<r> &state) {
    std::vector<std::shared_ptr<agario::Player<r>>> players(state.players.size());
    transform(
      // pull data from state.players
      state.players.begin(), state.players.end(),
      // stick it in players
      players.begin(),
      // transform it by grabbing second item
      [](auto &pair){ return pair.second; }
    );

    std::sort(
      players.begin(), players.end(),
      [](const auto p1, const auto p2) { return *p1 > *p2; }
    );

    // print them out in sorted order
    for (unsigned i = 0; i < players.size(); ++i) {
      os << i + 1 << ".\t" << std::setfill(' ') << std::setw(5);
      auto &player = *players[i];
      os <<  player.mass() << "\t" << player << std::endl;
    }
    return os;
  }

}
