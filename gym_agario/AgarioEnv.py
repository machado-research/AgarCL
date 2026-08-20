"""
This file wraps the Agar.io Learning Environment (agarcl)
in an OpenAI gym interface. The interface offers three different
kinds of observation types:

1. screen   - rendering of the agar.io game screen
              (only available if agarcl was compiled with OpenGL)

2. grid     - an image-like grid with channels for pellets, cells, viruses, boundaries, etc.
              I recommend this one the most since it produces fixed-size image-like data
              is much faster than the "screen" type and doesn't require compiling with
              OpenGL
3. GoBigger  - An observation of GoBigger Paper on a single agent



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


"""
from typing import List, Tuple
import gymnasium as gym
from gymnasium import spaces
import numpy as np
import cv2
import os
import warnings
import agarcl
from .agar_utils import get_color_array, Color
import random
class AgarioEnv(gym.Env):
    metadata = {'render_modes': ['human','rgb_array'], 'render_fps': 60}

    def __init__(self, obs_type='screen', render_mode = None, **kwargs):
        super(AgarioEnv, self).__init__()

        if obs_type not in ("ram", "screen", "grid", "gobigger"):
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
        # Recorded frames are held in memory until generate_video() is called.
        # Uncapped, that grows without bound: a 128x128 RGB frame is ~49 KB, so
        # a million steps would need ~49 GB. Recording stops at this many
        # frames (one warning, then silence) instead of exhausting memory.
        self.max_video_frames = kwargs.get("max_video_frames", 20000)
        self._video_buffer_warned = False

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
        if self.video_recorder_enabled:
            if len(self.video_recorder) < self.max_video_frames:
                self.video_recorder.append(self._make_video_observation(self.observations[0]))
            elif not self._video_buffer_warned:
                warnings.warn(
                    f"video recording stopped after {self.max_video_frames} frames "
                    f"to bound memory use; pass max_video_frames to raise the cap, "
                    f"or call generate_video() and disable_video_recorder() sooner",
                    RuntimeWarning, stacklevel=2)
                self._video_buffer_warned = True

        # get the "done" status of each agent
        dones = self._env.dones()
        assert len(dones) == self.num_agents

        # A step limit is a truncation, not a termination: under the Gymnasium
        # contract `terminated` means the MDP reached a terminal state, and
        # value bootstrapping at the boundary is wrong if a time limit is
        # reported that way. This previously set dones=True and left the
        # computed truncations unused.
        truncations = [False] * len(dones)
        if(self.steps >= self.number_of_steps and self.env_type == 0): #Episodic
            truncations = [True] * len(dones)
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


    # Palette for the agent-view video colouring, indexed by the label image
    # built in _make_video_observation. Row 0 is the background, which the
    # original code produced by zeroing the frame and filling channel 0 with
    # 255; that is preserved exactly here rather than changed.
    _VIDEO_PALETTE = np.array([
        [255, 0, 0],                                   # 0: background
        get_color_array(Color.WHITE),                   # 1: pellets
        get_color_array(Color.PURPLE),                  # 2: other players
        get_color_array(Color.GREEN),                   # 3: viruses
        get_color_array(Color.BLUE),                    # 4: main agent
        [26, 0, 0],                                     # 5: grid lines
    ], dtype=np.uint8)

    def _make_video_observation(self, observation):
        """ Builds a single H x W x 3 uint8 RGB frame for the video writer.

        Screen observations carry a leading frame dimension, so the raw
        observation is 4-D. Returning it unchanged (as the non-agent_view path
        used to) handed a 4-D array to cv2, which **crashed the process with a
        bus error**, and sized the video as (width, 1).
        """
        if self.obs_type == "grid" or self.obs_type == "gobigger":
            frame = np.asarray(self._env.get_frame()[0])
        elif not self.agent_view:
            frame = np.asarray(observation)
            if frame.ndim == 4:      # (frames, H, W, C) -> (H, W, C)
                frame = frame[0]
        else:
            observation = np.asarray(observation)[0]
            alpha = observation[..., 3]

            # Build a label image, then colour it with a single palette
            # lookup. This replaces five boolean-mask assignments into a
            # three-channel array (a scatter per class, per channel) with five
            # writes into a one-channel array plus one take, which is markedly
            # cheaper per frame. Classes are applied in the same order as
            # before, so later ones still override earlier ones and the output
            # is unchanged.
            labels = np.zeros(observation.shape[:2], dtype=np.uint8)  # 0: background
            labels[observation[..., 0] != 255] = 1                   # pellets
            labels[observation[..., 1] == 255] = 2                   # other players
            labels[observation[..., 2] == 255] = 3                   # viruses
            labels[(alpha <= 230) & (alpha > 30)] = 4                # main agent
            labels[alpha <= 30] = 5                                  # grid lines
            frame = self._VIDEO_PALETTE[labels]

        # cv2 needs a contiguous 3-channel uint8 image
        return np.ascontiguousarray(frame[..., :3], dtype=np.uint8)


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
        :param obs_type: the observation type one of "gobigger", "screen", or "grid"
        :param kwargs: environment configuration parameters
        :return: tuple of
                    1) the environment object
                    2) observation space
        """

        assert obs_type in ("screen", "grid", "gobigger")

        base_args = self._get_env_args(kwargs)

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
            # `grid_defaults | kwargs` (not the reverse): caller-supplied
            # values must win over the defaults. With the operands swapped
            # every user setting was silently overridden by grid_defaults.
            args = base_args
            env = agarcl.GridEnvironment(*args)
            env.configure_observation(grid_defaults | kwargs)

            channels, width, height = env.observation_shape()
            shape = (width, height, channels)
            dtype = np.int32
            observation_space = spaces.Box(-1, np.iinfo(dtype).max, shape, dtype=dtype)

        elif obs_type == "screen":
            if not agarcl.has_screen_env:
                raise ValueError("agarcl was not compiled to include ScreenEnvironment")

            # the screen environment requires the additional
            # arguments of screen width and height. We don't use
            # the "configure_observation" design here because it would
            # introduce some ugly work-arounds and layers of indirection
            # in the underlying C++ code

            screen_len = kwargs.get("screen_len", 84)
            self.agent_view = kwargs.get("agent_view", False)

            args = base_args  + (screen_len, screen_len)
            args += (self.agent_view, )
            env = agarcl.ScreenEnvironment(*args)
            observation_space = spaces.Box(low=0, high=255, shape=env.observation_shape(), dtype=np.uint8)
        elif obs_type == "gobigger":

            map_width   = kwargs.get("map_width", 512)
            map_height  = kwargs.get("map_height", 512)
            frame_limit = kwargs.get("frame_limit", 1000)
            agent_view  = kwargs.get("agent_view", False)

            full_args = (map_width, map_height, frame_limit) + base_args + (agent_view,)
            env = agarcl.GoBiggerEnvironment(*full_args)
            # Here we assume that the observation is returned as a NumPy array;
            # adjust dtype and bounds as necessary.
            shape = env.observation_shape()
            print( shape )
            observation_space = spaces.Box(low=0, high=255, shape=shape, dtype=np.float32)
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

        # make sure that the actions are well-formed, applying action noise if
        # enabled. This used to rebind the loop variable, so the noisy action
        # was validated and then discarded: the environment paid for the RNG
        # draw and the space checks but always received the original action.
        # Validate directly rather than via Space.contains(): the latter
        # allocates arrays and runs dtype/shape machinery on every step, which
        # is measurable per-step overhead for a two-element bounds check. The
        # accepted set and the raised error are unchanged: the continuous part
        # must be two values in [-1, 1] and the discrete part an integer in
        # {0, 1, 2}.
        out = []
        for action in actions:
            target, discrete = action[0], action[1]
            if self.add_noise:
                noise = np.random.normal(0, 0.1, size=(2,))
                target = (float(np.clip(target[0] + noise[0], -1, 1)),
                          float(np.clip(target[1] + noise[1], -1, 1)))
            try:
                dx, dy = float(target[0]), float(target[1])
                a = int(discrete)
                ok = (len(target) == 2
                      and -1.0 <= dx <= 1.0 and -1.0 <= dy <= 1.0
                      and 0 <= a < self.action_space[1].n)
            except (TypeError, ValueError, IndexError):
                ok = False
            if not ok:
                raise ValueError(f"action {(target, discrete)} not in action space")
            # format for the underlying module: passing the raw target numpy
            # array is tricky because of data formatting :(
            out.append((dx, dy, a))
        return out

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
        self.c_death         = kwargs.get("c_death", 0)
        self.mode            = kwargs.get("mode", 0)
        self.load_env_snapshot   = kwargs.get("load_env_snapshot", False)

        self.multi_agent = self.multi_agent or self.num_agents > 1

        # todo: more assertions
        if type(self.ticks_per_step) is not int or self.ticks_per_step <= 0:
            raise ValueError(f"ticks_per_step must be a positive integer")

        return self.num_agents, self.ticks_per_step, self.arena_size, \
               self.pellet_regen, self.num_pellets, \
               self.num_viruses, self.num_bots, self.reward_type, self.c_death, self.mode, \
               self.load_env_snapshot

    def seed(self, seed=None):
        # sets the random seed for reproducibility
        if seed is not None:
            self._seed = seed
            self._env.seed(seed)
            return [self._seed]

    def enable_video_recorder(self, max_frames=None):
        """ starts buffering frames for generate_video().
        :param max_frames: optional cap on buffered frames (memory bound)
        """
        self.video_recorder_enabled = True
        if max_frames is not None:
            self.max_video_frames = int(max_frames)

    def disable_video_recorder(self):
        self.video_recorder_enabled = False


    def generate_video(self, path, video_name, fps=None):
        """ Writes the recorded frames to `path/video_name`.

        :param fps: frame rate; defaults to real time. One environment step
            advances `ticks_per_step` engine ticks of 1/30 s each, so real time
            is 30 / ticks_per_step frames per second. The previous hardcoded
            60 fps played the game back roughly eight times too fast at the
            default of four ticks per step.

        Clears the frame buffer afterwards, so a subsequent recording starts
        empty rather than being appended to frames already written.
        """
        if not os.path.exists(path):
            os.makedirs(path, exist_ok=True)  # Create directory if it doesn't exist

        full_path = os.path.join(path, video_name)

        if self.video_recorder_enabled:
            if len(self.video_recorder) > 0:
                first = self.video_recorder[0]
                if first.ndim != 3 or first.shape[2] != 3:
                    raise ValueError(
                        f"video frames must be H x W x 3, got {first.shape}")
                height, width = first.shape[:2]
                if fps is None:
                    fps = max(1.0, 30.0 / max(1, int(self.ticks_per_step)))
                fourcc = cv2.VideoWriter_fourcc(*'MJPG')

                video = cv2.VideoWriter(full_path, fourcc, float(fps), (width, height))
                if not video.isOpened():
                    raise RuntimeError("Error: VideoWriter failed to open.")

                for frame in self.video_recorder:
                    if not isinstance(frame, np.ndarray):
                        raise TypeError("Error: A frame is not a numpy array.")
                    if frame.shape != first.shape:
                        raise ValueError(
                            f"inconsistent frame shapes: {frame.shape} vs {first.shape}")
                    video.write(cv2.cvtColor(frame, cv2.COLOR_RGB2BGR))  # Ensure correct format
                video.release()
                self.video_recorder = []
                self._video_buffer_warned = False
            else:
                print("No frames to generate video")
        else:
            print("Video recorder is not enabled. Please enable it before generating video")
