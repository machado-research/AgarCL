#pragma once

#include <string>
#include <algorithm>
#include <vector>

#include "agario/core/types.hpp"
#include "agario/core/Ball.hpp"
#include "agario/core/types.hpp"
#include "agario/core/Entities.hpp"
#include "agario/core/settings.hpp"
#include "agario/core/utils.hpp"

#include <assert.h>
#include <type_traits>

namespace agario {

template<bool renderable>
class Player {

  public:
    typedef Cell<renderable> Cell;

    std::vector<Cell> cells;
    agario::action action;
    Location target;
    agario::tick split_cooldown;
    agario::tick feed_cooldown;
    int num_viruses_eaten;

    Player() = delete;

    Player(agario::pid pid, std::string name, agario::color color):
      action(none),
      target(0, 0),
      split_cooldown(0),
      feed_cooldown(0),
      _pid(pid),
      _name(std::move(name)),
      _score(0),
      _color(color),
      _minMassCell(0),
      num_viruses_eaten(0)
    {}

    Player(agario::pid pid, const std::string &name) : Player(pid, name, random_color()) {}
    explicit Player(const std::string &name) : Player(-1, name) {}
    explicit Player(agario::pid pid) : Player(pid, "unnamed") {}

    agario::color color() const { return _color; }

    // renderable version of add_cell, must set cell color
    template<bool enable = renderable, typename... Args>
    typename std::enable_if<enable, void>::type
    add_cell(Args &&... args) {
      cells.emplace_back(std::forward<Args>(args)...);
      cells.back().color = _color;

    }

    // non-renderable version of add_cell
    template<bool enable = renderable, typename... Args>
    typename std::enable_if<!enable, void>::type
    add_cell(Args &&... args) {
      cells.emplace_back(std::forward<Args>(args)...);
    }

    void kill() {
      cells.clear();
      _minMassCell = CELL_MIN_SIZE;
    }

    bool dead() const { return cells.empty(); }

    void set_min_mass_cell(agario::mass maxMassCell) {
      _minMassCell = maxMassCell;
    }

    agario::mass get_min_mass_cell(){
      return _minMassCell;
    }

    void set_score(score new_score) { _score = new_score; }

    void increment_score(score inc) { _score += inc; }

    agario::distance x() const {
      agario::distance x_ = 0;
      for (auto &cell : cells)
        x_ += cell.x * cell.mass();
      return x_ / mass();
    }

    agario::distance y() const {
      agario::distance y_ = 0;
      for (auto &cell : cells)
        y_ += cell.y * cell.mass();
      return y_ / mass();
    }

    agario::Location location() const {
      return agario::Location(x(), y());
    }

    // Total mass of the player (sum of masses of all cells)
    agario::mass mass() const {
      agario::mass total_mass = 0;
      for (auto &cell : cells)
        total_mass += cell.mass();
      return total_mass;
    }

    agario::score score() const { return mass(); }

    agario::pid pid() const { return _pid; }

    std::string name() const { return _name; }

    bool operator==(const Player &other) const {
      return this->_pid == other.pid();
    }

    bool operator!=(const Player &other) const {
      return !(*this == other);
    }

    bool operator>(const Player &other) const { return mass() > other.mass(); }

    bool operator<(const Player &other) const { return mass() < other.mass(); }

    template <typename T, bool enable = renderable>
    typename std::enable_if<enable, void>::type
    draw(T &shader) {
      for (auto &cell : cells)
        cell.draw(shader);
    }

    /* override this function to define a bot's behavior */
    virtual void take_action(const GameState<renderable> &state) {
      static_cast<void>(state);
    }

    template <bool r = renderable>
    typename std::enable_if<r, void>::type
    add_cells(std::vector<Cell> &new_cells) {
      for (auto &cell : new_cells)
        cell.set_color(color());

      cells.insert(std::end(cells),
                   std::make_move_iterator(new_cells.begin()),
                   std::make_move_iterator(new_cells.end()));
    }

    template <bool r = renderable>
    typename std::enable_if<!r, void>::type
    add_cells(std::vector<Cell> &new_cells) {
      cells.insert(std::end(cells),
                   std::make_move_iterator(new_cells.begin()),
                   std::make_move_iterator(new_cells.end()));
    }

    // virtual destructor because it's polymorphic
    virtual ~Player() = default;
    Player(const Player & /* other */) = default;
    Player &operator=(const Player & /* other */) = default;
    Player(Player && /* other */) noexcept = default;
    Player &operator=(Player && /* other */) noexcept = default;

  private:
    agario::pid _pid;
    std::string _name;
    agario::score _score;
    agario::color _color;
    agario::mass _minMassCell;

  };


  template<bool r>
  std::ostream &operator<<(std::ostream &os, const Player<r> &player) {
    os << player.name() << "(" << player.pid() << ")";
    return os;
  }

}
