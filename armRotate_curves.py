"""
armRotate_curves.py
按当前 C 代码运动学模型（kinematic.c + CR.c，分支 LQTS）绘制：

  图A（armBend_curves.png, 3x3）: 弯曲视角 —— 9 电机位移随弯曲角 θ=-60°~+60° 的曲线
       （φ=0 上弯 / φ=180 下弯），含行程限位。原版主图，保留。
  图B（armRotate_curves.png, 3x3）: 旋转视角 —— 满弯曲 θ=60° 下，9 电机位移随
       旋转角 φ=0~360°（步长10°）的曲线 —— 端到端画圆旋转的电机需求曲线，含行程限位钳制。
  图C（armRotate_curves_2d.png, 3x3）: 9 电机在 (θ=-60°~+60°, φ=0~360°) 平面上的
       位移热图（限位后），一眼看清画圆路径各电机的收紧/放松扇区与触限区域。
  图D（armRotate_curves_summary.png）: 三段分配与关键电机位移验证。

复现逻辑（与 CR.c armBend_total_core 一致）：
  1. 级联式分段分配：段1 到指定位置(20°)后保持，段2/段3 分段做补偿。
      0~20°:  seg1=0→20°, seg2=0→2.3°(0.115·θ), seg3=0→2°(0.1·θ)
      20~40°: seg1=20°(保持), seg2=2.3→14°(2.3+0.58·(θ-20)), seg3=2→5°(2+0.15·(θ-20))
      40~60°: seg1=20°(保持), seg2=14°(保持), seg3=5→20°(2.382+0.881·(θ-40))
      ≥60°:   seg1=20°, seg2=14°, seg3=20°（全部保持）
  2. model_theta 直接取段输入角（armBend_total 入口已做端到端查表逆补偿，段内不再放大）
  3. 弯曲方向由 φ 决定：'u'→0, 'r'→π/2, 'd'→π, 'l'→3π/2（CR.c armBend_total 方向）
  4. calculate_L：R=[70,75,80]，r_bias=0.8 动态中心杆偏置，φ 决定各电机相位
  5. deltaL 按每段行程限钳制（安全限，防堵转）

注：armRotate（画圆）路径仍走 tendonCompensation（calib_a/b），本脚本只复现
armBend_total（弯曲）运动学路径，故不再乘 calib_a。
"""
import numpy as np
import matplotlib.pyplot as plt
plt.rcParams["mathtext.it"] = "Times New Roman"
plt.rcParams['font.sans-serif'] = ['Microsoft YaHei']
plt.rcParams['font.weight'] = 'bold'  # 设置全局字体为粗体
plt.rcParams['axes.unicode_minus'] = False

# ========== 机械参数（与 CR.c CR_init 一致） ==========
R = [70, 75, 80]                      # 三段弯曲半径 mm
R_BIAS = 0.85                      # 中心杆偏置系数 r_bias（与 CR.c CR_init 一致）
SEG_INPUT_LIMIT = 20.0                # 每段独立输入触限角度 °
TOTAL_ANGLE_LIMIT = 120.0             # 总角度安全限 °
SEG_LENGTH = [40.0, 70.0, 100.0]      # 各段肌腱累计行程限 mm（安全限，防堵转）
seg_map = [1, 1, 1, 2, 2, 2, 3, 3, 3] # 电机归属段（段号）

# ===== 级联式分段分配常量（与 CR.c armBend_total_core 一致） =====

SEG1_SLOPE1 = 0.4          # 0~20°: seg1

SEG2_SLOPE1 = 0.2          # 0~20°: seg2
SEG2_SLOPE2 = 0.4           # 20~40°: seg2

SEG3_SLOPE1 = 0.10           # 0~20°: seg3
SEG3_SLOPE2 = 0.15           # 20~40°: seg3
SEG3_SLOPE3 = 0.8809         # 40~60°: seg3


# ========== 模型函数（与 C 一致） ==========

