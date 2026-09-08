#include <stdint.h>
#include <math.h>

#include "kinematic.h"

#define pi 3.1415926535

void calculate_L(float R[], float theta[], float phi, float deltaL[]) {
    // 中心杆压缩偏置
    float r_bias = CR.joint_space.r_bias;

    deltaL[0] = (cos(phi + 1.0 / 3.0 * pi) + r_bias*fabs(cos(phi + 1.0 / 3.0 * pi)))*R[0] * theta[0];
    deltaL[1] = (cos(-phi + 1.0 / 3.0 * pi) + r_bias*fabs(cos(-phi + 1.0 / 3.0 * pi)))*R[0] * theta[0];
    deltaL[2] = -(cos(phi) - r_bias*fabs(cos(phi)))*R[0] * theta[0];
    deltaL[3] = deltaL[0] + (cos(phi + 1.0 / 3.0 * pi) + r_bias*fabs(cos(phi + 1.0 / 3.0 * pi)))*R[1] * theta[1];
    deltaL[4] = deltaL[1] + (cos(-phi + 1.0 / 3.0 * pi) + r_bias*fabs(cos(-phi + 1.0 / 3.0 * pi)))*R[1] * theta[1];
    deltaL[5] = deltaL[2] - (cos(phi) - r_bias*fabs(cos(phi)))*R[1] * theta[1];
    deltaL[6] = deltaL[3] + (cos(phi + 1.0 / 3.0 * pi) + r_bias*fabs(cos(phi + 1.0 / 3.0 * pi)))*R[2] * theta[2];
    deltaL[7] = deltaL[4] + (cos(-phi + 1.0 / 3.0 * pi) + r_bias*fabs(cos(-phi + 1.0 / 3.0 * pi)))*R[2] * theta[2];
    deltaL[8] = deltaL[5] - (cos(phi) - r_bias*fabs(cos(phi)))*R[2] * theta[2];
}