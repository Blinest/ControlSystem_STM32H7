"""
armBend_total(dir, total_angle) 各电机位移曲线
复现最新 C 代码完整逻辑：
  1. 总角度按曲率 9:0:1 分配到三段（段2=0，暂关闭）
  2. 每段独立补偿系数 (段1:a=1.0,b=-0.005, 段2:a=1.63(未用), 段3:a=3.0,b=-0.03)
  3. tendon_limit = SEG_LENGTH[i] / ((1+R_BIAS) * R[i])  每段独立行程
  4. calculate_L 计算 9 根驱动丝位移
"""
import numpy as np
import matplotlib.pyplot as plt

plt.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei']
plt.rcParams['axes.unicode_minus'] = False

# ========== 机械参数 ==========
R = [70, 75, 80]
pi = np.pi

# 肌腱行程限值（mm）：压缩侧M2/5/8的物理限
SEG_LENGTH = [25.0, 28.0, 51.2]
R_BIAS = 0.8  # 中心杆偏置系数
TENDON_LIMITS_DEG = [SEG_LENGTH[i] / ((1 - 0.07) * R[i]) * 180 / pi for i in range(3)]

# ===== 分段式角度分配参数 =====
# 0~20°: 段1独立, 段2=段3=0
# 20~40°: 段1=22.0°(clamp), 段2独立, 段3=0
# 40~60°: 段1=22.0°(clamp), 段2=23.0°(clamp), 段3独立
# 这样保证20°时M0=M3=M6≈24.2mm, 40°时M3=M6≈51.3mm, 60°时M6≈100.8mm
SEG_INPUT_LIMIT = [20, 20, 20]  # 每段独立输入的触限角度
# 线性补偿系数 A = limit_deg / SEG_INPUT_LIMIT （每段20°输入正好到限位）
A = [TENDON_LIMITS_DEG[i] / SEG_INPUT_LIMIT[i] for i in range(3)]
B = [0.0, 0.0, 0.0]  # 纯线性，精确到达限位点

# (旧RATIO相关变量标记废弃，仅保留供图注显示)
RATIO = [1.0, 0, 0]  # 占位

# ========== 模型函数 ==========

def tendon_compensation(seg, angle_deg):
    """复现 tendonCompensation(seg, val): 每段独立 a/b"""
    cmd = A[seg] * angle_deg + B[seg] * angle_deg ** 2
    #cmd = np.clip(cmd, MIN_RATIO * angle_deg, MAX_RATIO * angle_deg)
    # 安全底线（设计上不应触发）
    cmd = min(cmd, TENDON_LIMITS_DEG[seg])
    return cmd

def calculate_dL(model_theta, phi=0, r_bias=None):
    """复现 kinematic.c calculate_L 完整逻辑
    model_theta: 每段模型输入角度（度）
    phi: 旋转角（度）
    r_bias: 动态中心杆偏置系数，None 则用全局 R_BIAS
    返回 deltaL[9]（mm）
    """
    if r_bias is None:
        r_bias = R_BIAS
    theta_rad = np.deg2rad(model_theta)
    phi_rad = np.deg2rad(phi)
    re = r_bias * np.cos(phi_rad)
    dL = np.zeros(9)
    dL[0] = -(1+re)*R[0]*theta_rad[0]*np.cos(phi_rad + 2*pi/3)
    dL[1] = -(1+re)*R[0]*theta_rad[0]*np.cos(phi_rad + 4*pi/3)
    dL[2] = -(1-re)*R[0]*theta_rad[0]*np.cos(phi_rad)
    dL[3] = dL[0] - (1+re)*R[1]*theta_rad[1]*np.cos(phi_rad + 2*pi/3)
    dL[4] = dL[1] - (1+re)*R[1]*theta_rad[1]*np.cos(phi_rad + 4*pi/3)
    dL[5] = dL[2] - (1-re)*R[1]*theta_rad[1]*np.cos(phi_rad)
    dL[6] = dL[3] - (1+re)*R[2]*theta_rad[2]*np.cos(phi_rad + 2*pi/3)
    dL[7] = dL[4] - (1+re)*R[2]*theta_rad[2]*np.cos(phi_rad + 4*pi/3)
    dL[8] = dL[5] - (1-re)*R[2]*theta_rad[2]*np.cos(phi_rad)
    return dL

calc_dL = calculate_dL  # alias for direct calls


CIRCLE_BOOST = 1.0  # 画圆整体弯曲放大系数


def phi_compensation(phi_deg):
    """画圆合力矩补偿系数（未使用——实际物理中补偿会削弱强方向导致半圆）
    保留函数仅用于分析，画圆循环不再调用
    """
    pass  # 不再使用


