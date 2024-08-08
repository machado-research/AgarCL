#pragma once

#include <gtest/gtest.h>

#include <agario/engine/Engine.hpp>
#include <agario/test/renderable.hpp>

namespace {

  /* =========== Basic Stuff =========== */

  class EngineTest : public testing::Test {
  protected:
    EngineTest() : engine() {}

    void SetUp() override { }
    void TearDown() override {}

    agario::Engine<renderable> engine;
  };

  TEST_F(EngineTest, Construct) {
    SetUp();
    EXPECT_TRUE(true);
  }

  TEST_F(EngineTest, Dimensions) {
    SetUp();

    EXPECT_EQ(engine.arena_width(),  DEFAULT_ARENA_WIDTH)  << "Default arena width not respected";
    EXPECT_EQ(engine.arena_height(), DEFAULT_ARENA_HEIGHT) << "Default arena height not respected";
  }

  TEST(Engine, CustomDimensions) {
    agario::distance width = 169;
    agario::distance height = 128;
    agario::Engine<renderable> engine(width, height);

    EXPECT_EQ(engine.arena_width(),  width)  << "Arena width not respected";
    EXPECT_EQ(engine.arena_height(), height) << "Arena height not respected";
  }

  TEST_F(EngineTest, TickCounter) {
    SetUp();
    agario::time_delta dt(0.1);
    for (int i = 0; i < 100; i++) {
      EXPECT_EQ(engine.ticks(), i) << "Incorrect tick counter";
      engine.tick(dt);
    }
  }

  TEST_F(EngineTest, ConsistentState) {
    SetUp();

    // make sure const and non-const accessors reference same state object
    EXPECT_EQ((void*) &engine.get_game_state(), (void*) &engine.game_state());

    auto &state = engine.get_game_state();
    EXPECT_EQ(engine.arena_width(), state.config.arena_width) << "Engine/state arena width mismatch";
    EXPECT_EQ(engine.arena_height(), state.config.arena_height) << "Engine/state arena height mismatch";

    // make sure that state accessors reference same objects as the state
    EXPECT_EQ((void*) &engine.players(), (void*) &state.players);
    EXPECT_EQ((void*) &engine.pellets(), (void*) &state.pellets);
    EXPECT_EQ((void*) &engine.foods(),   (void*) &state.foods);
    EXPECT_EQ((void*) &engine.viruses(), (void*) &state.viruses);
  }

  TEST_F(EngineTest, ConsistentCounts) {
    SetUp();
    engine.reset();

    EXPECT_EQ(engine.player_count(), engine.players().size()) << "Inconsistent player count";
    EXPECT_EQ(engine.pellet_count(), engine.pellets().size()) << "Inconsistent pellet count";
    EXPECT_EQ(engine.food_count(), engine.foods().size()) << "Inconsistent food count";
    EXPECT_EQ(engine.virus_count(), engine.viruses().size()) << "Inconsistent virus count";
  }

  TEST_F(EngineTest, DefaultEntityCounts) {
    SetUp();
    engine.reset();

    EXPECT_EQ(engine.player_count(), 0ul) << "Initialized game with players";
    EXPECT_EQ(engine.virus_count(), DEFAULT_NUM_VIRUSES) << "Wrong initial virus count";
    EXPECT_EQ(engine.pellet_count(), DEFAULT_NUM_PELLETS) << "Wrong initial pellet count";
    EXPECT_EQ(engine.food_count(), 0ul) << "Wrong initial number of foods";
    EXPECT_EQ(engine.pellet_regen(), true) << "Pellet regeneration isn't default ON";
  }

  TEST(Engine, CustomEntityCounts) {
    int num_pellets = 138;
    int num_viruses = 42;
    int pellet_regen = false;

    agario::Engine<renderable> engine(128, 128, num_pellets=num_pellets,
                                                num_viruses=num_viruses,
                                                pellet_regen=pellet_regen);
    engine.reset();

    EXPECT_EQ(engine.player_count(), 0ul) << "Initialized game with players";
    EXPECT_EQ(engine.virus_count(), num_viruses) << "Wrong initial virus count";
    EXPECT_EQ(engine.pellet_count(), num_pellets) << "Wrong initial pellet count";
    EXPECT_EQ(engine.food_count(), 0ul) << "Wrong initial number of foods";
    EXPECT_EQ(engine.pellet_regen(), pellet_regen) << "Pellet regeneration can't be set off";
  }

  TEST_F(EngineTest, Reset) {
    SetUp();
    agario::time_delta dt(0.1);
    for (int i = 0; i < 100; i++)
      engine.tick(dt);

    engine.reset();

    EXPECT_EQ(engine.player_count(), 0ul) << "Initialized game with players";
    EXPECT_EQ(engine.virus_count(), DEFAULT_NUM_VIRUSES) << "Wrong initial virus count";
    EXPECT_EQ(engine.pellet_count(), DEFAULT_NUM_PELLETS) << "Wrong initial pellet count";
    EXPECT_EQ(engine.food_count(), 0ul) << "Wrong initial number of foods";
  }

  TEST_F(EngineTest, AddPlayers) {
    SetUp();

    using Player = agario::Player<renderable>;

    // add some players, make sure they're in the state
    for (unsigned long i = 1; i < 10; i++) {
      std::string name("TestPlayer", i);
      auto pid = engine.add_player<Player>(name);

      ASSERT_EQ(engine.player_count(), i) << "Player count not incremented";

      // const and non-const player accessor must reference to same object
      ASSERT_EQ((void*) &engine.player(pid), (void*) &engine.get_player(pid));

      auto &player = engine.get_player(pid);
      ASSERT_EQ(name, player.name()) << "Player name not recorded";
      ASSERT_EQ(pid, player.pid()) << "Inconsistent player ID";
      ASSERT_EQ(player.cells.size(), 1ul) << "Player does not spawned with single cell";

      // make sure the player is in-bounds
      ASSERT_GE(player.location().x, 0) << "Player x out of bounds left";
      ASSERT_LE(player.location().x, engine.arena_width()) << "Player x out of bounds right";

      ASSERT_GE(player.location().y, 0) << "Player y out of bounds bottom";
      ASSERT_LE(player.location().y, engine.arena_height()) << "Player x out of bounds top";
    }
  }


  /* =========== Game Mechanics =========== */


  TEST_F(EngineTest, PlayesrMove) {
    SetUp();
    engine.reset();
    using Player = agario::Player<renderable>;

    agario::time_delta dt(0.1);

    std::map<agario::pid, agario::Location> map;

    // add some players with random velocities
    for (unsigned long i = 1; i < 10; i++) {
      auto pid = engine.add_player<Player>("TestPlayer");
      auto &player = engine.player(pid);

      player.target = engine.random_location();

      map[pid] = player.location();
    }

    engine.tick(dt);

    // make sure that all players moved accordingly
    for (auto &pair : engine.players()) {
      auto &player = *pair.second;
      agario::Location loc = map[player.pid()];
      agario::Velocity vel = player.cells.front().velocity;

      // the expected location
      agario::Location expected = { loc.x + vel.dx * dt.count(),
                                    loc.y + vel.dy * dt.count() };

      // clamp it to be in-bounds
      expected.x = std::min<float>(std::max<float>(0.0, expected.x), engine.arena_width());
      expected.y = std::min<float>(std::max<float>(0.0, expected.y), engine.arena_height());

      ASSERT_FLOAT_EQ(player.x(), expected.x)
        << "x position did not move according to player velocity";

      ASSERT_FLOAT_EQ(player.y(), expected.y)
        << "y position did not move according to player velocity";
    }
  }

  // todo: more trixy tests

}
