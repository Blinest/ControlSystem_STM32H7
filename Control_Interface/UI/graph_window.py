# ui/graph_window.py

import os
import csv
from datetime import datetime
from PyQt5.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QPushButton, QLabel, QCheckBox,
    QWidget, QScrollArea, QApplication, QDoubleSpinBox, QListWidget,
    QListWidgetItem, QMessageBox
)
from PyQt5.QtCore import Qt, QTimer, pyqtSignal
import pyqtgraph as pg
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

# ===================== 抽象基类：绘图窗口 =====================
class BasePlotWindow(QDialog):
    """绘图窗口抽象基类，提供工具栏、自动保存、坐标标签、复选框等通用逻辑"""
    def __init__(self, title, is_history_mode=False, enable_auto_save=True, parent=None):
        super().__init__(parent)
        self.setWindowTitle(title)
        self.setMinimumSize(800, 500)
        self.is_history_mode = is_history_mode
        self.enable_auto_save = enable_auto_save

        # 数据存储（子类可扩展）
        self.time_data = []
        # 其他数据由子类具体定义

        # 控制器引用
        self._controller = None

        # 自动保存定时器
        self.auto_save_timer = QTimer()
        self.auto_save_timer.timeout.connect(self._auto_save)

        # 界面构建
        self._init_common_ui()
        self._create_plot_area()      # 子类实现：创建特定绘图区域
        self._init_checkboxes()       # 子类实现：创建复选框
        self._apply_style()

        # 鼠标点击事件连接（子类负责连接自己的画布）
        self._connect_click_event()

    def _init_common_ui(self):
        """构建通用的工具栏和控件"""
        main_layout = QVBoxLayout(self)

        # ---------- 工具栏 ----------
        toolbar_layout = QHBoxLayout()
        btn_style = "QPushButton { background: #f0e9de; color: #2e241b; border: 1px solid #c4b49a; border-radius: 4px; padding: 5px 10px; }"
        accent_btn_style = "QPushButton { background: #b5956b; color: white; border: none; border-radius: 4px; padding: 5px 10px; font-weight: bold; }"
        reset_btn_style = "QPushButton { background: #d9c6a3; color: #2e241b; border: 1px solid #c4b49a; border-radius: 4px; padding: 5px 10px; }"

        self.btn_focus = QPushButton("🎯 一键聚焦")
        self.btn_focus_x = QPushButton("📏 X轴聚焦")
        self.btn_focus_y = QPushButton("📐 Y轴聚焦")
        self.btn_reset = QPushButton("🔄 重置视图")
        self.btn_save = QPushButton("💾 保存数据")
        self.btn_load = QPushButton("📂 加载历史")

        for btn in (self.btn_focus, self.btn_focus_x, self.btn_focus_y):
            btn.setStyleSheet(btn_style)
        self.btn_reset.setStyleSheet(reset_btn_style)
        self.btn_save.setStyleSheet(accent_btn_style)
        self.btn_load.setStyleSheet(accent_btn_style)

        focus_row = QHBoxLayout()
        focus_row.addWidget(self.btn_focus)
        focus_row.addWidget(self.btn_focus_x)
        focus_row.addWidget(self.btn_focus_y)
        focus_row.addWidget(self.btn_reset)
        focus_row.addWidget(self.btn_save)
        focus_row.addWidget(self.btn_load)
        focus_row.addStretch()
        toolbar_layout.addLayout(focus_row)
        main_layout.addLayout(toolbar_layout)

        if self.is_history_mode:
            self.btn_save.hide()
            self.btn_load.hide()

        # ---------- 自动保存控件 ----------
        if self.enable_auto_save and not self.is_history_mode:
            auto_layout = QHBoxLayout()
            self.chk_auto_save = QCheckBox("⏱ 自动保存")
            self.spin_interval = QDoubleSpinBox()
            self.spin_interval.setRange(5.0, 3600.0)
            self.spin_interval.setValue(30.0)
            self.spin_interval.setSuffix(" 秒")
            self.spin_interval.setEnabled(False)
            self.chk_auto_save.toggled.connect(self.spin_interval.setEnabled)
            self.chk_auto_save.toggled.connect(self._on_auto_save_toggled)
            auto_layout.addWidget(self.chk_auto_save)
            auto_layout.addWidget(QLabel("间隔:"))
            auto_layout.addWidget(self.spin_interval)
            auto_layout.addStretch()
            main_layout.addLayout(auto_layout)
        else:
            self.chk_auto_save = None
            self.spin_interval = None

        # ---------- 坐标显示标签 ----------
        self.coord_label = QLabel("点击曲线查看该点坐标")
        self.coord_label.setStyleSheet("background-color: #f0e9de; border: 1px solid #c4b49a; border-radius: 4px; padding: 5px 10px; font-family: monospace; font-size: 10pt; color: #5a4636;")
        self.coord_label.setMinimumHeight(60)
        self.coord_label.setWordWrap(True)
        main_layout.addWidget(self.coord_label)

        # 保存主布局供子类添加绘图区域
        self.main_layout = main_layout

    def _create_plot_area(self):
        raise NotImplementedError

    def _init_checkboxes(self):
        raise NotImplementedError

    def _connect_click_event(self):
        raise NotImplementedError

    def update_plot(self):
        # 确保曲线对象已初始化（防御性检查，但不要重新创建画布）
        if self.line_target1 is None:
            print("ERROR: Plot curves not initialized. Please restart the window.")
            return

        # 设置可见性
        self.line_target1.set_visible(self.checkboxes[0].isChecked())
        self.line_current1.set_visible(self.checkboxes[1].isChecked())
        self.line_target2.set_visible(self.checkboxes[2].isChecked())
        self.line_current2.set_visible(self.checkboxes[3].isChecked())

        # 更新数据
        self.line_target1.set_data(self.time_data, self.target1_data)
        self.line_current1.set_data(self.time_data, self.current1_data)
        self.line_target2.set_data(self.time_data, self.target2_data)
        self.line_current2.set_data(self.time_data, self.current2_data)

        # 刷新视图
        self.ax.relim()
        self.ax.autoscale_view()
        self.canvas.draw_idle()

    def _apply_style(self):
        self.setStyleSheet("""
            QDialog { background-color: #fdfaf5; }
            QLabel { color: #5a4636; }
            QCheckBox { color: #5a4636; font-weight: bold; }
            QDoubleSpinBox { background: #f0e9de; border: 1px solid #c4b49a; border-radius: 4px; padding: 2px 5px; color: #2e241b; }
            QScrollArea { border: 1px solid #c4b49a; border-radius: 4px; background: #fdfaf5; }
        """)

    def set_controller(self, controller):
        self._controller = controller
        # 子类可以覆盖或调用 super().set_controller(controller) 后连接按钮
        if hasattr(controller, 'auto_focus'):
            self.btn_focus.clicked.connect(controller.auto_focus)
        if hasattr(controller, 'auto_focus_axis'):
            self.btn_focus_x.clicked.connect(lambda: controller.auto_focus_axis('x'))
            self.btn_focus_y.clicked.connect(lambda: controller.auto_focus_axis('y'))
        if hasattr(controller, 'reset_view'):
            self.btn_reset.clicked.connect(controller.reset_view)
        if hasattr(controller, 'save_data'):
            self.btn_save.clicked.connect(controller.save_data)
        # 加载历史由子类决定如何实现（通常通过本地对话框 + 控制器加载方法）
        if hasattr(controller, 'open_history_dialog'):
            self.btn_load.clicked.connect(controller.open_history_dialog)
        else:
            self.btn_load.clicked.connect(self._on_load_history)

    def _on_load_history(self):
        """默认实现：弹出本地历史文件对话框，子类可覆盖或调用"""
        pass   # 实际实现由子类完成

    # ---------- 自动保存 ----------
    def _on_auto_save_toggled(self, checked):
        if checked and self.spin_interval:
            interval_ms = int(self.spin_interval.value() * 1000)
            self.auto_save_timer.start(interval_ms)
        else:
            self.auto_save_timer.stop()

    def _auto_save(self):
        if self.time_data and not self.is_history_mode:
            self.save_data()    # 要求子类实现 save_data 或控制器提供

    def save_data(self):
        if self._controller and hasattr(self._controller, 'save_data'):
            self._controller.save_data()
        else:
            self._local_save_data()

    def _local_save_data(self):
        """子类可覆盖的本地保存实现"""
        QMessageBox.warning(self, "保存不可用", "未实现本地保存，请实现 _local_save_data 或提供控制器")


