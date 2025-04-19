"""
File: AgarioEnv
Date: 2019-07-30
Author: Jon Deaton (jonpauldeaton@gmail.com)

This file wraps the Agar.io Learning Environment (agarle)
in an OpenAI gym interface. The interface offers three different
kinds of observation types:

1. screen   - rendering of the agar.io game screen
              (only available if agarle was compiled with OpenGL)

2. grid     - an image-like grid with channels for pellets, cells, viruses, boundaries, etc.
              I recommend this one the most since it produces fixed-size image-like data
              is much faster than the "screen" type and doesn't require compiling with
              OpenGL (which works fine on my machine, but probably won't work on your machine LOL)

3. ram      - raw positions and velocities of every entity in a fixed-size vector
              I haven't tried this one, but I'm guessing that this is harder than "grid".


This gym supports multiple agents in the same game. By default, there will
only be a single agent, and the gym will conform to the typical gym interface.
(note that there may still be any number of "bots" in this environment)

However, if you pass "multi_agent": True to the environment configuration
then the environment will have multiple agents all interacting within the
same agar.io game simultaneously.

    env = gym.make("agario-grid-v0", **{
        "multi_agent": True,
        "num_agents": 5
    })

In this setting, the environment object will no longer conform to the
typical gym interface in the following ways.

    1. `step()` will expect a list of actions of the same length
    as the number of agents.

    2. The return value of `step()` will be a list of observations,
    list of rewards, and list of dones each with length equal to
    the number of agents. The `info` dictionary (4th return value)
    remains a single dictionary.

    3. `reset()` will return a list of observations of length equal
    to the number of agents

    4. When an agent is "done", observations will be None. The environment
    may still be stepped while some agents are not done. Only when
    all agents are done must the environment be reset.

Note that if you pass "num_agents" greater than 1, "multi_agent"
will be set True automatically.

"""
from typing import List, Tuple
import gymnasium as gym
from gymnasium import spaces
import numpy as np
import cv2
import os
import agarle
from .agar_utils import get_color_array, Color
import random
class AgarioEnv(gym.Env):
    metadata = {'render_modes': ['human','rgb_array'], 'render_fps': 60}

    def __init__(self, obs_type='grid', render_mode = None, **kwargs):
        super(AgarioEnv, self).__init__()

        if obs_type not in ("ram", "screen", "grid"):
            raise ValueError(obs_type)

        self._env, self.observation_space = self._make_environment(obs_type, kwargs)
        self.steps = None
        self.obs_type = obs_type
        self.agent_view = False
        self.action_space = spaces.Tuple((
            # (dx, dy) movemment vector
            spaces.Box(low=-1, high=1, shape=(2,)),
            # 0=noop  1=split  2=feed
            spaces.Discrete(3),
        ))
        self.render_mode = render_mode

        self.video_recorder = []
        self.video_recorder_enabled = False

        self.agent_view            = kwargs.get("agent_view", False)
        self.add_noise             = kwargs.get("add_noise", True)
        self.number_of_steps       = kwargs.get("number_steps", 500)
        self.mode                  = kwargs.get("mode", 0)
        self.env_type              = kwargs.get("env_type", 0) #0 -> Episodic or 1 -> Continuing
        self._seed = None

    def step(self, actions):
        """ take an action in the environment, advancing the environment
        along until the next time step
        :param actions: either a single tuple, or list of tuples of tuples
            of the form (x, y, a) where `x`, `y` are in [-1, 1] and `a` is
            in {0, 1, 2} corresponding to nothing, split, feed, respectively.
        :return: tuple of - observation, reward, episode_over
            observation (object) : the next state of the world.
            reward (float) : reward gained during the time step
            episode_over (bool) : whether the game is over or not
            info (dict) : diagnostic information (currently empty)
        """
        assert self.steps is not None, "Cannot call step() before calling reset()"

        actions = self._sanitize_actions(actions)
        self._env.take_actions(actions)

        # step the environment forwards through time
        rewards = self._env.step()
        assert len(rewards) == self.num_agents

        # observe the new state of the environment for each agent
        self.observations = self._make_observations()

        #Assume it is only one agent -> Needs a fix for multi-agent
        if(self.video_recorder_enabled== True):
            self.video_recorder.append(self._make_video_observation(self.observations[0]))

        # get the "done" status of each agent
        dones = self._env.dones()
        assert len(dones) == self.num_agents

        # set the "truncation" status of each agent to 'False'
        truncations = [False] * len(dones)
        if(self.steps >=  self.number_of_steps and self.mode != 0 and self.env_type == 0): #Episodic
            dones = [True] * len(dones)
        # unwrap observations, rewards, dones if not mult-agent
        if not self.multi_agent:
            self.observations = self.observations[0]
            rewards = rewards[0]
            dones = dones[0]
            truncations = truncations[0]



        self.steps += 1
        return self.observations, rewards, dones, truncations, {'steps': self.steps, 'untransformed_rewards': rewards}

    def reset(self, **kwargs):
        """ resets the environment
        :return: the state of the environment at the beginning
        """
        self.steps = 0
        self._env.reset()
        obs = self._make_observations()
        return obs if self.multi_agent else obs[0], {}

    def render(self):
        # to do: if statements should be changed to self.render_mode, where:
        # "human": The environment is continuously rendered in the current display or terminal, usually for human consumption.
        # "rgb_array": Return a single frame representing the current state of the environment.
        if self.render_mode == "human":
            self._env.render()

        if self.render_mode == "rgb_array":

            if self.obs_type == "screen":
                return self.observations

            if self.obs_type == "grid":
                return  self._env.get_frame()

    def load_env_state(self, filename):
        self._env.load_env_state(filename)

    def save_env_state(self, filename):
        self._env.save_env_state(filename)

    def close(self):
        self._env.close()


    def _make_video_observation(self, observation):
        if self.obs_type == "grid":
            return self._env.get_frame()[0]
        else:
            if not self.agent_view:
                return observation
            else:
                observation = observation[0]
                RGB_obs = np.zeros_like(observation[..., :3])
                RGB_obs[...,0].fill(255)  # White background

                pellets_mask = observation[..., 0] != 255
                bots_mask = observation[..., 1] == 255
                virus_mask  = observation[..., 2] == 255
                main_agent_mask = (observation[..., 3] <= 230) & (observation[..., 3] > 30)
                grid_lines_mask = observation[..., 3] <= 30

                RGB_obs[pellets_mask] = get_color_array(Color.WHITE)
                RGB_obs[bots_mask] = get_color_array(Color.PURPLE)
                RGB_obs[virus_mask] = get_color_array(Color.GREEN)
                RGB_obs[main_agent_mask] = get_color_array(Color.BLUE)
                RGB_obs[grid_lines_mask] = [26, 0, 0]
                return RGB_obs


    def _make_observations(self):
        """ creates an observation object from the underlying environment
        representing the current state of the game
        :return: An observation object
        """
        states = self._env.get_state()
        assert len(states) == self.num_agents

        if self.obs_type in ("grid", ):
            # convert NCHW to NHWC
            observations = [np.transpose(state, [1, 2, 0]) for state in states]

        else:
            observations = states

        assert len(observations) == self.num_agents
        return observations

    def _make_environment(self, obs_type, kwargs):
        """ Instantiates and configures the underlying Agar.io environment (C++ implementation)
        :param obs_type: the observation type one of "ram", "screen", or "grid"
        :param kwargs: environment configuration parameters
        :return: tuple of
                    1) the environment object
                    2) observation space
        """
        assert obs_type in ("ram", "screen", "grid")

        args = self._get_env_args(kwargs)
        if obs_type == "grid":
            grid_defaults = {
                'num_frames': 1,
                'ticks_per_step': 4,
                'grid_size': 128,
                'observe_cells': True,
                'observe_others': True,
                'observe_viruses': True,
                'observe_pellets': True,
                'c_death': 0,
            }
            env = agarle.GridEnvironment(*args)
            env.configure_observation(kwargs | grid_defaults)

            channels, width, height = env.observation_shape()
            shape = (width, height, channels)
            dtype = np.int32
            observation_space = spaces.Box(-1, np.iinfo(dtype).max, shape, dtype=dtype)

        elif obs_type == "ram":
            env = agarle.RamEnvironment(*args)
            shape = env.observation_shape()
            observation_space = spaces.Box(-np.inf, np.inf, shape)

        elif obs_type == "screen":
            if not agarle.has_screen_env:
                raise ValueError("agarle was not compiled to include ScreenEnvironment")

            # the screen environment requires the additional
            # arguments of screen width and height. We don't use
            # the "configure_observation" design here because it would
            # introduce some ugly work-arounds and layers of indirection
            # in the underlying C++ code

            screen_len = kwargs.get("screen_len", 84)
            self.agent_view = kwargs.get("agent_view", False)

            args += (screen_len, screen_len)
            args += (self.agent_view, )
            env = agarle.ScreenEnvironment(*args)
            observation_space = spaces.Box(low=0, high=255, shape=env.observation_shape(), dtype=np.uint8)

        else:
            raise ValueError(obs_type)

        return env, observation_space

    def _sanitize_actions(self, actions) -> List[Tuple[float, float, int]]:
        if not self.multi_agent and type(actions) is not list:
            # if not multi-agent then the action should just be a single tuple
            actions = [actions]

        if type(actions) is not list:
            raise ValueError("Action list must be a list of two-element tuples")

        if len(actions) != self.num_agents:
            raise ValueError(f"Number of actions {len(actions)} does not match number of agents {self.num_agents}")

        # make sure that the actions are well-formed
        for action in actions:
            # Add noise to the action
            noise = [0,0]
            if  self.add_noise == True:
                noise = np.random.normal(0, 0.1, size=(2,))
            action = ((np.clip(action[0][0] + noise[0], -1, 1), np.clip(action[0][1] + noise[1], -1, 1)), action[1])
            #make sure the action is in the action space
            if not (self.action_space[0].contains(action[0]) and self.action_space[1].contains(action[1])):
                raise ValueError(f"action {action} not in action space")

        # gotta format the action for the underlying module.
        # passing the raw target numpy array is tricky because
        # of data formatting :(
        actions = [(tgt[0], tgt[1], a) for tgt, a in actions]
        return actions

    def _get_env_args(self, kwargs):
        """ creates a set of positional arguments to pass to the learning environment
        which specify how difficult to make the environment
        :param kwargs: arguments from the instantiation of t
        :return: list of arguments to the underlying environment
        """
        difficulty = kwargs.get("difficulty", "normal").lower()
        if difficulty not in ["normal", "empty", "trivial"]:
            raise ValueError(f'Unrecognized difficulty: {difficulty}')

        multi_agent = False
        num_agents = 1

        self.grid_size = kwargs.get("grid_size", 128)

        # default values for the "normal"
        ticks_per_step = 4
        num_frames = 1
        arena_size = 1000
        num_pellets = 1000
        num_viruses = 0
        num_bots = 0
        pellet_regen = True
        allow_respawn = True
        reward_type   = 1 #means diff
        if difficulty == "normal":
            pass  # default

        elif difficulty == "empty":
            # same as "normal" but no enemies
            num_bots = 0

        elif difficulty == "trivial":
            arena_size = 50  # tiny arena
            num_pellets = 200  # plenty of food
            num_viruses = 0  # no viruses
            num_bots = 0  # no enemies

        # now, override any of the defaults with those from the arguments
        # this allows you to specify a difficulty, but also to override
        # values so you can have, say, "normal" but with zero viruses, or w/e u want
        self.multi_agent     = kwargs.get("multi_agent", multi_agent)
        self.num_agents      = kwargs.get("num_agents", num_agents)
        self.ticks_per_step  = kwargs.get("ticks_per_step", ticks_per_step)
        self.num_frames      = kwargs.get("num_frames", num_frames)
        self.arena_size      = kwargs.get("arena_size", arena_size)
        self.num_pellets     = kwargs.get("num_pellets", num_pellets)
        self.num_viruses     = kwargs.get("num_viruses", num_viruses)
        self.num_bots        = kwargs.get("num_bots", num_bots)
        self.pellet_regen    = kwargs.get("pellet_regen", pellet_regen)
        self.allow_respawn   = kwargs.get("allow_respawn", allow_respawn)
        self.reward_type     = kwargs.get("reward_type", reward_type)
        self.c_death         = kwargs.get("c_death", -100)
        self.mode            = kwargs.get("mode", 0)

        self.multi_agent = self.multi_agent or self.num_agents > 1

        # todo: more assertions
        if type(self.ticks_per_step) is not int or self.ticks_per_step <= 0:
            raise ValueError(f"ticks_per_step must be a positive integer")

        return self.num_agents, self.ticks_per_step, self.arena_size, \
               self.pellet_regen, self.num_pellets, \
               self.num_viruses, self.num_bots, self.reward_type, self.c_death, self.mode

    def seed(self, seed=None):
        # sets the random seed for reproducibility
        if seed is not None:
            self._seed = seed
            self._env.seed(seed)
            return [self._seed]

    def enable_video_recorder(self):
        self.video_recorder_enabled = True

    def disable_video_recorder(self):
        self.video_recorder_enabled = False


    def generate_video(self, path, video_name):
        if not os.path.exists(path):
            os.makedirs(path, exist_ok=True)  # Create directory if it doesn't exist

        full_path = os.path.join(path, video_name)

        if self.video_recorder_enabled:
            if len(self.video_recorder) > 0:
                sz = (self.video_recorder[0].shape[1], self.video_recorder[0].shape[0])  # Get width and height correctly
                fourcc = cv2.VideoWriter_fourcc(*'MJPG')

                video = cv2.VideoWriter(full_path, fourcc, 60.0, sz)
                if not video.isOpened():
                    raise RuntimeError("Error: VideoWriter failed to open.")

                for frame in self.video_recorder:
                    if not isinstance(frame, np.ndarray):
                        raise TypeError("Error: A frame is not a numpy array.")
                    rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                    cv2.imwrite(f"{path}/frame_0.png", rgb_frame)
                    video.write(cv2.cvtColor(frame, cv2.COLOR_RGB2BGR))  # Ensure correct format


                video.release()
            else:
                print("No frames to generate video")
        else:
            print("Video recorder is not enabled. Please enable it before generating video")