def arm_bend_total(total_deg, phi=0):
    """分段式 armBend_total:
    0~20°: 段1独占, 段2=段3=0
    20~40°: 段1=22°(已触限), 段2独占剩余, 段3=0
    40~60°: 段1=22°, 段2=23°, 段3独占剩余
    >>> 保证限位点 M0=24.2@20°, M3=51.3@40°, M6=100.8@60°
    """
    rem = total_deg
    seg_input = [0.0, 0.0, 0.0]
    for i in range(3):
        take = min(rem, SEG_INPUT_LIMIT[i])
        seg_input[i] = take
        rem -= take
        if rem <= 0:
            break

    model_theta = np.zeros(3)
    for i in range(3):
        if seg_input[i] > 0:
            cmd = tendon_compensation(i, seg_input[i])
            model_theta[i] = min(cmd, TENDON_LIMITS_DEG[i])
        else:
            model_theta[i] = 0.0

    dL = calculate_dL(model_theta, phi)
    return seg_input, model_theta, dL

# ========== 计算 ==========
angles = np.arange(0, 121, 1)
phi = 0

# 存储结果
seg_dist = np.zeros((3, len(angles)))  # 分配角度
model_theta_all = np.zeros((3, len(angles)))  # clamp后模型输入
deltaL_all = np.zeros((9, len(angles)))
cmd_all = np.zeros((3, len(angles)))  # clamp前补偿值

for i, t in enumerate(angles):
    seg_d, model, dL = arm_bend_total(t, phi)
    seg_dist[:, i] = seg_d
    model_theta_all[:, i] = model
    deltaL_all[:, i] = dL
    # 重新算clamp前补偿值
    for s in range(3):
        cmd_all[s, i] = tendon_compensation(s, seg_d[s])

# ========== 图1: 弯曲过程总览 (2x2) ==========
fig1, axes1 = plt.subplots(2, 2, figsize=(14, 10))
fig1.suptitle('armBend_total 三段耦合补偿 (曲率9:0:1, 各段独立a/b补偿)', fontsize=15)

# 左上：三段补偿后 model_theta vs 分配角度
ax = axes1[0, 0]
colors_seg = ['#e41a1c', '#377eb8', '#4daf4a']
for s in range(3):
    ax.plot(angles, seg_dist[s], '--', color=colors_seg[s], lw=1.5, alpha=0.6,
            label=f'段{s+1}分配({RATIO[s]:.1%})')
    ax.plot(angles, model_theta_all[s], '-', color=colors_seg[s], lw=2.5,
            label=f'段{s+1}模型输入.clamp{TENDON_LIMITS_DEG[s]}°')
for lim in TENDON_LIMITS_DEG:
    ax.axhline(y=lim, color='gray', ls=':', alpha=0.3)
ax.plot(70, model_theta_all[0][70], 'ko', ms=5)
ax.annotate(f'{model_theta_all[0][70]:.0f}°', (70, model_theta_all[0][70]),
            xytext=(5,5), textcoords='offset points', fontsize=8)
ax.set_xlabel('总输入角度 (°)')
ax.set_ylabel('角度 (°)')
ax.set_title('模型输入分配 (补偿后 clamp)')
ax.legend(fontsize=7)
ax.grid(True, alpha=0.3)

# 右上：补偿系数 vs 三段的 clip 效果
ax = axes1[0, 1]
for s in range(3):
    factor = np.where(angles > 0, cmd_all[s] / np.maximum(seg_dist[s], 0.001), 1.0)
    ax.plot(angles, factor, color=colors_seg[s], lw=2, label=f'段{s+1}补偿系数')
    # 标注 clamp 段
    clip_mask = cmd_all[s] > TENDON_LIMITS_DEG[s]
    if np.any(clip_mask):
        first_clip = np.where(clip_mask)[0][0]
        ax.axvline(x=angles[first_clip], color=colors_seg[s], ls=':', alpha=0.5)
ax.axhline(y=1.0, color='gray', lw=0.8)
ax.set_xlabel('总输入角度 (°)')
ax.set_ylabel('补偿系数')
ax.set_title('补偿系数（虚线=触限点）')
ax.legend(fontsize=7)
ax.grid(True, alpha=0.3)

# 左下：三段的补偿值 vs clamp
ax = axes1[1, 0]
for s in range(3):
    ax.plot(angles, cmd_all[s], '--', color=colors_seg[s], lw=1.5, alpha=0.5,
            label=f'段{s+1}补偿(未限)')
    ax.plot(angles, model_theta_all[s], '-', color=colors_seg[s], lw=2.5,
            label=f'段{s+1}clamp后')
ax.axhline(y=TENDON_LIMITS_DEG[0], color='gray', ls=':', alpha=0.3)
ax.set_xlabel('总输入角度 (°)')
ax.set_ylabel('角度 (°)')
ax.set_title('补偿前后对比')
ax.legend(fontsize=7)
ax.grid(True, alpha=0.3)

