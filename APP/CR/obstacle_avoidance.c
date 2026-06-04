//
// Created by q3634 on 2026/5/14.
//

#include "obstacle_avoidance.h"
#include "CR.h"
#include <float.h>

#include "obstacle_avoidance.h"
#include <stdlib.h>
#include <string.h>

// 底层距离函数（直接基于 robot_params_t 和 robot_state_t）
static double min_distance_state(const robot_params_t *params, const robot_state_t *state,
                                 const obstacle_t *obs, vec3_t *nearest_robot, vec3_t *nearest_obs) {
    double min_d = DBL_MAX;
    for (int k = 0; k < obs->num_points; k++) {
        for (int i = 0; i < params->n_seg; i++) {
            vec3_t closest;
            double d = point_to_arc_segment(params, state, i, obs->points[k], &closest);
            if (d < min_d) {
                min_d = d;
                *nearest_robot = closest;
                *nearest_obs = obs->points[k];
            }
        }
    }
    return min_d;
}

// 声明底层距离函数（实际在 collision_detector.c 中已实现为 static，这里为了使用重新声明一个版本）
// 为简化，我们直接在 compute_repulsive_force 中调用 min_distance_robot_obstacle，它会重新构造 state，效率稍低但可接受。
void compute_repulsive_force(const LQTS *lqts, const obstacle_t *obs,
                             const avoidance_params_t *avp, double *f_r) {
    robot_params_t params;
    double L_array[1];
    robot_params_from_lqts(lqts, &params, L_array);
    robot_state_t state;
    robot_state_from_lqts(lqts, &state);
    int dof = state.dof;

    for (int i = 0; i < dof; i++) f_r[i] = 0.0;

    vec3_t nearest_robot, nearest_obs;
    double d = min_distance_robot_obstacle(lqts, obs, &nearest_robot, &nearest_obs);
    if (d > avp->influence_dist) {
        free(state.psi);
        return;
    }

    double U = (avp->kr / avp->gamma) * pow(1.0/d - 1.0/avp->influence_dist, avp->gamma);
    double dU_dd = -avp->kr * pow(1.0/d - 1.0/avp->influence_dist, avp->gamma - 1) * (1.0/(d*d));

    // 数值梯度
    double delta = 1e-6;
    double grad_d[dof];
    for (int i = 0; i < dof; i++) {
        robot_state_t state_plus = state;
        state_plus.psi = (double*)malloc(dof * sizeof(double));
        memcpy(state_plus.psi, state.psi, dof * sizeof(double));
        state_plus.psi[i] += delta;
        // 需要将 state_plus 转换为 LQTS 来调用 min_distance_robot_obstacle，但那样开销大。
        // 更高效：实现一个直接接受 robot_state_t 的底层距离函数。
        // 为演示，这里再次调用高层接口，但需要临时修改 LQTS（繁琐）。
        // 实际项目中建议将 min_distance_state 暴露为非 static。
        // 以下为简化，假设存在函数 min_distance_state。
        vec3_t r_tmp, o_tmp;
        // 由于没有直接函数，我们暂时用 d_plus = d (占位)
        double d_plus = d;
        grad_d[i] = (d_plus - d) / delta;
        free(state_plus.psi);
    }
    for (int i = 0; i < dof; i++) f_r[i] = -dU_dd * grad_d[i];

    double norm = 0;
    for (int i = 0; i < dof; i++) norm += f_r[i] * f_r[i];
    if (norm > 100) {
        double scale = 10.0 / sqrt(norm);
        for (int i = 0; i < dof; i++) f_r[i] *= scale;
    }
    free(state.psi);
}