#include <stdint.h>
#include <math.h>

#include "kinematic.h"

#define pi 3.1415926535
/**********************************************************
***	编写作者：blinest

***	qq：1071378062
**********************************************************/
void calculate_L(float R[], float theta[], float phi, float deltaL[]) {
    // 中心杆压缩偏置: 从结构体获取 r_bias，避免与 CR_init 不一致
    float r_bias = CR.joint_space.r_bias;

    float R_eff[3];
    for (int i = 0; i < 3; i++)
        R_eff[i] = r_bias * cosf(phi);
    deltaL[0] = -(1+R_eff[0])*R[0] * theta[0] * cos(phi + 2.0 / 3.0 * pi);
    deltaL[1] = -(1+R_eff[0])*R[0] * theta[0] * cos(phi + 4.0 / 3.0 * pi);
    deltaL[2] = -(1-R_eff[0])*R[0] * theta[0] * cos(phi);
    deltaL[3] = deltaL[0] - (1+R_eff[1])*R[1] * theta[1] * cos(phi + 2.0 / 3.0 * pi);
    deltaL[4] = deltaL[1] - (1+R_eff[1])*R[1] * theta[1] * cos(phi + 4.0 / 3.0 * pi);
    deltaL[5] = deltaL[2] - (1-R_eff[1])*R[1] * theta[1] * cos(phi);
    deltaL[6] = deltaL[3] - (1+R_eff[2])*R[2] * theta[2] * cos(phi + 2.0 / 3.0 * pi);
    deltaL[7] = deltaL[4] - (1+R_eff[2])*R[2] * theta[2] * cos(phi + 4.0 / 3.0 * pi);
    deltaL[8] = deltaL[5] - (1-R_eff[2])*R[2] * theta[2] * cos(phi);
}
// ========== 肌腱补偿模型 ==========
// 非线性补偿: commanded_deg = a * desired_deg + b * desired_deg^2
// a —— 线性项（效率系数倒数），b —— 二次项（大角度效率衰减/饱和）
// 实验表明系统存在显著非线性（70°命令仅产生20°弯曲），
// 故引入二次补偿模型，当b<0时实现大角度饱和（效率随角度增大而降低）。
// 限幅 [min_ratio, max_ratio] 作为安全边界。

// 非线性补偿: cmd_deg = a * desired + b * desired^2
double tendonCompensation(int seg, float angle_deg)
{
    // 使用 calib_a / calib_b 做非线性映射
    double a = CR.arm_params[seg-1].calib_a;
    double b = CR.arm_params[seg-1].calib_b;

    // 命令角度（度） = a * desired + b * desired^2
    double cmd_deg = a * angle_deg + b * angle_deg * angle_deg;
    if (cmd_deg < 0) cmd_deg = 0;

    // 安全限幅
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

    return cmd_deg * pi / 180.0;
}