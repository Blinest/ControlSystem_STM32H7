# ==========================================
# 5. 设备选项卡
# ==========================================

import struct
import math
import time
from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, QGridLayout,
                             QLabel, QComboBox, QDoubleSpinBox, QPushButton, QTabWidget,
                             QFrame, QSplitter, QMessageBox, QGraphicsDropShadowEffect, QAbstractSpinBox, QScrollArea)
from PyQt5.QtCore import Qt, QTimer, pyqtSlot

from Core.serial_worker import SerialWorker
from Core.auth import GlobalHistory
from Core.protocol import ProtocolParser, DataFilter
from .widgets import AnimatedButton
from Utils.controller import PID
from UI.styles import get_main_window_style, get_device_tab_extra_style


class DeviceTab(QWidget):
    def __init__(self, port_name, parent_logger, auth_service=None, debug_check=None):
        super().__init__()
        self.auth_service = auth_service

        # ========== 基本属性 ==========
        self.port_name = port_name
        self.logger = parent_logger
        self.debug_check = debug_check
        self.serial_error = False
        self.start_time = None

        # ========== 串口通信与数据缓冲 ==========
        self.worker = SerialWorker(port_name)
        self.worker.signal_data.connect(self.parse_data)
        self.worker.signal_error.connect(self.handle_serial_error)
        self.recv_buffer = bytearray()

        # ========== 系统运行状态 ==========
        self.is_started = False
        self.closed_loop_enabled = False   # 闭环暂未使用，保留以备扩展
        self.closed_loop_target_angle = 0.0

        # ========== 电机与弯曲传感器数量及数据 ==========
        self.num_m = 0
        self.num_s = 0
        self.motor_data = []          # 当前电机数据 [pos, vel, acc]
        self.motor_target = []        # 目标值 [pos, vel, acc]
        self.motor_states = []        # 电机运行状态 0/1
        self.sensor_data = []         # 当前传感器数据 [pitch, roll, yaw]

        # ========== 臂体弯曲角度（用于右侧看板显示） ==========
        self.target_angle1 = 0.0
        self.current_angle1 = 0.0
        self.target_angle2 = 0.0
        self.current_angle2 = 0.0

        # 滤波相关
        self.angle_filter_alpha = 0.3
        self.filtered_bend_angle = 0.0
        self.current_bend_angle = 0.0
        self.current_area_change = 0.0

        # ========== 历史数据缓存（曲线） ==========
        self.hist_time = []
        self.hist_motors = []
        self.hist_sensors = []
        self.hist_bend_time = []
        self.hist_target1 = []
        self.hist_current1 = []
        self.hist_target2 = []
        self.hist_current2 = []

        # ========== UI 组件引用 ==========
        self.m_page = 0
        self.s_page = 0
        self.cards_motor = []
        self.cards_sensor = []
        self.bend_graph_window = None
        self.bend_graph_controller = None
        self.active_graph = None
        self.active_type = None
        self.active_graph_ui = None
        self.active_graph_controller = None

        # ========== PID 闭环控制（保留结构，但不在界面暴露） ==========
        self.pid = PID(
            Kp=1, Ki=0.01, Kd=0.01, dt=0.2,
            output_limits=(-70, 70), integral_limits=(-20, 20)
        )
        self.last_sent_angle = None
        self.angle_deadband = 1

        # ========== 定时器 ==========
        self.control_timer = QTimer()
        self.control_timer.timeout.connect(self.closed_loop_control)
        self.control_timer.start(200)

        self.history_timer = QTimer()
        self.history_timer.timeout.connect(self.record_history)
        self.history_timer.start(10)

        # ========== 数据处理（滤波） ==========
        self.data_filter = DataFilter(window_size=3)

        # ========== 初始化界面并启动工作线程 ==========
        self.init_ui()
        combined_style = get_main_window_style() + "\n" + get_device_tab_extra_style()
        self.setStyleSheet(combined_style)
        self.worker.start()

    def init_ui(self):
        main_layout = QHBoxLayout(self)
        splitter = QSplitter(Qt.Horizontal)

        content_widget = QWidget()
        main_layout = QHBoxLayout(content_widget)
        splitter = QSplitter(Qt.Horizontal)

        # 左侧面板
        left_widget = QWidget()
        left_layout = QVBoxLayout(left_widget)
        left_widget.setMaximumWidth(800)

        # ---- 1. 系统操作权限 ----
        g_power = QGroupBox("1. 系统操作权限")
        l_power = QHBoxLayout(g_power)

        self.btn_toggle = AnimatedButton("▶ 启动控制系统", "#107C10", "#063A06")
        self.btn_toggle.setCheckable(True)
        shadow = QGraphicsDropShadowEffect()
        self.btn_toggle.setGraphicsEffect(shadow)
        self.btn_toggle.toggled.connect(self.sys_toggle)

        self.btn_stop = AnimatedButton("⏹紧急停止", "red", "#A80000")
        shadow2 = QGraphicsDropShadowEffect()
        self.btn_stop.setGraphicsEffect(shadow2)
        self.btn_stop.setProperty("class", "emergency")
        self.btn_stop.clicked.connect(self.sys_stop)

        l_power.addWidget(self.btn_toggle)
        l_power.addWidget(self.btn_stop)
        left_layout.addWidget(g_power)
        # ---- 2. 系统控制 ----
        g_quick = QGroupBox("2. 系统控制")
        l_quick = QVBoxLayout(g_quick)
        l_quick.setSpacing(8)
        l_quick.setContentsMargins(10, 10, 10, 10)

        # ---------- 第一行：一键归中 + 抓取控制（紧凑左对齐）----------
        row1 = QHBoxLayout()
        row1.setSpacing(0)

        self.btn_home = AnimatedButton("⌂ 一键归中", "#1E1E1E", "#505050")
        self.btn_home.setFixedHeight(36)
        row1.addWidget(self.btn_home)
        row1.setSpacing(20)
        # 抓取标签+输入框作为紧密小组
        grab_group = QHBoxLayout()
        grab_group.setSpacing(2)   # 标签与输入框紧紧挨着

        grab_label = QLabel("抓取角度:")
        grab_label.setStyleSheet("color: white; font-size: 14pt;font-weight: bold;")
        grab_group.addWidget(grab_label)

        self.spin_grab_angle = QDoubleSpinBox()
        self.spin_grab_angle.setRange(0, 90)
        self.spin_grab_angle.setValue(30)
        self.spin_grab_angle.setSuffix("°")
        self.spin_grab_angle.setDecimals(1)
        self.spin_grab_angle.setFixedWidth(80)
        self.spin_grab_angle.setButtonSymbols(QAbstractSpinBox.NoButtons)
        self.spin_grab_angle.setStyleSheet("""
            QDoubleSpinBox {
                min-height: 34px;
                font-size: 10pt;
                font-weight: bold;
                border: 1px solid #cfe2f2;
                border-radius: 10px;
                background: #2C3E50;
                padding: 4px 8px;
                color: white;
            }
            QDoubleSpinBox:focus {
                border-color: #1e6f9f;
            }
        """)
        grab_group.addWidget(self.spin_grab_angle)
        row1.addLayout(grab_group)

        row1.addSpacing(8)

        self.btn_grab = AnimatedButton("⚫ 抓取", "#107C10", "#063A06")
        self.btn_grab.setFixedHeight(36)
        self.btn_reverse_grab = AnimatedButton("⚪ 反向抓取", "#D13438", "#6B1418")
        self.btn_reverse_grab.setFixedHeight(36)
        self.btn_grab.clicked.connect(self.grab)
        self.btn_reverse_grab.clicked.connect(self.reverse_grab)

        row1.addWidget(self.btn_grab)
        row1.addWidget(self.btn_reverse_grab)
        # 末尾不加 stretch，控件自然靠左，右侧不会出现大片空白
        l_quick.addLayout(row1)

        # ---------- 第二行：地址选择 1/2/3/4（紧凑排列，不加 stretch）----------
        addr_layout = QHBoxLayout()
        addr_layout.setSpacing(8)   # 各组之间的固定间距
        self.addr_buttons = []
        self.addr_spinboxes = []

        switch_style = """
        QPushButton {
            background-color: #505050;
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 12pt;
            font-weight: bold;
            min-width: 36px;
            max-width: 36px;
            min-height: 36px;
            max-height: 36px;
        }
        QPushButton:checked {
            background-color: #107C10;
            color: white;
        }
        QPushButton:hover {
            background-color: #6B1418;
        }
        QPushButton:checked:hover {
            background-color: #0a5e0a;
        }
        """

        for i in range(1, 5):
            group = QHBoxLayout()
            group.setSpacing(4)

            btn = QPushButton(str(i))
            btn.setCheckable(True)
            btn.setStyleSheet(switch_style)
            self.addr_buttons.append(btn)
            group.addWidget(btn)

            spin = QDoubleSpinBox()
            spin.setRange(0, 90)
            spin.setValue(30)
            spin.setSuffix("°")
            spin.setDecimals(1)
            spin.setFixedWidth(80)
            spin.setButtonSymbols(QAbstractSpinBox.NoButtons)
            spin.setStyleSheet("""
                QDoubleSpinBox {
                    min-height: 34px;
                    font-size: 10pt;
                    font-weight: bold;
                    border: 1px solid #cfe2f2;
                    border-radius: 10px;
                    background: #2C3E50;
                    padding: 4px 8px;
                    color: white;
                }
                QDoubleSpinBox:focus {
                    border-color: #1e6f9f;
                }
            """)
            self.addr_spinboxes.append(spin)
            group.addWidget(spin)

            addr_layout.addLayout(group)
            # 不再添加 stretch，四组自然靠左

        l_quick.addLayout(addr_layout)

        # ---------- 第三行：动作按钮（紧凑靠左）----------
        action_layout = QHBoxLayout()
        action_layout.setSpacing(10)

        self.btn_forward = AnimatedButton("⬆ 正向弯曲", "#107C10", "#063A06")
        self.btn_forward.setFixedHeight(38)
        self.btn_release = AnimatedButton("⏺ 释放", "#D13438", "#6B1418")
        self.btn_release.setFixedHeight(38)
        self.btn_backward = AnimatedButton("⬇ 反向弯曲", "#0078D4", "#005A9E")
        self.btn_backward.setFixedHeight(38)

        self.btn_forward.clicked.connect(self.bend_forward)
        self.btn_release.clicked.connect(self.bend_release)
        self.btn_backward.clicked.connect(self.bend_backward)

        action_layout.addWidget(self.btn_forward)
        action_layout.addWidget(self.btn_release)
        action_layout.addWidget(self.btn_backward)
        # 不加 stretch，按钮自然排列在左侧
        l_quick.addLayout(action_layout)

        left_layout.addWidget(g_quick)
        # ---- 3. 电机控制 ----
        g_addr = QGroupBox("3. 电机控制")
        f_addr = QGridLayout(g_addr)
        self.cb_motor_id = QComboBox()
        self.spin_m_pos = self._create_custom_spinbox(-720, 720, 0, prefix="角度：", suffix='度')
        self.spin_m_vel = self._create_custom_spinbox(-20, 20, 10, prefix="角速度: ", suffix=" rpm")
        self.spin_m_acc = self._create_custom_spinbox(-10, 10, 10, prefix="角加速度: ", suffix=" ")
        self.btn_send_m = AnimatedButton("发至电机", "#00BCD4", "#505050")
        self.btn_send_m.clicked.connect(self.send_motor)
        f_addr.addWidget(QLabel("电机ID:"), 0, 0)
        f_addr.addWidget(self.cb_motor_id, 0, 1)
        f_addr.addWidget(self.btn_send_m, 0, 2)
        self.motor_status_ball = QLabel("●")
        self.motor_status_ball.setStyleSheet("color: #c46b5b; font-size: 8pt;")
        f_addr.addWidget(self.motor_status_ball, 0, 3)
        self.cb_motor_id.currentIndexChanged.connect(self.update_motor_status_ball)
        f_addr.addWidget(self.spin_m_pos, 1, 0)
        f_addr.addWidget(self.spin_m_vel, 1, 1)
        f_addr.addWidget(self.spin_m_acc, 1, 2)
        left_layout.addWidget(g_addr)

        # ---- 4. 弯曲传感器数据监控 ----
        g_sensor = QGroupBox("4. 弯曲传感器数据监控")
        l_sensor = QVBoxLayout(g_sensor)
        self.cb_sensor_monitor = QComboBox()
        self.cb_sensor_monitor.currentIndexChanged.connect(self.update_sensor_monitor)
        h_sensor_line = QHBoxLayout()
        h_sensor_line.addWidget(QLabel("弯曲传感器 ID:"))
        h_sensor_line.addWidget(self.cb_sensor_monitor)
        l_sensor.addLayout(h_sensor_line)
        left_layout.addWidget(g_sensor)
        left_layout.addStretch()

        # ---- 右侧看板 ----
        right_widget = QWidget()
        right_widget.setMaximumWidth(580)
        self.right_layout = QVBoxLayout(right_widget)
        self.tabs = QTabWidget()

        # 选项卡1：电机与弯曲传感器数据监控
        tab_all = QWidget()
        v_all = QVBoxLayout(tab_all)
        self.grid_m = QGridLayout()
        h_m_page = QHBoxLayout()
        self.btn_m_prev = AnimatedButton("◀ 上一页", "grey", "#505050")
        self.btn_m_prev.clicked.connect(lambda: self.change_page('m', -1))
        self.btn_m_prev.setProperty("class", "page-btn")
        self.btn_m_next = AnimatedButton("下一页 ▶", "grey", "#505050")
        self.btn_m_next.clicked.connect(lambda: self.change_page('m', 1))
        self.btn_m_next.setProperty("class", "page-btn")
        self.lbl_m_page = QLabel("电机 1/1 页")
        self.lbl_m_page.setProperty("class", "page-btn")
        self.lbl_m_page.setAlignment(Qt.AlignCenter)
        h_m_page.addWidget(self.btn_m_prev)
        h_m_page.addWidget(self.lbl_m_page)
        h_m_page.addWidget(self.btn_m_next)

        self.grid_s = QGridLayout()
        h_s_page = QHBoxLayout()
        self.btn_s_prev = AnimatedButton("◀ 上一页", "grey", "#505050")
        self.btn_s_prev.clicked.connect(lambda: self.change_page('s', -1))
        self.btn_s_prev.setProperty("class", "page-btn")
        self.btn_s_next = AnimatedButton("下一页 ▶", "grey", "#505050")
        self.btn_s_next.clicked.connect(lambda: self.change_page('s', 1))
        self.btn_s_next.setProperty("class", "page-btn")
        self.lbl_s_page = QLabel("弯曲传感器 1/1 页")
        self.lbl_s_page.setProperty("class", "page-btn")
        self.lbl_s_page.setAlignment(Qt.AlignCenter)
        h_s_page.addWidget(self.btn_s_prev)
        h_s_page.addWidget(self.lbl_s_page)
        h_s_page.addWidget(self.btn_s_next)

        v_all.addLayout(h_m_page)
        v_all.addLayout(self.grid_m)
        v_all.addStretch()
        v_all.addLayout(h_s_page)
        v_all.addLayout(self.grid_s)
        v_all.addStretch()
        self.tabs.addTab(tab_all, "👁 电机与弯曲传感器数据监控")

        # 选项卡2：柔性臂运动数据监控
        tab_bend = QWidget()
        v_bend = QVBoxLayout(tab_bend)
        hbox_angle1 = QHBoxLayout()
        self.target_angle1_card, self.target_angle1_val = self.create_flat_card(
            "第一段目标角度 (deg)", "0.00", "#b5956b")
        hbox_angle1.addWidget(self.target_angle1_card)
        self.current_angle1_card, self.current_angle1_val = self.create_flat_card(
            "第一段当前角度 (deg)", "0.00", "#b5956b")
        hbox_angle1.addWidget(self.current_angle1_card)
        v_bend.addLayout(hbox_angle1)

        hbox_angle2 = QHBoxLayout()
        self.target_angle2_card, self.target_angle2_val = self.create_flat_card(
            "第二段目标角度 (deg)", "0.00", "#b5956b")
        hbox_angle2.addWidget(self.target_angle2_card)
        self.current_angle2_card, self.current_angle2_val = self.create_flat_card(
            "第二段当前角度 (deg)", "0.00", "#b5956b")
        hbox_angle2.addWidget(self.current_angle2_card)
        v_bend.addLayout(hbox_angle2)
        self.tabs.addTab(tab_bend, "🔧 柔性臂运动数据监控")

        # 选项卡3：定点监测
        tab_single = QWidget()
        v_single = QVBoxLayout(tab_single)
        h_sel = QHBoxLayout()
        self.cb_view_type = QComboBox()
        self.cb_view_type.addItems(["定点监测: 电机", "定点监测: 弯曲传感器"])
        self.cb_view_type.currentIndexChanged.connect(self.update_single_monitor_labels)
        self.cb_view_id = QComboBox()
        self.cb_view_id.currentIndexChanged.connect(lambda: self.update_ui())
        h_sel.addWidget(self.cb_view_type)
        h_sel.addWidget(self.cb_view_id)
        h_sel.addStretch()
        v_single.addLayout(h_sel)
        self.single_cards = []
        init_titles = ["角度 (度)", "转速 (rpm)", "角加速度 ()"]
        for default_title in init_titles:
            card_frame = QFrame()
            card_frame.setObjectName("singleValueCard")
            card_layout = QHBoxLayout(card_frame)
            title_label = QLabel(default_title)
            title_label.setObjectName("singleCardTitle")
            value_label = QLabel("0.00")
            value_label.setObjectName("singleCardValue")
            value_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
            card_layout.addWidget(title_label)
            card_layout.addStretch()
            card_layout.addWidget(value_label)
            v_single.addWidget(card_frame)
            self.single_cards.append((title_label, value_label))
        self.tabs.addTab(tab_single, "🎯 定点监测(电机与弯曲传感器)")

        self.right_layout.addWidget(self.tabs)
        splitter.addWidget(left_widget)
        splitter.addWidget(right_widget)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 3)
        main_layout.addWidget(splitter)

        scroll_area = QScrollArea()
        scroll_area.setWidgetResizable(True)
        scroll_area.setWidget(content_widget)
        scroll_area.setMaximumHeight(750)
        scroll_area.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        scroll_area.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.setLayout(QVBoxLayout())
        self.layout().addWidget(scroll_area)

        self.rebuild_cards()

    # ---------- 更新单点监测标签 ----------
    def update_single_monitor_labels(self):
        is_motor = (self.cb_view_type.currentIndex() == 0)
        if is_motor:
            titles = ["角度 (deg)", "转速 (rpm)", "角加速度 ()"]
            self.update_single_monitor_ids(range(1, self.num_m + 1))
        else:
            titles = ["弯曲角(原始) (deg)", "弯曲角(滤波) (deg)", "编码值"]
            self.update_single_monitor_ids(range(1, self.num_s + 1))
        for i, (title_label, value_label) in enumerate(self.single_cards):
            title_label.setText(titles[i])
        self.update_ui()

    def update_single_monitor_ids(self, ids_range):
        current_id = self.cb_view_id.currentText()
        self.cb_view_id.blockSignals(True)
        self.cb_view_id.clear()
        id_list = [f"ID {i}" for i in ids_range]
        if id_list:
            self.cb_view_id.addItems(id_list)
            if current_id in id_list:
                self.cb_view_id.setCurrentText(current_id)
            else:
                self.cb_view_id.setCurrentIndex(0)
        else:
            self.cb_view_id.addItem("无")
        self.cb_view_id.blockSignals(False)

    # ---------- 卡片创建 ----------
    def create_motor_card(self, title, color):
        frame = QFrame()
        frame.setObjectName("motorCard")
        main_layout = QVBoxLayout(frame)
        main_layout.setContentsMargins(8, 8, 8, 8)
        main_layout.setSpacing(10)

        top_widget = QWidget()
        top_layout = QHBoxLayout(top_widget)
        top_layout.setContentsMargins(0, 0, 0, 0)
        title_label = QLabel(title)
        title_label.setObjectName("motorCardTitle")
        title_label.setAlignment(Qt.AlignCenter)
        top_layout.addWidget(title_label)
        top_layout.addStretch()
        state_ball = QLabel("●")
        state_ball.setObjectName("motorStateBall")
        top_layout.addWidget(state_ball)
        main_layout.addWidget(top_widget)

        def create_block(block_name, unit):
            block_widget = QWidget()
            block_layout = QVBoxLayout(block_widget)
            block_layout.setContentsMargins(0, 0, 0, 0)
            block_layout.setSpacing(4)
            title_lbl = QLabel(f"{block_name} ({unit})")
            title_lbl.setObjectName("motorBlockTitle")
            title_lbl.setAlignment(Qt.AlignCenter)
            block_layout.addWidget(title_lbl)
            value_widget = QWidget()
            value_layout = QHBoxLayout(value_widget)
            value_layout.setContentsMargins(0, 0, 0, 0)
            value_layout.setSpacing(30)
            cur_label = QLabel("当前: 0.00")
            cur_label.setObjectName("motorCurValue")
            cur_label.setAlignment(Qt.AlignCenter)
            tar_label = QLabel("目标: 0.00")
            tar_label.setObjectName("motorTarValue")
            tar_label.setAlignment(Qt.AlignCenter)
            value_layout.addStretch()
            value_layout.addWidget(cur_label)
            value_layout.addWidget(tar_label)
            value_layout.addStretch()
            block_layout.addWidget(value_widget)
            return block_widget, cur_label, tar_label

        block_pos, cur_pos, tar_pos = create_block("角度", "deg")
        block_vel, cur_vel, tar_vel = create_block("转速", "rpm")
        block_acc, cur_acc, tar_acc = create_block("角加速度", "")

        main_layout.addWidget(block_pos)
        line1 = QFrame()
        line1.setFrameShape(QFrame.HLine)
        line1.setObjectName("motorLine")
        main_layout.addWidget(line1)
        main_layout.addWidget(block_vel)
        line2 = QFrame()
        line2.setFrameShape(QFrame.HLine)
        line2.setObjectName("motorLine")
        main_layout.addWidget(line2)
        main_layout.addWidget(block_acc)

        return frame, [cur_pos, tar_pos, cur_vel, tar_vel, cur_acc, tar_acc, state_ball]

    def create_sensor_card(self, title, color):
        frame = QFrame()
        frame.setObjectName("sensorCard")
        main_layout = QVBoxLayout(frame)
        main_layout.setContentsMargins(8, 8, 8, 8)
        main_layout.setSpacing(8)
        top_widget = QWidget()
        top_layout = QHBoxLayout(top_widget)
        top_layout.setContentsMargins(0, 0, 0, 0)
        title_label = QLabel(title)
        title_label.setAlignment(Qt.AlignLeft)
        top_layout.addWidget(title_label)
        main_layout.addWidget(top_widget)

        def create_axis_block(axis_name, unit):
            block_widget = QWidget()
            block_layout = QHBoxLayout(block_widget)
            block_layout.setContentsMargins(0, 0, 0, 0)
            block_layout.setSpacing(10)
            block_layout.addStretch()
            label = QLabel(f"{axis_name} ({unit}):")
            label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
            block_layout.addWidget(label)
            value_label = QLabel("0.00")
            value_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
            block_layout.addWidget(value_label)
            block_layout.addStretch()
            return block_widget, value_label

        block_pitch, val_pitch = create_axis_block("弯曲角(原始)", "deg")
        block_roll, val_roll = create_axis_block("弯曲角(滤波)", "deg")
        block_yaw, val_yaw = create_axis_block("编码值", "")
        main_layout.addWidget(block_pitch)
        line1 = QFrame()
        line1.setFrameShape(QFrame.HLine)
        main_layout.addWidget(line1)
        main_layout.addWidget(block_roll)
        line2 = QFrame()
        line2.setFrameShape(QFrame.HLine)
        main_layout.addWidget(line2)
        main_layout.addWidget(block_yaw)
        return frame, [val_pitch, val_roll, val_yaw]

    def create_flat_card(self, title, val, color):
        frame = QFrame()
        frame.setProperty("flatCard", True)
        layout = QHBoxLayout(frame)
        layout.setContentsMargins(12, 8, 12, 8)
        lbl_val = QLabel(val)
        title_label = QLabel(title)
        layout.addWidget(title_label)
        layout.addStretch()
        layout.addWidget(lbl_val)
        return frame, lbl_val

    # ---------- 设备扩容 ----------
    def expand_device(self, dev_type):
        if dev_type == 'motor':
            self.num_m += 1
            self.motor_data.append([0.0, 0.0, 0.0])
            self.motor_states.append(0)
            self.motor_target.append([0.0, 0.0, 0.0])
            self.hist_motors = [step + [[0.0, 0.0, 0.0]] for step in self.hist_motors]
            self.m_page = 0
        else:
            self.num_s += 1
            self.sensor_data.append([10.0, 0.0, 0.0])
            self.hist_sensors = [step + [[10.0, 0.0, 0.0]] for step in self.hist_sensors]
            self.s_page = 0
        self.rebuild_cards()
        self.refresh_pagination()
        self.logger(f"🔧 成功扩容了一个{dev_type}，当前 M:{self.num_m}, S:{self.num_s}", port=self.port_name)

    def rebuild_cards(self):
        # 清空网格布局
        for i in reversed(range(self.grid_m.count())):
            self.grid_m.itemAt(i).widget().setParent(None)
        for i in reversed(range(self.grid_s.count())):
            self.grid_s.itemAt(i).widget().setParent(None)
        self.cb_motor_id.clear()
        self.cb_sensor_monitor.clear()
        self.cb_view_id.clear()

        # 确保数据列表长度匹配
        if self.num_m > 0:
            while len(self.motor_data) < self.num_m:
                self.motor_data.append([0.0, 0.0, 0.0])
                self.motor_target.append([0.0, 0.0, 0.0])
            while len(self.motor_states) < self.num_m:
                self.motor_states.append(0)
            self.motor_data = self.motor_data[:self.num_m]
            self.motor_states = self.motor_states[:self.num_m]
        else:
            self.motor_data.clear()
            self.motor_states.clear()

        if self.num_s > 0:
            while len(self.sensor_data) < self.num_s:
                self.sensor_data.append([0.0, 0.0, 0.0])
            self.sensor_data = self.sensor_data[:self.num_s]
        else:
            self.sensor_data.clear()

        # 重建电机卡片
        self.cards_motor.clear()
        for i in range(self.num_m):
            card, lbls = self.create_motor_card(f"电机 ID:{i + 1}", "#000")
            self.cards_motor.append((card, lbls))
            self.grid_m.addWidget(card, 0, i % 3)
            self.cb_motor_id.addItem(f"电机 {i + 1}")

        # 重建传感器卡片
        self.cards_sensor.clear()
        for i in range(self.num_s):
            card, lbls = self.create_sensor_card(f"弯曲传感器 ID:{i + 1}", "#D83B01")
            self.cards_sensor.append((card, lbls))
            self.grid_s.addWidget(card, 0, i % 3)
            self.cb_sensor_monitor.addItem(f"弯曲传感器 {i + 1}")

        max_id = max(self.num_m, self.num_s)
        self.cb_view_id.addItems([f"ID {i + 1}" for i in range(max_id)])
        self.refresh_pagination()
        self.update_single_monitor_labels()
        self.update_ui()

    # ---------- 分页 ----------
    def change_page(self, t, delta):
        if t == 'm':
            self.m_page += delta
        else:
            self.s_page += delta
        self.refresh_pagination()

    def refresh_pagination(self):
        m_pages = max(1, math.ceil(self.num_m / 3))
        self.m_page = max(0, min(self.m_page, m_pages - 1))
        self.lbl_m_page.setText(f"电机 {self.m_page + 1}/{m_pages} 页")
        for i, (card, _) in enumerate(self.cards_motor):
            card.setVisible(self.m_page * 3 <= i < (self.m_page + 1) * 3)

        s_pages = max(1, math.ceil(self.num_s / 3))
        self.s_page = max(0, min(self.s_page, s_pages - 1))
        self.lbl_s_page.setText(f"弯曲传感器 {self.s_page + 1}/{s_pages} 页")
        for i, (card, _) in enumerate(self.cards_sensor):
            card.setVisible(self.s_page * 3 <= i < (self.s_page + 1) * 3)

    # ================== 系统控制 ==================
    def sys_toggle(self, checked):
        if checked:
            self.btn_toggle.setText("⏹ 关闭控制系统")
            self.btn_toggle.set_normal_color("#D13438")
            self.btn_toggle.set_hover_color("#6B1418")
            self.btn_toggle.style().unpolish(self.btn_toggle)
            self.btn_toggle.style().polish(self.btn_toggle)
            self.sys_start()
        else:
            self.btn_toggle.setText("▶ 启动控制系统")
            self.btn_toggle.set_normal_color("#107C10")
            self.btn_toggle.set_hover_color("#063A06")
            self.btn_toggle.style().unpolish(self.btn_toggle)
            self.btn_toggle.style().polish(self.btn_toggle)
            self.sys_close()

    def sys_close(self):
        self.is_started = False
        self.send_cmd(0x00, "失能", "关闭控制系统", is_motor=True)

    def sys_start(self):
        if self.is_started:
            return
        self.is_started = True
        self.send_cmd(0x01, "使能", "启动控制系统", is_motor=True)

    def sys_stop(self):
        if self.is_started:
            self.btn_toggle.blockSignals(True)
            self.btn_toggle.setChecked(False)
            self.btn_toggle.setText("▶ 启动控制系统")
            self.btn_toggle.set_normal_color("#107C10")
            self.btn_toggle.set_hover_color("#063A06")
            self.btn_toggle.style().unpolish(self.btn_toggle)
            self.btn_toggle.style().polish(self.btn_toggle)
            self.btn_toggle.blockSignals(False)
        self.is_started = False
        self.send_cmd(0x02, "紧急停止", "LQTS紧急停止按钮", is_motor=True)

    def handle_serial_error(self, error_msg):
        self.serial_error = True
        if self.history_timer.isActive():
            self.history_timer.stop()
        self.hist_time.clear()
        self.hist_motors.clear()
        self.hist_sensors.clear()
        self.logger(f"❌ 串口异常: {error_msg}", port=self.port_name)
        QMessageBox.critical(self, "串口断连", f"当前串口设备 {self.port_name} 已断开连接！")
        # 禁用所有操作按钮
        buttons = [
            self.btn_toggle, self.btn_stop, self.btn_home,
            self.btn_forward, self.btn_release, self.btn_backward,
            self.btn_send_m, self.btn_m_prev, self.btn_m_next,
            self.btn_s_prev, self.btn_s_next,
        ]
        for btn in buttons:
            if btn:
                btn.setEnabled(False)
        for btn in self.addr_buttons:
            btn.setEnabled(False)

    # ================== 命令发送 ==================
    def send_cmd(self, func_code, action, detail, data=b'', is_motor=True):
        debug_mode = False
        if self.debug_check:
            debug_mode = self.debug_check()
        try:
            if func_code not in [0x00, 0x01, 0x02, 0x04, 0x05, 0x06, 0xFE] and not self.is_started and not debug_mode:
                QMessageBox.warning(self, "拒绝", "请先点击启动控制系统")
                self.logger(f"❌ 请先点击启动控制系统", level="ERROR", port=self.port_name)
                return

            frame_head = 0xAA if is_motor else 0xBB
            frame = struct.pack('>BBB', frame_head, func_code, len(data)) + data
            frame += bytes([sum(frame) & 0xFF])

            if debug_mode and self.serial_error:
                self.logger(f"📤 [DEBUG] {action} -> {detail} (模拟发送)", raw_data=frame, level="DEBUG", port=self.port_name)
                return

            self.worker.send_data(frame)
            GlobalHistory.add_record(self.port_name, action, detail, frame.hex().upper())

            if func_code == 0x02:
                self.logger(f"📤 {action} -> {detail}", raw_data=frame, level="WARNING", port=self.port_name)
            else:
                self.logger(f"📤 {action} -> {detail}", raw_data=frame, port=self.port_name)

        except Exception as e:
            QMessageBox.critical(self, "错误", f"发送命令失败: {str(e)}")
            self.logger(f"❌ 发送命令失败: {str(e)}", level="ERROR", port=self.port_name)

    def send_motor(self):
        debug_mode = self.debug_check() if self.debug_check else False
        if self.num_m == 0 and not debug_mode:
            QMessageBox.warning(self, "错误", "当前没有可用的电机设备")
            return
        if debug_mode and self.num_m == 0:
            m_id = 1
            pos, vel, acc = 0, 10, 10
        else:
            m_id = self.cb_motor_id.currentIndex() + 1
            if m_id > self.num_m:
                QMessageBox.warning(self, "错误", f"电机ID {m_id} 无效")
                return
            pos = int(self.spin_m_pos.spin.value() * 100)
            vel = int(self.spin_m_vel.spin.value() * 100)
            acc = int(self.spin_m_acc.spin.value() * 100)

        while len(self.motor_target) < self.num_m:
            self.motor_target.append([0.0, 0.0, 0.0])
        if m_id <= len(self.motor_target):
            self.motor_target[m_id - 1] = [pos / 100, vel / 100, acc / 100]

        direction = 0 if pos >= 0 else 1
        distance = abs(pos)
        data = struct.pack('>BBHHH', m_id, direction, distance, vel, acc)
        self.send_cmd(0x03, f"控制电机{m_id}", f"角度:{pos/100}, 转速:{vel/100}, 角加速度:{acc/100}", data, is_motor=True)

    def send_home_command(self):
        debug_mode = self.debug_check() if self.debug_check else False
        if not self.is_started and not debug_mode:
            QMessageBox.warning(self, "错误", "请先点击启动控制系统")
            return
        if self.num_m == 0:
            QMessageBox.warning(self, "错误", "当前没有可用的电机设备")
            return
        for spin in self.addr_spinboxes:
            spin.setValue(0)
            # 新增：
            self.spin_grab_angle.setValue(0)
        try:
            count = self.num_m
            start_addr = 1
            distances = [0] * count
            data = struct.pack('>BB', count, start_addr)
            for dist in distances:
                data += struct.pack('>H', dist)
            self.send_cmd(0x04, "一键归中", "所有电机距离复位为0", data, is_motor=True)

            # 重置显示的角度目标
            self.target_angle1 = 0.0
            self.target_angle2 = 0.0
            # 重置所有地址输入框
            for spin in self.addr_spinboxes:
                spin.setValue(0)
        except Exception as e:
            QMessageBox.critical(self, "错误", f"发送一键归中命令失败: {str(e)}")
            self.logger(f"❌ 发送一键归中命令失败: {str(e)}", level="ERROR", port=self.port_name)


    # ================== 新增：多地址弯曲控制 ==================
    def bend_forward(self):
        self._send_multi_addr_bend(direction=1)

    def bend_backward(self):
        self._send_multi_addr_bend(direction=-1)

    def bend_release(self):
        self._send_multi_addr_bend(direction=0)

    def grab(self):
        """抓取：所有地址弯曲相同角度（正向）"""
        angle = self.spin_grab_angle.value()
        if self.num_m < 4:
            QMessageBox.warning(self, "错误", "电机数量不足4个，无法执行抓取")
            return
        # 发送1~4号地址，角度为正
        self._send_bend_command([1, 2, 3, 4], [angle] * 4)

    def reverse_grab(self):
        """反向抓取：所有地址弯曲相同角度（反向）"""
        angle = self.spin_grab_angle.value()
        if self.num_m < 4:
            QMessageBox.warning(self, "错误", "电机数量不足4个，无法执行反向抓取")
            return
        # 发送1~4号地址，角度为负
        self._send_bend_command([1, 2, 3, 4], [-angle] * 4)

    def _send_multi_addr_bend(self, direction):
        selected_addrs = []
        values = []
        for i in range(4):
            if self.addr_buttons[i].isChecked():
                addr = i + 1
                val = self.addr_spinboxes[i].value()
                if direction == 0:
                    val = 0
                elif direction == -1:
                    val = -val
                selected_addrs.append(addr)
                values.append(val)
        if not selected_addrs:
            QMessageBox.warning(self, "提示", "请至少勾选一个地址")
            return
        self._send_bend_command(selected_addrs, values)

    def _send_bend_command(self, addrs, angles):
        debug_mode = self.debug_check() if self.debug_check else False
        if not self.is_started and not debug_mode:
            QMessageBox.warning(self, "错误", "请先启动控制系统")
            return

        # 组装协议数据（示例：命令码 0x07）
        data = bytearray()
        data.append(len(addrs))
        for addr, angle in zip(addrs, angles):
            abs_angle = abs(int(angle * 100))
            direction_flag = 0 if angle >= 0 else 1
            data.extend(struct.pack('<BHBB', addr, abs_angle, direction_flag, 0x00))

        # 更新显示的目标角度（假设用最高地址的角度）
        if angles:
            self.target_angle1 = angles[0] if len(angles) >= 1 else 0.0
            self.target_angle2 = angles[1] if len(angles) >= 2 else angles[0]

        self.send_cmd(0x07, "多地址弯曲", f"地址:{addrs} 角度:{angles}", bytes(data), is_motor=True)

    # ================== 闭环控制（保留但暂未使用） ==================
    def closed_loop_control(self):
        if not self.closed_loop_enabled:
            return
        # 原有的闭环逻辑，可保留作为未来扩展
        pass

    def apply_pid_params(self):
        pass  # 暂时不暴露 PID 调节界面

    # ================== 数据解析 ==================
    @pyqtSlot(bytes)
    def parse_data(self, data):
        self.recv_buffer.extend(data)
        if len(self.recv_buffer) > 4096:
            self.recv_buffer = self.recv_buffer[-2048:]

        i = 0
        while i < len(self.recv_buffer):
            if i + 3 > len(self.recv_buffer):
                break
            if self.recv_buffer[i] != 0xBB or self.recv_buffer[i + 1] != 0x02:
                i += 1
                continue

            total_len = self.recv_buffer[i + 2]
            frame_total = total_len + 4
            if i + frame_total > len(self.recv_buffer):
                break

            frame = bytes(self.recv_buffer[i:i + frame_total])
            if (sum(frame[:-1]) & 0xFF) != frame[-1]:
                i += 1
                continue

            status = ProtocolParser.parse_frame(frame, apply_filter=True, filter_obj=self.data_filter)
            if status is not None:
                if status.num_motors != self.num_m or status.num_sensors != self.num_s:
                    self.num_m = status.num_motors
                    self.num_s = status.num_sensors
                    self.rebuild_cards()

                self.motor_data = [[m.pos, m.vel, m.acc] for m in status.motors]
                self.motor_states = [m.status for m in status.motors]
                self.sensor_data = [[s.pitch, s.roll, s.yaw] for s in status.sensors]
                self.current_angle1 = status.bend_angle1
                self.current_angle2 = status.bend_angle2

                self.filtered_bend_angle = (self.angle_filter_alpha * self.current_angle2 +
                                            (1 - self.angle_filter_alpha) * self.filtered_bend_angle)
                self.current_bend_angle = self.filtered_bend_angle

                if self.motor_data:
                    self.current_area_change = (1 - self.motor_data[0][0] / (2 * 3.1415926 * 50)) ** 2 * 100

                self.update_ui()

            self.recv_buffer = self.recv_buffer[i + frame_total:]
            i = 0

    def update_ui(self):
        if len(self.cards_motor) != self.num_m or len(self.cards_sensor) != self.num_s:
            return

        # 更新电机卡片
        for i in range(self.m_page * 3, min((self.m_page + 1) * 3, self.num_m)):
            labels = self.cards_motor[i][1]
            cur_pos, cur_vel, cur_acc = self.motor_data[i]
            tar_pos, tar_vel, tar_acc = self.motor_target[i] if i < len(self.motor_target) else (0, 0, 0)
            state = self.motor_states[i]
            labels[0].setText(f"当前: {cur_pos:.2f}")
            labels[1].setText(f"目标: {tar_pos:.2f}")
            labels[2].setText(f"当前: {cur_vel:.2f}")
            labels[3].setText(f"目标: {tar_vel:.2f}")
            labels[4].setText(f"当前: {cur_acc:.2f}")
            labels[5].setText(f"目标: {tar_acc:.2f}")
            lbl_state = labels[6]
            lbl_state.setStyleSheet("color: #e74c3c; font-size: 10pt;" if state == 0 else "color: #2ecc71; font-size: 10pt;")

        # 更新传感器卡片
        for i in range(self.s_page * 3, min((self.s_page + 1) * 3, self.num_s)):
            self.cards_sensor[i][1][0].setText(f"{self.sensor_data[i][0]:.2f}")
            self.cards_sensor[i][1][1].setText(f"{self.sensor_data[i][1]:.2f}")
            self.cards_sensor[i][1][2].setText(f"{self.sensor_data[i][2]:.2f}")

        # 定点监测更新
        idx = self.cb_view_id.currentIndex()
        is_motor = (self.cb_view_type.currentIndex() == 0)
        if is_motor:
            if 0 <= idx < self.num_m:
                values = self.motor_data[idx]
                for i, (_, value_label) in enumerate(self.single_cards):
                    value_label.setText(f"{values[i]:.2f}")
            else:
                for _, value_label in self.single_cards:
                    value_label.setText("--")
        else:
            if 0 <= idx < self.num_s:
                values = self.sensor_data[idx]
                for i, (_, value_label) in enumerate(self.single_cards):
                    value_label.setText(f"{values[i]:.2f}")
            else:
                for _, value_label in self.single_cards:
                    value_label.setText("--")

        # 电机状态指示灯
        if hasattr(self, 'motor_status_ball') and hasattr(self, 'motor_states'):
            idx = self.cb_motor_id.currentIndex()
            if 0 <= idx < len(self.motor_states):
                state_val = self.motor_states[idx]
                self.motor_status_ball.setStyleSheet(
                    "color: #D13438; font-size: 8pt;" if state_val == 0 else "color: #107C10; font-size: 8pt;")

        # 弯曲角度显示卡片
        if hasattr(self, 'target_angle1_val'):
            self.target_angle1_val.setText(f"{self.target_angle1:.2f}")
        if hasattr(self, 'current_angle1_val'):
            self.current_angle1_val.setText(f"{self.current_angle1:.2f}")
        if hasattr(self, 'target_angle2_val'):
            self.target_angle2_val.setText(f"{self.target_angle2:.2f}")
        if hasattr(self, 'current_angle2_val'):
            self.current_angle2_val.setText(f"{self.current_angle2:.2f}")

    def update_motor_status_ball(self, idx=None):
        if idx is None:
            idx = self.cb_motor_id.currentIndex()
        if hasattr(self, 'motor_status_ball') and hasattr(self, 'motor_states'):
            if 0 <= idx < len(self.motor_states):
                state_val = self.motor_states[idx]
                self.motor_status_ball.setStyleSheet(
                    "color: #D13438; font-size: 8pt;" if state_val == 0 else "color: #107C10; font-size: 8pt;")

    def update_sensor_monitor(self, idx=None):
        pass

    # ================== 历史记录与曲线 ==================
    def record_history(self):
        if self.serial_error:
            return
        if self.start_time is None:
            self.start_time = time.time()

        current_time_sec = time.time() - self.start_time

        # 弯曲角度历史
        self.hist_bend_time.append(current_time_sec)
        self.hist_target1.append(self.target_angle1)
        self.hist_current1.append(self.current_angle1)
        self.hist_target2.append(self.target_angle2)
        self.hist_current2.append(self.current_angle2)

        while len(self.hist_bend_time) > 0 and self.hist_bend_time[0] < current_time_sec - 60:
            self.hist_bend_time.pop(0)
            self.hist_target1.pop(0)
            self.hist_current1.pop(0)
            self.hist_target2.pop(0)
            self.hist_current2.pop(0)

        if self.bend_graph_window and self.bend_graph_window.isVisible():
            self.bend_graph_controller.window.update_bend_data(
                self.hist_bend_time,
                self.hist_target1, self.hist_current1,
                self.hist_target2, self.hist_current2
            )

        # 电机/传感器历史
        should_record_motor = (self.num_m > 0 and self.motor_data and len(self.motor_data) == self.num_m)
        should_record_sensor = (self.num_s > 0 and self.sensor_data and len(self.sensor_data) == self.num_s)

        if should_record_motor or should_record_sensor:
            self.hist_time.append(current_time_sec)
            if should_record_motor:
                self.hist_motors.append(self.motor_data.copy())
            else:
                self.hist_motors.append([])
            if should_record_sensor:
                self.hist_sensors.append(self.sensor_data.copy())
            else:
                self.hist_sensors.append([])

            while self.hist_time and self.hist_time[0] < current_time_sec - 60:
                self.hist_time.pop(0)
                if self.hist_motors:
                    self.hist_motors.pop(0)
                if self.hist_sensors:
                    self.hist_sensors.pop(0)

        if hasattr(self, 'active_graph_controller') and self.active_graph_controller:
            if hasattr(self, 'active_graph_ui') and self.active_graph_ui and self.active_graph_ui.isVisible():
                if self.active_type == 'motor' and self.hist_motors and len(self.hist_motors) > 0:
                    valid_motor_data = [data for data in self.hist_motors if data and len(data) > 0]
                    valid_times = self.hist_time[-len(valid_motor_data):] if valid_motor_data else []
                    self.active_graph_controller.update_multi_data(valid_times, valid_motor_data)
                elif self.active_type == 'sensor' and self.hist_sensors and len(self.hist_sensors) > 0:
                    valid_sensor_data = [data for data in self.hist_sensors if data and len(data) > 0]
                    valid_times = self.hist_time[-len(valid_sensor_data):] if valid_sensor_data else []
                    self.active_graph_controller.update_multi_data(valid_times, valid_sensor_data)

    def open_bend_graph(self):
        if self.bend_graph_window is None:
            from UI.graph_window import BendGraphWindow
            from Core.GraphController import BendGraphController
            self.bend_graph_window = BendGraphWindow(self)
            self.bend_graph_controller = BendGraphController(self.bend_graph_window, self)
        self.bend_graph_window.show()
        if self.hist_bend_time:
            self.bend_graph_controller.window.update_bend_data(
                self.hist_bend_time,
                self.hist_target1, self.hist_current1,
                self.hist_target2, self.hist_current2
            )

    # ---------- 辅助：自定义 SpinBox ----------
    def _create_custom_spinbox(self, min_val, max_val, default, prefix='', suffix='', step=1.0):
        container = QWidget()
        layout = QHBoxLayout(container)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)

        spin = QDoubleSpinBox()
        spin.setRange(min_val, max_val)
        spin.setValue(default)
        if prefix:
            spin.setPrefix(prefix)
        if suffix:
            spin.setSuffix(suffix)
        spin.setSingleStep(step)
        spin.setButtonSymbols(QAbstractSpinBox.NoButtons)
        spin.setStyleSheet("""
            QDoubleSpinBox {
                min-height: 34px;
                font-size: 10pt;
                font-weight: bold;
                border: 1px solid #cfe2f2;
                border-radius: 10px;
                background: white;
                padding: 4px 8px;
                color: #1a3b4f;
            }
            QDoubleSpinBox:focus {
                border-color: #1e6f9f;
            }
        """)

        btn_plus = QPushButton("+")
        btn_plus.setFixedSize(34, 34)
        btn_plus.setCursor(Qt.PointingHandCursor)
        btn_plus.setProperty("spinBoxButton", True)
        btn_plus.clicked.connect(lambda: spin.stepUp())

        btn_minus = QPushButton("−")
        btn_minus.setFixedSize(34, 34)
        btn_minus.setCursor(Qt.PointingHandCursor)
        btn_minus.setProperty("spinBoxButton", True)
        btn_minus.clicked.connect(lambda: spin.stepDown())

        layout.addWidget(spin)
        layout.addWidget(btn_plus)
        layout.addWidget(btn_minus)

        container.spin = spin
        return container

    def on_debug_changed(self, enabled):
        if enabled:
            if self.num_m == 0:
                self.expand_device('motor')
                self.logger("🐛 Debug 模式：已自动创建虚拟电机 ID:1")
            if self.num_s == 0:
                self.expand_device('sensor')
                self.logger("🐛 Debug 模式：已自动创建虚拟传感器 ID:1")
        else:
            self.num_m = 0
            self.num_s = 0
            self.rebuild_cards()