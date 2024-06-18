#pragma once

#include <gtest/gtest.h>
#include <environment/envs/GridEnvironment.hpp>

#include <environment/renderable.hpp>

using namespace agario::env;

namespace {

  using GridEnvironment = agario::env::GridEnvironment<int, renderable>;
  using dtype = GridEnvironment::dtype;
  using Observation = GridEnvironment::Observation;

  /* checks to make sure that the observation is not zero-filled */
  bool has_non_zero(const Observation &obs) {
    bool found = false;
    const dtype *data = obs.data();
    for (int i = 0; i < obs.length(); i++) {
      if (data[i] != 0)
        found = true;
    }
    return found;
  }

  /* Constructor tests */
  TEST(GridEnvTest, NumAgents) {
    for (int num_agents = 0; num_agents < 10; num_agents++) {
      GridEnvironment env(num_agents, 1, 0, false, 0, 0, 0);
      ASSERT_EQ(num_agents, env.num_agents()) << "Number of agents didn't match";
    }
  }

  TEST(GridEnvTest, TicksPerStep) {
    for (int ticks_per_step = 0; ticks_per_step < 10; ticks_per_step++) {
      GridEnvironment env(4, ticks_per_step, 0, false, 0, 0, 0);
      ASSERT_EQ(ticks_per_step, env.ticks_per_step()) << "Ticks per step didn't match";
    }
  }

  /* make sure that the observation shape is correct */
  TEST(GridEnvTest, ObservationShape) {
    GridEnvironment env(4, 4, 1000, false,
      0, 0, 0);

    for (int num_frames = 0; num_frames <  4; num_frames++)
      for (int grid_size = 0; grid_size <  4; grid_size++)
        for (bool observe_cells : { false, true })
          for (bool observe_others : { false, true })
            for (bool observe_viruses : { false, true })
              for (bool observe_pellets : { false, true })
              {
                env.configure_observation(num_frames, grid_size, observe_cells, observe_others, observe_viruses, observe_pellets, false, 0);

                int channels, width, height;
                std::tie(channels, width, height) = env.observation_shape();

                ASSERT_EQ(width, grid_size) << "observation width didn't match grid_size";
                ASSERT_EQ(height, grid_size) << "observation height didn't match grid_size";

                int channels_per_frame = 1; // OOB
                if (observe_cells) channels_per_frame++;
                if (observe_others) channels_per_frame++;
                if (observe_viruses) channels_per_frame++;
                if (observe_pellets) channels_per_frame++;

                auto expected_channels = num_frames * channels_per_frame;
                ASSERT_EQ(channels, expected_channels) << "wrong number of channels in observation";

                /* also make sure that the observation is of the correct size */
                auto &observations = env.get_observations();
                for (auto &o : observations) {

                  ASSERT_EQ(o.shape(), env.observation_shape())
                  << "Observation shape mismatch";

                  auto *data = const_cast<Observation::dtype*>(o.data());

                  // fill it with zeros to test
                  std::fill(data, data + o.length(), 0);
                }
              }
  }

  /* ===================== Fixture Tests ===================== */

  class EnvTest : public testing::Test {
  protected:
    EnvTest() : env(nullptr) { }
    void SetUp(int num_agents=4, int ticks_per_step=4, int arena_size=1000,
               bool pellet_regen=true,
               int num_pellets=1000, int num_viruses=25, int num_bots=25) {

      env = new GridEnvironment(num_agents, ticks_per_step, arena_size,
                                pellet_regen, num_pellets,
                                num_viruses, num_bots);

      env->configure_observation(2, 128, true, true, true, true, false, 0);
    }

    template <typename ...Config>
    bool configure_observation(Config&&... config) {
      env->configure_observation(config...);
    }

    void TearDown() override { delete env; }
    GridEnvironment *env;
  };

  /* just take actions for all agents */
  TEST_F(EnvTest, TakeActions) {
    SetUp();
    std::vector<Action> actions;
    actions.reserve(env->num_agents());

    for (int i = 0; i < env->num_agents(); i++)
      actions.emplace_back(0.0, 0.0, agario::action::none);
    EXPECT_NO_THROW(    env->take_actions(actions)) << "failure to take actions";
  }

  /* make sure that it throws when number of actions != number of agents */
  TEST_F(EnvTest, TakeActionsWrongSize) {
    SetUp();
    std::vector<Action> actions;
    actions.reserve(env->num_agents());

    EXPECT_THROW(env->take_actions(actions), EnvironmentException);

    for (int a = 1; a <= 1 + env->num_agents(); a++) {
      actions.emplace_back(0.0, 0.0, agario::action::none);
      if (a != env->num_agents())
        EXPECT_THROW(env->take_actions(actions), EnvironmentException);
      else
        EXPECT_NO_THROW(env->take_actions(actions));
    }
  }

  TEST_F(EnvTest, Reset) {
    SetUp();
    EXPECT_NO_THROW(env->reset());
  }

  TEST_F(EnvTest, Step) {
    SetUp();

    // make list of actions
    std::vector<Action> actions;
    actions.reserve(env->num_agents());
    for (int i = 0; i < env->num_agents(); i++)
      actions.emplace_back(0.0, 0.0, agario::action::none);

    for (int i = 0; i < 10; i++) {
      ASSERT_NO_THROW(env->take_actions(actions));
      auto rewards = env->step();
      ASSERT_EQ(rewards.size(), env->num_agents());
    }
  }

  TEST_F(EnvTest, GetState) {
    SetUp();
    std::vector<Action> actions;
    actions.reserve(env->num_agents());
    for (int i = 0; i < env->num_agents(); i++)
      actions.emplace_back(0.0, 0.0, agario::action::none);

    for (int i = 0; i < 10; i++) {
      ASSERT_NO_THROW(env->take_actions(actions));
      auto _ = env->step();

      auto &observations = env->get_observations();
      ASSERT_EQ(observations.size(), env->num_agents());
      for (auto &o : observations)
        ASSERT_TRUE(has_non_zero(o));
    }
  }

  /* ===================== Rendering Tests ===================== */
  TEST_F(EnvTest, Render) {
    SetUp();
    ASSERT_NO_THROW(env->render());
  }
  TEST_F(EnvTest, close) {
    SetUp();
    ASSERT_NO_THROW(env->close());
  }
}

