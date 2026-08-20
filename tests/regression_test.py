#!/usr/bin/env python
"""
Regression tests for previously-fixed defects in the Python-facing layer.

Each test names the behaviour that regressed, so a future failure explains
itself. These exercise the gym wrapper and the C++ bindings together, covering
ground the C++ suites cannot reach (observation delivery, reward shaping,
episode flags, and the headless render path).
"""
import unittest

import numpy as np
import gymnasium as gym

import gym_agario  # noqa: F401  (registers the agario-* environments)


def make(env_id="agario-screen-v0", **overrides):
    """Small, fast environment; overrides win."""
    config = dict(
        ticks_per_step=4,
        arena_size=300,
        num_pellets=150,
        num_viruses=4,
        num_bots=2,
        pellet_regen=True,
        reward_type=1,
        num_agents=1,
        c_death=0,
        add_noise=False,
        mode=0,
        env_type=1,
    )
    config.update(overrides)
    return gym.make(env_id, **config)


def rollout(env, steps, action=None, seed=0):
    """Steps the env, returning (last_observation, rewards, terminateds, truncateds)."""
    rng = np.random.default_rng(seed)
    obs = rewards = None
    terms, truncs = [], []
    for _ in range(steps):
        act = action if action is not None else (
            rng.uniform(-1, 1, 2).astype(np.float32), int(rng.integers(0, 3)))
        obs, reward, terminated, truncated, _ = env.step(act)
        rewards = reward
        terms.append(terminated)
        truncs.append(truncated)
    return obs, rewards, terms, truncs


class ObservationRegressionTest(unittest.TestCase):

    def test_grid_observations_are_not_all_zero(self):
        """The grid observation returned an all-zero array on every step: the
        destination frame index was computed from a tick index that is always
        0, evaluating negative, so the game state was never copied into the
        buffer that had just been cleared."""
        env = make("agario-grid-v0", obs_type="grid", grid_size=32, num_frames=1)
        env.unwrapped.seed(1)
        env.reset()
        total_nonzero = 0
        for _ in range(30):
            obs, *_ = env.step((np.zeros(2, dtype=np.float32), 0))
            total_nonzero += int(np.count_nonzero(np.asarray(obs)))
        env.close()
        self.assertGreater(total_nonzero, 0,
                           "grid observations contained no non-zero values")

    def test_grid_env_honours_caller_configuration(self):
        """configure_observation was called with `kwargs | grid_defaults`, whose
        right-hand precedence let the defaults override every caller-supplied
        value, so grid_size and the observe_* flags were ignored."""
        env = make("agario-grid-v0", obs_type="grid", grid_size=24, num_frames=1)
        env.unwrapped.seed(1)
        obs, _ = env.reset()
        env.close()
        self.assertEqual(np.asarray(obs).shape[:2], (24, 24),
                         "requested grid_size was ignored")

    def test_screen_observation_has_rendered_content(self):
        """Observation capture read a hidden window's back buffer instead of the
        framebuffer object, which is undefined for occluded/headless surfaces
        and could yield a blank frame."""
        env = make(obs_type="screen", render_mode="rgb_array",
                   screen_len=64, agent_view=False)
        env.unwrapped.seed(1)
        env.reset()
        obs, *_ = rollout(env, 20)
        env.close()
        frame = np.asarray(obs)[0]
        distinct = len(np.unique(frame.reshape(-1, frame.shape[-1]), axis=0))
        self.assertGreaterEqual(distinct, 3,
                                f"screen observation looks blank ({distinct} distinct colors)")

    def test_agent_view_alpha_channel_is_populated(self):
        """agent_view encodes the main agent and grid lines in the alpha
        channel. A headless EGL config without alpha bits returns 255
        everywhere, silently destroying that encoding."""
        env = make(obs_type="screen", render_mode="rgb_array",
                   screen_len=64, agent_view=True)
        env.unwrapped.seed(1)
        env.reset()
        obs, *_ = rollout(env, 20)
        env.close()
        frame = np.asarray(obs)[0]
        self.assertEqual(frame.shape[-1], 4, "agent_view should produce 4 channels")
        self.assertGreater(len(np.unique(frame[..., 3])), 1,
                           "alpha channel is constant: the encoding is lost")

    def test_observations_are_always_finite(self):
        """A dead player's centroid divided by zero mass, producing NaN
        coordinates that propagated into the observation (and casting NaN to
        int is undefined behaviour). Uses a crowded arena so deaths occur."""
        env = make("agario-grid-v0", obs_type="grid", grid_size=16,
                   arena_size=100, num_pellets=40, num_bots=8, num_viruses=0)
        env.unwrapped.seed(3)
        env.reset()
        for _ in range(600):
            obs, reward, *_ = env.step((np.zeros(2, dtype=np.float32), 0))
            arr = np.asarray(obs)
            self.assertTrue(np.isfinite(arr).all(), "observation contained NaN/inf")
            self.assertTrue(np.isfinite(float(reward)), "reward was NaN/inf")
            self.assertGreaterEqual(arr.min(), -1, "observation below its declared minimum")
        env.close()


