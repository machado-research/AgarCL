#pragma once

#include <math.h>
#include<random>
#include "agario/core/types.hpp"

#define CELL_EAT_MARGIN 1.1

namespace agario {

  class Ball {
  public:
    void * quad_node = nullptr;
    int id;


    Ball() = delete;

    int generate_random_id() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 1000);
        return dis(gen);
    }

    explicit Ball(const Location &loc) : x(loc.x), y(loc.y) {}

    Ball(distance x, distance y) : Ball(Location(x, y)) {
      id = generate_random_id();
    }

    virtual distance radius() const = 0;

    virtual agario::mass mass() const = 0;

    distance height() const { return 2 * radius(); }

    distance width() const { return 2 * radius(); }

    bool collides_with(const Ball &other) const {
      auto sqr_rads = pow(std::max(radius(), other.radius()), 2);
      return sqr_rads >= sqr_distance_to(other);
    }

    bool touches(const Ball& other) const {
      return touches_with_margin(other, 0);
    }

    bool touches_with_margin(const Ball &other, const float margin) const {
      auto sqr_rads = pow(radius() + other.radius(), 2);
      return sqr_rads >= sqr_distance_to(other) + margin;
    }

    bool can_eat(const Ball &other) const {
      return mass() > other.mass() * CELL_EAT_MARGIN;
    }

    Location location() const { return Location(x, y); }

    // mass based comparison, useful for sorting and such
    bool operator==(const Ball &other) const { return mass() == other.mass(); }
    bool operator<(const Ball &other) const { return id < other.id; }
    bool operator>(const Ball &other) const { return id > other.id; }

    distance x;
    distance y;

    virtual ~Ball() = default;

  private:

    distance sqr_distance_to(const Ball &other) const {
      return (location() - other.location()).norm_sqr();
    }
  };

  // Virtual inheritance because of
  class MovingBall : virtual public Ball {
  public:
    using Ball::Ball;

    MovingBall() = delete;

    template<typename Loc, typename Vel>
    MovingBall(Loc &&loc, Vel &&vel) : Ball(loc), velocity(vel) {}

    float speed() const { return velocity.speed(); }

    void accelerate(float accel, float dt) {
      velocity.accelerate(accel, dt);
    }

    void decelerate(float decel, float dt) {
      velocity.decelerate(decel, dt);
    }

    virtual void move(float dt) {
      x += velocity.dx * dt;
      y += velocity.dy * dt;
    }

    agario::Velocity velocity;
  };

}
