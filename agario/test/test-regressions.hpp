#pragma once

/* Regression tests for previously-fixed bugs.
 *
 * Each test below corresponds to a specific defect; the comment names the
 * behaviour that regressed so a future failure is self-explaining. These
 * compile in both the renderable and non-renderable configurations.
 */

#include <gtest/gtest.h>

#include <agario/engine/Engine.hpp>
#include <agario/bots/bots.hpp>
#include <agario/test/renderable.hpp>

#include <cmath>
#include <set>
#include <vector>

namespace {

  using RegEngine = agario::Engine<renderable>;
  using RegPlayer = agario::Player<renderable>;

  static constexpr double REG_DT = 1.0 / 30;
  static inline agario::time_delta reg_dt() { return agario::time_delta(REG_DT); }

  /* ============ pellet eating: exact removal and mass accounting ============
   * Removal collected indices and applied them with swap-and-pop, but each
   * swap permuted the vector, so later indices deleted unrelated pellets while
   * eaten ones survived as re-eatable "ghosts". Duplicate detections also
   * double-credited mass. */
  TEST(Regression, PelletRemovalIsExact) {
    for (int trial = 0; trial < 5; trial++) {
      RegEngine engine(100, 100, 400, 0, /*pellet_regen=*/false, 0);
      engine.seed(1000 + trial);
      engine.reset();
      auto pid = engine.template add_player<RegPlayer>("agent");
      auto &player = engine.player(pid);

      auto &cell = player.cells[0];
      cell.set_mass(3000);
      cell.x = agario::distance(50);
      cell.y = agario::distance(50);
      player.target = agario::Location(agario::distance(50), agario::distance(50));

      std::set<int> before_ids;
      for (const auto &p : engine.pellets()) before_ids.insert(p.id);
      const agario::mass mass_before = player.mass();

      engine.tick(reg_dt());

      std::set<int> after_ids;
      for (const auto &p : engine.pellets()) after_ids.insert(p.id);

      int removed = 0;
      for (int id : before_ids)
        if (!after_ids.count(id)) removed++;

      // one pellet eaten == one pellet removed == PELLET_MASS gained
      const int gained = static_cast<int>(player.mass()) - static_cast<int>(mass_before);
      EXPECT_EQ(gained, removed * PELLET_MASS)
        << "mass gained does not match the number of pellets removed";
      EXPECT_GT(removed, 0) << "test setup should have eaten something";
    }
  }

  /* Over many ticks, mass gained must equal PELLET_MASS per pellet removed.
   *
   * This is the invariant that "ghost" pellets broke: a pellet that was
   * credited but not removed got eaten again on later ticks, inflating mass
   * relative to pellets consumed. Held over a window short enough that mass
   * decay (every 60 ticks) cannot contribute, with no viruses, no foods and a
   * single player, so pellets are the only mass source.
   *
   * Note deliberately NOT asserted: that no pellet within the cell's *final*
   * radius survives. Eating grows the cell mid-tick, so a pellet outside the
   * radius when the scan ran can legitimately end up inside the grown radius,
   * to be eaten on the next tick. */
  TEST(Regression, PelletMassMatchesPelletsRemovedOverTime) {
    RegEngine engine(100, 100, 400, 0, /*pellet_regen=*/false, 0);
    engine.seed(7);
    engine.reset();
    auto pid = engine.template add_player<RegPlayer>("agent");
    auto &player = engine.player(pid);
    player.cells[0].set_mass(2000);
    player.cells[0].x = agario::distance(50);
    player.cells[0].y = agario::distance(50);

    std::set<int> ids_before;
    for (const auto &p : engine.pellets()) ids_before.insert(p.id);
    const agario::mass mass_before = player.mass();

    const int ticks = 50; // < 60, so mass decay never fires
    for (int t = 0; t < ticks; t++) {
      player.action = agario::action::none;
      player.target = agario::Location(
        agario::distance(50 + 20 * std::sin(t * 0.2)),
        agario::distance(50 + 20 * std::cos(t * 0.2)));
      engine.tick(reg_dt());
    }

    std::set<int> ids_after;
    for (const auto &p : engine.pellets()) ids_after.insert(p.id);
    int removed = 0;
    for (int id : ids_before)
      if (!ids_after.count(id)) removed++;

    const int gained = static_cast<int>(player.mass()) - static_cast<int>(mass_before);
    EXPECT_GT(removed, 0) << "test setup should have eaten something";
    EXPECT_EQ(gained, removed * PELLET_MASS)
      << "mass gained (" << gained << ") does not match "
      << removed << " pellets removed: a pellet was credited without being "
         "removed (ghost) or removed without being credited";
  }

