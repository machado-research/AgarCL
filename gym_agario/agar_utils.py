import numpy as np
from enum import Enum
import random



class Color(Enum):
    RED = 1
    ORANGE = 2
    YELLOW = 3
    GREEN = 4
    BLUE = 5
    PURPLE = 6
    WHITE = 7
    BLACK = 8
    LAST = 9

red_color = np.array([1.0, 0.0, 0.0]) * 255
blue_color = np.array([0.0, 0.0, 1.0]) * 255
green_color = np.array([0.0, 1.0, 0.0]) * 255
orange_color = np.array([1.0, 0.65, 0.0]) * 255
purple_color = np.array([0.6, 0.2, 0.8]) * 255
yellow_color = np.array([1.0, 1.0, 0.0]) * 255
black_color = np.array([0.0, 0.0, 0.0]) * 255
white_color = np.array([1.0, 1.0, 1.0]) * 255


def random_color():
    return get_color_array(Color(random.randint(1, Color.LAST.value)))

def get_color_array(c):
    if c == Color.RED:
        return red_color
    elif c == Color.BLUE:
        return blue_color
    elif c == Color.GREEN:
        return green_color
    elif c == Color.ORANGE:
        return orange_color
    elif c == Color.PURPLE:
        return purple_color
    elif c == Color.YELLOW:
        return yellow_color
    elif c == Color.WHITE:
        return white_color
    elif c == Color.BLACK:
        return black_color
    else:
        raise ValueError("Not a color")
