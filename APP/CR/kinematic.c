#include <stdint.h>
#include <math.h>

#include "kinematic.h"

#define pi 3.1415926535
/**********************************************************
***	编写作者：blinest

***	qq：1071378062
**********************************************************/
void calculate_L(float R[], float theta[], float phi, float deltaL[]) {
	deltaL[0] = -R[0] * theta[0] * cos(phi + 2.0 / 3.0 * pi);
	deltaL[1] = -R[0] * theta[0] * cos(phi + 4.0 / 3.0 * pi);
	deltaL[2] = -R[0] * theta[0] * cos(phi);
	deltaL[3] = deltaL[0] - R[1] * theta[1] * cos(phi + 2.0 / 3.0 * pi);
	deltaL[4] = deltaL[1] - R[1] * theta[1] * cos(phi + 4.0 / 3.0 * pi);
	deltaL[5] = deltaL[2] - R[1] * theta[1] * cos(phi);
	deltaL[6] = deltaL[3] - R[2] * theta[2] * cos(phi + 2.0 / 3.0 * pi);
	deltaL[7] = deltaL[4] - R[2] * theta[2] * cos(phi + 4.0 / 3.0 * pi);
	deltaL[8] = deltaL[5] - R[2] * theta[2] * cos(phi);
}
// ========== 肌腱补偿模型 ==========
// 非线性补偿: commanded_deg = a * desired_deg + b * desired_deg^2
// a —— 线性项（效率系数倒数），b —— 二次项（大角度效率衰减/饱和）
// 实验表明系统存在显著非线性（70°命令仅产生20°弯曲），
// 故引入二次补偿模型，当b<0时实现大角度饱和（效率随角度增大而降低）。
// 限幅 [min_ratio, max_ratio] 作为安全边界。

int direction_to_index(char direction) {
    switch(direction) {
        case 'u': return 0;
        case 'r': return 1;
        case 'd': return 2;
        case 'l': return 3;
        default: return 0;
    }
}

// 非线性补偿
double tendonCompensation(int seg, char direction, float angle_deg)
{
    int dir_idx = direction_to_index(direction);
    double angle_rad = angle_deg * pi / 180.0;

    // 使用 calib_a / calib_b 做非线性映射
    double a = CR.arm_params[seg-1].calib_a[dir_idx];
    double b = CR.arm_params[seg-1].calib_b[dir_idx];

    // 命令角度（度） = a * desired + b * desired^2
    double cmd_deg = a * angle_deg + b * angle_deg * angle_deg;
    if (cmd_deg < 0) cmd_deg = 0;

    // 安全限幅（用户可配置的 clamp 范围）
    double max_ratio = CR.arm_params[seg-1].calib_max_ratio;
    double min_ratio = CR.arm_params[seg-1].calib_min_ratio;
    double min_allowed = min_ratio * angle_deg;
    double max_allowed = max_ratio * angle_deg;

    if (cmd_deg < min_allowed) {
        cmd_deg = min_allowed;
    }
    else if (cmd_deg > max_allowed) {
        cmd_deg = max_allowed;
    }

    // 返回弧度（供下游 theta 分配使用）
    return cmd_deg * pi / 180.0;
}