class RewardRegressionTest(unittest.TestCase):

    def test_c_death_penalises_death(self):
        """c_death was dead four ways: the flag gating it was never set, the
        sign made it a bonus, one environment discarded it, and the observation
        hooks zeroed it before the reward was computed."""
        def total(c_death):
            env = make("agario-grid-v0", obs_type="grid", grid_size=16,
                       arena_size=100, num_pellets=30, num_bots=8, num_viruses=0,
                       c_death=c_death, reward_type=1)
            env.unwrapped.seed(11)
            env.reset()
            rewards = [env.step((np.zeros(2, dtype=np.float32), 0))[1]
                       for _ in range(400)]
            env.close()
            return sum(rewards), min(rewards)

        penalty = 100
        base_sum, base_min = total(0)
        pen_sum, pen_min = total(penalty)

        self.assertLess(pen_sum, base_sum, "c_death did not reduce total reward")

        # The penalty must appear at full magnitude on a death step. Note it is
        # exactly -penalty rather than -penalty-something: an agent that dies at
        # spawn mass has a zero mass difference (25 -> 0 -> respawn 25), so
        # without c_death a death step scores 0 and carries no death signal at
        # all. That is precisely why this parameter matters.
        self.assertLessEqual(pen_min, -penalty + 1e-6,
                             "c_death was not applied on death steps")

        # c_death changes only the reward, never the dynamics, so with the same
        # seed and actions the two runs follow identical trajectories and the
        # totals must differ by exactly (number of deaths) * penalty.
        diff = base_sum - pen_sum
        self.assertGreater(diff, 0)
        self.assertAlmostEqual(diff % penalty, 0.0, places=5,
                               msg=f"reward difference {diff} is not a multiple of "
                                   f"c_death={penalty}: the penalty is misapplied or "
                                   f"c_death altered the trajectory")

    def test_reward_count_matches_agent_count(self):
        env = make("agario-grid-v0", obs_type="grid", grid_size=16)
        env.unwrapped.seed(1)
        env.reset()
        _, reward, *_ = env.step((np.zeros(2, dtype=np.float32), 0))
        env.close()
        self.assertIsInstance(float(reward), float)


class EpisodeFlagRegressionTest(unittest.TestCase):

    def test_step_limit_is_truncation_not_termination(self):
        """The step limit set terminated=True and left the computed truncation
        unused, which breaks value bootstrapping at episode boundaries for any
        algorithm that respects the Gymnasium contract."""
        limit = 5
        env = make("agario-grid-v0", obs_type="grid", grid_size=16,
                   env_type=0, number_steps=limit, num_bots=0, num_viruses=0)
        env.unwrapped.seed(1)
        env.reset()
        _, _, terms, truncs = rollout(env, limit + 2,
                                      action=(np.zeros(2, dtype=np.float32), 0))
        env.close()
        self.assertTrue(truncs[-1], "step limit did not set truncated")
        self.assertFalse(terms[-1], "step limit incorrectly set terminated")

    def test_continuing_mode_never_terminates(self):
        """mode 0 with env_type=1 is the continual task: it must not end."""
        env = make("agario-grid-v0", obs_type="grid", grid_size=16, env_type=1)
        env.unwrapped.seed(1)
        env.reset()
        _, _, terms, truncs = rollout(env, 60,
                                      action=(np.zeros(2, dtype=np.float32), 0))
        env.close()
        self.assertFalse(any(terms), "continuing environment reported termination")
        self.assertFalse(any(truncs), "continuing environment reported truncation")


