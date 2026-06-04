//
// Created by q3634 on 2026/5/14.
//

#ifndef CONTROLSYSTEM_STM32H7_ANTI_COLLISION_SYSTEM_H
#define CONTROLSYSTEM_STM32H7_ANTI_COLLISION_SYSTEM_H
#include "CR.h"
typedef struct {
	vec3_t *points;   // 障碍物点云 (外部管理)
	int num_points;
} obstacle_t;

typedef struct {
	vec3_t *points;     // 障碍物点云数组
	int num_points;
} obstacle_t;

// 计算机器人与障碍物的最小距离（使用 LQTS）
double min_distance_robot_obstacle(const LQTS *lqts, const obstacle_t *obs,
								   vec3_t *nearest_robot, vec3_t *nearest_obs);

#endif //CONTROLSYSTEM_STM32H7_ANTI_COLLISION_SYSTEM_H