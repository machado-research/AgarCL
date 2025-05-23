import gymnasium as gym
import gym_agario

import numpy as np

import unittest


class ScreenGymTest(unittest.TestCase):

    env_name = "agario-screen-v0"
    env_config = {
        'ticks_per_step': 1,
        'arena_size': 1000,
        'pellet_regen': True,
        'num_pellets': 100,
        'num_viruses': 25,
        'num_bots': 25,
        'screen_len': 1024,
        "c_death": 0
    }

    def gen_random_actions(self):
        max_val, min_val = 1, -1
        range_size = max_val - min_val
        random_values = np.random.rand(2) * range_size + min_val
        return ([(random_values[0], random_values[1]),0])

    def test_creation(self):
        env = gym.make(self.env_name, **self.env_config)
        self.assertIsInstance(env, gym.Env)

    def test_step(self):
        env = gym.make(self.env_name, **self.env_config)
        env.reset()

        next_state, reward, done, info = env.step(self.gen_random_actions())

        self.assertIsInstance(next_state, np.ndarray)
        self.assertIsInstance(reward, float)
        self.assertIsInstance(done, bool)
        self.assertIsInstance(info, dict)

        self.assertEqual(next_state.shape, env.observation_space.shape)

        self.assertGreater(np.sum(next_state), 0)
        self.assertFalse(np.all(next_state == 255))

    def test_steps(self):
        env = gym.make(self.env_name, **self.env_config)
        env.reset()
        for _ in range(10):

            next_state, reward, done, info = env.step(self.gen_random_actions())

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
