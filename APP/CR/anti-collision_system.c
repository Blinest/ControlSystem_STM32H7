//
// Created by q3634 on 2026/5/14.
//

#include "anti-collision_system.h"

#include <float.h>
#include <stdlib.h>

// 底层距离函数（基于 robot_params_t 和 robot_state_t）
static double point_to_arc_segment(const robot_params_t *param, const robot_state_t *state,
                                   int seg_idx, vec3_t point, vec3_t *closest) {
    double theta = state->psi[1 + 2*seg_idx];
    double alpha = state->psi[2 + 2*seg_idx];
    double L = param->L[seg_idx];
    vec3_t start, end; double Rtmp[3][3];
    body_point_fk(param, state, seg_idx, 0.0, &start, Rtmp);
    body_point_fk(param, state, seg_idx, 1.0, &end, Rtmp);
    if (fabs(theta) < EPS) {
        // 直线段处理
        vec3_t dir = { end.x - start.x, end.y - start.y, end.z - start.z };
        double len = vec3_norm(dir);
        if (len < EPS) {
            *closest = start;
            return vec3_norm(vec3_sub(point, start));
        }
        vec3_t w = vec3_sub(point, start);
        double t = vec3_dot(w, dir) / (len*len);
        if (t <= 0) { *closest = start; return vec3_norm(w); }
        else if (t >= 1) { *closest = end; return vec3_norm(vec3_sub(point, end)); }
        else {
            closest->x = start.x + t * dir.x;
            closest->y = start.y + t * dir.y;
            closest->z = start.z + t * dir.z;
            return vec3_norm(vec3_sub(point, *closest));
        }
    } else {
        // 圆弧段
        double rho = L / theta;
        double plane_x = cos(alpha), plane_y = sin(alpha);
        vec3_t center;
        center.x = start.x - rho * (1 - cos(theta)) * plane_x;
        center.y = start.y - rho * (1 - cos(theta)) * plane_y;
        center.z = start.z - rho * sin(theta);
        vec3_t v_obs = vec3_sub(point, center);
        vec3_t v1 = vec3_sub(start, center);
        vec3_t v2 = vec3_sub(end, center);
        double r = vec3_norm(v1);
        double angle_start = atan2(v1.y, v1.x);
        double angle_end   = atan2(v2.y, v2.x);
        double angle_obs   = atan2(v_obs.y, v_obs.x);
        double angle_diff = fmod(angle_obs - angle_start, 2*PI);
        if (angle_diff < 0) angle_diff += 2*PI;
        double sweep = fmod(angle_end - angle_start, 2*PI);
        if (sweep < 0) sweep += 2*PI;
        if (angle_diff <= sweep) {
            double dist = fabs(r - vec3_norm(v_obs));
            double scale = r / vec3_norm(v_obs);
            closest->x = center.x + scale * v_obs.x;
            closest->y = center.y + scale * v_obs.y;
            closest->z = center.z + scale * v_obs.z;
            return dist;
        } else {
            double d_start = vec3_norm(vec3_sub(point, start));
            double d_end   = vec3_norm(vec3_sub(point, end));
            if (d_start < d_end) { *closest = start; return d_start; }
            else                 { *closest = end;   return d_end; }
        }
    }
}

double min_distance_robot_obstacle(const LQTS *lqts, const obstacle_t *obs,
                                   vec3_t *nearest_robot, vec3_t *nearest_obs) {
    robot_params_t params;
    double L_array[1];
    robot_params_from_lqts(lqts, &params, L_array);
    robot_state_t state;
    robot_state_from_lqts(lqts, &state);
    double min_d = DBL_MAX;
    for (int k = 0; k < obs->num_points; k++) {
        for (int i = 0; i < params.n_seg; i++) {
            vec3_t closest;
            double d = point_to_arc_segment(&params, &state, i, obs->points[k], &closest);
            if (d < min_d) {
                min_d = d;
                *nearest_robot = closest;
                *nearest_obs = obs->points[k];
            }
        }
    }
    free(state.psi);
    return min_d;
}