# 右下：M0 M2 M5 M8 位移
ax = axes1[1, 1]
for m_idx, m_label, color in [(0, 'M0', 'r'), (2, 'M2', 'b'), (5, 'M5', 'g'), (8, 'M8', 'orange')]:
    ax.plot(angles, deltaL_all[m_idx], color=color, lw=2, label=m_label)
ax.set_xlabel('总输入角度 (°)')
ax.set_ylabel('deltaL (mm)')
ax.set_title('关键电机位移')
ax.legend(fontsize=9)
ax.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig('armBend_overview.png', dpi=150)

# ========== 图2: 各电机独立位移曲线 (3x3) ==========
fig2, axes2 = plt.subplots(3, 3, figsize=(14, 10), sharex=True)
fig2.suptitle('armBend_total - 各电机理论位移曲线', fontsize=16)

colors9 = ['#e41a1c', '#377eb8', '#4daf4a',
           '#984ea3', '#ff7f00', '#a65628',
           '#f781bf', '#999999', '#66c2a5']
seg_map = [0, 0, 0, 1, 1, 1, 2, 2, 2]
phase_offsets_deg = [120, 240, 0] * 3

for m in range(9):
    ax = axes2[m // 3][m % 3]
    ax.plot(angles, deltaL_all[m], '-', color=colors9[m], lw=2.5)
    ax.axhline(y=0, color='gray', ls='--', lw=0.5)
    ax.set_title(f'Motor {m} (段{seg_map[m]+1}, 相位{phase_offsets_deg[m]}°)', fontsize=11)
    ax.set_ylabel('位移 (mm)')
    ax.grid(True, alpha=0.3)
    for ang in [0, 10, 20, 30, 50, 70]:
        if ang < len(angles):
            val = deltaL_all[m][ang]
            if abs(val) > 0.1:
                ax.plot(ang, val, 's', color='black', markersize=4, zorder=5)
                ax.annotate(f'{val:.1f}', (ang, val),
                           xytext=(0, 8 if val >= 0 else -12),
                           textcoords='offset points',
                           fontsize=6, ha='center', fontweight='bold',
                           bbox=dict(boxstyle='round,pad=0.15', facecolor='white', alpha=0.7))

for ax in axes2[2]:
    ax.set_xlabel('总输入角度 (°)')

plt.tight_layout()
plt.savefig('armRotate_curves.png', dpi=150)

# ========== 图3: 补偿分析 ==========
fig3, axes3 = plt.subplots(2, 2, figsize=(14, 10))
fig3.suptitle('弯曲补偿分析 (各段独立a/b, R_BIAS=0.8)', fontsize=14)

ax = axes3[0, 0]
target = np.arange(0, 121, 1)
total_model = np.zeros(len(target))
for i, t in enumerate(target):
    _, model, _ = arm_bend_total(t, 0)
    total_model[i] = np.sum(model)
ax.plot(target, target, 'k--', lw=1, label='1:1 理想')
ax.plot(target, total_model, 'b-', lw=2.5, label='三段 model_theta 合计')
for t_mark in [20, 40, 60, 80, 100, 120]:
    if t_mark < len(target):
        ax.plot(t_mark, total_model[t_mark], 'bo', ms=5)
        ax.annotate(f'{total_model[t_mark]:.0f}°', (t_mark, total_model[t_mark]),
                   fontsize=7, xytext=(3, -10), textcoords='offset points')
ax.set_xlabel('总输入角度 (°)')
ax.set_ylabel('总 model_theta (°)')
ax.grid(True, alpha=0.3)
ax.legend(fontsize=9)
ax.set_title('三段 model_theta 合计 vs 总输入')

ax = axes3[0, 1]
for s in range(3):
    vals = [model_theta_all[s][i] for i in range(0, len(angles), 5)]
    ax.plot(np.arange(0, len(angles), 5), vals, 'o-', color=colors_seg[s], lw=1.5, label=f'段{s+1}')
ax.set_xlabel('总输入角度 (°)')
ax.set_ylabel('model_theta (°)')
ax.legend(fontsize=9)
ax.grid(True, alpha=0.3)
ax.set_title('三段 model_theta')

ax = axes3[1, 0]
for m in range(9):
    vals = [deltaL_all[m][i] for i in range(0, len(angles), 5)]
    ax.plot(np.arange(0, len(angles), 5), vals, color=colors9[m], lw=1.5, label=f'M{m}')
ax.set_xlabel('总输入角度 (°)')
ax.set_ylabel('deltaL (mm)')
ax.legend(fontsize=7, ncol=3)
ax.grid(True, alpha=0.3)
ax.set_title('各电机 deltaL')

ax = axes3[1, 1]
ax.axis('off')
info = [
    "当前补偿参数 (CR_init):",
    "",
    "tendonCompensation (各段独立):",
    "  段1: a=1.0,  b=-0.005",
    "  段2: a=1.63, b=-0.005",
    "  段3: a=3.0,  b=-0.03",
    "  clamp [0.3, 5.0]",
    "",
    "中心杆偏置:",
    "  R_eff = (1 ± R_BIAS*cos(phi)) * R[i]",
    "  R_BIAS = 0.8",
    "",
    "model_theta 肌腱限 (r_bias):",
    "  段1 ≤ 18.2° (R=70, (1+0.8)×R)",
    "  段2 ≤ 17.0° (R=75, (1+0.8)×R)",
    "  段3 ≤ 15.9° (R=80, (1+0.8)×R)",
    "",
    "曲线分布 (9:0:1):",
    "  θ1 : θ2 : θ3 = 9 : 0 : 1",
]
for i, line in enumerate(info):
    ax.text(0.02, 0.98 - i*0.038, line, transform=ax.transAxes,
            fontsize=9, verticalalignment='top')

plt.tight_layout()
plt.savefig('armBend_compensation_analysis.png', dpi=150)

print("图片已生成:")
print("  armBend_overview.png")
print("  armRotate_curves.png")
print("  armBend_compensation_analysis.png")

# =================================================================
# ========== 图4: action_group_demo 动作组时序曲线 ==========
# =================================================================
# armRotate(30,30): 先弯曲再绕12步旋转，每步theta不变(10,15,30)度
# armBend(1,'u',30): model_theta[0]=tendonComp(1,30)，phi=0
# auto_straight: model_theta=[0,0,0], phi=0
# armBend(1,'d',30): model_theta=[tendonComp(1,30), 0, 0], phi=π
# auto_straight
# armBend(1,'l',30): model_theta=[tendonComp(1,30), 0, 0], phi=3π/2
# auto_straight
# armBend(1,'r',30): model_theta=[tendonComp(1,30), 0, 0], phi=π/2
# auto_straight

# 构建动作序列
seq_theta = []   # list of [θ0, θ1, θ2] (度)
seq_phi = []     # list of φ (度)
seq_dL = []      # list of deltaL[9]
seq_label = []   # phase label

# --- Phase 1: armRotate 初始弯曲 (用 arm_bend_total 补偿后的 model_theta) ---
THETA_ROT_TOTAL = 10.0  # armRotate 的总弯曲角
_, model_th_rot, _ = arm_bend_total(THETA_ROT_TOTAL, phi=0)
rot_theta = [float(model_th_rot[0]), float(model_th_rot[1]), float(model_th_rot[2])]
seq_theta.append(rot_theta)
seq_phi.append(0.0)
seq_label.append("RotInit")

# --- Phase 1b: 7200步旋转 (0.05°步长, model_theta固定) ---
STEP = 0.05
for p in np.arange(STEP, 360.0 + STEP * 0.5, STEP):
    seq_theta.append(rot_theta)
    seq_phi.append(float(round(p, 2)))
    seq_label.append("Rotate")
rot_count = len(seq_label) - 1  # 减1扣除RotInit那步

# --- Phase 2: armBend(1,'u',30) 使用tendonCompensation ---
# 注意：armBend(1,...)只改model_theta[0]，段2/段3保留旋转结束时的值
t_cmd = tendon_compensation(0, 10)  # ~22°
seq_theta.append([t_cmd, rot_theta[1], rot_theta[2]])
seq_phi.append(0.0)
seq_label.append("BendUp")

# --- Phase 3: auto_straight ---
seq_theta.append([0.0, 0.0, 0.0])
seq_phi.append(0.0)
seq_label.append("Zero")

# --- Phase 4: armBend(1,'d',30) ---
# 归零后段2/段3=0，段1单独弯曲
seq_theta.append([t_cmd, 0.0, 0.0])
seq_phi.append(180.0)
seq_label.append("BendDown")

# --- Phase 5: auto_straight ---
seq_theta.append([0.0, 0.0, 0.0])
seq_phi.append(0.0)
seq_label.append("Zero")

# --- Phase 6: armBend(1,'l',30) ---
seq_theta.append([t_cmd, 0.0, 0.0])
seq_phi.append(270.0)
seq_label.append("BendLeft")

# --- Phase 7: auto_straight ---
seq_theta.append([0.0, 0.0, 0.0])
seq_phi.append(0.0)
seq_label.append("Zero")

# --- Phase 8: armBend(1,'r',30) ---
seq_theta.append([t_cmd, 0.0, 0.0])
seq_phi.append(90.0)
seq_label.append("BendRight")

# --- Phase 9: auto_straight ---
seq_theta.append([0.0, 0.0, 0.0])
seq_phi.append(0.0)
seq_label.append("Zero")

# 计算每个时间点的 deltaL（含画圆补偿）
for i in range(len(seq_theta)):
    if seq_label[i] in ('RotInit', 'Rotate'):
        # armRotate 画圆补偿：三项叠加（与 CR.c 和 fig6 一致）
        phi_val = seq_phi[i]
        d135 = min(abs(phi_val - 135), abs(phi_val - 135 + 360), abs(phi_val - 135 - 360))
        d225 = min(abs(phi_val - 225), abs(phi_val - 225 + 360), abs(phi_val - 225 - 360))
        dmin = min(d135, d225)
        d180 = abs(phi_val - 180)
        phi_rad = np.deg2rad(phi_val)
        # cos²平坦滚降: d<55→1.0, d>100→0.0
        g_dual = np.exp(-d135 * d135 / (35 * 35)) + np.exp(-d225 * d225 / (35 * 35))
        boost = 3.00 * g_dual
        asym = 0.60 * g_dual
        b_val = np.exp(-dmin * dmin / (35 * 35))
        r_bias_dyn = 0.8 - 0.5 * b_val
        mid_band = 0.5 * (np.tanh((phi_val - 120) / 10) - np.tanh((phi_val - 240) / 10))
        th0_comp = seq_theta[i][0] * (1.0 + boost * (1.0 - 0.75 * mid_band))
        th_add = th0_comp * (0.20 * mid_band)
        th0_new = th0_comp - th_add
        th1_new = seq_theta[i][1] * (1.0 + boost + asym * 0.75) + th_add * 0.75
        th2_new = seq_theta[i][2] * (1.0 + boost + asym * 0.25) + th_add * 0.25
        dL = calc_dL(np.array([th0_new, th1_new, th2_new]), seq_phi[i], r_bias=r_bias_dyn)
    else:
        dL = calc_dL(seq_theta[i], seq_phi[i])
    seq_dL.append(dL)

N = len(seq_theta)  # 1 + 7200 + 1 + 1 + 1 + 1 + 1 + 1 + 1 = 7208 步

fig4, axes4 = plt.subplots(3, 3, figsize=(16, 10), sharex=True)
fig4.suptitle(r'action\_group\_demo: 各电机位移时序曲线 (armRotate30 → U→0→D→0→L→0→R→0)', fontsize=15)

phase_colors = {
    'RotInit': '#e41a1c',
    'Rotate': '#377eb8',
    'BendUp': '#4daf4a',
    'BendDown': '#984ea3',
    'BendLeft': '#ff7f00',
    'BendRight': '#a65628',
    'Zero': '#999999',
}

# 阶段边界（非连续跳变用竖线标出）
transitions = []
prev_label = seq_label[0]
for i in range(1, N):
    if seq_label[i] != prev_label:
        transitions.append(i - 0.5)
        prev_label = seq_label[i]

for m in range(9):
    ax = axes4[m // 3][m % 3]
    vals = [l[m] for l in seq_dL]

    # 按阶段着色绘制（大数据量，不用散点标记）
    start = 0
    curr_label = seq_label[0]
    for i in range(1, N):
        if seq_label[i] != curr_label:
            color = phase_colors.get(curr_label, '#333')
            ax.plot(range(start, i+1), vals[start:i+1],
                    color=color, lw=1.0)
            start = i
            curr_label = seq_label[i]
    # 最后一段
    color = phase_colors.get(curr_label, '#333')
    ax.plot(range(start, N), vals[start:N],
            color=color, lw=1.0)

    ax.axhline(y=0, color='gray', ls='--', lw=0.5)
    ax.set_title(f'Motor {m}', fontsize=11)
    ax.set_ylabel('位移 (mm)')
    ax.grid(True, alpha=0.3)

    # 阶段分界线
    for t in transitions:
        ax.axvline(x=t, color='gray', ls=':', alpha=0.4)

# 图例（放在最右下角子图旁边）
handles = [plt.Line2D([0], [0], color=c, lw=2, label=l)
           for l, c in phase_colors.items()]
axes4[2, 2].legend(handles=handles, loc='lower right', fontsize=8,
                    title='Phase', title_fontsize=9)

for ax in axes4[2]:
    ax.set_xlabel('时序步')
# 用自定义标签标注 X 轴阶段名（对大数据量精简标注）
tick_positions = [0]
tick_labels = ['Init']
# Rotate 阶段标每90°
rot_start_idx = 1
rot_step_90 = int(90.0 / STEP)
for k in range(1, 5):
    tick_positions.append(rot_start_idx + k * rot_step_90 - 1)
    tick_labels.append(f'{k*90}°')
# 后续各个非 Rotate 阶段
i = rot_start_idx + rot_count
while i < N:
    lbl = seq_label[i]
    j = i
    while j < N and seq_label[j] == lbl:
        j += 1
    mid = (i + j - 1) / 2.0
    short = lbl.replace('RotInit', 'Init')
    tick_positions.append(mid)
    tick_labels.append(short)
    i = j

axes4[2, 2].set_xticks(tick_positions)
axes4[2, 2].set_xticklabels(tick_labels, fontsize=7, rotation=45)

plt.tight_layout()
plt.savefig('armBend_action_group.png', dpi=150)

# ========== 图5: action_group 状态总览 (theta, phi, deltaL各阶段均值和极值) ==========
fig5, axes5 = plt.subplots(3, 3, figsize=(16, 10))
fig5.suptitle(r'action\_group\_demo: 各动作段电机位移分布 (箱线图)', fontsize=15)

# 按阶段名分组
from collections import defaultdict
phases_order = [
    ('RotInit', 'RotInit'),
    ('Rotate', 'Rotate(12步)'),
    ('BendUp', 'BendUp'),
    ('Zero', 'Zero(1)'),
    ('BendDown', 'BendDown'),
    ('Zero', 'Zero(2)'),
    ('BendLeft', 'BendLeft'),
    ('Zero', 'Zero(3)'),
    ('BendRight', 'BendRight'),
    ('Zero', 'Zero(4)'),
]

phase_groups = defaultdict(list)
label_display = {}
for i, lbl in enumerate(seq_label):
    phase_groups[lbl].append(seq_dL[i])

# 阶段展示名
phase_display = {
    'RotInit': 'Init\n(R1)',
    'Rotate': f'Rotate\n({rot_count}步)',
    'BendUp': 'Up',
    'BendDown': 'Down',
    'BendLeft': 'Left',
    'BendRight': 'Right',
    'Zero': 'Zero',
}

phase_order = ['RotInit', 'Rotate', 'BendUp', 'BendDown', 'BendLeft', 'BendRight', 'Zero']
phase_keys = [p for p in phase_order if p in phase_groups]
phase_labels = [phase_display[p] for p in phase_keys]

for m in range(9):
    ax = axes5[m // 3][m % 3]
    data = [np.array([d[m] for d in phase_groups[p]]) for p in phase_keys]
    bp = ax.boxplot(data, patch_artist=True, widths=0.5)
    ax.set_xticklabels(phase_labels, fontsize=8)
    # 着色
    for patch, p in zip(bp['boxes'], phase_keys):
        patch.set_facecolor(phase_colors.get(p, '#ccc'))
        patch.set_alpha(0.6)
    ax.axhline(y=0, color='gray', ls='--', lw=0.5)
    ax.set_title(f'Motor {m}', fontsize=11)
    ax.set_ylabel('位移 (mm)')
    ax.grid(True, alpha=0.3)
    ax.tick_params(axis='x', labelsize=8)

plt.tight_layout()
plt.savefig('armBend_action_box.png', dpi=150)

print("  armBend_action_group.png")
print("  armBend_action_box.png")

# =================================================================
# ========== 图6: 末端画圆仿真 (固定theta + phi连续旋转) ==========
# =================================================================
# 实现思路：
# 1. 先通过 armBend_total(total_deg) 确定三段弯曲角度 model_theta（含肌腱补偿）
# 2. 保持 model_theta 不变，phi 从 0 步进到 360°
# 3. 正向运动学计算末端轨迹
# =================================================================

def forward_kinematics(model_theta_deg, phi_deg, L_seg=SEG_LENGTH):
    """
    三段连续体正向运动学，计算末端位置
    model_theta_deg: [θ1, θ2, θ3] (度)
    phi_deg: 旋转角 (度)
    L_seg: 每段长度 [mm] (三个元素)
    返回 (x, y, z) [mm]
    """
    phi = np.deg2rad(phi_deg)

    # 从基座开始，z 沿臂体轴线
    pos = np.array([0.0, 0.0, 0.0])

    # 局部坐标系（3x3 旋转矩阵），初始为单位阵
    R_mat = np.eye(3)

    for i in range(3):
        theta_deg = model_theta_deg[i]
        if abs(theta_deg) < 1e-8:
            # 直段：沿当前 z 方向平移 L
            pos += L_seg[i] * R_mat[:, 2]
            # 旋转矩阵不变（方向不变）
            continue

        theta = np.deg2rad(theta_deg)
        r = L_seg[i] / theta  # 弯曲半径

        # 弯曲平面由 φ 定义
        # 在弯曲平面内，弧的几何：
        #   沿当前 z 方向：r * sin(theta)
        #   在弯曲方向 (perpendicular to z in bending plane)：r * (1 - cos(theta))
        # 弯曲方向单位向量 = cos(phi) * x_axis + sin(phi) * y_axis
        bend_dir = np.cos(phi) * R_mat[:, 0] + np.sin(phi) * R_mat[:, 1]

        # 位移
        pos += r * (1 - np.cos(theta)) * bend_dir
        pos += r * np.sin(theta) * R_mat[:, 2]

        # 旋转矩阵更新：绕 bend_axis (垂直于弯曲平面的轴) 旋转 theta
        # bend_axis = x_axis × bend_dir = -sin(phi)*x + cos(phi)*y
        bend_axis = -np.sin(phi) * R_mat[:, 0] + np.cos(phi) * R_mat[:, 1]
        # 罗德里格斯公式
        K = np.array([
            [0, -bend_axis[2], bend_axis[1]],
            [bend_axis[2], 0, -bend_axis[0]],
            [-bend_axis[1], bend_axis[0], 0]
        ])
        R_rot = np.eye(3) + np.sin(theta) * K + (1 - np.cos(theta)) * (K @ K)
        R_mat = R_rot @ R_mat

    return pos[0], pos[1], pos[2]


# 仿真固定弯曲角度画圆
print()
print("=" * 60)
print("末端画圆 仿真")
print("=" * 60)

# 选择总弯曲角度（多种角度对比）
circle_angles = [10, 20, 30]
circle_phi_step = 10  # 度
circle_phi_range = np.arange(0, 360.001, circle_phi_step)

fig6 = plt.figure(figsize=(16, 10))
ax_xy = fig6.add_subplot(2, 3, 1)
ax_xz = fig6.add_subplot(2, 3, 2)
ax_3d = fig6.add_subplot(2, 3, 3, projection='3d')
ax_m1 = fig6.add_subplot(2, 3, 4)
ax_m2 = fig6.add_subplot(2, 3, 5)
ax_m3 = fig6.add_subplot(2, 3, 6)

axes6 = np.array([[ax_xy,  ax_xz,  ax_3d],
                  [ax_m1,  ax_m2,  ax_m3]])

fig6.suptitle(r'末端画圆仿真: 固定弯曲角 + $\phi$ 连续旋转 (0→360°)', fontsize=15)

# 色系
circle_colors = ['#e41a1c', '#377eb8', '#4daf4a', '#ff7f00']

# 存储各角度下位移的极值
motor_peak = {m: [] for m in range(9)}

for a_idx, total_deg in enumerate(circle_angles):
    # 1) 获取补偿后的 model_theta (phi=0)
    _, model_th, dL_init = arm_bend_total(total_deg, phi=0)
    print(f'\n总角度={total_deg}°: model_theta=[{model_th[0]:.2f}, {model_th[1]:.2f}, {model_th[2]:.2f}]°')

    tip_xs, tip_ys, tip_zs = [], [], []
    motor_traces = [[] for _ in range(9)]

    for phi_val in circle_phi_range:
        # 2) 画圆增强补偿：三项叠加（与 CR.c armRotate 同步）
        #    非对称项 cos(phi): +12%@0°, -12%@180° 补偿两电机协同摩擦
        #    高斯项: 250%@135°/225°, sigma=40
        #    动态 r_bias: 0.8→0.0 @ 135°/225°, 释放M0/M3/M6拉力
        d135 = min(abs(phi_val - 135), abs(phi_val - 135 + 360), abs(phi_val - 135 - 360))
        d225 = min(abs(phi_val - 225), abs(phi_val - 225 + 360), abs(phi_val - 225 - 360))
        dmin = min(d135, d225)
        d180 = abs(phi_val - 180)
        phi_rad = np.deg2rad(phi_val)
        # tanh平坦滚降(100°~260°), 与 CR.c 同步
        g_dual = np.exp(-d135 * d135 / (35 * 35)) + np.exp(-d225 * d225 / (35 * 35))
        boost = 2.00 * g_dual
        asym = 0.35 * g_dual  # 仅用于段2/段3
        b = np.exp(-dmin * dmin / (40 * 40))
        seg_shift = 0.50 * np.exp(-d180 * d180 / (25 * 25))
        r_bias_dyn = 0.8 - 0.8 * b
        th0_comp = model_th[0] * (1.0 + boost)
        th_add = th0_comp * seg_shift
        th_c = np.array([
            th0_comp - th_add,
            model_th[1] * (1.0 + boost + asym * 0.75) + th_add * 0.75,
            model_th[2] * (1.0 + boost + asym * 0.25) + th_add * 0.25,
        ])
        dL = calculate_dL(th_c, phi_val, r_bias=r_bias_dyn)
        for m in range(9):
            motor_traces[m].append(dL[m])
        # 3) 正向运动学
        x, y, z = forward_kinematics(th_c, phi_val)
        tip_xs.append(x)
        tip_ys.append(y)
        tip_zs.append(z)

    # 画 XY 轨迹 (ax 0,0)
    ax = axes6[0, 0]
    ax.plot(tip_xs, tip_ys, '-', color=circle_colors[a_idx], lw=1.5,
            label=f'{total_deg}°')
    # 标注起点
    ax.plot(tip_xs[0], tip_ys[0], 'o', color=circle_colors[a_idx], ms=5)
    ax.text(tip_xs[0]+2, tip_ys[0], f'{total_deg}°',
            color=circle_colors[a_idx], fontsize=9)

    # 画 XZ 轨迹 (ax 0,1)
    ax = axes6[0, 1]
    ax.plot(tip_xs, tip_zs, '-', color=circle_colors[a_idx], lw=1.5,
            label=f'{total_deg}°')

    # 画 3D 轨迹 (ax 0,2)
    ax = axes6[0, 2]
    ax.plot(tip_xs, tip_ys, tip_zs, '-', color=circle_colors[a_idx], lw=1.5,
            label=f'{total_deg}°')

    # 画 3 个关键电机的位移时序
    for mi, mx in [(3, 1), (4, 1), (5, 1)]:  # M3/M4/M5 in ax[1,1]
        pass  # 放后面

    # 记录极值
    for m in range(9):
        vals = motor_traces[m]
        motor_peak[m].append((total_deg, min(vals), max(vals)))

# XY 平面
axes6[0, 0].set_xlabel('X (mm)')
axes6[0, 0].set_ylabel('Y (mm)')
axes6[0, 0].set_title('末端 XY 轨迹')
axes6[0, 0].grid(True, alpha=0.3)
axes6[0, 0].axis('equal')
axes6[0, 0].legend(fontsize=8)

# XZ 平面
axes6[0, 1].set_xlabel('X (mm)')
axes6[0, 1].set_ylabel('Z (mm)')
axes6[0, 1].set_title('末端 XZ 轨迹')
axes6[0, 1].grid(True, alpha=0.3)
axes6[0, 1].axis('equal')
axes6[0, 1].legend(fontsize=8)

# 3D
axes6[0, 2].set_xlabel('X (mm)')
axes6[0, 2].set_ylabel('Y (mm)')
axes6[0, 2].set_zlabel('Z (mm)')
axes6[0, 2].set_title('末端 3D 轨迹')
axes6[0, 2].legend(fontsize=8)

# 下排：电机位移 vs phi (选 M0,M3,M6 段1; M3,M4,M5 段2; M6,M7,M8 段3)
motor_groups = [
    (([0, 3, 6], '段1耦合电机 (M0 M3 M6)'), axes6[1, 0]),
    (([3, 4, 5], '段2电机 (M3 M4 M5)'), axes6[1, 1]),
    (([6, 7, 8], '段3电机 (M6 M7 M8)'), axes6[1, 2]),
]

for (motors_to_plot, title), ax in motor_groups:
    # 用总角度=10°显示（补偿后），带画圆增强补偿
    _, th_10, _ = arm_bend_total(10, 0)
    for m in motors_to_plot:
        trace = []
        for phi_val in circle_phi_range:
            d135 = min(abs(phi_val - 135), abs(phi_val - 135 + 360), abs(phi_val - 135 - 360))
            d225 = min(abs(phi_val - 225), abs(phi_val - 225 + 360), abs(phi_val - 225 - 360))
            dmin = min(d135, d225)
            d180 = abs(phi_val - 180)
            g = 0.5 * (np.tanh((phi_val - 100) / 8) - np.tanh((phi_val - 260) / 8))
            boost = 1.80 * g
            asym = 0.40 * g  # 仅用于段2/段3
            b1 = 1.0 * np.exp(-dmin * dmin / (40 * 40))
            b2 = 1.0 * np.exp(-d180 * d180 / (30 * 30))
            b = min(b1 + b2, 1.0)
            seg_shift = 0.50 * g
            r_bias_dyn = 0.8 - 0.8 * b
            th0_comp = th_10[0] * (1.0 + boost)
            th_add = th0_comp * seg_shift
            th_c = np.array([
                th0_comp - th_add,
                th_10[1] * (1.0 + boost + asym * 0.75) + th_add * 0.75,
                th_10[2] * (1.0 + boost + asym * 0.25) + th_add * 0.25,
            ])
            dL = calculate_dL(th_c, phi_val, r_bias=r_bias_dyn)
            trace.append(dL[m])
        ax.plot(circle_phi_range, trace, lw=1.5, label=f'M{m}')
    ax.set_xlabel('旋转角 φ (°)')
    ax.set_ylabel('位移 (mm)')
    ax.set_title(title)
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=8)

plt.tight_layout()
plt.savefig('armBend_tip_circle.png', dpi=150)

print()
print(f'画圆电机位移极值: (总角度: min ~ max mm)')
for m in range(9):
    parts = [f'{d}°: {mi:.1f}~{ma:.1f}' for d, mi, ma in motor_peak[m]]
    print(f'  M{m}:  {" | ".join(parts)}')

print()
print("  armBend_tip_circle.png")
print("仿真结论:")
print("  - 固定 θ 时, phi 旋转产生椭圆轨迹 (非标准圆,因三段不等长不等角)")
print("  - 角度越小, 轨迹越接近圆; 角度越大, Z 向漂移越明显")
print("  - 若需标准圆形, 需在前向运动学中预补偿 phi")
