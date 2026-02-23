#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "referee.h"
#include "game/possession.h"
#include "entities/team.h"

/**
 * @brief Determines whether a goal has been scored.
 *
 * This function checks if the ball has completely crossed either goal line
 * while also being fully inside the vertical goalmouth.
 *
 * Important details:
 * - A goal is only valid if the *entire ball* (taking BALL_RADIUS into account)
 *   has crossed the goal line.
 * - The ball must be vertically contained between the goal posts.
 * - The right goal corresponds to Team 1 scoring.
 * - The left goal corresponds to Team 2 scoring.
 * @return
 * - 1 if Team 1 scores,
 * - 2 if Team 2 scores,
 * - 0 if no goal has occurred.
 */
static int goal(float x, float y) {
    // Constructing the borders
    const float left_line = PITCH_X;
    const float right_line = PITCH_X + PITCH_W;
    const float goal_top = CENTER_Y - (float) GOAL_HEIGHT / 2.0f;
    const float goal_bottom = CENTER_Y + (float) GOAL_HEIGHT / 2.0f;

    // Checking the borders
    const bool within_goals = (y - BALL_RADIUS >= goal_top) && (y + BALL_RADIUS <= goal_bottom);
    const bool past_left_line = (x + BALL_RADIUS < left_line);
    const bool past_right_line = (x - BALL_RADIUS > right_line);

    // Final result
    if (within_goals) {
        if (past_left_line) {
            printf("GOAL! Left net hit at x:%.2f, y=%.2f\n", x, y);
            return 2;
        } else if (past_right_line) {
            printf("GOAL! Right net hit at x:%.2f, y=%.2f\n", x, y);
            return 1;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

/**
 * @brief Checks whether the ball is out of bounds.
 *
 * The ball is considered out only when its *entire area* lies outside
 * the pitch boundaries. Partial overlap with the pitch does NOT count as out.
 *
 * Notes for students:
 * - Use BALL_RADIUS to ensure the whole ball has crossed a boundary.
 * - All four pitch sides (left, right, top, bottom) must be considered.
 * - This function does not handle goals; goal detection is performed separately.
 * @return true if the ball is fully out of bounds, false otherwise.
 */
static bool out(float x, float y) {
    // Constructing the borders
    const float left_line = PITCH_X;
    const float right_line = PITCH_X + PITCH_W;
    const float top_line = PITCH_Y;
    const float bottom_line = PITCH_Y + PITCH_H;

    // Checking the borders
    const bool past_left = (x + BALL_RADIUS < left_line);
    const bool past_right = (x - BALL_RADIUS > right_line);
    const bool past_top = (y + BALL_RADIUS < top_line);
    const bool past_bottom = (y - BALL_RADIUS > bottom_line);

    // Final result
    if (past_left || past_right || past_top || past_bottom) {
        printf("Ball is out: x=%.2f, y=%.2f\n", x, y);
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Acts as the game referee for one simulation step.
 *
 * This function is responsible for detecting and resolving game events
 * related to the ball, such as goals and out-of-bounds situations.
 *
 * Responsibilities:
 * - Check for goals BEFORE checking for out-of-bounds.
 * - Update team scores if a goal is detected.
 * - Report the appropriate game event.
 *
 * Notes for students:
 * - A goal must be checked first because a scored goal is technically out.
 * - If no event occurs, the game continues normally.
 *
 * @param scene Pointer to the current game scene.
 *
 * @return
 * - GOAL if a goal has been scored,
 * - OUT if the ball is out of bounds,
 * - 0 if no event occurred.
 */
int referee(struct Scene *scene) {
    // Sanity check
    if (!scene || !scene->ball) {
        return PLAY_ON;
    }

    // Getting the ball's x and y
    const float x = scene->ball->position.x;
    const float y = scene->ball->position.y;

    // Checking the conditions
    const int goal_result = goal(x, y);
    if (goal_result == 1) {
        if (scene->first_team) {
            scene->first_team->score++;
        }
        return GOAL;
    } else if (goal_result == 2) {
        if (scene->second_team) {
            scene->second_team->score++;
        }
        return GOAL;
    } else {
        if (out(x, y)) {
            return OUT;
        } else {
            return PLAY_ON;
        }
    }
}


/**
 * @brief Verifies the validity of a player's talent distribution.
 *
 * This function checks whether each individual talent value is within
 * the allowed range and whether the total talent points do not exceed
 * the maximum allowed per player.
 *
 * Notes for students:
 * - Each skill must be between 1 and MAX_TALENT_PER_SKILL (inclusive).
 * - The sum of all skills must not exceed MAX_TALENT_PER_PLAYER.
 * - Invalid configurations should be reported as errors.
 *
 * @param talents The talent structure to validate.
 */
void verify_talents(struct Talents talents) {
    // Checking each range
    const bool defence_check = (1 <= talents.defence) && (talents.defence <= MAX_TALENT_PER_SKILL);
    const bool agility_check = (1 <= talents.agility) && (talents.agility <= MAX_TALENT_PER_SKILL);
    const bool dribbling_check = (1 <= talents.dribbling) && (talents.dribbling <= MAX_TALENT_PER_SKILL);
    const bool shooting_check = (1 <= talents.shooting) && (talents.shooting <= MAX_TALENT_PER_SKILL);
    const int sum = talents.defence + talents.agility + talents.dribbling + talents.shooting;
    const bool sum_check = (sum <= MAX_TALENT_PER_PLAYER);

    // Printing results
    if (!defence_check || !agility_check || !dribbling_check || !shooting_check || !sum_check) {
        printf("ERROR: Invalid talents! Values: defence=%d, agility=%d, dribbling=%d, shooting=%d, sum=%d\n",
               talents.defence, talents.agility, talents.dribbling, talents.shooting, sum);
    }
}


/**
 * @brief Verifies the correctness of a player's current state.
 *
 * Ensures that the player's state is consistent with the game rules.
 * In particular, only the player who currently possesses the ball
 * is allowed to be in the SHOOTING state.
 *
 * Notes for students:
 * - If a player attempts to shoot without possessing the ball,
 *   their state must be corrected.
 *
 * @param player Pointer to the player being verified.
 * @param scene  Pointer to the current game scene.
 */
void verify_state(struct Player *player, struct Scene *scene) {
    // Sanity check
    if (!player || !scene || !scene->ball) {
        return;
    }

    // Check the state and correct if needed
    if (player->state == SHOOTING && scene->ball->possessor != player) {
        printf(" ERROR: the ball is not yours, you can't shoot! (team %d, player %d)\n",
               player->team, player->kit);
        player->state = MOVING;
    }
}

/**
 * @brief Verifies and limits a player's movement speed.
 *
 * This function ensures that a player's velocity does not exceed
 * the maximum allowed speed derived from their agility talent.
 *
 * Notes for students:
 * - Maximum speed scales linearly with the agility talent.
 * - Both x and y velocity components must be checked independently.
 * - If a component exceeds the limit, it must be clamped.
 *
 * @param player Pointer to the player whose movement is being verified.
 */
void verify_movement(struct Player *player) {
    // Sanity Check
    if (!player) {
        return;
    }

    // Constructing boundaries
    const float max_velocity = MAX_PLAYER_VELOCITY * ((float) player->talents.agility / (float) MAX_TALENT_PER_SKILL);

    // Final check
    if (fabsf(player->velocity.x) > max_velocity) {
        printf(" ERROR: Demanding to run too fast in dimension x! (team %d, player %d)\n",
               player->team, player->kit);
        player->velocity.x = (player->velocity.x > 0.0f) ? max_velocity : -max_velocity;
    }
    if (fabsf(player->velocity.y) > max_velocity) {
        printf(" ERROR: Demanding to run too fast in dimension y! (team %d, player %d)\n",
               player->team, player->kit);
        player->velocity.y = (player->velocity.y > 0.0f) ? max_velocity : -max_velocity;
    }
}

/**
 * @brief Verifies the validity of a ball shot.
 *
 * This function ensures that the ball's velocity after a shot does not
 * exceed the maximum allowed speed derived from the shooter's talent.
 *
 * Additional kickoff rules:
 * - During kickoff, the ball must be played into the player's own half.
 *
 * Notes for students:
 * - Ball velocity must be clamped if it exceeds the allowed maximum.
 * - Both velocity components must be checked independently.
 *
 * @param ball    Pointer to the ball being shot.
 * @param kickoff True if the shot occurs during kickoff.
 */
void verify_shoot(struct Ball *ball, bool kickoff) {
    // TODO 7: implement this function

    // Sanity Check
    if (!ball || !ball->possessor) {
        return;
    }

    // Constructing boundaries
    Player *player = ball->possessor;
    const float max_velocity =
            MAX_BALL_VELOCITY * ((float) player->talents.shooting / (float) MAX_TALENT_PER_SKILL);

    // Check the ball velocity
    if (fabsf(ball->velocity.x) > max_velocity) {
        printf(" ERROR: Demanding to shoot too fast in dimension x! (team %d, player %d)\n",
               player->team, player->kit);
        ball->velocity.x = (ball->velocity.x > 0.0f) ? max_velocity : -max_velocity;
    }
    if (fabsf(ball->velocity.y) > max_velocity) {
        printf(" ERROR: Demanding to shoot too fast in dimension y! (team %d, player %d)\n",
               player->team, player->kit);
        ball->velocity.y = (ball->velocity.y > 0.0f) ? max_velocity : -max_velocity;
    }

    // Check whether the ball is in the center
    const bool ball_is_center =
            (fabsf(ball->position.x - CENTER_X) < 0.5f) && (fabsf(ball->position.y - CENTER_Y) < 0.5f);

    // Check for the kick-off state
    if (kickoff && ball_is_center) {
        if ((player->team == 1 && ball->velocity.x > 0) || (player->team == 2 && ball->velocity.x < 0)) {
            printf(" ERROR: You must pass to your own half! (team %d, player %d)\n", player->team, player->kit);
        }
    }

    /*// if the velocity was exactly 0, force a tiny legal direction
    if (lengthVec2(&ball->velocity) == 0.0f) {
        ball->velocity.x = (player->team == 1) ? -1.0f : 1.0f;
    }*/
}