class ReproducibilityRegressionTest(unittest.TestCase):

    def test_same_seed_gives_identical_rewards_and_pixels(self):
        """Three separate causes broke seeded reproducibility: reset_state
        reseeded from random_device, bots drew from the global C RNG, and both
        collision resolution and bot target selection depended on
        unordered_map iteration order."""
        def run():
            env = make(obs_type="screen", render_mode="rgb_array",
                       screen_len=64, agent_view=False,
                       num_pellets=200, num_bots=6, num_viruses=4)
            env.unwrapped.seed(99)
            env.reset()
            rewards = []
            for _ in range(100):
                obs, reward, *_ = env.step((np.zeros(2, dtype=np.float32), 0))
                rewards.append(reward)
            frame = np.asarray(obs).copy()
            env.close()
            return rewards, frame

        r1, f1 = run()
        r2, f2 = run()
        self.assertEqual(r1, r2, "identical seeds produced different reward streams")
        self.assertTrue(np.array_equal(f1, f2),
                        "identical seeds produced different observations")

    def test_seed_before_reset_controls_world_generation(self):
        """World generation draws from the engine's RNG during reset(), so
        seeding must take effect for the generated world, not just for later
        steps."""
        def first_frame(seed):
            env = make(obs_type="screen", render_mode="rgb_array",
                       screen_len=64, agent_view=False, num_bots=0, num_viruses=0)
            env.unwrapped.seed(seed)
            obs, _ = env.reset()
            frame = np.asarray(obs).copy()
            env.close()
            return frame

        self.assertTrue(np.array_equal(first_frame(7), first_frame(7)),
                        "same seed produced different initial worlds")
        self.assertFalse(np.array_equal(first_frame(7), first_frame(8)),
                         "different seeds produced identical initial worlds")


class ActionRegressionTest(unittest.TestCase):

    def test_action_noise_reaches_the_environment(self):
        """The noisy action was computed into a rebound loop variable, so it was
        validated and then discarded: the environment always received the
        original action."""
        env = make("agario-grid-v0", obs_type="grid", grid_size=16, add_noise=True)
        env.unwrapped.seed(1)
        env.reset()
        np.random.seed(0)
        noisy = env.unwrapped._sanitize_actions(
            (np.array([0.0, 0.0], dtype=np.float32), 0))
        env.close()
        dx, dy, _ = noisy[0]
        self.assertNotEqual((dx, dy), (0.0, 0.0),
                            "add_noise did not perturb the action")

    def test_invalid_actions_are_rejected(self):
        env = make("agario-grid-v0", obs_type="grid", grid_size=16, add_noise=False)
        env.unwrapped.seed(1)
        env.reset()
        for bad in [(np.array([5.0, 0.0], dtype=np.float32), 0),   # out of bounds
                    (np.array([0.0, 0.0], dtype=np.float32), 9)]:  # invalid discrete
            with self.assertRaises(ValueError):
                env.step(bad)
        env.close()


class LifetimeRegressionTest(unittest.TestCase):

    def test_multiple_environments_coexist_in_one_process(self):
        """Closing one environment called the process-global glfwTerminate(),
        destroying every other environment's GL context and aborting the
        process."""
        envs = []
        for i in range(3):
            env = make(obs_type="screen", render_mode="rgb_array",
                       screen_len=32, agent_view=False,
                       num_pellets=40, num_bots=1, num_viruses=1)
            env.unwrapped.seed(i)
            env.reset()
            envs.append(env)

        for _ in range(5):
            for env in envs:
                env.step((np.zeros(2, dtype=np.float32), 0))

        # closing one must leave the others usable
        envs[0].close()
        for env in envs[1:]:
            obs, *_ = env.step((np.zeros(2, dtype=np.float32), 0))
            self.assertTrue(np.isfinite(np.asarray(obs)).all())
        for env in envs[1:]:
            env.close()


if __name__ == "__main__":
    unittest.main()
