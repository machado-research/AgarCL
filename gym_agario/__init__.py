#!/usr/bin/env python
"""
File: __init__.py
"""

import agarcl
from gymnasium.envs.registration import register

register(id='agario-grid-v0',
         entry_point='gym_agario.AgarioEnv:AgarioEnv',
         kwargs={'obs_type': 'grid'})


if agarcl.has_screen_env:
    # only register the screen environment if its available
    register(id='agario-screen-v0',
             entry_point='gym_agario.AgarioEnv:AgarioEnv',
             kwargs={'obs_type': 'screen'})