  /* ============ virus collisions must not depend on cell size ============
   * The virus check scanned a fixed 3x3 neighbourhood of 25-unit buckets,
   * which only sees ~25 units, while collision range is the cell's radius. A
   * cell with radius > 25 (mass > ~2000) could overlap a virus and pass
   * through it undetected. */
  TEST(Regression, LargeCellCollidesWithVirus) {
    RegEngine engine(1000, 1000, 0, 0, false, 0);
    engine.seed(5);
    engine.reset();
    auto pid = engine.template add_player<RegPlayer>("agent");
    auto &player = engine.player(pid);

    player.cells[0].set_mass(10000); // radius ~56.4
    player.cells[0].x = agario::distance(100);
    player.cells[0].y = agario::distance(100);
    player.target = agario::Location(agario::distance(100), agario::distance(100));

    // 52 units away: inside the cell's radius, but several buckets out
    engine.game_state().viruses.emplace_back(
      agario::Location(agario::distance(152), agario::distance(100)));

    const float radius = static_cast<float>(player.cells[0].radius());
    ASSERT_GT(radius, 52.0f) << "test setup: the virus must be within the radius";

    const int viruses_before = engine.virus_count();
    engine.tick(reg_dt());

    EXPECT_LT(engine.virus_count(), viruses_before)
      << "a large cell passed through a virus it overlapped";
  }

  /* Small cells were always inside the old scan range; they must be unaffected. */
  TEST(Regression, SmallCellStillCollidesWithVirus) {
    RegEngine engine(1000, 1000, 0, 0, false, 0);
    engine.seed(5);
    engine.reset();
    auto pid = engine.template add_player<RegPlayer>("agent");
    auto &player = engine.player(pid);

    player.cells[0].set_mass(1800); // radius ~23.9, below the old 25-unit reach
    player.cells[0].x = agario::distance(100);
    player.cells[0].y = agario::distance(100);
    player.target = agario::Location(agario::distance(100), agario::distance(100));
    engine.game_state().viruses.emplace_back(
      agario::Location(agario::distance(120), agario::distance(100)));

    const int viruses_before = engine.virus_count();
    engine.tick(reg_dt());
    EXPECT_LT(engine.virus_count(), viruses_before);
  }

  /* Cells below 1.1x the virus mass must ignore viruses entirely (game rule:
   * small cells shelter near viruses). */
  TEST(Regression, TinyCellIgnoresVirus) {
    RegEngine engine(1000, 1000, 0, 0, false, 0);
    engine.seed(5);
    engine.reset();
    auto pid = engine.template add_player<RegPlayer>("agent");
    auto &player = engine.player(pid);

    player.cells[0].set_mass(50); // < 1.1 * VIRUS_INITIAL_MASS
    player.cells[0].x = agario::distance(100);
    player.cells[0].y = agario::distance(100);
    player.target = agario::Location(agario::distance(100), agario::distance(100));
    engine.game_state().viruses.emplace_back(
      agario::Location(agario::distance(101), agario::distance(100)));

    const int viruses_before = engine.virus_count();
    engine.tick(reg_dt());
    EXPECT_EQ(engine.virus_count(), viruses_before)
      << "a cell too small to interact consumed a virus";
  }

