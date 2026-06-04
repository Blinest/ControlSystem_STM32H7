//
// Created by q3634 on 2026/5/14.
//

#ifndef CONTROLSYSTEM_STM32H7_MASTER_CONTROL_H
#define CONTROLSYSTEM_STM32H7_MASTER_CONTROL_H
#include "CR.h"

typedef struct {
	double Kp;          // 正定增益矩阵（对角）
	double lambda_dls;  // 阻尼因子
} control_params_t;

// 计算配置速度 dpsi (长度 dof)
void compute_control(const LQTS *lqts, const vec3_t *pos_des, const double R_des[3][3],
					 const double *Vd, const double *f_r, const control_params_t *ctrl,
					 double *dpsi);

#endif //CONTROLSYSTEM_STM32H7_MASTER_CONTROL_H