# ===================== 抽象基类：历史文件对话框 =====================
class BaseHistoryDialog(QDialog):
    """历史文件选择对话框抽象基类"""
    file_selected = pyqtSignal(str)

    def __init__(self, title, parent=None):
        super().__init__(parent)
        self.setWindowTitle(title)
        self.setMinimumSize(640, 480)
        self._init_ui()
        self.refresh_list()

    def _init_ui(self):
        layout = QVBoxLayout(self)

        title_label = QLabel("📁 双击文件加载历史数据")
        title_label.setStyleSheet("font-size: 10pt; font-weight: bold; padding: 5px; color: #5a4636;")
        layout.addWidget(title_label)

        self.file_list = QListWidget()
        self.file_list.setStyleSheet("""
            QListWidget { background-color: #fbf7f0; border: 1px solid #c4b49a; border-radius: 4px; font-size: 10pt; color: #2e241b; }
            QListWidget::item { padding: 6px; border-bottom: 1px solid #c4b49a; }
            QListWidget::item:selected { background-color: #b5956b; color: white; }
            QListWidget::item:hover { background-color: #e8dfd0; }
        """)
        self.file_list.itemDoubleClicked.connect(self._on_item_double_clicked)
        layout.addWidget(self.file_list)

        btn_layout = QHBoxLayout()
        btn_refresh = QPushButton("🔄 刷新列表")
        btn_refresh.setStyleSheet("QPushButton { background: #f0e9de; color: #2e241b; border: 1px solid #c4b49a; border-radius: 4px; padding: 5px 10px; }")
        btn_refresh.clicked.connect(self.refresh_list)
        btn_layout.addWidget(btn_refresh)
        btn_layout.addStretch()

        btn_close = QPushButton("关闭")
        btn_close.setStyleSheet("QPushButton { background: #d9c6a3; color: #2e241b; border: 1px solid #c4b49a; border-radius: 4px; padding: 5px 10px; }")
        btn_close.clicked.connect(self.reject)
        btn_layout.addWidget(btn_close)
        layout.addLayout(btn_layout)

        self.status_label = QLabel("")
        self.status_label.setStyleSheet("color: #997b5e; font-size: 10pt; padding: 3px;")
        layout.addWidget(self.status_label)

    def _get_data_dir(self):
        raise NotImplementedError

    def refresh_list(self):
        self.file_list.clear()
        data_dir = self._get_data_dir()
        if not os.path.exists(data_dir):
            self.status_label.setText("数据目录不存在，请先保存数据")
            return
        files = []
        for fname in os.listdir(data_dir):
            if fname.endswith('.csv'):
                fpath = os.path.join(data_dir, fname)
                mtime = os.path.getmtime(fpath)
                size = os.path.getsize(fpath)
                files.append((fname, fpath, mtime, size))
        if not files:
            self.status_label.setText("暂无历史数据文件")
            return
        files.sort(key=lambda x: x[2], reverse=True)
        for fname, fpath, mtime, size in files:
            mtime_str = datetime.fromtimestamp(mtime).strftime("%Y-%m-%d %H:%M")
            size_str = f"{size/1024:.1f} KB" if size < 1024*1024 else f"{size/(1024*1024):.2f} MB"
            item = QListWidgetItem(f"{fname}\n{mtime_str}  {size_str}")
            item.setData(Qt.UserRole, fpath)
            item.setToolTip(f"路径: {fpath}\n大小: {size_str}\n修改时间: {mtime_str}")
            self.file_list.addItem(item)
        self.status_label.setText(f"共 {len(files)} 个文件")

    def _on_item_double_clicked(self, item):
        file_path = item.data(Qt.UserRole)
        if file_path:
            self.file_selected.emit(file_path)
            self.accept()


