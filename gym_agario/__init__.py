#!/usr/bin/env python
"""
File: __init__.py
Date: 1/27/19 -05/12/23
Author: Jon Deaton (jdeaton@stanford.edu) - Mohamed Ayman (mamoham3@ualberta.ca)
"""

import agarle
from gymnasium.envs.registration import register

register(id='agario-ram-v0',
         entry_point='gym_agario.AgarioEnv:AgarioEnv',
         kwargs={'obs_type': 'ram'})

register(id='agario-grid-v0',
         entry_point='gym_agario.AgarioEnv:AgarioEnv',
         kwargs={'obs_type': 'grid'})


if agarle.has_screen_env:
    # only register the screen environment if its available
    register(id='agario-screen-v0',
             entry_point='gym_agario.AgarioEnv:AgarioEnv',
             kwargs={'obs_type': 'screen'})
