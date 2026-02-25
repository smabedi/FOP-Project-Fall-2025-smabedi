#include "coach.h"
#include "core/constants.h"
#include "entities/ball.h"
#include "entities/team.h"
#include "game/scene.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

// Set to false to let the other team use their own logic (if you implement it)
// Set to true to test your logic on both teams
bool coach_both_teams = true;

/* -------------------------------------------------------------------------
 * Logic Functions
 *  TODO 1: You must implement the following functions in Phase 2.
 *        Each player in each team has its own functions.
 *        You can add new functions, but are NOT ALLOWED to remove
 *        the existing functions or change their structure.
 * ------------------------------------------------------------------------- 
 * ⚠️ STUDENT RULES FOR PHASE 2:
 * You are restricted to modifying ONLY specific variables in each function:
 *
 * 1. MOVEMENT FUNCTIONS (movement_logic_X_Y):
 * Allowed: player->velocity
 * Goal:    Determine the direction and speed of movement.
 *
 * 2. SHOOTING FUNCTIONS (shooting_logic_X_Y):
 * Allowed: ball->velocity
 * Goal:    Determine the direction and power of the kick/pass.
 *
 * 3. CHANGE STATE FUNCTIONS (change_state_logic_X_Y):
 * Allowed: player->state
 * Goal:    Switch between IDLE, MOVING, SHOOTING, or INTERCEPTING.
 *
 * NOTE: Directly modifying any other attributes will be flagged as a violation.
 * Thank you for your attention to this matter!
 * ------------------------------------------------------------------------- */

/* Team 1 movement logic */
void movement_logic_1_0(struct Player *self, struct Scene *scene) { (void)scene; }
void movement_logic_1_1(struct Player *self, struct Scene *scene) { (void)scene; }
void movement_logic_1_2(struct Player *self, struct Scene *scene) { (void)scene; }
void movement_logic_1_3(struct Player *self, struct Scene *scene) { (void)scene; }
void movement_logic_1_4(struct Player *self, struct Scene *scene) { (void)scene; }
void movement_logic_1_5(struct Player *self, struct Scene *scene) { (void)scene; }

/* Team 2 movement logic */
void movement_logic_2_0(struct Player *self, struct Scene *scene) { (void)scene; }
void movement_logic_2_1(struct Player *self, struct Scene *scene) { (void)scene; }
void movement_logic_2_2(struct Player *self, struct Scene *scene) { (void)scene; }
void movement_logic_2_3(struct Player *self, struct Scene *scene) { (void)scene; }
void movement_logic_2_4(struct Player *self, struct Scene *scene) { (void)scene; }
void movement_logic_2_5(struct Player *self, struct Scene *scene) { (void)scene; }

/* Team 1 shooting logic */
void shooting_logic_1_0(struct Player *self, struct Scene *scene) {
    // THE CODE BELOW IS A HINT ON HOW TO CHECK IF IT IS KICK-OFF
    struct Ball *ball = scene->ball;
    if (ball->position.x == CENTER_X &&
        ball->position.y == CENTER_Y &&
        ball->velocity.x == 0.0f &&
        ball->velocity.y == 0.0f
    )       // it is kick-off, pass to your own half
        ball->velocity.x = (self->team == 1)? -350.0f: 350.0f;
    else {  // it is not kick-off, let's play air hockey!
        ball->velocity.x = 8350.0f;
        ball->velocity.y = 8350.0f;
    }

 }


void shooting_logic_1_1(struct Player *self, struct Scene *scene) { (void)scene; }
void shooting_logic_1_2(struct Player *self, struct Scene *scene) { (void)scene; }
void shooting_logic_1_3(struct Player *self, struct Scene *scene) { (void)scene; }
void shooting_logic_1_4(struct Player *self, struct Scene *scene) { (void)scene; }
void shooting_logic_1_5(struct Player *self, struct Scene *scene) { (void)scene; }

/* Team 2 shooting logic */
void shooting_logic_2_0(struct Player *self, struct Scene *scene) { (void)scene; }
void shooting_logic_2_1(struct Player *self, struct Scene *scene) { (void)scene; }
void shooting_logic_2_2(struct Player *self, struct Scene *scene) { (void)scene; }
void shooting_logic_2_3(struct Player *self, struct Scene *scene) { (void)scene; }
void shooting_logic_2_4(struct Player *self, struct Scene *scene) { (void)scene; }
void shooting_logic_2_5(struct Player *self, struct Scene *scene) { (void)scene; }

/* Team 1 change_state logic */
void change_state_logic_1_0(struct Player *self, struct Scene *scene) { (void)scene; }
void change_state_logic_1_1(struct Player *self, struct Scene *scene) { (void)scene; }
void change_state_logic_1_2(struct Player *self, struct Scene *scene) { (void)scene; }
void change_state_logic_1_3(struct Player *self, struct Scene *scene) { (void)scene; }
void change_state_logic_1_4(struct Player *self, struct Scene *scene) { (void)scene; }
void change_state_logic_1_5(struct Player *self, struct Scene *scene) { (void)scene; }