# ===================== 具体实现：电机/传感器绘图窗口（使用 pyqtgraph） =====================
class GraphWindowUI(BasePlotWindow):
    def __init__(self, title, is_motor=True, num_devices=1, parent=None, enable_auto_save=True, is_history_mode=False):
        self.is_motor = is_motor
        self.num_devices = num_devices
        self.curves = []
        self.checkboxes = []
        super().__init__(title, is_history_mode, enable_auto_save, parent)

    def _create_plot_area(self):
        pg.setConfigOptions(antialias=True)
        self.plot_widget = pg.PlotWidget(background='#fdfaf5')
        self.plot_widget.addLegend()
        self.plot_widget.setLabel('bottom', '时间', units='秒', color='#5a4636')
        self.plot_widget.showGrid(x=True, y=True, alpha=0.2)
        self.plot_widget.setLimits(xMin=0)
        self.plot_widget.setMenuEnabled(True)
        self.plot_widget.scene().contextMenu = self._create_context_menu_wrapper
        self.main_layout.addWidget(self.plot_widget)

    def _init_checkboxes(self):
        self.chk_layout = QHBoxLayout()
        colors = ['#b5956b', '#c46b5b', '#7a8b5e', '#4a6b8a',
                  '#9467bd', '#8c564b', '#e377c2', '#7f7f7f']
        prefix = "电机" if self.is_motor else "传感器"
        self.checkboxes = []
        for i in range(self.num_devices):
            chk = QCheckBox(f"{prefix} {i + 1}")
            chk.setChecked(True)
            color = colors[i % len(colors)]
            chk.setStyleSheet(f"color: {color}; font-weight: bold;")
            self.chk_layout.addWidget(chk)
            self.checkboxes.append(chk)
            # 创建曲线（稍后在 _recreate_curves 中也能用）
        scroll = QScrollArea()
        scroll.setFixedHeight(60)
        scroll.setWidgetResizable(True)
        cw = QWidget()
        cw.setLayout(self.chk_layout)
        scroll.setWidget(cw)
        self.main_layout.addWidget(scroll)

    def _connect_click_event(self):
        self.plot_widget.scene().sigMouseClicked.connect(self._on_mouse_clicked)

    def _create_curves_for_devices(self, num_devices):
        # 创建曲线对象，存储到 self.curves
        colors = ['#b5956b', '#c46b5b', '#7a8b5e', '#4a6b8a',
                  '#9467bd', '#8c564b', '#e377c2', '#7f7f7f']
        prefix = "电机" if self.is_motor else "传感器"
        self.curves = []
        for i in range(num_devices):
            color = colors[i % len(colors)]
            name_x = f"{prefix}{i + 1}-位移" if self.is_motor else f"{prefix}{i + 1}-偏航X"
            name_y = f"{prefix}{i + 1}-速度" if self.is_motor else f"{prefix}{i + 1}-偏航Y"
            name_z = f"{prefix}{i + 1}-加速度" if self.is_motor else f"{prefix}{i + 1}-偏航Z"
            c_x = self.plot_widget.plot(name=name_x, pen=pg.mkPen(color=color, width=2, style=Qt.SolidLine))
            c_y = self.plot_widget.plot(name=name_y, pen=pg.mkPen(color=color, width=2, style=Qt.DashLine))
            c_z = self.plot_widget.plot(name=name_z, pen=pg.mkPen(color=color, width=2, style=Qt.DotLine))
            self.curves.append((c_x, c_y, c_z))

    def update_plot(self):
        # 由控制器调用，更新曲线数据
        pass   # 实际在 GraphController 中处理

    def _recreate_curves_and_checkboxes(self, num_devices):
        # 清除旧曲线
        for curve_set in self.curves:
            for curve in curve_set:
                self.plot_widget.removeItem(curve)
        # 清除旧复选框
        for chk in self.checkboxes:
            chk.setParent(None)
        self.curves.clear()
        self.checkboxes.clear()
        while self.chk_layout.count():
            item = self.chk_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

        colors = ['#b5956b', '#c46b5b', '#7a8b5e', '#4a6b8a',
                  '#9467bd', '#8c564b', '#e377c2', '#7f7f7f']
        prefix = "电机" if self.is_motor else "传感器"
        for i in range(num_devices):
            chk = QCheckBox(f"{prefix} {i + 1}")
            chk.setChecked(True)
            color = colors[i % len(colors)]
            chk.setStyleSheet(f"color: {color}; font-weight: bold;")
            self.chk_layout.addWidget(chk)
            self.checkboxes.append(chk)

            name_x = f"{prefix}{i + 1}-位移" if self.is_motor else f"{prefix}{i + 1}-偏航X"
            name_y = f"{prefix}{i + 1}-速度" if self.is_motor else f"{prefix}{i + 1}-偏航Y"
            name_z = f"{prefix}{i + 1}-加速度" if self.is_motor else f"{prefix}{i + 1}-偏航Z"
            c_x = self.plot_widget.plot(name=name_x, pen=pg.mkPen(color=color, width=2, style=Qt.SolidLine))
            c_y = self.plot_widget.plot(name=name_y, pen=pg.mkPen(color=color, width=2, style=Qt.DashLine))
            c_z = self.plot_widget.plot(name=name_z, pen=pg.mkPen(color=color, width=2, style=Qt.DotLine))
            self.curves.append((c_x, c_y, c_z))

        if self._controller:
            for chk in self.checkboxes:
                chk.stateChanged.connect(self._on_checkbox_changed)

    def _on_checkbox_changed(self):
        if self._controller:
            self._controller.update_curve_visibility()

    def _create_context_menu_wrapper(self, event):
        if self._controller:
            return self._controller.create_context_menu(event)
        return False

    def _on_mouse_clicked(self, event):
        if self._controller:
            self._controller._on_mouse_clicked(event)


