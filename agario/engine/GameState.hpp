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

  template<bool renderable>
  class GameState {
  public:
    using PlayerMap = std::unordered_map<agario::pid, std::shared_ptr<agario::Player<renderable>>>;

    PlayerMap players;
    std::vector<agario::Pellet<renderable>> pellets;
    std::vector<agario::Food<renderable>> foods;
    std::vector<agario::Virus<renderable>> viruses;

    agario::distance arena_width, arena_height;
    std::mt19937_64 rng;
    agario::tick ticks = 0;
    agario::pid next_pid = 0;

    explicit GameState (agario::distance arena_width, agario::distance arena_height) :
      arena_width(arena_width), arena_height(arena_height),
      rng(std::random_device{}()) { }

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

    // make a sorted list of (pointers to) players
    std::vector<std::shared_ptr<agario::Player<r>>> leaderboard;
    using pp = std::shared_ptr<Player<r>>;
    for (auto &pair : state.players) {
      auto it = std::lower_bound(leaderboard.begin(), leaderboard.end(), pair.second,
                              [&](const pp &p1, const pp &p2) {
                                return *p1 > *p2;
                              });
      leaderboard.insert(it, pair.second);
    }

    // print them out in sorted order
    for (unsigned i = 0; i < leaderboard.size(); ++i) {
      os << i + 1 << ".\t" << std::setfill(' ') << std::setw(5);
      auto &player = *leaderboard[i];
      os <<  player.mass() << "\t" << player << std::endl;
    }
    return os;
  }

}
