#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "kinematic.h"

#define pi 3.1415926535
/**********************************************************
***	编写作者：blinest

***	qq：1071378062
**********************************************************/
void calculate_L(uint8_t R, float theta, float phi, float deltaL[]) {
	deltaL[1] = R * theta * cos(phi);
	deltaL[2] = R * theta * cos(phi + 2.0 / 3.0 * pi);
	deltaL[3] = R * theta * cos(phi + 4.0 / 3.0 * pi);
}
// ========== 正运动学 ==========
void forward_kinematics(const robot_params_t *param, const robot_state_t *state,
						vec3_t *pos, double R[3][3]) {
	double d = state->psi[0];
	se3_t T = { .m = {{1,0,0,0},{0,1,0,0},{0,0,1,d},{0,0,0,1}} };
	for (int i = 0; i < param->n_seg; i++) {
		double theta = state->psi[1 + 2*i];
		double alpha = state->psi[2 + 2*i];
		double L = param->L[i];
		se3_t seg;
		if (fabs(theta) < EPS) {
			mat3_identity(seg.m);
			seg.m[0][3] = 0; seg.m[1][3] = 0; seg.m[2][3] = L;
		} else {
			double rho = L / theta;
			vec3_t axis = { -sin(alpha), cos(alpha), 0 };
			double norm = sqrt(axis.x*axis.x + axis.y*axis.y + axis.z*axis.z);
			axis.x /= norm; axis.y /= norm; axis.z /= norm;
			mat3_rot_axis_angle(axis, theta, seg.m);
			seg.m[0][3] = rho * (1 - cos(theta)) * cos(alpha);
			seg.m[1][3] = rho * (1 - cos(theta)) * sin(alpha);
			seg.m[2][3] = rho * sin(theta);
		}
		seg.m[3][0] = seg.m[3][1] = seg.m[3][2] = 0; seg.m[3][3] = 1;
		se3_t T_new;
		// 矩阵乘法 T = T * seg
		for (int r = 0; r < 4; r++)
			for (int c = 0; c < 4; c++) {
				T_new.m[r][c] = 0;
				for (int k = 0; k < 4; k++)
					T_new.m[r][c] += T.m[r][k] * seg.m[k][c];
			}
		T = T_new;
	}
	// 末端旋转
	double ar = state->psi[param->dof - 1];
	double R_end[3][3] = { {cos(ar), -sin(ar), 0}, {sin(ar), cos(ar), 0}, {0,0,1} };
	double R_cur[3][3];
	for (int i=0;i<3;i++) for(int j=0;j<3;j++) R_cur[i][j] = T.m[i][j];
	mat3_mul(R_cur, R_end, R);
	pos->x = T.m[0][3]; pos->y = T.m[1][3]; pos->z = T.m[2][3];
}
void body_point_fk(const robot_params_t *param, const robot_state_t *state,
                   int seg_idx, double eta, vec3_t *point, double R[3][3]) {
    double d = state->psi[0];
    se3_t T = { .m = {{1,0,0,0},{0,1,0,0},{0,0,1,d},{0,0,0,1}} };
    // 累积到 seg_idx 段之前
    for (int i = 0; i < seg_idx; i++) {
        double theta = state->psi[1 + 2*i];
        double alpha = state->psi[2 + 2*i];
        double L = param->L[i];
        se3_t seg;
        if (fabs(theta) < EPS) {
            mat3_identity(seg.m);
            seg.m[0][3] = 0; seg.m[1][3] = 0; seg.m[2][3] = L;
        } else {
            double rho = L / theta;
            vec3_t axis = { -sin(alpha), cos(alpha), 0 };
            double norm = sqrt(axis.x*axis.x + axis.y*axis.y + axis.z*axis.z);
            axis.x /= norm; axis.y /= norm; axis.z /= norm;
            mat3_rot_axis_angle(axis, theta, seg.m);
            seg.m[0][3] = rho * (1 - cos(theta)) * cos(alpha);
            seg.m[1][3] = rho * (1 - cos(theta)) * sin(alpha);
            seg.m[2][3] = rho * sin(theta);
        }
        seg.m[3][0]=seg.m[3][1]=seg.m[3][2]=0; seg.m[3][3]=1;
        se3_t T_new;
        for (int r=0;r<4;r++) for(int c=0;c<4;c++) {
            T_new.m[r][c] = 0;
            for(int k=0;k<4;k++) T_new.m[r][c] += T.m[r][k] * seg.m[k][c];
        }
        T = T_new;
    }
    // 当前段的 eta 部分
    double theta = state->psi[1 + 2*seg_idx];
    double alpha = state->psi[2 + 2*seg_idx];
    double L = param->L[seg_idx];
    se3_t seg_eta;
    double theta_eta = eta * theta;
    if (fabs(theta) < EPS) {
        mat3_identity(seg_eta.m);
        seg_eta.m[0][3] = 0; seg_eta.m[1][3] = 0; seg_eta.m[2][3] = eta * L;
    } else {
        double rho = L / theta;
        vec3_t axis = { -sin(alpha), cos(alpha), 0 };
        double norm = sqrt(axis.x*axis.x + axis.y*axis.y + axis.z*axis.z);
        axis.x /= norm; axis.y /= norm; axis.z /= norm;
        mat3_rot_axis_angle(axis, theta_eta, seg_eta.m);
        seg_eta.m[0][3] = rho * (1 - cos(theta_eta)) * cos(alpha);
        seg_eta.m[1][3] = rho * (1 - cos(theta_eta)) * sin(alpha);
        seg_eta.m[2][3] = rho * sin(theta_eta);
    }
    seg_eta.m[3][0]=seg_eta.m[3][1]=seg_eta.m[3][2]=0; seg_eta.m[3][3]=1;
    se3_t T_total;
    for (int r=0;r<4;r++) for(int c=0;c<4;c++) {
        T_total.m[r][c] = 0;
        for(int k=0;k<4;k++) T_total.m[r][c] += T.m[r][k] * seg_eta.m[k][c];
    }
    point->x = T_total.m[0][3]; point->y = T_total.m[1][3]; point->z = T_total.m[2][3];
    for (int i=0;i<3;i++) for(int j=0;j<3;j++) R[i][j] = T_total.m[i][j];
}

