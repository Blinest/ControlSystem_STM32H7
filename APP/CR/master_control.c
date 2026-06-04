//
// Created by q3634 on 2026/5/14.
//

#include "master_control.h"
#include "kinematic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "stdbool.h"
// 解 6x6 线性系统（用于阻尼伪逆）
static bool solve_6x6(const double A[6][6], const double b[6], double x[6]) {
    double M[6][7];
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) M[i][j] = A[i][j];
        M[i][6] = b[i];
    }
    for (int i = 0; i < 6; i++) {
        int max_row = i;
        for (int r = i+1; r < 6; r++)
            if (fabs(M[r][i]) > fabs(M[max_row][i])) max_row = r;
        if (fabs(M[max_row][i]) < 1e-12) return false;
        if (max_row != i) {
            for (int c = i; c <= 6; c++) {
                double tmp = M[i][c];
                M[i][c] = M[max_row][c];
                M[max_row][c] = tmp;
            }
        }
        for (int r = i+1; r < 6; r++) {
            double factor = M[r][i] / M[i][i];
            for (int c = i; c <= 6; c++) M[r][c] -= factor * M[i][c];
        }
    }
    for (int i = 5; i >= 0; i--) {
        x[i] = M[i][6];
        for (int j = i+1; j < 6; j++) x[i] -= M[i][j] * x[j];
        x[i] /= M[i][i];
    }
    return true;
}

// 计算阻尼伪逆 J* = J^T (J J^T + λ^2 I)^{-1}
static void damped_pseudoinverse(int dof, const double J[][dof], double lambda,
                                 double Jstar[][6]) {
    double JJT[6][6] = {{0}};
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            double sum = 0;
            for (int k = 0; k < dof; k++) sum += J[i][k] * J[j][k];
            JJT[i][j] = sum;
        }
        JJT[i][i] += lambda * lambda;
    }
    double JJT_inv[6][6];
    double I6[6][6] = {{0}};
    for (int i = 0; i < 6; i++) I6[i][i] = 1.0;
    for (int col = 0; col < 6; col++) {
        double b[6];
        for (int i = 0; i < 6; i++) b[i] = I6[i][col];
        double x[6];
        if (!solve_6x6(JJT, b, x)) {
            // 奇异，返回零矩阵
            memset(JJT_inv, 0, sizeof(JJT_inv));
            break;
        }
        for (int i = 0; i < 6; i++) JJT_inv[i][col] = x[i];
    }
    // Jstar = J^T * JJT_inv
    for (int i = 0; i < dof; i++) {
        for (int j = 0; j < 6; j++) {
            double sum = 0;
            for (int k = 0; k < 6; k++) sum += J[k][i] * JJT_inv[k][j];
            Jstar[i][j] = sum;
        }
    }
}

void compute_control(const LQTS *lqts, const vec3_t *pos_des, const double R_des[3][3],
                     const double *Vd, const double *f_r, const control_params_t *ctrl,
                     double *dpsi) {
    robot_params_t params;
    double L_array[1];
    robot_params_from_lqts(lqts, &params, L_array);
    robot_state_t state;
    robot_state_from_lqts(lqts, &state);
    int dof = state.dof;

    vec3_t pos_cur; double R_cur[3][3];
    forward_kinematics(&params, &state, &pos_cur, R_cur);

    // 位姿误差
    double e[6];
    e[0] = pos_des->x - pos_cur.x;
    e[1] = pos_des->y - pos_cur.y;
    e[2] = pos_des->z - pos_cur.z;

    double R_curT[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            R_curT[i][j] = R_cur[j][i];
    double R_err[3][3];
    // mat3_mul
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            R_err[i][j] = 0;
            for (int k = 0; k < 3; k++)
                R_err[i][j] += R_des[i][k] * R_curT[k][j];
        }
    double trace = R_err[0][0] + R_err[1][1] + R_err[2][2];
    double theta_err = acos(fmax(-1.0, fmin(1.0, (trace - 1.0) / 2.0)));
    if (fabs(theta_err) < EPS) {
        e[3] = e[4] = e[5] = 0;
    } else {
        double s = 0.5 * theta_err / sin(theta_err);
        e[3] = s * (R_err[2][1] - R_err[1][2]);
        e[4] = s * (R_err[0][2] - R_err[2][0]);
        e[5] = s * (R_err[1][0] - R_err[0][1]);
    }

    double V_main[6];
    for (int i = 0; i < 6; i++) V_main[i] = Vd[i] + ctrl->Kp * e[i];

    double J[6][dof];
    jacobian_numerical(&params, &state, dof, J);

    double Jstar[dof][6];
    damped_pseudoinverse(dof, J, ctrl->lambda_dls, Jstar);

    double JstarJ[dof][dof];
    for (int i = 0; i < dof; i++)
        for (int j = 0; j < dof; j++) {
            double sum = 0;
            for (int k = 0; k < 6; k++) sum += Jstar[i][k] * J[k][j];
            JstarJ[i][j] = sum;
        }
    double null_proj[dof][dof];
    for (int i = 0; i < dof; i++)
        for (int j = 0; j < dof; j++)
            null_proj[i][j] = (i == j ? 1.0 : 0.0) - JstarJ[i][j];

    for (int i = 0; i < dof; i++) {
        double sum1 = 0, sum2 = 0;
        for (int j = 0; j < 6; j++) sum1 += Jstar[i][j] * V_main[j];
        for (int j = 0; j < dof; j++) sum2 += null_proj[i][j] * f_r[j];
        dpsi[i] = sum1 + sum2;
    }
    free(state.psi);
}