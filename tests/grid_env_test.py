#!/usr/bin/env python

"""
File: grid_test
Date: 6/7/19 
Author: Jon Deaton (jonpauldeaton@gmail.com)
"""

import gymnasium as gym
import gym_agario
import numpy as np

import unittest

from tests import default_config, null_action

env_name = "agario-grid-v0"


def set_bit_count(n):
    """ counts and returns the number of bits that are "on"
    in the binary representation of the number `n`
    """
    count = 0
    while n:
        count += n & 1
        n >>= 1
    return count


class GridGymTest(unittest.TestCase):

    def test_creation(self):
        """ tests to make sure that you can instantiate
        an environment and that it is the correct type
        """
        env = gym.make(env_name, **default_config)
        self.assertIsInstance(env, gym.Env)

    def test_reset(self):
        """ tests that resetting the environment returns a valid state
        """
        env = gym.make(env_name, **default_config)
        
        state = env.reset()
        self._assertValidState(env, state)

    def test_action_space(self):
        """ tests that valid actions are within the action
        space and invalid actions are not within the action space
        """
        env = gym.make(env_name, **default_config)

        for x in range(-10, 10):
            for y in range(-10, 10):
                for a in (0, 1, 2):
                    action = (np.array([float(x), float(y)]), a)
                    self.assertTrue(action in env.action_space, "valid action is not within action space")

                for a in (-1, -2, 3, 4, 5):
                    action = (np.array([float(x), float(y)]), a)
                    self.assertFalse(action in env.action_space, "invalid action is within action space")

    def test_steps(self):
        """ test that you can "step" the environment and
        that the return values of the step are well-formed
        """
        env = gym.make(env_name, **default_config)
        env.reset()
        for _ in range(1024):
            state, reward, done, info = env.step(null_action)
            self.assertIsInstance(reward, float, "reward was not a float")
            self.assertIsInstance(done, bool, "done is not a boolean")
            self.assertIsInstance(info, dict, "info is not a dictionary")
            self._assertValidState(env, state)  # make sure the state is valid

    def test_shape(self):
        """ tests that the shape of the observation
        is consistent with the env configuration
        """
        for ticks_per_step in range(4):
            for num_frames in range(ticks_per_step):
                for grid_size in (0, 10, 100):
                    for o in range(2 << 3):
                        observe_cells   = bool(o & 1)
                        observe_others  = bool(o & 2)
                        observe_viruses = bool(o & 4)
                        observe_pellets = bool(o & 8)

                        env = gym.make(env_name, **{
                            'ticks_per_step': ticks_per_step,
                            'num_frames':      num_frames,
                            'arena_size':      100,
                            'num_pellets':     50,
                            'num_viruses':     5,
                            'num_bots':        5,
                            'pellet_regen':    True,
                            'grid_size':       grid_size,
                            'observe_cells':   observe_cells,
                            'observe_others':  observe_others,
                            'observe_viruses': observe_viruses,
                            'observe_pellets': observe_pellets
                        })

                        channels_per_frame = 1 + set_bit_count(o)
                        channels_expected = num_frames * channels_per_frame

                        shape = env.observation_space.shape
                        self.assertIsInstance(shape, tuple, "observation space shape is not a tuple")
                        self.assertEqual(len(shape), 3, "observation space rank/order is not 3")

                        width, height, channels = shape

                        self.assertEqual(channels, channels_expected, "wrong number of channels")
                        self.assertEqual(width, grid_size, "observation width not equal to")

                        s = env.reset()
                        self.assertEqual(s.shape, shape, "observation shape not equal to observation space shape")
                        self.assertTrue(s in env.observation_space, "state is not in observation space")

                        for _ in range(10):
                            s, done, *_ = env.step(null_action)
                            if done: break
                            self._assertCorrectShape(env, s)

    def _assertValidState(self, env, state):
        """ asserts that the state which was returned by a `reset` or `step` from the
        environment `env` is well-formed. Checks the type, data type, shape, and values
        to make sure that it is a plausible state for the given environment.
        """
        self.assertIsInstance(state, np.ndarray, "state is not a numpy array")
        self.assertEqual(state.dtype, np.int32, "incorrect data type: %s" % state.dtype)

        self._assertCorrectShape(env, state)

        self.assertGreaterEqual(state.min(), -1)
        self.assertLess(state.max(), 1000)

        # this one is really important
        self.assertLess(state.min(), state.max())  # not all just one value

        # fill the state array with zeros ensures that the numpy data pointer
        # points to a valid array. If an invalid pointer was provided, this would
        # probably cause a segmentation fault shortly after
        state.fill(0)

    def _assertCorrectShape(self, env, state):
        """ asserts that the shape of `state` is valid given the observation space of `env`
        """
        self.assertEqual(state.shape, env.observation_space.shape, "state's shape doesnt match observation space shape")
        self.assertTrue(state in env.observation_space, "state is not in observation space")


if __name__ == "__main__":
    unittest.main()
