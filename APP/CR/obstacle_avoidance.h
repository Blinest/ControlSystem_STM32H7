//
// Created by q3634 on 2026/5/14.
//

#ifndef CONTROLSYSTEM_STM32H7_OBSTACLE_AVOIDANCE_H
#define CONTROLSYSTEM_STM32H7_OBSTACLE_AVOIDANCE_H

#include "CR.h"
#include "anti-collision_system.h"

typedef struct {
	double influence_dist;   // dl
	double kr;               // 斥力系数
	int gamma;               // 指数
} avoidance_params_t;

// 计算斥力 f_r (长度 dof)
void compute_repulsive_force(const LQTS *lqts, const obstacle_t *obs,
							 const avoidance_params_t *avp, double *f_r);

#endif //CONTROLSYSTEM_STM32H7_OBSTACLE_AVOIDANCE_H