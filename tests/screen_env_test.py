"""
File: screen_test
Date: 5/18/19 
Author: Jon Deaton (jdeaton@stanford.edu)
"""

import gymnasium as gym
import gym_agario

import numpy as np

import unittest


class ScreenGymTest(unittest.TestCase):

    env_name = "agario-screen-v0"
    env_config = {
        'ticks_per_step': 4,
        'arena_size': 1000,
        'num_pellets': 1000,
        'num_viruses': 25,
        'num_bots': 25,
        'pellet_regen': True,
        'screen_len': 1024
    }

    def test_creation(self):
        env = gym.make(self.env_name, **self.env_config)
        self.assertIsInstance(env, gym.Env)

    def test_step(self):
        env = gym.make(self.env_name, **self.env_config)
        next_state, reward, done, info = env.step((0.0, 0.0, 0))

        self.assertIsInstance(next_state, np.ndarray)
        self.assertIsInstance(reward, float)
        self.assertIsInstance(done, bool)
        self.assertIsInstance(info, dict)

        self.assertEqual(next_state.shape, env.observation_space.shape)

        self.assertGreater(np.sum(next_state), 0)
        self.assertFalse(np.all(next_state == 255))

    def test_steps(self):
        env = gym.make(self.env_name, **self.env_config)
        for _ in range(10):
            next_state, reward, done, info = env.step((0.0, 0.0, 0))

            self.assertIsInstance(next_state, np.ndarray)
            self.assertIsInstance(reward, float)
            self.assertIsInstance(done, bool)
            self.assertIsInstance(info, dict)

            self.assertEqual(next_state.shape, env.observation_space.shape)

            self.assertGreater(np.sum(next_state), 0)
            self.assertFalse(np.all(next_state == 255))


# if you wanna just run these tests, you can
if __name__ == "__main__":
    unittest.main()