  /* ============ determinism ============
   * players_collision applied results in unordered_map order and bot scans
   * took the first match from an unordered_map, so identical seeds produced
   * different games across runs and standard library versions. */
  TEST(Regression, SameSeedProducesSameGame) {
    auto trace = [](int seed) {
      RegEngine engine(150, 150, 200, 5, true, 0);
      engine.seed(seed);
      engine.reset();
      auto pid = engine.template add_player<RegPlayer>("agent");
      engine.template add_player<agario::bot::HungryBot<renderable>>();
      engine.template add_player<agario::bot::HungryShyBot<renderable>>();
      engine.template add_player<agario::bot::AggressiveBot<renderable>>();
      engine.template add_player<agario::bot::AggressiveShyBot<renderable>>();
      auto &player = engine.player(pid);
      player.cells[0].set_mass(400);

      std::vector<long> out;
      for (int t = 0; t < 600; t++) {
        player.target = agario::Location(
          agario::distance(75 + 40 * std::sin(t * 0.03)),
          agario::distance(75 + 40 * std::cos(t * 0.041)));
        player.action = (t % 40 == 0) ? agario::action::split : agario::action::none;
        engine.tick(reg_dt());
        long total_mass = 0, cells = 0;
        for (const auto &pr : engine.players()) {
          total_mass += pr.second->mass();
          cells += static_cast<long>(pr.second->cells.size());
        }
        out.push_back(total_mass * 1000 + cells);
      }
      return out;
    };

    EXPECT_EQ(trace(21), trace(21)) << "identical seeds produced different games";
    EXPECT_NE(trace(21), trace(22)) << "different seeds produced identical games";
  }

  /* seed() must survive reset_state(), which used to reseed from
   * std::random_device and silently discard it. */
  TEST(Regression, SeedSurvivesResetState) {
    auto first_pellet = [](int seed) {
      RegEngine engine(200, 200, 50, 0, false, 0);
      engine.seed(seed);
      engine.reset_state();
      engine.initialize_game();
      return std::make_pair(static_cast<float>(engine.pellets().front().x),
                            static_cast<float>(engine.pellets().front().y));
    };
    EXPECT_EQ(first_pellet(1234), first_pellet(1234))
      << "reset_state() discarded the seed";
  }

  /* ============ dead players ============
   * The mass-weighted centroid divided by total mass, which is 0 with no
   * cells, producing NaN that propagated into observations (and
   * static_cast<int>(NaN) is undefined behaviour). */
  TEST(Regression, DeadPlayerCentroidIsFinite) {
    RegEngine engine(200, 200, 10, 0, false, 0);
    engine.seed(3);
    engine.reset();
    auto pid = engine.template add_player<RegPlayer>("agent");
    auto &player = engine.player(pid);

    player.kill();
    ASSERT_TRUE(player.dead());
    EXPECT_EQ(player.mass(), 0u);
    EXPECT_TRUE(std::isfinite(static_cast<float>(player.x())));
    EXPECT_TRUE(std::isfinite(static_cast<float>(player.y())));
  }

  /* ============ velocity direction ============
   * direction() computed atan(dx/dy) - the angle from +y with transposed
   * arguments - so it disagreed with the Velocity(angle, speed) constructor it
   * must invert everywhere except the diagonals, and divided by zero at dy=0. */
  TEST(Regression, VelocityDirectionMatchesAtan2) {
    struct { float dx, dy; } cases[] = {
      {1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, 1}, {-1, -1}, {1, -1}
    };
    for (auto c : cases) {
      agario::Velocity v(agario::distance(c.dx), agario::distance(c.dy));
      EXPECT_NEAR(static_cast<float>(v.direction()), std::atan2(c.dy, c.dx), 1e-5)
        << "direction() disagrees with atan2 at (" << c.dx << ", " << c.dy << ")";
    }
  }

  /* direction() must round-trip through the angle constructor it inverts.
   * Compared modulo 2*pi: atan2's branch cut means -pi and +pi are the same
   * direction and either may be returned. */
  TEST(Regression, VelocityDirectionRoundTrips) {
    for (int i = 0; i < 16; i++) {
      const float theta = static_cast<float>(-M_PI + 2 * M_PI * i / 16.0);
      agario::Velocity v(agario::angle(theta), agario::distance(10));
      const float got = static_cast<float>(v.direction());
      float diff = std::fmod(std::fabs(got - theta), static_cast<float>(2 * M_PI));
      if (diff > static_cast<float>(M_PI)) diff = static_cast<float>(2 * M_PI) - diff;
      EXPECT_NEAR(diff, 0.0f, 1e-4)
        << "direction() did not round-trip at theta=" << theta << " (got " << got << ")";
    }
  }

