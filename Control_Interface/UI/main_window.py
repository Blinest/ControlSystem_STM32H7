# ==========================================
# 6. 主窗口
# ==========================================
# ui/main_window.py
# Qt类
from PyQt5.QtWidgets import (QMainWindow, QToolBar, QLabel, QComboBox, QPushButton,
                             QTabWidget, QDockWidget, QWidget, QVBoxLayout, QHBoxLayout,
                             QTextEdit, QCheckBox, QSplitter, QLineEdit, QFileDialog, QApplication,
                             QSizePolicy, QGraphicsOpacityEffect, QMessageBox, QDialog)
from PyQt5.QtCore import Qt, QTimer

# 自定义类
from .device_tab import DeviceTab
from .graph_window import GraphWindowUI
from .log_manager_win import LogManagerWindow
from .widgets import AnimatedButton
from .Local3DViewer import Local3DViewer
from .log_window import LoginWindow
from Core.logger import default_log_manager as log_manager
from Core.GraphController import GraphController
from UI.styles import get_main_window_style

# 工具类
import os
import sys
import re
from datetime import datetime
import time
import serial
import serial.tools.list_ports


class MainWindow(QMainWindow):
    # TARGET_PORT = "COM10"
    def __init__(self, auth_service=None):
        super().__init__()
        self.auth_service = auth_service  # 保存认证服务引用
        self.setWindowTitle("空中水母控制界面")
        # 设置窗口标志，确保最大化窗口可用
        self.setWindowFlags(self.windowFlags() | Qt.WindowMaximizeButtonHint)


        # 根据分辨率自动动态设置窗口大小
        screen = QApplication.primaryScreen()
        size = screen.availableGeometry().size()
        new_height = min(size.height(), 1000)
        new_width = min(size.width(), 1500)
        self.resize(new_width, new_height)
        self.devices = {}
        self.auto_added_ports = set()
        self.auto_connect_timer = QTimer()
        self.auto_connect_timer.timeout.connect(self.auto_check_target_port)
        self.auto_connect_timer.start(200)
        self.target_port = self._default_target_port()


        # 创建状态栏
        self.statusBar().showMessage("就绪")

        # 创建工具栏
        toolbar = QToolBar("管理栏")
        self.addToolBar(toolbar)

        # 端口相关控件
        self.combo_ports = QComboBox()
        self.refresh_ports()
        self.btn_add = AnimatedButton("🔌 接入新设备", "grey", "#505050")
        self.btn_add.setProperty("class", "page-btn")
        self.btn_add.clicked.connect(self.add_device)
        toolbar.addWidget(QLabel(" 端口:"))
        toolbar.addWidget(self.combo_ports)
        toolbar.addWidget(self.btn_add)
        toolbar.addSeparator()
        toolbar.addWidget(QLabel("手动输入: "))
        self.manual_port_edit = QLineEdit()
        self.manual_port_edit.setPlaceholderText("COM14 或 /dev/ttyCH341USB0")
        self.manual_port_edit.setFixedWidth(150)
        self.btn_manual_add = AnimatedButton("➕ 手动添加", "grey", "#505050")
        self.btn_manual_add.setProperty("class", "page-btn")
        self.btn_manual_add.clicked.connect(self.add_manual_device)
        toolbar.addWidget(self.manual_port_edit)
        toolbar.addWidget(self.btn_manual_add)
        toolbar.addSeparator()
        toolbar.addAction("📈 电机反馈数据曲线", lambda: self.open_graph('motor'))
        toolbar.addAction("📉 弯曲传感器反馈数据曲线", lambda: self.open_graph('sensor'))
        toolbar.addSeparator()

        # 空中水母弯曲历史曲线
        toolbar.addAction("📊 空中水母弯曲数据曲线", self.open_bend_graph)
        toolbar.addSeparator()

        # 日志管理按钮 - 只有管理员可见
        self.log_manager_action = toolbar.addAction("📋 日志管理")
        self.log_manager_action.triggered.connect(self.open_log_manager)

        # 添加 3D 视图按钮 - 只有管理员可见
        self.btn_3D =  toolbar.addAction("🌐 3D模型")
        self.btn_3D.triggered.connect(self.open_3d_viewer)


        # 根据用户角色控制日志管理、3D视图按钮的可见性
        if self.auth_service and not self.auth_service.is_admin():
            self.log_manager_action.setVisible(False)
            self.btn_3D.setVisible(False)

        # 添加用户信息和登出按钮到工具栏
        self._setup_toolbar_with_user(toolbar)

        # 标签页
        self.tabs = QTabWidget()
        self.tabs.setTabsClosable(True)
        self.tabs.tabCloseRequested.connect(self.close_device)
        self.setCentralWidget(self.tabs)



        # 日志窗口
        dock_log = QDockWidget("通信日志与调试器", self)
        log_widget = QWidget()
        log_layout = QVBoxLayout(log_widget)
        log_ctrl_layout = QHBoxLayout()
        self.chk_debug = QCheckBox("🐛 启用 Debug 模式 (分屏同步显示底层发送的十六进制码)")
        self.chk_debug.setStyleSheet("font-weight: bold; color: #D13438;")
        self.chk_debug.stateChanged.connect(self.toggle_debug_view)
        log_ctrl_layout.addWidget(self.chk_debug)
        log_ctrl_layout.addStretch()
        log_layout.addLayout(log_ctrl_layout)
        self.log_splitter = QSplitter(Qt.Horizontal)
        self.text_log = QTextEdit()
        self.text_log.setReadOnly(True)
        self.text_log.setStyleSheet("background-color: #fdfdfd; border: 1px solid #cfe2f2; color: #1e1e1e;")
        self.text_raw = QTextEdit()
        self.text_raw.setReadOnly(True)
        self.text_raw.setStyleSheet("background-color: #f5f9ff; color: #1e6f9f; font-family: Consolas; border: 1px solid #cfe2f2;")
        self.text_raw.hide()
        self.log_splitter.addWidget(self.text_log)
        self.log_splitter.addWidget(self.text_raw)
        log_layout.addWidget(self.log_splitter)
        dock_log.setWidget(log_widget)
        self.addDockWidget(Qt.BottomDockWidgetArea, dock_log)
        self.setStyleSheet(get_main_window_style())
        # 显示欢迎信息（使用 QTimer 延迟显示，确保窗口已完全加载）
        if self.auth_service and self.auth_service.is_logged_in():
            username = self.auth_service.get_current_user()
            print(f"DEBUG: 用户已登录: {username}")  # 调试信息
            QTimer.singleShot(200, lambda: self._show_welcome(username))
            QTimer.singleShot(500, lambda: self._update_ui_for_user(username))
        else:
            print("DEBUG: 用户未登录或auth_service为空")  # 调试信息

    def _setup_toolbar_with_user(self, toolbar):
        """在工具栏右侧添加用户信息和登出按钮"""
        # 添加弹性空间
        spacer = QWidget()
        spacer.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        toolbar.addWidget(spacer)

        # 显示当前用户
        if self.auth_service:
            username = self.auth_service.get_current_user()
            role = self.auth_service.get_current_role()

            # 根据角色显示不同图标
            role_icon = "👑" if role == "admin" else "👤"
            self.role_text = "管理员" if role == "admin" else "普通用户"

            # 用户标签
            user_label = QLabel(f"{role_icon} {username} ({self.role_text})")
            user_label.setObjectName("toolbarUserLabel")
            toolbar.addWidget(user_label)

            # 登出按钮
            btn_logout = QPushButton("🚪 登出")
            btn_logout.setObjectName("toolbarLogoutButton")

            btn_logout.clicked.connect(self.logout)
            toolbar.addWidget(btn_logout)

    def logout(self):
        """登出操作"""
        reply = QMessageBox.question(
            self, "确认登出", "确定要退出登录吗？",
            QMessageBox.Yes | QMessageBox.No
        )
        if reply == QMessageBox.Yes:
            if self.auth_service:
                self.auth_service.logout()

            # 停止所有定时器
            self.auto_connect_timer.stop()

            # 关闭所有设备连接
            for port, device in list(self.devices.items()):
                if hasattr(device, 'worker'):
                    device.worker.stop()
                if hasattr(device, 'history_timer'):
                    device.history_timer.stop()

            self.close()

            # 重新打开登录窗口
            self.login_window = LoginWindow()
            if self.login_window.exec_() == QDialog.Accepted:
                self.main_window = MainWindow(self.auth_service)
                self.main_window.show()

    def _show_welcome(self, username):
        """显示欢迎信息"""
        print(f"DEBUG: _show_welcome 被调用, username={username}")  # 调试信息

        # 1. 更新窗口标题
        self.setWindowTitle(f"空中水母控制界面 - 当前用户: {username}")

        # 2. 状态栏显示
        self.statusBar().showMessage(f"👤 当前用户: {username} | ✅ 就绪", 0)

        # 3. 日志窗口显示
        self.log("=" * 50, level="INFO")
        self.log(f"🎉 欢迎使用空中水母控制平台", level="INFO")
        self.log(f"👤 当前登录用户: {username}({self.role_text})", level="INFO")
        self.log(f"🕐 登录时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}", level="INFO")
        self.log("=" * 50, level="INFO")

        # 4. 创建浮动提示
        self._show_toast(f"{username}, 欢迎使用空中水母控制平台！")

    def _show_toast(self, message, duration=2000):
        """显示短暂的提示信息"""

        toast = QLabel(message, self)
        toast.setAlignment(Qt.AlignCenter)

        # 初始样式
        toast.setStyleSheet("""
            QLabel {
                background-color: #F7A24F;
                color: white;
                font-size: 18px;
                font-weight: bold;
                padding: 15px 30px;
                border-radius: 10px;
                border: 2px solid white;
            }
        """)
        toast.adjustSize()

        # 显示在窗口顶部中央
        x = (self.width() - toast.width()) // 2
        y = 80
        toast.move(x, y)

        # 创建透明度效果
        opacity_effect = QGraphicsOpacityEffect(opacity=1.0)
        toast.setGraphicsEffect(opacity_effect)
        toast.show()
        toast.raise_()

        # 保存引用
        self._current_toast = toast
        self._toast_effect = opacity_effect
        self._fade_step = 0

        # 使用 QTimer 逐步淡出
        def fade_step():
            if hasattr(self, '_toast_effect') and self._toast_effect:
                self._fade_step += 1
                # 20步完成淡出，每步50ms，总共1000ms
                if self._fade_step <= 20:
                    opacity = 1.0 - (self._fade_step / 20.0)
                    self._toast_effect.setOpacity(opacity)
                    # 继续下一步
                    QTimer.singleShot(50, fade_step)
                else:
                    # 淡出完成，删除
                    if hasattr(self, '_current_toast') and self._current_toast:
                        self._current_toast.deleteLater()
                        self._current_toast = None
                        self._toast_effect = None

        # 在指定时间后开始淡出
        QTimer.singleShot(duration, fade_step)

    def _update_ui_for_user(self, username):
        """根据用户更新界面"""
        pass


    def log(self, msg, raw_data=None, level="INFO", port=None):
        t_str = time.strftime('%H:%M:%S')

        # 根据 Debug 模式未开启
        if not self.chk_debug.isChecked():
            # Debug 未开启：显示 INFO/WARNING/ERROR 日志
            formatted_msg = log_manager.create_log_entry(level, msg, port, raw_data)
            self.text_log.append(f"[{t_str}] {formatted_msg}")
            self.text_log.verticalScrollBar().setValue(self.text_log.verticalScrollBar().maximum())

        # Debug 模式开启时，显示 DEBUG 级别的原始数据日志
        if self.chk_debug.isChecked() and raw_data is not None:
            hex_str = ' '.join([f'{b:02X}' for b in raw_data])

            # 十六进制面板显示
            debug_raw_msg = f"[{t_str}] TX >>  {hex_str}"
            self.text_raw.append(debug_raw_msg)
            self.text_raw.verticalScrollBar().setValue(self.text_raw.verticalScrollBar().maximum())

            # 主日志面板显示 DEBUG 日志
            debug_log_msg = f"[{msg}] 发送原始数据: {hex_str}" if msg else f"发送原始数据: {hex_str}"
            debug_formatted = log_manager.create_log_entry("DEBUG", debug_log_msg, port)
            self.text_log.append(f"[{t_str}] {debug_formatted}")
            self.text_log.verticalScrollBar().setValue(self.text_log.verticalScrollBar().maximum())

    def toggle_debug_view(self, state):
        if state == Qt.Checked:
            self.text_raw.show()
            self.text_raw.append(">>> DEBUG_MODE_ENABLED: HEX MACHINE CODE TERMINAL <<<")
        else:
            self.text_raw.hide()

        # 通知当前设备选项卡 Debug 状态变化
        current_tab = self.tabs.currentWidget()
        if current_tab and hasattr(current_tab, 'on_debug_changed'):
            current_tab.on_debug_changed(state == Qt.Checked)
        else:
            print("No current tab with on_debug_changed")  # 调试输出

    def auto_check_target_port(self):
        all_ports = serial.tools.list_ports.comports()
        valid_ports = []

        # 优先把目标端口放在第一位
        for port in all_ports:
            if port.device == self.target_port:
                valid_ports.append(port.device)
                break

        for port in all_ports:
            if port.device == self.target_port:
                continue
            if self.is_valid_serial_port(port.device):
                valid_ports.append(port.device)

        if not valid_ports:
            if self.combo_ports.currentText() != "无可用串口":
                self.combo_ports.clear()
                self.combo_ports.addItem("无可用串口")
            return
        current_items = [self.combo_ports.itemText(i) for i in range(self.combo_ports.count())]
        if set(valid_ports) != set(current_items):
            old_selection = self.combo_ports.currentText()
            self.combo_ports.clear()
            seen = set()
            unique_ports = [x for x in valid_ports if not (x in seen or seen.add(x))]
            self.combo_ports.addItems(unique_ports)
            if self.target_port in unique_ports and old_selection != self.target_port:
                self.combo_ports.setCurrentText(self.target_port)
            elif old_selection in unique_ports:
                self.combo_ports.setCurrentText(old_selection)
        # 只自动添加目标端口，其余端口由用户手动选择
        if self.target_port not in self.devices and self.target_port not in self.auto_added_ports:
            if self.target_port in valid_ports:
                self.connect_port(self.target_port)
                self.auto_added_ports.add(self.target_port)
                self.log(f"自动添加设备 {self.target_port}")

    def connect_port(self, port):
        if port in self.devices:
            return
        dev_tab = DeviceTab(port, self.log, debug_check=self.chk_debug.isChecked)
        self.devices[port] = dev_tab
        self.tabs.addTab(dev_tab, f"📍 {port}")
        self.tabs.setCurrentWidget(dev_tab)

        # 如果 Debug 模式已勾选，立即创建虚拟设备
        if self.chk_debug.isChecked():
            dev_tab.on_debug_changed(True)


        QTimer.singleShot(100, lambda: dev_tab.send_cmd(0xFE, "自动握手", "唤醒通信", is_motor=True))

    def add_device(self):
        port = self.combo_ports.currentText()
        if not port or port == "无可用串口":
            QMessageBox.warning(self, "提示", "没有可用的串口设备，请检查硬件连接后刷新重试。")
            return
        if port in self.devices:
            QMessageBox.warning(self, "提示", f"设备 {port} 已经添加")
            return
        dev_tab = DeviceTab(port, self.log, debug_check=self.chk_debug.isChecked)
        self.devices[port] = dev_tab
        self.tabs.addTab(dev_tab, f"📍 {port}")
        self.tabs.setCurrentWidget(dev_tab)

        if self.chk_debug.isChecked():
            dev_tab.on_debug_changed(True)

        QTimer.singleShot(100, lambda: dev_tab.send_cmd(0xFE, "底层握手", "唤醒通信", is_motor=True))

    def close_device(self, index):
        widget = self.tabs.widget(index)
        port_name = widget.port_name
        widget.worker.stop()
        if hasattr(widget, 'history_timer'):
            widget.history_timer.stop()
        del self.devices[port_name]
        self.tabs.removeTab(index)

    def open_graph(self, g_type):
        dev = self.tabs.currentWidget()
        if not dev:
            return

        dev.active_type = g_type

        if g_type == 'motor':
            title = f"电机反馈数据曲线图 ({dev.port_name})"
            is_motor = True
            num_devices = dev.num_m
        else:
            title = f"弯曲传感器反馈数据曲线图 ({dev.port_name})"
            is_motor = False
            num_devices = dev.num_s


        ui = GraphWindowUI(title, is_motor=is_motor, num_devices=num_devices)
        controller = GraphController(ui, is_history_mode=False)
        controller.main_window = self          # <--- 设置 main_window 引用
        ui.set_controller(controller)

        dev.active_graph_ui = ui
        dev.active_graph_controller = controller

        ui.show()

    def refresh_ports(self):
        self.combo_ports.clear()
        all_ports = serial.tools.list_ports.comports()
        valid_ports = []

        # 优先把目标端口放在第一位
        for port in all_ports:
            if port.device == self.target_port:
                valid_ports.append(port.device)
                break

        for port in all_ports:
            if port.device == self.target_port:
                continue
            if self.is_valid_serial_port(port.device):
                valid_ports.append(port.device)

        if valid_ports:
            seen = set()
            unique_ports = [x for x in valid_ports if not (x in seen or seen.add(x))]
            self.combo_ports.addItems(unique_ports)
            if self.target_port in unique_ports:
                self.combo_ports.setCurrentText(self.target_port)
        else:
            self.combo_ports.addItem("无可用串口")
    def add_manual_device(self):
        port = self.manual_port_edit.text().strip()
        if not port:
            QMessageBox.warning(self, "提示", "请输入串口路径，如 /dev/ttyCH341USB0")
            return
        if port in self.devices:
            QMessageBox.warning(self, "提示", f"设备 {port} 已经添加")
            return
        if not self.port_exists(port):
            QMessageBox.warning(self, "错误", f"设备 {port} 不存在")
            return
        self.connect_port(port)

    def port_exists(self, port):
        try:
            ser = serial.Serial(port, timeout=0.1)
            ser.close()
            return True
        except (serial.SerialException, FileNotFoundError):
            return False

    def is_valid_serial_port(self, device):
        """判断串口设备是否为有效的用户可用端口（跨平台）"""
        if sys.platform == 'win32':
            # Windows: COM 口，排除蓝牙等虚拟端口
            return bool(re.match(r'^COM\d+$', device, re.IGNORECASE))
        else:
            # Linux/macOS: 排除系统保留端口，保留 USB/ACM/CH341 类设备
            if device.startswith('/dev/ttyS') or device.startswith('/dev/ttyAMA'):
                return False
            return any(k in device.upper() for k in ('USB', 'ACM', 'CH341'))

    def _default_target_port(self):
        """根据平台返回默认目标串口"""
        if sys.platform == 'win32':
            return "COM14"
        return "/dev/ttyCH341USB0"

    def open_log_manager(self):
        log_window = LogManagerWindow(self)
        log_window.exec_()

    def open_history_graph(self, file_path=None, is_motor=True):
        """加载电机或传感器历史数据"""
        if file_path is None:
            # 弹出文件选择对话框
            from PyQt5.QtWidgets import QFileDialog
            data_dir = os.path.expanduser(f"~/.lqts/analyze_data/{'motor_data' if is_motor else 'sensor_data'}")
            if not os.path.exists(data_dir):
                os.makedirs(data_dir, exist_ok=True)
            file_path, _ = QFileDialog.getOpenFileName(
                self,
                f"选择{'电机' if is_motor else '传感器'}历史数据文件",
                data_dir,
                "CSV文件 (*.csv)"
            )
            if not file_path:
                return
        title = "历史数据 - " + os.path.basename(file_path)
        ui = GraphWindowUI(title, is_motor=is_motor, num_devices=1,
                           enable_auto_save=False, is_history_mode=True)
        controller = GraphController(ui, is_history_mode=True)
        ui.set_controller(controller)
        ui.show()
        QTimer.singleShot(50, lambda: controller._load_csv_file(file_path))

    def open_bend_history_graph(self):
        """打开弯曲角度历史数据加载窗口（独立实现）"""
        from UI.graph_window import BendGraphWindow, BendHistoryFileDialog
        from Core.GraphController import BendGraphController

        dialog = BendHistoryFileDialog(self)

        def on_file_selected(file_path):
            try:
                # 可选：复用已有窗口（简单实现：每次新窗口）
                win = BendGraphWindow(is_history_mode=True, enable_auto_save=False)
                # 设置窗口标题为文件名
                win.setWindowTitle(f"历史弯曲数据 - {os.path.basename(file_path)}")

                controller = BendGraphController(win, data_provider=None)
                win.set_controller(controller)

                # 加载数据（如果失败，内部会弹错误框，不显示窗口）
                controller.load_history_file(file_path)
                win.show()
            except Exception as e:
                QMessageBox.critical(self, "错误", f"加载失败：{str(e)}")
            finally:
                dialog.accept()

        dialog.file_selected.connect(on_file_selected)
        dialog.exec_()

    def open_bend_graph(self):
        """打开当前设备选项卡的喷管弯曲数据曲线窗口"""
        current_tab = self.tabs.currentWidget()
        if not current_tab:
            QMessageBox.warning(self, "提示", "没有打开任何设备，请先添加串口设备。")
            return
        # 调用 DeviceTab 中的 open_bend_graph 方法
        if hasattr(current_tab, 'open_bend_graph'):
            current_tab.open_bend_graph()
        else:
            QMessageBox.warning(self, "提示", "当前设备选项卡不支持弯曲历史曲线功能。")

    def open_3d_viewer(self):

        file_path = os.path.expanduser("~/.lqts/auth_data/LQTS.html")
        if not os.path.exists(file_path):
            QMessageBox.warning(self, "错误", f"3D模型文件不存在:\n{file_path}\n请检查文件是否放置正确。")
            return
        self.viewer = Local3DViewer()
        self.viewer.load_file(file_path)
        self.viewer.show()