/* Team 2 change_state logic */
void change_state_logic_2_0(struct Player *self, struct Scene *scene) { (void)scene; }
void change_state_logic_2_1(struct Player *self, struct Scene *scene) { (void)scene; }
void change_state_logic_2_2(struct Player *self, struct Scene *scene) { (void)scene; }
void change_state_logic_2_3(struct Player *self, struct Scene *scene) { (void)scene; }
void change_state_logic_2_4(struct Player *self, struct Scene *scene) { (void)scene; }
void change_state_logic_2_5(struct Player *self, struct Scene *scene) { (void)scene; }

/* -------------------------------------------------------------------------
 * Lookup tables for factory
 * ------------------------------------------------------------------------- */
static PlayerLogicFn team1_movement[6] = {
        movement_logic_1_0, movement_logic_1_1, movement_logic_1_2,
        movement_logic_1_3, movement_logic_1_4, movement_logic_1_5
};

static PlayerLogicFn team2_movement[6] = {
        movement_logic_2_0, movement_logic_2_1, movement_logic_2_2,
        movement_logic_2_3, movement_logic_2_4, movement_logic_2_5
};

static PlayerLogicFn team1_shooting[6] = {
        shooting_logic_1_0, shooting_logic_1_1, shooting_logic_1_2,
        shooting_logic_1_3, shooting_logic_1_4, shooting_logic_1_5
};

static PlayerLogicFn team2_shooting[6] = {
        shooting_logic_2_0, shooting_logic_2_1, shooting_logic_2_2,
        shooting_logic_2_3, shooting_logic_2_4, shooting_logic_2_5
};

static PlayerLogicFn team1_change_state[6] = {
        change_state_logic_1_0, change_state_logic_1_1, change_state_logic_1_2,
        change_state_logic_1_3, change_state_logic_1_4, change_state_logic_1_5
};

static PlayerLogicFn team2_change_state[6] = {
        change_state_logic_2_0, change_state_logic_2_1, change_state_logic_2_2,
        change_state_logic_2_3, change_state_logic_2_4, change_state_logic_2_5
};

/* -------------------------------------------------------------------------
 * Factory functions
 * ------------------------------------------------------------------------- */
PlayerLogicFn get_movement_logic(int team, int kit) {
    if (coach_both_teams) return team1_movement[kit];
    return (team == 1) ? team1_movement[kit] : team2_movement[kit];
}

PlayerLogicFn get_shooting_logic(int team, int kit) {
    if (coach_both_teams) return team1_shooting[kit];
    return (team == 1) ? team1_shooting[kit] : team2_shooting[kit];
}

PlayerLogicFn get_change_state_logic(int team, int kit) {
    if (coach_both_teams) return team1_change_state[kit];
    return (team == 1) ? team1_change_state[kit] : team2_change_state[kit];
}

/* -------------------------------------------------------------------------
 * TALENTS
 * ------------------------------------------------------------------------- */
/* Team 1 */
static struct Talents team1_talents[6] = {
        {2, 6, 6, 6},
        {3, 6, 6, 5},
        {3, 7, 5, 5},
        {8, 6, 3, 3},
        {8, 5, 4, 3},
        {8, 6, 3, 3},
};

/* Team 2 */
static struct Talents team2_talents[6] = {
        {2, 7, 5, 6},
        {3, 6, 5, 6},
        {4, 6, 5, 5},
        {8, 5, 4, 3},
        {7, 6, 4, 3},
        {8, 5, 4, 3},
};

struct Talents get_talents(int team, int kit) {
    if (coach_both_teams) return team1_talents[kit];
    return (team == 1) ? team1_talents[kit] : team2_talents[kit];
}


/* -------------------------------------------------------------------------
 * Positioning
 * ------------------------------------------------------------------------- */
/* Team 1 */
static struct Vec2 team1_positions[6] = {
        {350, CENTER_Y},
        {400, CENTER_Y - 125},
        {400, CENTER_Y + 125},
        {250, CENTER_Y - 75},
        {250, CENTER_Y + 75},
        {80, CENTER_Y},
};

/* Team 2 */
static struct Vec2 team2_positions[6] = {
        {SCREEN_WIDTH - 350, CENTER_Y},
        {SCREEN_WIDTH - 400, CENTER_Y - 125},
        {SCREEN_WIDTH - 400, CENTER_Y + 125},
        {SCREEN_WIDTH - 250, CENTER_Y - 75},
        {SCREEN_WIDTH - 250, CENTER_Y + 75},
        {SCREEN_WIDTH - 80, CENTER_Y},
};

struct Vec2 get_positions(int team, int kit) {
    return (team == 1) ? team1_positions[kit] : team2_positions[kit];
}