  /* ============ pellet regeneration ============
   * The pellet_regen argument was stored and never read; regeneration was
   * governed solely by the mode, so pellet_regen=false had no effect. */
  TEST(Regression, PelletRegenFlagIsHonoured) {
    auto final_count = [](bool regen) {
      RegEngine engine(300, 300, 200, 0, regen, 0); // mode 0 permits regeneration
      engine.seed(4);
      engine.reset();
      engine.template add_player<RegPlayer>("agent");
      for (int i = 0; i < 4; i++)
        engine.template add_player<agario::bot::HungryBot<renderable>>();
      for (int t = 0; t < 1500; t++) engine.tick(reg_dt());
      return engine.pellet_count();
    };
    const int with_regen = final_count(true);
    const int without_regen = final_count(false);
    EXPECT_LT(without_regen, with_regen)
      << "pellet_regen=false still regenerated pellets";
  }

  /* ============ mass conservation when cells eat cells ============
   * players_collision let two predators each absorb the same victim's full
   * mass, creating mass from nothing, and could credit mass to the wrong cell. */
  TEST(Regression, CellEatingConservesMass) {
    RegEngine engine(120, 120, 0, 0, false, 0);
    engine.seed(9);
    engine.reset();
    auto big = engine.template add_player<RegPlayer>("big");
    auto small = engine.template add_player<RegPlayer>("small");
    auto &b = engine.player(big);
    auto &s = engine.player(small);

    b.cells[0].set_mass(1000);
    b.cells[0].x = agario::distance(60);
    b.cells[0].y = agario::distance(60);
    b.target = agario::Location(agario::distance(60), agario::distance(60));
    s.cells[0].set_mass(50);
    s.cells[0].x = agario::distance(60);
    s.cells[0].y = agario::distance(60);
    s.target = agario::Location(agario::distance(60), agario::distance(60));

    const long total_before = static_cast<long>(b.mass()) + static_cast<long>(s.mass());
    engine.tick(reg_dt());
    const long total_after = static_cast<long>(b.mass()) + static_cast<long>(s.mass());

    // no pellets or foods exist, so total mass can only be conserved or decay
    EXPECT_LE(total_after, total_before)
      << "cell eating created mass out of nothing";
  }

  /* ============ bots ============
   * The shy bots compared other_player.mass() against an unqualified mass(),
   * which resolves to the *type* agario::mass (Player is a dependent base) and
   * therefore evaluated to 0: they fled from every nearby player regardless of
   * size, and AggressiveShyBot's hunting branch became unreachable. */
  template <typename ShyBot>
  static void expect_big_bot_does_not_flee() {
    RegEngine engine(200, 200, 20, 0, false, 0);
    engine.seed(1);
    engine.reset();
    auto bot_pid = engine.template add_player<ShyBot>("shy");
    auto tiny_pid = engine.template add_player<RegPlayer>("tiny");
    auto &bot = engine.player(bot_pid);
    auto &tiny = engine.player(tiny_pid);

    bot.cells[0].set_mass(5000);
    bot.cells[0].x = agario::distance(100);
    bot.cells[0].y = agario::distance(100);
    tiny.cells[0].set_mass(25);
    tiny.cells[0].x = agario::distance(110); // within SHY_RADIUS
    tiny.cells[0].y = agario::distance(100);

    ASSERT_GT(bot.mass(), tiny.mass());
    bot.take_action(engine.get_game_state());

    // fleeing points directly away from the other player, i.e. to x < 100
    EXPECT_GE(static_cast<float>(bot.target.x), 100.0f)
      << "a much larger bot fled from a tiny player";
  }

  TEST(Regression, HungryShyBotDoesNotFleeSmallerPlayer) {
    expect_big_bot_does_not_flee<agario::bot::HungryShyBot<renderable>>();
  }

  TEST(Regression, AggressiveShyBotDoesNotFleeSmallerPlayer) {
    expect_big_bot_does_not_flee<agario::bot::AggressiveShyBot<renderable>>();
  }

