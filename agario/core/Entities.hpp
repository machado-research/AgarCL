#pragma once

#include "agario/core/Ball.hpp"
#include "agario/core/types.hpp"
#include "agario/core/utils.hpp"
#include "agario/core/settings.hpp"
#include "agario/core/color.hpp"

#define PELLET_MASS 1
#define FOOD_MASS 10
#define VIRUS_MASS 100

#define PELLET_SIDES 5
#define VIRUS_SIDES 150
#define FOOD_SIDES 7
#define CELL_SIDES 50

#define CELL_EAT_REQUIREMENT 25

namespace agario {

  template<bool renderable, unsigned NumSides = PELLET_SIDES>
  class Pellet : public std::conditional<renderable, RenderableBall<NumSides>, Ball>::type {
  public:
    typedef typename std::conditional<renderable, RenderableBall<PELLET_SIDES>, Ball>::type Super;

    // constructors explicitly put here because of virtual inheritance rules...
    template<typename Loc, bool r = renderable>
    explicit Pellet(Loc &&loc, typename std::enable_if<r>::type* = 0) : Ball(loc), Super(loc) {}
    template<typename Loc, bool r = renderable>
    explicit Pellet(Loc &&loc, typename std::enable_if<!r>::type* = 0) : Ball(loc) {}

    distance radius() const override { return radius_conversion(mass()); }

    agario::mass mass() const override { return PELLET_MASS; }

  private:
  };

  template<bool renderable, unsigned NumSides = FOOD_SIDES>
  class Food : public std::conditional<renderable, RenderableMovingBall<NumSides>, MovingBall>::type {
  public:

    typedef RenderableMovingBall<FOOD_SIDES> RenderableMovingBall;
    typedef typename std::conditional<renderable, RenderableMovingBall, MovingBall>::type Super;

    template<typename Loc, typename Vel>
    Food(Loc &&loc, Vel &&vel) : Ball(loc), Super(loc, vel) {}

    distance radius() const override { return radius_conversion(mass()); }

    agario::mass mass() const override { return FOOD_MASS; }

  private:
  };

  /**
   * This (super-unreadable) struct allows for the `Virus` class to override the
   * _create_vertices virtual method of RenderableBall, which allows it to
   * have a different visual appearance (wavy border)
   */
  template<bool r, unsigned NumSides>
  struct oVirus : virtual public RenderableMovingBall<NumSides> {
    void _create_vertices() override {
      auto num_verts = RenderableMovingBall<NumSides>::NVertices;
      this->circle.verts[0] = 0;
      this->circle.verts[1] = 0;
      this->circle.verts[2] = 0;
      for (unsigned i = 1; i < num_verts; i++) {
        auto radius = 1 + sin(30 * M_PI * i / NumSides) / 15;
        this->circle.verts[i * 3] = radius * cos(i * 2 * M_PI / NumSides);
        this->circle.verts[i * 3 + 1] = radius * sin(i * 2 * M_PI / NumSides);
        this->circle.verts[i * 3 + 2] = 0;
      }
    }
  };

  template<unsigned NumSides>
  struct oVirus<false, NumSides> { };

  template<bool renderable, unsigned NumSides = VIRUS_SIDES>
  class Virus : public std::conditional<renderable, oVirus<renderable, NumSides>, MovingBall>::type {
  public:

    typedef RenderableMovingBall<VIRUS_SIDES> RenderableMovingBall;
    typedef typename std::conditional<renderable, RenderableMovingBall, MovingBall>::type Super;

    // renderable constructor
    template <typename Loc, typename Vel, bool r = renderable>
    Virus(Loc &&loc, Vel &&vel, typename std::enable_if<r>::type* = 0) :
      Ball(loc), RenderableMovingBall(loc, vel) { this->color = agario::color::green; }

    // non-renderable constructor
    template <typename Loc, typename Vel, bool r = renderable>
    Virus(Loc &&loc, Vel &&vel, typename std::enable_if<!r>::type* = 0) :
      Ball(loc), MovingBall(loc, vel) { }

    template <typename Loc>
    explicit Virus(Loc &&loc) : Virus(loc, Velocity()) {}

    distance radius() const override { return radius_conversion(mass()); }

    agario::mass mass() const override { return VIRUS_MASS; }

  private:
  };

  template<bool renderable, unsigned NumSides = CELL_SIDES>
  class Cell : public std::conditional<renderable, RenderableMovingBall<NumSides>, MovingBall>::type {
  public:
    typedef typename std::conditional<renderable, RenderableMovingBall<CELL_SIDES>, MovingBall>::type Super;

    // gotta redeclare all the constructors because of virtual inheritance...
    template<typename Loc, typename Vel>
    Cell(Loc &&loc, Vel &&vel, agario::mass mass) : Ball(loc), Super(loc, vel),
                                                    _mass(mass), _can_recombine(false) {
      set_mass(mass);
      _recombine_timer = std::chrono::steady_clock::now();
    }

    template<typename Loc>
    Cell(Loc &&loc, agario::mass mass) : Cell(loc, Velocity(), mass) {}

    Cell(distance x, distance y, agario::mass mass) : Cell(Location(x, y), Velocity(), mass) {}

    agario::mass mass() const override { return _mass; }

    template<typename T>
    bool can_eat(const T& other) const {
      return Ball::can_eat(other);
    }

    // Enforce only eating others once size is sufficiently high.
    bool can_eat(const Cell& other) const {

      return mass() > CELL_EAT_REQUIREMENT && Ball::can_eat(other);
    }

    distance radius() const override {
      return radius_conversion(mass());
    }

    void move(float dt) override {
      this->x += (this->velocity.dx + splitting_velocity.dx) * dt;
      this->y += (this->velocity.dy + splitting_velocity.dy) * dt;
    }


    void set_mass(agario::mass new_mass) {

      if(CELL_MIN_SIZE >= VIRUS_MASS)
        _mass = new_mass;
      else
        _mass = std::max<agario::mass>(new_mass, CELL_MIN_SIZE);
    }

    void increment_mass(agario::mass inc) { set_mass(mass() + inc); }

    void reduce_mass_by_factor(float factor) { set_mass(mass() / factor); }

    bool can_recombine() {
      if (_can_recombine) return true;
      _can_recombine = std::chrono::steady_clock::now() >= _recombine_timer;
      return _can_recombine;
    }

    void reset_recombine_timer() {
      _recombine_timer = std::chrono::steady_clock::now() +
                         std::chrono::seconds(RECOMBINE_TIMER_SEC);
      _can_recombine = false;
    }
    

    agario::Velocity splitting_velocity;
    agario::real_time _recombine_timer;

  private:
    agario::mass _mass;
    bool _can_recombine;
  };

}