def arm_bend_total(total_deg, phi_deg=0.0):
    """复现 armBend_total + calculate_L（CR.c armBend_total_core）：
    输入弯曲角大小 total_deg（°），旋转角 phi_deg（°）决定弯曲方向：
      φ=0 上弯，φ=π 下弯（与 CR.c armBend_total direction 一致）。
    返回 (seg_input[3], model_theta_deg[3], deltaL[9])。

    注意：model_theta 直接取段输入角（端到端逆补偿在 armBend_total 入口按总角折算，
    段内不再放大）；方向完全由 phi 决定。"""
    rem = abs(float(total_deg))
    seg_input = [0.0, 0.0, 0.0]

    # 段1: 0~20° 升到 20°，之后保持
    seg_input[0] = min(rem, SEG_INPUT_LIMIT) * (1 + SEG1_SLOPE1)

    # 段2
    if rem <= 20.0:
        seg_input[1] = SEG2_SLOPE1 * rem
    elif rem < 40.0:
        seg_input[1] = SEG2_SLOPE1 * rem + SEG2_SLOPE2 * (rem - 20.0)
    else:
        seg_input[1] = SEG2_SLOPE1 * SEG_INPUT_LIMIT * 2 + SEG2_SLOPE2 * SEG_INPUT_LIMIT

    # 段3: 0~20°
    if rem <= 20.0:
        seg_input[2] = SEG3_SLOPE1 * rem
    elif rem < 40.0:
        seg_input[2] = SEG3_SLOPE1 * rem + SEG3_SLOPE2 * (rem - 20.0)
    elif rem < 60.0:
        seg_input[2] = SEG3_SLOPE1 * rem + SEG3_SLOPE2 * (rem - 20) + SEG3_SLOPE3 * (rem - 40.0)
    else:
        seg_input[2] = SEG3_SLOPE1 * SEG_INPUT_LIMIT * 3 + SEG3_SLOPE2 * SEG_INPUT_LIMIT * 2 + SEG3_SLOPE3 * SEG_INPUT_LIMIT

    # model_theta 直接取段输入角（不乘 calib_a —— 端到端逆补偿已在入口按总角折算）
    model_theta = list(seg_input)

    model_theta_rad = [np.deg2rad(t) for t in model_theta]
    dL = calculate_dL(np.array(model_theta_rad), phi_deg, r_bias=R_BIAS)
    return np.array(seg_input), np.array(model_theta), dL


def calculate_dL(model_theta_rad, phi_deg, r_bias=None):
    """复现 kinematic.c calculate_L（当前公式）：
    model_theta_rad: [θ1,θ2,θ3] 弧度；phi_deg: 旋转角（°）
    返回 deltaL[9]（mm）。

    公式（级联累计弧长 S_cum[s] = Σ_{i≤s} R[i]·θ[i]）：
      dL[3s+0] = +(cos(φ +  60°) + R_eff·|cos(φ +  60°)|) · S_cum[s]
      dL[3s+1] = +(cos(-φ +  60°) + R_eff·|cos(-φ +  60°)|) · S_cum[s]
      dL[3s+2] = -(cos(φ)      - R_eff·|cos(φ)|)      · S_cum[s]
      R_eff = r_bias（常数）
      偏置项恒正：M0/M1 放大正端(1.8×)、缩小负端(0.2×)；M2 缩小负端、放大正端"""
    if r_bias is None:
        r_bias = R_BIAS
    phi_rad = np.deg2rad(phi_deg)
    R_eff = r_bias
    S_cum = np.cumsum(np.array(R) * np.array(model_theta_rad))
    p60 = np.pi / 3
    dL = np.zeros(9)
    for s in range(3):
        c0 = np.cos(phi_rad + p60)
        c1 = np.cos(-phi_rad + p60)
        c2 = np.cos(phi_rad)
        dL[3 * s + 0] = +(c0 + R_eff * np.abs(c0)) * S_cum[s]
        dL[3 * s + 1] = +(c1 + R_eff * np.abs(c1)) * S_cum[s]
        dL[3 * s + 2] = -(c2 - R_eff * np.abs(c2)) * S_cum[s]
    return dL


# 每段行程限映射到电机：段 s 的所有电机（含 M0/1、M3/4、M6/7）统一取 SEG_LENGTH[s]
# 与 CR.c 设计意图一致：上弯 φ=0 时 M0≈35.4mm(θ=20°, 限36)、M3≈63.1mm(θ=40°, 限62)、
# M6≈102mm(θ=60°, 限102)；下弯 φ=π 时 M2/5/8 以 1.8 系数收紧 → 早早触限。
MOTOR_SEG = [seg_map[m] - 1 for m in range(9)]                          # 电机所在段索引
POS_LIMIT = np.array([SEG_LENGTH[MOTOR_SEG[m]] for m in range(9)])      # 正向（收紧）行程限 mm
NEG_LIMIT = np.array([SEG_LENGTH[MOTOR_SEG[m]] for m in range(9)])      # 反向（放松）行程限 mm（对称）