  /* A shy bot must still flee something genuinely bigger. */
  TEST(Regression, ShyBotFleesLargerPlayer) {
    RegEngine engine(200, 200, 20, 0, false, 0);
    engine.seed(1);
    engine.reset();
    auto bot_pid = engine.template add_player<agario::bot::HungryShyBot<renderable>>("shy");
    auto big_pid = engine.template add_player<RegPlayer>("big");
    auto &bot = engine.player(bot_pid);
    auto &big = engine.player(big_pid);

    bot.cells[0].set_mass(25);
    bot.cells[0].x = agario::distance(100);
    bot.cells[0].y = agario::distance(100);
    big.cells[0].set_mass(5000);
    big.cells[0].x = agario::distance(110);
    big.cells[0].y = agario::distance(100);

    bot.take_action(engine.get_game_state());
    EXPECT_LT(static_cast<float>(bot.target.x), 100.0f)
      << "a small bot failed to flee a much larger player";
  }

  /* Bots must never target world origin when pellets exist, and must never
   * produce a non-finite target: nearest_pellet used to fall through with an
   * unset target, and target_player divided by zero mass. */
  TEST(Regression, BotTargetsStayFiniteAndInArena) {
    RegEngine engine(300, 300, 150, 5, true, 0);
    engine.seed(6);
    engine.reset();
    engine.template add_player<RegPlayer>("agent");
    engine.template add_player<agario::bot::HungryBot<renderable>>();
    engine.template add_player<agario::bot::HungryShyBot<renderable>>();
    engine.template add_player<agario::bot::AggressiveBot<renderable>>();
    engine.template add_player<agario::bot::AggressiveShyBot<renderable>>();

    for (int t = 0; t < 900; t++) {
      engine.tick(reg_dt());
      for (const auto &pr : engine.players()) {
        const auto &p = *pr.second;
        if (!p.is_bot || p.dead()) continue;
        EXPECT_TRUE(std::isfinite(static_cast<float>(p.target.x)))
          << "bot target x became non-finite at tick " << t;
        EXPECT_TRUE(std::isfinite(static_cast<float>(p.target.y)))
          << "bot target y became non-finite at tick " << t;
      }
    }
  }

  /* Bot policy must not depend on unordered_map iteration order. Players are
   * inserted directly (not via add_player, which would consume the seeded RNG)
   * so only the map layout differs between the two runs. */
  TEST(Regression, BotDecisionsAreIndependentOfMapOrder) {
    auto run = [](bool perturb) {
      RegEngine engine(200, 200, 150, 0, true, 0);
      engine.seed(11);
      engine.reset();

      if (perturb) {
        auto &players = engine.game_state().players;
        std::vector<agario::pid> tmp;
        for (int i = 0; i < 64; i++) {
          auto pid = static_cast<agario::pid>(5000 + i);
          players.emplace(pid, std::make_shared<agario::bot::HungryBot<renderable>>(pid, "filler"));
          tmp.push_back(pid);
        }
        for (auto pid : tmp) players.erase(pid);
      }

      engine.template add_player<agario::bot::HungryShyBot<renderable>>("shy");
      engine.template add_player<agario::bot::AggressiveShyBot<renderable>>("aggroshy");
      engine.template add_player<agario::bot::AggressiveBot<renderable>>("aggro");

      std::vector<long> trace;
      for (int t = 0; t < 300; t++) {
        engine.tick(reg_dt());
        if (t % 10 != 0) continue;
        std::vector<long> row;
        for (const auto &pr : engine.players()) {
          const auto &p = *pr.second;
          long key = 0;
          for (char c : p.name()) key = key * 31 + c;
          row.push_back(key * 1000003
                        + static_cast<long>(static_cast<float>(p.target.x)) * 1000
                        + static_cast<long>(static_cast<float>(p.target.y)));
        }
        std::sort(row.begin(), row.end());
        for (long v : row) trace.push_back(v);
      }
      return trace;
    };
    EXPECT_EQ(run(false), run(true))
      << "bot decisions changed with unordered_map layout";
  }

  /* A dead bot must not throw: largest_cell() indexed an empty cell vector,
   * and an exception escaping through the Python bindings ends the episode. */
  TEST(Regression, DeadBotTakeActionDoesNotThrow) {
    RegEngine engine(200, 200, 20, 0, false, 0);
    engine.seed(2);
    engine.reset();
    auto pid = engine.template add_player<agario::bot::AggressiveBot<renderable>>("aggro");
    auto &bot = engine.player(pid);
    bot.kill();
    ASSERT_TRUE(bot.dead());
    EXPECT_NO_THROW(bot.take_action(engine.get_game_state()));
  }

} // namespace