// 数值雅可比
void jacobian_numerical(const robot_params_t *param, const robot_state_t *state,
                        int dof, double J[][dof]) {
    vec3_t pos0; double R0[3][3];
    forward_kinematics(param, state, &pos0, R0);
    double delta = 1e-6;
    for (int j = 0; j < dof; j++) {
        robot_state_t state_plus = *state;
        state_plus.psi = (double*)malloc(dof * sizeof(double));
        memcpy(state_plus.psi, state->psi, dof*sizeof(double));
        state_plus.psi[j] += delta;
        vec3_t pos1; double R1[3][3];
        forward_kinematics(param, &state_plus, &pos1, R1);
        J[0][j] = (pos1.x - pos0.x) / delta;
        J[1][j] = (pos1.y - pos0.y) / delta;
        J[2][j] = (pos1.z - pos0.z) / delta;
        double R_rel[3][3];
        double R0_T[3][3];
        for (int i=0;i<3;i++) for(int k=0;k<3;k++) R0_T[i][k] = R0[k][i];
        mat3_mul(R1, R0_T, R_rel);
        double trace = R_rel[0][0] + R_rel[1][1] + R_rel[2][2];
        double theta = acos(fmax(-1.0, fmin(1.0, (trace-1)/2)));
        if (fabs(theta) < EPS) {
            J[3][j] = J[4][j] = J[5][j] = 0;
        } else {
            double s = 0.5 * theta / sin(theta);
            J[3][j] = s * (R_rel[2][1] - R_rel[1][2]) / delta;
            J[4][j] = s * (R_rel[0][2] - R_rel[2][0]) / delta;
            J[5][j] = s * (R_rel[1][0] - R_rel[0][1]) / delta;
        }
        free(state_plus.psi);
    }
}