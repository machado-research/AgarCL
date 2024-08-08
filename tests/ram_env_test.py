#!/usr/bin/env python

"""
File: ram_env_test
Date: 9/7/19 
Author: Jon Deaton (jonpauldeaton@gmail.com)
"""

import gymnasium as gym
import gym_agario

import numpy as np
import unittest
from tests import default_config, null_action


env_name = "agario-ram-v0"


class RamGymTest(unittest.TestCase):

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
        pass


    def _assertValidState(self, env, state):
        """ asserts that the state which was returned by a `reset` or `step` from the
        environment `env` is well-formed. Checks the type, data type, shape, and values
        to make sure that it is a plausible state for the given environment.
        """
        self.assertIsInstance(state, np.ndarray, "state is not a numpy array")
        self.assertEqual(state.dtype, np.float32, "incorrect data type: %s" % state.dtype)

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