# ===================== 具体实现：弯曲角度绘图窗口（使用 matplotlib） =====================
class BendGraphWindow(BasePlotWindow):
    def __init__(self, parent=None, is_history_mode=False, enable_auto_save=True):
        super().__init__("柔性臂弯曲角度曲线", is_history_mode, enable_auto_save, parent)

        # 特定数据存储
        self.target1_data = []
        self.current1_data = []
        self.target2_data = []
        self.current2_data = []

        # 连接视图控制按钮，点击时隐藏虚线
        self.btn_reset.clicked.connect(self._hide_vline)
        self.btn_focus.clicked.connect(self._hide_vline)
        self.btn_focus_x.clicked.connect(self._hide_vline)
        self.btn_focus_y.clicked.connect(self._hide_vline)

    def _create_plot_area(self):
        self.figure = Figure(figsize=(8, 4), facecolor='#fbf7f0')
        self.canvas = FigureCanvas(self.figure)
        self.ax = self.figure.add_subplot(111)
        self.ax.set_facecolor('#fdfaf5')
        for spine in self.ax.spines.values():
            spine.set_color('#c4b49a')
        self.ax.tick_params(colors='#5a4636')
        self.ax.set_xlabel('时间 (秒)', color='#5a4636')
        self.ax.set_ylabel('角度 (度)', color='#5a4636')
        self.ax.set_title('弯曲角度实时曲线', color='#5a4636')
        self.main_layout.addWidget(self.canvas)

        # 创建曲线对象
        self.line_target1, = self.ax.plot([], [], color='#b5956b', linewidth=2, label='第一段目标')
        self.line_current1, = self.ax.plot([], [], color='#c46b5b', linewidth=2, label='第一段当前')
        self.line_target2, = self.ax.plot([], [], color='#7a8b5e', linewidth=2, label='第二段目标')
        self.line_current2, = self.ax.plot([], [], color='#4a6b8a', linewidth=2, label='第二段当前')
        self.ax.legend(loc='upper right', framealpha=0.3, edgecolor='#c4b49a')
        # 添加红色垂直虚线
        self.vline = self.ax.axvline(x=0, color='red', linestyle='--', linewidth=1, alpha=0.8)
        self.vline.set_visible(False)

    def _init_checkboxes(self):
        self.chk_layout = QHBoxLayout()
        colors = {'第一段目标': '#b5956b', '第一段当前': '#c46b5b',
                  '第二段目标': '#7a8b5e', '第二段当前': '#4a6b8a'}
        self.checkboxes = []
        for name, color in colors.items():
            chk = QCheckBox(name)
            chk.setChecked(True)
            chk.setStyleSheet(f"color: {color}; font-weight: bold;")
            chk.stateChanged.connect(self._on_checkbox_changed)
            self.chk_layout.addWidget(chk)
            self.checkboxes.append(chk)
        scroll = QScrollArea()
        scroll.setFixedHeight(60)
        scroll.setWidgetResizable(True)
        cw = QWidget()
        cw.setLayout(self.chk_layout)
        scroll.setWidget(cw)
        self.main_layout.addWidget(scroll)

    def _connect_click_event(self):
        self.canvas.mpl_connect('button_press_event', self._on_click)

    def update_plot(self):
        # 原有代码（设置可见性、更新数据、刷新画布）
        self.line_target1.set_visible(self.checkboxes[0].isChecked())
        self.line_current1.set_visible(self.checkboxes[1].isChecked())
        self.line_target2.set_visible(self.checkboxes[2].isChecked())
        self.line_current2.set_visible(self.checkboxes[3].isChecked())

        self.line_target1.set_data(self.time_data, self.target1_data)
        self.line_current1.set_data(self.time_data, self.current1_data)
        self.line_target2.set_data(self.time_data, self.target2_data)
        self.line_current2.set_data(self.time_data, self.current2_data)

        self.ax.relim()
        self.ax.autoscale_view()
        self.canvas.draw_idle()

    def update_bend_data(self, time, target1, current1, target2, current2):
        self.time_data = time
        self.target1_data = target1
        self.current1_data = current1
        self.target2_data = target2
        self.current2_data = current2
        self.update_plot()

    def _on_checkbox_changed(self):
        self.update_plot()

    def _on_click(self, event):
        if event.inaxes != self.ax:
            # 点击坐标轴外部时隐藏虚线
            self.vline.set_visible(False)
            self.canvas.draw_idle()
            return

        if not self.time_data:
            self.coord_label.setText("无数据")
            self.vline.set_visible(False)
            self.canvas.draw_idle()
            return

        xdata = event.xdata
        # 查找最近的时间点
        idx = min(range(len(self.time_data)), key=lambda i: abs(self.time_data[i] - xdata))
        nearest_time = self.time_data[idx]

        # 更新虚线位置
        self.vline.set_xdata([nearest_time, nearest_time])
        self.vline.set_visible(True)

        # 显示坐标信息
        lines = [self.line_target1, self.line_current1, self.line_target2, self.line_current2]
        names = ["第一段目标", "第一段当前", "第二段目标", "第二段当前"]
        best_dist = float('inf')
        best_info = None
        for line, name in zip(lines, names):
            if not line.get_visible():
                continue
            x_line = line.get_xdata()
            y_line = line.get_ydata()
            if len(x_line) == 0:
                continue
            # 直接使用前面找到的 idx，因为时间轴相同
            if idx < len(y_line):
                y_val = y_line[idx]
                dist = abs(x_line[idx] - nearest_time) + abs(y_val - event.ydata)
                if dist < best_dist:
                    best_dist = dist
                    best_info = (name, nearest_time, y_val)
        if best_info:
            name, x, y = best_info
            self.coord_label.setText(f"曲线: {name}  |  时间: {x:.3f} 秒  |  角度: {y:.3f}°")
        else:
            self.coord_label.setText(f"时间: {nearest_time:.3f} 秒 (无对应曲线数据)")

        self.canvas.draw_idle()

    def _hide_vline(self):
        if hasattr(self, 'vline') and self.vline is not None:
            self.vline.set_visible(False)
            self.canvas.draw_idle()

    def _on_load_history(self):
        dialog = BendHistoryFileDialog(self)
        def on_file_selected(file_path):
            if self._controller and hasattr(self._controller, 'load_history_file'):
                self._controller.load_history_file(file_path)
            dialog.accept()
        dialog.file_selected.connect(on_file_selected)
        dialog.exec_()


# ===================== 具体实现：通用电机/传感器历史文件对话框 =====================
class HistoryFileDialog(BaseHistoryDialog):
    def __init__(self, is_motor=True, parent=None):
        self.is_motor = is_motor
        title = "历史数据文件 - " + ("电机" if is_motor else "传感器")
        super().__init__(title, parent)

    def _get_data_dir(self):
        base = os.path.expanduser("~/.lqts/analyze_data")
        sub = "motor_data" if self.is_motor else "sensor_data"
        return os.path.join(base, sub)


# ===================== 具体实现：弯曲角度历史文件对话框 =====================
class BendHistoryFileDialog(BaseHistoryDialog):
    def __init__(self, parent=None):
        super().__init__("历史弯曲数据 - 选择文件", parent)

    def _get_data_dir(self):
        return os.path.expanduser("~/.lqts/analyze_data/bend_data")