# ========== 计算：-60°(下弯) → +60°(上弯) ==========
angles = np.arange(-60, 61, 1)
N = len(angles)

deltaL_all = np.zeros((9, N))
deltaL_clamped = np.zeros((9, N))
seg_input_all = np.zeros((3, N))
model_theta_all = np.zeros((3, N))
cmd_all = np.zeros((3, N))

for i, t in enumerate(angles):
    # 上弯 φ=0，下弯 φ=π（与 CR.c armBend_total direction 一致）
    phi = 0.0 if t >= 0 else 180.0
    seg_in, model, dL = arm_bend_total(t, phi)
    seg_input_all[:, i] = seg_in
    model_theta_all[:, i] = model
    deltaL_all[:, i] = dL
    # 物理行程限位：超出段行程限则截断（M2/5/8 下弯早触限，上弯 M0/3/6 略低于限）
    deltaL_clamped[:, i] = np.clip(dL, -NEG_LIMIT, POS_LIMIT)
    for s in range(3):
        cmd_all[s, i] = seg_in[s]   # 模型角即段输入角（端到端逆补偿已在入口折算）

# ========== 图A: 弯曲视角 —— 各电机位移随弯曲角 θ 曲线 (3x3) ==========
colors9 = ['#A34D4E', '#516CA3', '#8DAE7F',
           '#b99243', '#f6531a', '#FF1763',
           '#0093E5', '#FF43B0', '#00BF93']

figA, axesA = plt.subplots(3, 3, figsize=(16, 12), sharex=True)
figA.suptitle('各电机位移随弯曲角 θ 的曲线（φ=0 上弯 / φ=180 下弯，含限位）', fontsize=16)

# 位移等值标注点（对称使用 ± 值）
mark_vals = [10, 20, 30, 40, 50, 60]

