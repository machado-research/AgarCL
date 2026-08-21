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

    def test_action_noise_is_covered_by_seed(self):
        """The action noise drew from numpy's global generator, so two runs
        with the same env seed still took different actions: seeded
        experiments with add_noise=True (the default) were not reproducible."""
        def noisy_actions(seed):
            env = make("agario-grid-v0", obs_type="grid", grid_size=16,
                       add_noise=True)
            env.unwrapped.seed(seed)
            env.reset()
            acts = [env.unwrapped._sanitize_actions((np.zeros(2, dtype=np.float32), 0))[0]
                    for _ in range(5)]
            env.close()
            return acts

        np.random.seed(123)
        first = noisy_actions(7)
        np.random.seed(456)   # perturb the global RNG between runs
        second = noisy_actions(7)
        self.assertEqual(first, second,
                         "same env seed produced different noisy actions")
        self.assertNotEqual(noisy_actions(7), noisy_actions(8),
                            "different env seeds produced identical noise")

    def test_invalid_actions_are_rejected(self):
        env = make("agario-grid-v0", obs_type="grid", grid_size=16, add_noise=False)
        env.unwrapped.seed(1)
        env.reset()
        for bad in [(np.array([5.0, 0.0], dtype=np.float32), 0),   # out of bounds
                    (np.array([0.0, 0.0], dtype=np.float32), 9)]:  # invalid discrete
            with self.assertRaises(ValueError):
                env.step(bad)
        env.close()


class VideoRecorderRegressionTest(unittest.TestCase):

    def _record(self, steps, **overrides):
        env = make(obs_type="screen", render_mode="rgb_array",
                   screen_len=32, num_pellets=60, num_bots=1, num_viruses=1,
                   **overrides)
        env.unwrapped.seed(1)
        env.reset()
        env.unwrapped.enable_video_recorder()
        for _ in range(steps):
            env.step((np.zeros(2, dtype=np.float32), 0))
        return env

    def test_recorded_frames_are_2d_rgb_images(self):
        """Screen observations carry a leading frame dimension. The
        non-agent_view path returned that 4-D array unchanged, which sized the
        video as (width, 1) and **crashed the process with a bus error** inside
        cv2.cvtColor."""
        for agent_view in (False, True):
            env = self._record(5, agent_view=agent_view)
            frames = env.unwrapped.video_recorder
            env.close()
            self.assertGreater(len(frames), 0)
            for f in frames:
                self.assertEqual(f.ndim, 3, f"agent_view={agent_view}: frame is not 2-D RGB")
                self.assertEqual(f.shape[2], 3, f"agent_view={agent_view}: not 3 channels")
                self.assertEqual(f.dtype, np.uint8)

    def test_generate_video_writes_file_and_clears_buffer(self):
        """The buffer was never cleared, so a second recording appended to the
        frames already written."""
        import os
        import tempfile
        for agent_view in (False, True):
            env = self._record(8, agent_view=agent_view)
            out = tempfile.mkdtemp()
            env.unwrapped.generate_video(out, "v.avi")
            path = os.path.join(out, "v.avi")
            self.assertTrue(os.path.exists(path), "no video file written")
            self.assertGreater(os.path.getsize(path), 0, "video file is empty")
            self.assertEqual(len(env.unwrapped.video_recorder), 0,
                             "frame buffer was not cleared after writing")
            env.close()

    def test_agent_view_frames_decode_the_channel_encoding(self):
        """The agent-view colouriser assumed an obsolete encoding (background
        alpha 255, pellets ch0 != 255), so its final `alpha <= 30` rule painted
        the whole frame near-black: videos showed a blue dot on darkness.
        Assert the palette rows are applied per the actual encoding produced by
        ScreenObservation::post_processing_frame_data."""
        # 128x128 (the paper's resolution): at the 32x32 used elsewhere for
        # speed, cells span less than a pixel and rasterise to nothing.
        env = make(obs_type="screen", render_mode="rgb_array",
                   screen_len=128, num_pellets=60, num_bots=1, num_viruses=1,
                   agent_view=True)
        env.unwrapped.seed(1)
        env.reset()
        env.unwrapped.enable_video_recorder()
        for _ in range(10):
            obs, *_ = env.step((np.zeros(2, dtype=np.float32), 0))
        obs = np.asarray(obs)[0]   # (frames, H, W, 4) -> (H, W, 4)
        frame = env.unwrapped.video_recorder[-1]
        env.close()

        background = (obs == 0).all(axis=-1)
        pellets = obs[..., 0] == 255
        agent = (obs[..., 3] > 30) & (obs[..., 3] <= 230)

        self.assertGreater(background.sum(), 0, "no background in test scene")
        self.assertGreater(agent.sum(), 0, "main agent not in its own view")
        self.assertTrue((frame[background] == [255, 255, 255]).all(),
                        "background pixels are not white")
        self.assertTrue((frame[agent] == [0, 0, 255]).all(),
                        "main agent pixels are not blue")
        if pellets.sum():
            self.assertTrue((frame[pellets] == [255, 165, 0]).all(),
                            "pellet pixels are not orange")
        # the old bug: near-black [26, 0, 0] dominating the frame
        dark = (frame == [26, 0, 0]).all(axis=-1).mean()
        self.assertEqual(dark, 0.0, "obsolete grid colour present in frame")

    def test_frame_buffer_is_bounded(self):
        """Frames are held in memory until generate_video() is called; uncapped
        this grows without bound (~49 KB per step at 128x128)."""
        import warnings as w
        env = make(obs_type="screen", render_mode="rgb_array", screen_len=32,
                   num_pellets=40, num_bots=0, num_viruses=0, agent_view=False)
        env.unwrapped.seed(1)
        env.reset()
        env.unwrapped.enable_video_recorder(max_frames=6)
        with w.catch_warnings(record=True) as caught:
            w.simplefilter("always")
            for _ in range(20):
                env.step((np.zeros(2, dtype=np.float32), 0))
            runtime_warnings = [c for c in caught if issubclass(c.category, RuntimeWarning)]
        env.close()
        self.assertEqual(len(env.unwrapped.video_recorder), 6,
                         "frame buffer exceeded its cap")
        self.assertEqual(len(runtime_warnings), 1,
                         "expected exactly one warning when the cap is reached")

    def test_default_fps_is_real_time(self):
        """fps was hardcoded to 60 while one step advances ticks_per_step ticks
        of 1/30 s, so playback ran about eight times too fast at the default.
        Verified by reading the written file back rather than by patching cv2."""
        import os
        import tempfile

        import cv2

        env = self._record(6, agent_view=False, ticks_per_step=4)
        out = tempfile.mkdtemp()
        env.unwrapped.generate_video(out, "v.avi")
        env.close()

        cap = cv2.VideoCapture(os.path.join(out, "v.avi"))
        try:
            self.assertTrue(cap.isOpened(), "could not reopen the written video")
            fps = cap.get(cv2.CAP_PROP_FPS)
            width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
            height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        finally:
            cap.release()

        # 4 ticks per step at 1/30 s each => 7.5 frames per second of game time
        self.assertAlmostEqual(fps, 30.0 / 4, places=2,
                              msg="video frame rate is not real time")
        self.assertEqual((width, height), (32, 32),
                         "video dimensions do not match the frame size")


class DefaultsRegressionTest(unittest.TestCase):

    def test_default_observation_type_is_screen(self):
        from gym_agario.AgarioEnv import AgarioEnv
        import inspect
        default = inspect.signature(AgarioEnv.__init__).parameters["obs_type"].default
        self.assertEqual(default, "screen")


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
