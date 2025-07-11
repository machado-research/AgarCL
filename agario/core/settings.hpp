#pragma once

#define SEED 42

#define CELL_MIN_SIZE 25
#define CELL_MAX_SPEED 300
#define CELL_SPLIT_MINIMUM 50
#define SPLIT_DECELERATION 80

#define FOOD_SPEED 100
#define FOOD_DECEL 80

#define RECOMBINE_TIMER_SEC 10

// must be `CELL_EAT_MARGIN` times larger
// than another cell in order to eat it
#define CELL_EAT_MARGIN 1.1

#define MASS_AREA_RADIO 1.0

// the (approximate) amount by which a cell is reduced in size
// when it collides with a virus
#define CELL_POP_REDUCTION 2
#define CELL_POP_SIZE  25

#define DEFAULT_ARENA_WIDTH 250
#define DEFAULT_ARENA_HEIGHT 250

#define DEFAULT_NUM_PELLETS 500
#define DEFAULT_NUM_VIRUSES 10
#define PLAYER_CELL_LIMIT 14

//split condition
#define NUM_CELLS_TO_SPLIT PLAYER_CELL_LIMIT
#define MIN_CELL_SPLIT_MASS 130

//MASS DECAY
#define PLAYER_RATE 0.002
// #define GAME_RATE_MODIFIER 1.0
#define DECAY_FOR_NUM_SECONDS 1  //nearly a second

// Virus Feeding
#define NUMBER_OF_FOOD_HITS 7

// Auto-Split
#define MAX_MASS_IN_THE_GAME 22500
#define NEW_MASS_IF_NO_SPLIT 22000

// Anti-Teaming
#define ANTI_TEAM_ACTIVATION_TIME 60  //  one minute
#define NUM_VIRUSES_TO_EAT        3