for m in range(9):
    ax = axesA[m // 3][m % 3]
    # 理论（未限位）曲线淡色
    ax.plot(angles, deltaL_all[m], '-', color=colors9[m], lw=1.0, alpha=0.35)
    # 限位后实际曲线（粗线）
    ax.plot(angles, deltaL_clamped[m], '-', color=colors9[m], lw=3.5)
    ax.axhline(y=0, color='gray', ls='--', lw=0.5)

    # 行程限位线（虚线 + 标注）
    ax.axhline(y=POS_LIMIT[m], color='gray', ls=':', lw=1.0, alpha=0.8)
    ax.axhline(y=-NEG_LIMIT[m], color='gray', ls=':', lw=1.0, alpha=0.8)
    ax.text(0.99, POS_LIMIT[m] + 0.02 * (POS_LIMIT[m] + 1),
            f'{POS_LIMIT[m]:.0f}限', transform=ax.get_yaxis_transform(),
            ha='right', va='bottom', fontsize=7, color='gray')

    # 标注特征点（正/负侧对称；限位后标注实际截断值）
    for ang in mark_vals:
        for t in (ang, -ang):
            if t < -60 or t > 60:
                continue
            val = deltaL_clamped[m][angles == t][0]
            if abs(val) > 0.1:
                ax.plot(t, val, 's', color='black', markersize=4, zorder=5)
                ax.annotate(f'{val:.1f}', (t, val),
                            xytext=(0, 8 if val >= 0 else -12),
                            textcoords='offset points',
                            fontsize=6, ha='center', fontweight='bold',
                            bbox=dict(boxstyle='round,pad=0.15',
                                      facecolor='white', alpha=0.7))

    ax.set_title(f'Motor {m}—段{seg_map[m]}', fontsize=11)
    ax.set_ylabel('位移 (mm)')
    ax.grid(True, alpha=0.3)

for ax in axesA[2]:
    ax.set_xlabel('弯曲角 θ (°)')

plt.tight_layout()
plt.savefig('armBend_curves.png', dpi=300)

# ========== 图B: 旋转（画圆）路径 —— 满弯曲 θ=60°，φ=0~360°（步长10°） ==========
ROT_THETA = 60.0                     # 画圆路径保持满弯曲角
PHI_STEP = 10.0                      # φ 步长 °
phis = np.arange(0.0, 360.0, PHI_STEP)  # 0,10,...,350 (36 点)
NP = len(phis)

deltaL_rot = np.zeros((9, NP))       # 原始（未限位）
deltaL_rot_c = np.zeros((9, NP))     # 限位后
for i, ph in enumerate(phis):
    _, _, dL = arm_bend_total(ROT_THETA, ph)
    deltaL_rot[:, i] = dL
    deltaL_rot_c[:, i] = np.clip(dL, -NEG_LIMIT, POS_LIMIT)

# ========== 图B: 满弯曲下 9 电机位移随 φ 全周曲线 (3x3) ==========
figB, axesB = plt.subplots(3, 3, figsize=(16, 12), sharex=True)
figB.suptitle(f'满弯曲 θ={ROT_THETA:.0f}° 下各电机位移随旋转角 φ 的曲线（φ=0~360°, 步长10°）', fontsize=16)

for m in range(9):
    ax = axesB[m // 3][m % 3]
    # 原始（未限位）曲线淡色
    ax.plot(phis, deltaL_rot[m], '-', color=colors9[m], lw=1.0, alpha=0.35)
    # 限位后实际曲线（粗线）
    ax.plot(phis, deltaL_rot_c[m], '-', color=colors9[m], lw=3.5)
    ax.axhline(y=0, color='gray', ls='--', lw=0.5)

    # 行程限位线（虚线 + 标注）
    ax.axhline(y=POS_LIMIT[m], color='gray', ls=':', lw=1.0, alpha=0.8)
    ax.axhline(y=-NEG_LIMIT[m], color='gray', ls=':', lw=1.0, alpha=0.8)
    ax.text(0.99, POS_LIMIT[m] + 0.02 * (POS_LIMIT[m] + 1),
            f'{POS_LIMIT[m]:.0f}限', transform=ax.get_yaxis_transform(),
            ha='right', va='bottom', fontsize=7, color='gray')
    ax.text(0.99, -NEG_LIMIT[m] - 0.02 * (NEG_LIMIT[m] + 1),
            f'-{NEG_LIMIT[m]:.0f}限', transform=ax.get_yaxis_transform(),
            ha='right', va='top', fontsize=7, color='gray')

    # 标注方向基准点：u/r/d/l（φ=0/90/180/270°）
    for ph0, lbl in [(0, 'u'), (90, 'r'), (180, 'd'), (270, 'l')]:
        v = deltaL_rot_c[m][phis == ph0][0]
        ax.plot(ph0, v, 's', color='black', markersize=4, zorder=5)
        ax.annotate(f'{lbl}:{v:.1f}', (ph0, v), xytext=(0, 8 if v >= 0 else -12),
                    textcoords='offset points', fontsize=6, ha='center',
                    fontweight='bold',
                    bbox=dict(boxstyle='round,pad=0.15', facecolor='white', alpha=0.7))

    # 触限区段（半透明红色块标注）
    for j in range(NP):
        if abs(deltaL_rot[m][j]) > POS_LIMIT[m] + 1e-9:
            ax.axvspan(phis[j] - PHI_STEP / 2, phis[j] + PHI_STEP / 2,
                       color='r', alpha=0.15, lw=0)

    ax.set_title(f'Motor {m}—段{seg_map[m]}', fontsize=11)
    ax.set_ylabel('位移 (mm)')
    ax.set_xticks(np.arange(0, 361, 45))
    ax.grid(True, alpha=0.3)

for ax in axesB[2]:
    ax.set_xlabel('旋转角 φ (°)')

plt.tight_layout()
plt.savefig('armRotate_curves.png', dpi=300)

# ========== 图C: (θ, φ) 二维位移热图 (3x3) ==========
theta_grid = np.arange(-60, 61, 2)
phi_grid = np.arange(0, 360, 5)
TG, PG = np.meshgrid(theta_grid, phi_grid)
dL2d_c = np.zeros((9, PG.shape[0], PG.shape[1]))
for i, th in enumerate(theta_grid):
    for j, ph in enumerate(phi_grid):
        # 弯曲方向：θ 与 φ 共同决定（负 θ 等效 φ+180°）
        phi_eff = ph if th >= 0 else (ph + 180.0) % 360.0
        _, _, dL = arm_bend_total(abs(th), phi_eff)
        dL2d_c[:, j, i] = np.clip(dL, -NEG_LIMIT, POS_LIMIT)

figC, axesC = plt.subplots(3, 3, figsize=(16, 12), sharex=True, sharey=True)
figC.suptitle('9 电机位移热图（θ=-60°~+60°, φ=0~360°, 限位后）', fontsize=16)
vmax = np.max(np.abs(dL2d_c))
for m in range(9):
    ax = axesC[m // 3][m % 3]
    im = ax.pcolormesh(TG, PG, dL2d_c[m], cmap='RdBu_r', vmin=-vmax, vmax=vmax,
                       shading='auto')
    ax.set_title(f'Motor {m}—段{seg_map[m]}', fontsize=11)
    ax.set_yticks(np.arange(0, 361, 60))
    ax.grid(True, alpha=0.2)
    if m % 3 == 0:
        ax.set_ylabel('旋转角 φ (°)')
for ax in axesC[2]:
    ax.set_xlabel('弯曲角 θ (°)')
figC.colorbar(im, ax=axesC, shrink=0.8, label='位移 (mm)')
plt.tight_layout()
plt.savefig('armRotate_curves_2d.png', dpi=300)


# ========== 图D: 三段 model_theta 与补偿验证 (2x2) ==========
colors_seg = ['#e41a1c', '#377eb8', '#4daf4a']

figD, axesD = plt.subplots(2, 2, figsize=(14, 9))
figD.suptitle('armRotate 运动学分配验证 (级联式分段, model_theta=段输入角, 含行程限位)', fontsize=14)

# 左上：三段分配输入 vs model_theta（段输入角）
ax = axesD[0, 0]
for s in range(3):
    ax.plot(angles, seg_input_all[s], '--', color=colors_seg[s], lw=1.5, alpha=0.6,
            label=f'段{s+1}输入分配')
    ax.plot(angles, model_theta_all[s], '-', color=colors_seg[s], lw=2.2,
            label=f'段{s+1} model_theta(段输入角)')
for s in range(3):
    lim = SEG_INPUT_LIMIT if s == 0 else (14.0 if s == 1 else 20.0)
    ax.axhline(y=lim, color=colors_seg[s], ls=':', alpha=0.4)
    ax.axhline(y=-lim, color=colors_seg[s], ls=':', alpha=0.4)
ax.set_xlabel('总输入弯曲角 θ (°)')
ax.set_ylabel('角度 (°)')
ax.set_title('三段输入分配与补偿后模型角度')
ax.legend(fontsize=8)
ax.grid(True, alpha=0.3)

# 右上：总 model_theta 合成（三段叠加）vs 输入
ax = axesD[0, 1]
total_model = np.sum(model_theta_all, axis=0)
ax.plot(angles, angles, 'k--', lw=1, label='1:1 理想')
ax.plot(angles, total_model, 'b-', lw=2.5, label='三段 model_theta 合计')
for t_mark in [-40, -20, 0, 20, 40, 60]:
    val = total_model[angles == t_mark][0]
    ax.plot(t_mark, val, 'bo', ms=5)
    ax.annotate(f'{val:.1f}', (t_mark, val), fontsize=7,
                xytext=(4, -10), textcoords='offset points')
ax.set_xlabel('总输入弯曲角 θ (°)')
ax.set_ylabel('总 model_theta (°)')
ax.set_title('三段 model_theta 合计 vs 输入（阶梯触限）')
ax.legend(fontsize=9)
ax.grid(True, alpha=0.3)

# 左下：M0/M3/M6 位移（上弯收紧/下弯放松）
ax = axesD[1, 0]
for m_idx, m_label, color in [(0, 'M0', 'r'), (3, 'M3', 'b'), (6, 'M6', 'g')]:
    ax.plot(angles, deltaL_all[m_idx], color=color, lw=1.2, alpha=0.4)
    ax.plot(angles, deltaL_clamped[m_idx], color=color, lw=2, label=m_label)
ax.axhline(y=0, color='gray', ls='--', lw=0.5)
for m_idx in [0, 3, 6]:
    ax.axhline(y=POS_LIMIT[m_idx], color='gray', ls=':', lw=0.8)
    ax.axhline(y=-NEG_LIMIT[m_idx], color='gray', ls=':', lw=0.8)
ax.set_xlabel('弯曲角 θ (°)')
ax.set_ylabel('deltaL (mm)')
ax.set_title('M0/M3/M6 (段1/2/3 的 0°/120°/240°侧, 上弯收紧)')
ax.legend(fontsize=9)
ax.grid(True, alpha=0.3)

# 右下：M2/M5/M8 位移（上弯放松/下弯收紧）
ax = axesD[1, 1]
for m_idx, m_label, color in [(2, 'M2', 'r'), (5, 'M5', 'b'), (8, 'M8', 'g')]:
    ax.plot(angles, deltaL_all[m_idx], color=color, lw=1.2, alpha=0.4)
    ax.plot(angles, deltaL_clamped[m_idx], color=color, lw=2, label=m_label)
ax.axhline(y=0, color='gray', ls='--', lw=0.5)
for m_idx in [2, 5, 8]:
    ax.axhline(y=POS_LIMIT[m_idx], color='gray', ls=':', lw=0.8)
    ax.axhline(y=-NEG_LIMIT[m_idx], color='gray', ls=':', lw=0.8)
ax.set_xlabel('弯曲角 θ (°)')
ax.set_ylabel('deltaL (mm)')
ax.set_title('M2/M5/M8 (段1/2/3 的 0°侧, 下弯收紧)')
ax.legend(fontsize=9)
ax.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig('armRotate_curves_summary.png', dpi=150)

# ========== 控制台输出 ==========
print("=" * 64)
print(f"满弯曲 θ={ROT_THETA:.0f}° 各电机位移随 φ 0~360° (mm)  [x=限位后截断]")
print("级联式分段: seg1→20°后保持, seg2→14°后保持, seg3→20°后保持 | r_bias=0.8")
print("行程限(累计): SEG_LENGTH=[36,58,90]mm (安全限)")
print("=" * 64)
header = f"{'φ(°)':>6}"
for m in range(9):
    header += f" | {f'M{m}':>7}"
print(header)
print("-" * 64)
for t in [0, 45, 90, 135, 180, 225, 270, 315]:
    _, _, dL_t = arm_bend_total(ROT_THETA, t)
    clp_t = np.clip(dL_t, -NEG_LIMIT, POS_LIMIT)
    row = f"{t:>6.0f}"
    for m in range(9):
        raw = dL_t[m]
        clp = clp_t[m]
        s = f"{clp:>7.1f}" if raw == clp else f"{clp:>6.1f}*"
        row += f" | {s}"
    print(row)

print()
print("各电机行程限 (mm):")
for m in range(9):
    print(f"  M{m}: [-{NEG_LIMIT[m]:.1f}, +{POS_LIMIT[m]:.1f}]  (段{MOTOR_SEG[m]+1} 限=SEG_LENGTH{MOTOR_SEG[m]+1})")
print()
print(f"φ 扫描触限范围 (θ={ROT_THETA:.0f}°, 满周 0~360°):")
for m in range(9):
    hi = [ph for j, ph in enumerate(phis) if abs(deltaL_rot[m][j]) > POS_LIMIT[m] + 1e-9]
    if hi:
        # 聚拢成连续区间
        spans = []
        lo0 = ph = hi[0]
        for nxt in hi[1:]:
            if nxt - ph <= PHI_STEP + 1e-9:
                ph = nxt
            else:
                spans.append((lo0, ph))
                lo0 = ph = nxt
        spans.append((lo0, ph))
        desc = ", ".join(f"{a:.0f}~{b:.0f}°" if a != b else f"{a:.0f}°" for a, b in spans)
        print(f"  M{m}: φ∈({desc}) 触限 | max|dL|={np.max(np.abs(deltaL_rot[m])):.1f}mm")
    else:
        print(f"  M{m}: 全程未触限 | max|dL|={np.max(np.abs(deltaL_rot[m])):.1f}mm")

print()
print("图片已生成:")
print("  armBend_curves.png          (图A 弯曲视角: 9 电机位移随弯曲角 θ=-60°~+60°, 含限位)")
print("  armRotate_curves.png        (图B 旋转视角: 满弯曲 θ=60° 各电机位移随 φ 0~360°, 含限位)")
print("  armRotate_curves_2d.png     (图C (θ,φ) 二维位移热图, 限位后)")
print("  armRotate_curves_summary.png (图D 分配验证与关键电机位移)")
