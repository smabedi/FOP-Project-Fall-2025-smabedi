#include "coach.h"
#include "core/constants.h"
#include "entities/ball.h"
#include "entities/team.h"
#include "game/scene.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

// Set to false to let the other team use their own logic (if you implement it)
// Set to true to test your logic on both teams
bool coach_both_teams = true;

/* ------------------------------------------------------------------------- *
 * Assistant Functions
 * ------------------------------------------------------------------------- */

static float clamp_float(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float distance_squared(Vec2 a, Vec2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static float distance(Vec2 a, Vec2 b) {
    return hypotf(a.x - b.x, a.y - b.y);
}

static Vec2 subtract_vec2(Vec2 a, Vec2 b) {
    return (Vec2) {a.x - b.x, a.y - b.y};
}

static Vec2 add_vec2(Vec2 a, Vec2 b) {
    return (Vec2) {a.x + b.x, a.y + b.y};
}

static Vec2 scale_vec2(Vec2 v, float s) {
    return (Vec2) {v.x * s, v.y * s};
}

static Vec2 normalize_vec2(Vec2 v) {
    const float len = hypotf(v.x, v.y);
    if (len < 1e-6f) return (Vec2) {0.0f, 0.0f};
    return (Vec2) {v.x / len, v.y / len};
}

static float max_player_component_speed(const struct Player *p) {
    const float scale = (float) p->talents.agility / (float) MAX_TALENT_PER_SKILL;
    return scale * (float) MAX_PLAYER_VELOCITY;
}

static float max_ball_component_speed(const struct Player *p) {
    const float scale = (float) p->talents.shooting / (float) MAX_TALENT_PER_SKILL;
    return scale * (float) MAX_BALL_VELOCITY;
}

static bool ball_in_own_half(int team, float ball_x) {
    return (team == 1) ? (ball_x < (float) CENTER_X) : (ball_x > (float) CENTER_X);
}

static float opponent_direction(int team) {
    /* Team 1 attacks +x (to the right), Team 2 attacks -x */
    return (team == 1) ? 1.0f : -1.0f;
}

static float own_goal_x(int team) {
    return (team == 1) ? PITCH_X : (PITCH_X + PITCH_W);
}

static Vec2 opponent_goal_center(int team) {
    const float goal_top = CENTER_Y - (float) GOAL_HEIGHT / 2.0f;
    const float goal_mid_y = goal_top + (float) GOAL_HEIGHT / 2.0f;

    if (team == 1) {
        /* shoot into right net area */
        return (Vec2) {(PITCH_X + PITCH_W) + (float) GOAL_WIDTH * 0.5f, goal_mid_y};
    } else {
        /* shoot into left net area */
        return (Vec2) {PITCH_X - (float) GOAL_WIDTH * 0.5f, goal_mid_y};
    }
}

static Vec2 clamp_inside_pitch(Vec2 p, float margin) {
    const float left = PITCH_X + margin;
    const float right = (PITCH_X + PITCH_W) - margin;
    const float top = PITCH_Y + margin;
    const float bottom = (PITCH_Y + PITCH_H) - margin;

    p.x = clamp_float(p.x, left, right);
    p.y = clamp_float(p.y, top, bottom);
    return p;
}

static float point_to_segment_distance(Vec2 p, Vec2 a, Vec2 b) {
    const Vec2 ab = subtract_vec2(b, a);
    const Vec2 ap = subtract_vec2(p, a);

    const float ab_len2 = ab.x * ab.x + ab.y * ab.y;
    if (ab_len2 < 1e-6f) return distance(p, a);

    float t = (ap.x * ab.x + ap.y * ab.y) / ab_len2;
    t = clamp_float(t, 0.0f, 1.0f);

    const Vec2 proj = (Vec2) {a.x + ab.x * t, a.y + ab.y * t};
    return distance(p, proj);
}

static const struct Team *get_own_team_pointer(const struct Scene *scene, int team) {
    return (team == 1) ? scene->first_team : scene->second_team;
}

static const struct Team *get_opponent_team_pointer(const struct Scene *scene, int team) {
    return (team == 1) ? scene->second_team : scene->first_team;
}

/* ------------------------------------------------------------------------- *
 * Role system
 * kits:
 *  3 => Goalkeeper
 *  2,4 => Defenders
 *  others => Attackers/Wingers
 * ------------------------------------------------------------------------- */

typedef enum {
    ATTACKER,
    DEFENDER,
    GOALKEEPER
} Role;

Role role_of_kit(int kit) {
    if (kit == 3) return GOALKEEPER;
    if (kit == 2 || kit == 4) return DEFENDER;
    return ATTACKER;
}

/* Find the closest teammate (excluding GoalKeeper) to chase the ball */
static struct Player *find_chaser(const struct Scene *scene, int team) {
    const struct Team *t = get_own_team_pointer(scene, team);
    const struct Ball *ball = scene->ball;

    struct Player *best = NULL;
    float best_d2 = FLT_MAX;

    for (int i = 0; i < PLAYER_COUNT; i++) {
        struct Player *p = t->players[i];
        if (!p) continue;
        if (p->kit == 3) continue; /* exclude GK */
        const float d2 = distance_squared(p->position, ball->position);
        if (d2 < best_d2) {
            best_d2 = d2;
            best = p;
        }
    }

    return best;
}

/* Is there any opponent close to self? */
static bool opponent_is_close(const struct Scene *scene, const struct Player *self, float r) {
    const struct Team *opp = get_opponent_team_pointer(scene, self->team);
    const float r2 = r * r;

    for (int i = 0; i < PLAYER_COUNT; i++) {
        const struct Player *o = opp->players[i];
        if (!o) continue;
        if (distance_squared(o->position, self->position) < r2) return true;
    }
    return false;
}

/* Check if pass line is "open" (no opponent is too close to segment) */
static bool pass_line_is_open(const struct Scene *scene, int team, Vec2 from, Vec2 to) {
    const struct Team *opp = get_opponent_team_pointer(scene, team);

    /* threshold for "blocking" the pass lane */
    const float block_r = (float) PLAYER_RADIUS * 2.0f;

    for (int i = 0; i < PLAYER_COUNT; i++) {
        const struct Player *o = opp->players[i];
        if (!o) continue;

        const float d = point_to_segment_distance(o->position, from, to);
        if (d < block_r) return false;
    }
    return true;
}

/* Find a teammate ahead and open, best by being most advanced */
static struct Player *find_best_open_teammate_ahead(const struct Scene *scene, const struct Player *self) {
    const struct Team *t = get_own_team_pointer(scene, self->team);
    const struct Ball *ball = scene->ball;

    struct Player *best = NULL;
    float best_score = -FLT_MAX;

    for (int i = 0; i < PLAYER_COUNT; i++) {
        struct Player *mate = t->players[i];
        if (!mate || mate == self) continue;

        /* must be ahead (closer to opponent goal direction) */
        float mate_to_goal = distance(mate->position, opponent_goal_center(self->team));
        float self_to_goal = distance(self->position, opponent_goal_center(self->team));
        if (self_to_goal < mate_to_goal) continue;
        /* score: larger distance is better */
        const float score = self_to_goal - mate_to_goal;
        if (score <= best_score) continue;

        /* basic range limit so we don't rocket-pass across the whole pitch */
        if (distance(ball->position, mate->position) > 500.0f) continue;

        if (!pass_line_is_open(scene, self->team, ball->position, mate->position)) continue;

        best = mate;
        best_score = score;
    }

    return best;
}

/* Find the nearest teammate to whom the passing lane is open */
static struct Player *find_best_open_teammate(const struct Scene *scene, const struct Player *self) {
    const struct Team *t = get_own_team_pointer(scene, self->team);
    const struct Ball *ball = scene->ball;

    struct Player *best = NULL;
    float best_dist = FLT_MAX;

    for (int i = 0; i < PLAYER_COUNT; i++) {
        struct Player *mate = t->players[i];
        if (!mate || mate == self)
            continue;

        float d = distance(ball->position, mate->position);
        // Ignore teammates that are too far away
        if (d > 500.0f)
            continue;

        if (!pass_line_is_open(scene, self->team, ball->position, mate->position))
            continue;

        if (d < best_dist) {
            best_dist = d;
            best = mate;
        }
    }
    return best;
}

/* Kickoff detection: ball at exact center */
static bool ball_is_at_kickoff_position(const struct Ball *ball) {
    return (fabsf(ball->position.x - (float) CENTER_X) < 0.5f) &&
           (fabsf(ball->position.y - (float) CENTER_Y) < 0.5f);
}

/* Ball near boundary */
static bool ball_near_pitch_edge(const struct Ball *ball) {
    const float left = PITCH_X;
    const float right = PITCH_X + PITCH_W;
    const float top = PITCH_Y;
    const float bottom = PITCH_Y + PITCH_H;

    const float m = 8.0f; /* small margin */
    return (ball->position.x < left + m) || (ball->position.x > right - m) ||
           (ball->position.y < top + m) || (ball->position.y > bottom - m);
}

/* Choose a safe restart target inside the pitch */
static Vec2 safe_restart_target(const struct Scene *scene, const struct Player *self) {
    /* aim towards center */
    return (Vec2) {(float) CENTER_X, (float) CENTER_Y};
}

/* Choose kickoff backward pass target (own half) */
static Vec2 kickoff_backward_target(const struct Scene *scene, const struct Player *self) {
    const struct Team *self_team = get_own_team_pointer(scene, self->team);

    const struct Player *nearest_teammate = self_team->players[1];
    for (int i = 0; i < PLAYER_COUNT; i++) {
        const struct Player *mate = self_team->players[i];
        if (mate == self) continue;
        if (distance(self->position, mate->position) < distance(self->position, nearest_teammate->position)) {
            nearest_teammate = mate;
        }
    }
    return nearest_teammate->position;
}

/* ------------------------------------------------------------------------- *
 * Generic logic implementations
 * ------------------------------------------------------------------------- */

static void movement_generic(struct Player *self, const struct Scene *scene) {
    const struct Ball *ball = scene->ball;
    const Role role = role_of_kit(self->kit);

    const float vmax = max_player_component_speed(self) * 0.90f; /* stay below cap */
    Vec2 target;

    /* If player is outside pitch (happens after throw-in), return quickly */
    const float left = PITCH_X + self->radius;
    const float right = PITCH_X + PITCH_W - self->radius;
    const float top = PITCH_Y + self->radius;
    const float bottom = PITCH_Y + PITCH_H - self->radius;

    const bool outside =
            (self->position.x < left) || (self->position.x > right) ||
            (self->position.y < top) || (self->position.y > bottom);

    if (outside) {
        target = clamp_inside_pitch(self->position, self->radius);
    } else if (role == GOALKEEPER) {
        /* GK: stay close to own goal, follow ball.y in own half */
        const float gx = (self->team == 1)
                         ? (PITCH_X + 25.0f)
                         : (PITCH_X + PITCH_W - 25.0f);

        float gy = (ball_in_own_half(self->team, ball->position.x) ? ball->position.y : (float) CENTER_Y);

        /* clamp within goal height (slightly) */
        const float goal_top = CENTER_Y - GOAL_HEIGHT * 0.5f + 10.0f;
        const float goal_bottom = CENTER_Y + GOAL_HEIGHT * 0.5f - 10.0f;
        gy = clamp_float(gy, goal_top, goal_bottom);

        target = (Vec2) {gx, gy};
        target = clamp_inside_pitch(target, self->radius);
    } else if (role == DEFENDER) {
        /* DEF: stay in own half, position between ball and own goal */
        const float ogx = own_goal_x(self->team);
        const bool in_own = ball_in_own_half(self->team, ball->position.x);

        if (in_own) {
            /* Put defender in between */
            float tx = (ball->position.x + ogx) * 0.5f;

            /* Slight vertical separation by kit */
            float offset = 0.0f;
            if (self->kit == 2) offset = -70.0f;
            if (self->kit == 4) offset = 70.0f;

            target = (Vec2) {tx, ball->position.y + offset};
        } else {
            /* return to home position, slight y-follow */
            Vec2 home = get_positions(self->team, self->kit);
            target.x = home.x;
            target.y = home.y + (ball->position.y - home.y) * 0.25f;
        }

        target = clamp_inside_pitch(target, self->radius);
    } else {
        /* ATTACKERS: only the closest (chaser) aggressively goes to the ball */
        const bool is_chaser = find_chaser(scene, self->team) == self;

        if (ball->possessor == self) {
            /* Dribble towards opponent goal */
            Vec2 goal = opponent_goal_center(self->team);
            target = (Vec2) {goal.x,
                             clamp_float(ball->position.y, (float) PITCH_Y + 50.0f,
                                         (float) (PITCH_Y + PITCH_H) - 50.0f)};

            /* If pressured, drift slightly away from center to avoid crowd */
            if (opponent_is_close(scene, self, 85.0f)) {
                const float sign = (ball->position.y < (float) CENTER_Y) ? 1.0f : -1.0f;
                target.y = clamp_float(target.y + 90.0f * sign, (float) PITCH_Y + 60.0f,
                                       (float) (PITCH_Y + PITCH_H) - 60.0f);
            }
        } else if (is_chaser &&
                   ((ball->possessor != NULL && ball->possessor->team != self->team) || ball->possessor == NULL)) {
            /* Press opponent ball carrier */
            target = clamp_inside_pitch(ball->position, self->radius);
        } else if (!ball->possessor || ball->possessor->team != self->team) {
            target = ball->position;

            target = clamp_inside_pitch(target, self->radius);
        } else {
            /* Support: Try to stay between the ball and the opponents goal */
            target.x = ball->position.x * 0.8f + opponent_goal_center(self->team).x * 0.2f;
            target.y = ball->position.y * 0.8f + opponent_goal_center(self->team).y * 0.2f;

            target = clamp_inside_pitch(target, self->radius);
        }
    }

    /* Anti-crowd: small repulsion from teammates (if the ball is ours) */
    const struct Team *t = get_own_team_pointer(scene, self->team);
    Vec2 repel = {0.0f, 0.0f};
    const float min_d = (float) PLAYER_RADIUS * 4.0f;

    for (int i = 0; i < PLAYER_COUNT; i++) {
        const struct Player *mate = t->players[i];
        if (!mate || mate == self) continue;

        const float d = distance(self->position, mate->position);
        if (d < 1e-3f) continue;

        if (d < min_d) {
            Vec2 away = normalize_vec2(subtract_vec2(self->position, mate->position));
            float strength;
            if (ball->possessor && ball->possessor->team == self->team) {
                strength = 40.0f * (min_d - d) / min_d;
            } else {
                strength = 5.0f * (min_d - d) / min_d;
            }
            repel = add_vec2(repel, scale_vec2(away, 60.0f * strength));
        }
    }

    target = add_vec2(target, repel);
    target = clamp_inside_pitch(target, self->radius);

    /* Velocity towards target */
    Vec2 delta = subtract_vec2(target, self->position);
    const float d = hypotf(delta.x, delta.y);

    if (d < 3.0f) {
        self->velocity.x = 0.0f;
        self->velocity.y = 0.0f;
        return;
    }

    Vec2 velocity_vector = scale_vec2(normalize_vec2(delta), vmax);

    /* Ensure each component stays within vmax */
    velocity_vector.x = clamp_float(velocity_vector.x, -vmax, vmax);
    velocity_vector.y = clamp_float(velocity_vector.y, -vmax, vmax);

    self->velocity.x = velocity_vector.x;
    self->velocity.y = velocity_vector.y;
}

static void shooting_generic(struct Player *self, const struct Scene *scene) {
    struct Ball *ball = scene->ball;

    /* Determine max allowed component speed from shooting talent */
    const float vmax = max_ball_component_speed(self) * 0.95f; /* stay below cap */

    Vec2 target;

    /* Kickoff: must pass backwards into own half */
    if (ball_is_at_kickoff_position(ball)) {
        target = kickoff_backward_target(scene, self);
    }
        /* Restart from out/corner: keep it inside */
    else if (ball_near_pitch_edge(ball)) {
        target = safe_restart_target(scene, self);

        /* If we see a clear pass to a teammate inside, prefer it */
        struct Player *mate = find_best_open_teammate_ahead(scene, self);
        if (mate) target = clamp_inside_pitch(mate->position, (float) BALL_RADIUS + 8.0f);
    } else {
        /* Normal play: pass if open teammate ahead exists, else shoot if near goal */
        struct Player *mate = find_best_open_teammate_ahead(scene, self);

        const Vec2 goal = opponent_goal_center(self->team);

        const float goal_dist = distance(ball->position, goal);

        if (mate) {
            target = clamp_inside_pitch(mate->position, (float) BALL_RADIUS + 8.0f);
        } else if (goal_dist < 260.0f) {
            target = goal; /* attempt a shot */
        } else {
            /* no open pass: dribble-like kick slightly forward */
            const float dir = opponent_direction(self->team);
            target = (Vec2) {ball->position.x + dir * 180.0f,
                             clamp_float(ball->position.y, (float) PITCH_Y + 60.0f,
                                         (float) (PITCH_Y + PITCH_H) - 60.0f)};
            target = clamp_inside_pitch(target, (float) BALL_RADIUS + 8.0f);
        }
    }

    Vec2 delta = subtract_vec2(target, ball->position);
    Vec2 direction_vector = normalize_vec2(delta);
    Vec2 velocity = scale_vec2(direction_vector, vmax);

    /* ensure component-wise within vmax */
    velocity.x = clamp_float(velocity.x, -vmax, vmax);
    velocity.y = clamp_float(velocity.y, -vmax, vmax);

    /* never completely zero */
    if (fabsf(velocity.x) < 0.5f && fabsf(velocity.y) < 0.5f) {
        velocity.x = (self->team == 1) ? -1.0f : 1.0f;
        velocity.y = 0.0f;
    }

    ball->velocity.x = velocity.x;
    ball->velocity.y = velocity.y;
}

static void change_state_generic(struct Player *self, const struct Scene *scene) {
    const struct Ball *ball = scene->ball;
    const Role role = role_of_kit(self->kit);

    const float collide_r = self->radius + ball->radius;
    const float d = distance(self->position, ball->position);

    const bool has_ball = (ball->possessor == self);
    const bool teammate_has_ball = (ball->possessor && ball->possessor->team == self->team);
    const bool opponent_has_ball = (ball->possessor && ball->possessor->team != self->team);

    if (has_ball) {
        /* Decide to shoot/pass if:
           - open teammate ahead, OR
           - near opponent goal, OR
           - under pressure
        */

        const Vec2 goal = opponent_goal_center(self->team);
        const float goal_dist = distance(ball->position, goal);

        struct Player *mate = find_best_open_teammate_ahead(scene, self);
        const bool pressured = opponent_is_close(scene, self, 80.0f);

        if (mate || goal_dist < 260.0f || pressured || role == GOALKEEPER || role == DEFENDER) {
            self->state = SHOOTING;
        } else {
            self->state = MOVING;
        }
        return;
    }

    /* If teammate has ball, don't intercept it */
    if (teammate_has_ball) {
        self->state = MOVING;
        return;
    }

    /* If ball is free or opponent has ball, intercept only when close enough */
    if ((ball->possessor == NULL || opponent_has_ball) && d <= collide_r &&
        hypotf(ball->position.x - CENTER_X, ball->position.y - CENTER_Y) >= 30 &&
        hypotf(ball->position.x - PITCH_X, ball->position.y - PITCH_Y) >= 20 &&
        hypotf(ball->position.x - PITCH_X, ball->position.y - (SCREEN_HEIGHT - PITCH_MARGIN)) >= 20 &&
        hypotf(ball->position.x - (SCREEN_WIDTH - PITCH_MARGIN), ball->position.y - CENTER_Y) >= 20 &&
        hypotf(ball->position.x - (SCREEN_WIDTH - PITCH_MARGIN), ball->position.y - (SCREEN_HEIGHT - PITCH_MARGIN)) >=
        20) {
        self->state = INTERCEPTING;
        return;
    }

    /* default */
    self->state = MOVING;
}

/* -------------------------------------------------------------------------
 * Logic Functions
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
void movement_logic_1_0(struct Player *self, struct Scene *scene) { movement_generic(self, scene); }

void movement_logic_1_1(struct Player *self, struct Scene *scene) { movement_generic(self, scene); }

void movement_logic_1_2(struct Player *self, struct Scene *scene) { movement_generic(self, scene); }

void movement_logic_1_3(struct Player *self, struct Scene *scene) { movement_generic(self, scene); }

void movement_logic_1_4(struct Player *self, struct Scene *scene) { movement_generic(self, scene); }

void movement_logic_1_5(struct Player *self, struct Scene *scene) { movement_generic(self, scene); }

/* Team 2 movement logic */
void movement_logic_2_0(struct Player *self, struct Scene *scene) { movement_generic(self, scene); }

void movement_logic_2_1(struct Player *self, struct Scene *scene) { movement_generic(self, scene); }

void movement_logic_2_2(struct Player *self, struct Scene *scene) { movement_generic(self, scene); }

void movement_logic_2_3(struct Player *self, struct Scene *scene) { movement_generic(self, scene); }

void movement_logic_2_4(struct Player *self, struct Scene *scene) { movement_generic(self, scene); }

void movement_logic_2_5(struct Player *self, struct Scene *scene) { movement_generic(self, scene); }

/* Team 1 shooting logic */
void shooting_logic_1_0(struct Player *self, struct Scene *scene) { shooting_generic(self, scene); }

void shooting_logic_1_1(struct Player *self, struct Scene *scene) { shooting_generic(self, scene); }

void shooting_logic_1_2(struct Player *self, struct Scene *scene) { shooting_generic(self, scene); }

void shooting_logic_1_3(struct Player *self, struct Scene *scene) { shooting_generic(self, scene); }

void shooting_logic_1_4(struct Player *self, struct Scene *scene) { shooting_generic(self, scene); }

void shooting_logic_1_5(struct Player *self, struct Scene *scene) { shooting_generic(self, scene); }

/* Team 2 shooting logic */
void shooting_logic_2_0(struct Player *self, struct Scene *scene) { shooting_generic(self, scene); }

void shooting_logic_2_1(struct Player *self, struct Scene *scene) { shooting_generic(self, scene); }

void shooting_logic_2_2(struct Player *self, struct Scene *scene) { shooting_generic(self, scene); }

void shooting_logic_2_3(struct Player *self, struct Scene *scene) { shooting_generic(self, scene); }

void shooting_logic_2_4(struct Player *self, struct Scene *scene) { shooting_generic(self, scene); }

void shooting_logic_2_5(struct Player *self, struct Scene *scene) { shooting_generic(self, scene); }

/* Team 1 change_state logic */
void change_state_logic_1_0(struct Player *self, struct Scene *scene) { change_state_generic(self, scene); }

void change_state_logic_1_1(struct Player *self, struct Scene *scene) { change_state_generic(self, scene); }

void change_state_logic_1_2(struct Player *self, struct Scene *scene) { change_state_generic(self, scene); }

void change_state_logic_1_3(struct Player *self, struct Scene *scene) { change_state_generic(self, scene); }

void change_state_logic_1_4(struct Player *self, struct Scene *scene) { change_state_generic(self, scene); }

void change_state_logic_1_5(struct Player *self, struct Scene *scene) { change_state_generic(self, scene); }

/* Team 2 change_state logic */
void change_state_logic_2_0(struct Player *self, struct Scene *scene) { change_state_generic(self, scene); }

void change_state_logic_2_1(struct Player *self, struct Scene *scene) { change_state_generic(self, scene); }

void change_state_logic_2_2(struct Player *self, struct Scene *scene) { change_state_generic(self, scene); }

void change_state_logic_2_3(struct Player *self, struct Scene *scene) { change_state_generic(self, scene); }

void change_state_logic_2_4(struct Player *self, struct Scene *scene) { change_state_generic(self, scene); }

void change_state_logic_2_5(struct Player *self, struct Scene *scene) { change_state_generic(self, scene); }

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
/*
 * striker/attacker
 * 1: winger
 * 2: defender
 * 3: goalkeeper
 * 4: defender
 * 5: winger
 * Each row: {defence, agility, dribbling, shooting} , sum <= MAX_TALENT_PER_PLAYER (20)
*/

/* Team 1 */
static struct Talents team1_talents[6] = {
        {2, 6, 6, 6},
        {3, 6, 6, 5},
        {8, 6, 3, 3},
        {5, 5, 7, 3},
        {8, 6, 3, 3},
        {3, 7, 5, 5},
};

/* Team 2 */
static struct Talents team2_talents[6] = {
        {4, 6, 5, 5},
        {2, 7, 5, 6},
        {8, 5, 4, 3},
        {4, 6, 7, 3},
        {8, 5, 4, 3},
        {3, 6, 5, 6},
};

struct Talents get_talents(int team, int kit) {
    if (coach_both_teams) return team1_talents[kit];
    return (team == 1) ? team1_talents[kit] : team2_talents[kit];
}


/* ------------------------------------------------------------------------- *
 * Positioning
 * Players must stay on their half, outside center circle at kickoff.
 * ------------------------------------------------------------------------- */

/* Team 1 */
static struct Vec2 team1_positions[6] = {
        {325, CENTER_Y},
        {425, CENTER_Y + 125},
        {200, CENTER_Y + 75},
        {80,  CENTER_Y},
        {200, CENTER_Y - 75},
        {425, CENTER_Y - 125},
};

/* Team 2 */
static struct Vec2 team2_positions[6] = {
        {SCREEN_WIDTH - 325, CENTER_Y},
        {SCREEN_WIDTH - 425, CENTER_Y + 125},
        {SCREEN_WIDTH - 200, CENTER_Y + 75},
        {SCREEN_WIDTH - 80,  CENTER_Y},
        {SCREEN_WIDTH - 200, CENTER_Y - 75},
        {SCREEN_WIDTH - 425, CENTER_Y - 125},
};

struct Vec2 get_positions(int team, int kit) {
    return (team == 1) ? team1_positions[kit] : team2_positions[kit];
}
