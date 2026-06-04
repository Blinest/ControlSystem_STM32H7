# Qt类
from PyQt5.QtCore import QObject, QTimer, Qt
from PyQt5.QtWidgets import QMenu, QMessageBox, QFileDialog
from PyQt5.QtGui import QCursor

# 自定义类
from UI.graph_window import HistoryFileDialog


# 工具类
import os
import csv
import bisect
import unicodedata
import pyqtgraph as pg
import numpy as np
from datetime import datetime

class GraphController(QObject):
    def __init__(self, ui, is_history_mode=False):
        super().__init__()
        self.ui = ui
        self._time_data = []
        self._data_matrix = []
        self.is_history_mode = is_history_mode
        self.main_window = None   # 由外部设置
        # 绘制竖线
        self.vline = pg.InfiniteLine(angle=90, movable=False, pen=pg.mkPen(color='#FF0000', width=1, style=Qt.DashLine))
        self.ui.plot_widget.addItem(self.vline)
        self.vline.hide()

        self.ui.plot_widget.scene().sigMouseClicked.connect(self._on_mouse_clicked)
        if not is_history_mode:
            # 实时模式：启用自动保存
            self.auto_save_timer = QTimer()
            self.auto_save_timer.timeout.connect(self._auto_save)
            self.ui.chk_auto_save.toggled.connect(self._on_auto_save_toggled)
            self.ui.spin_interval.valueChanged.connect(self._on_interval_changed)
            self._on_auto_save_toggled(self.ui.chk_auto_save.isChecked())
        else:
            # 历史模式：隐藏自动保存控件
            if hasattr(self.ui, 'chk_auto_save') and self.ui.chk_auto_save is not None:
                self.ui.chk_auto_save.hide()
            if hasattr(self.ui, 'spin_interval') and self.ui.spin_interval is not None:
                self.ui.spin_interval.hide()

    # ---------- 自动保存 ----------
    def _on_auto_save_toggled(self, checked):
        if checked:
            self.auto_save_timer.start(int(self.ui.spin_interval.value() * 1000))
        else:
            self.auto_save_timer.stop()

    def _on_interval_changed(self, value):
        if self.ui.chk_auto_save.isChecked():
            self.auto_save_timer.stop()
            self.auto_save_timer.start(int(value * 1000))

    def _auto_save(self):
        if not self._time_data or not self._data_matrix:
            return
        try:
            filename = self._get_auto_filename()
            self._save_to_csv(filename)
            print(f"[自动保存] 成功写入 {filename}")
        except Exception as e:
            print(f"[自动保存] 失败: {e}")

    def _get_auto_filename(self):
        data_dir = self._ensure_data_dir()
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        prefix = "motor" if self.ui.is_motor else "sensor"
        return os.path.join(data_dir, f"{prefix}_auto_{timestamp}.csv")
    def _get_data_dir(self):
        """返回当前类型对应的数据子目录"""
        base = os.path.expanduser("~/.lqts/analyze_data")
        sub = "motor_data" if self.ui.is_motor else "sensor_data"
        return os.path.join(base, sub)

    def _ensure_data_dir(self):
        """确保数据目录存在"""
        data_dir = self._get_data_dir()
        os.makedirs(data_dir, exist_ok=True)
        return data_dir

    # ---------- 数据保存 ----------
    def save_data(self):
        data_dir = self._ensure_data_dir()
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        prefix = "motor" if self.ui.is_motor else "sensor"
        default_path = os.path.join(data_dir, f"{prefix}_data_{timestamp}.csv")

        filename, _ = QFileDialog.getSaveFileName(
            self.ui, "保存曲线数据", default_path, "CSV Files (*.csv);;All Files (*)"
        )
        if not filename:
            return
        try:
            self._save_to_csv(filename)
            QMessageBox.information(self.ui, "保存成功", f"数据已保存到:\n{filename}")
        except Exception as e:
            QMessageBox.critical(self.ui, "保存失败", str(e))

    def _save_to_csv(self, filepath):
        """统一保存逻辑：从当前曲线或缓存提取数据写入CSV"""
        # 优先使用缓存的时间和数据矩阵
        if self._time_data and self._data_matrix:
            time_data = self._time_data
            data_matrix = self._data_matrix
        else:
            # 若缓存为空，则从曲线对象中提取
            if self.ui.curves and self.ui.curves[0][0].xData is not None:
                time_data = self.ui.curves[0][0].xData
                data_matrix = []
                for t_idx in range(len(time_data)):
                    step = []
                    for curve_set in self.ui.curves:
                        vals = []
                        for curve in curve_set:
                            y = curve.yData
                            vals.append(y[t_idx] if y is not None and t_idx < len(y) else 0.0)
                        step.append(vals)
                    data_matrix.append(step)
            else:
                time_data, data_matrix = [], []

        if not time_data:
            raise ValueError("无数据可保存")

        num_devices = len(self.ui.curves)
        prefix_name = "电机" if self.ui.is_motor else "传感器"
        headers = ["时间"]
        for i in range(num_devices):
            if self.ui.is_motor:
                headers.extend([f"{prefix_name}{i+1}_位移", f"{prefix_name}{i+1}_速度", f"{prefix_name}{i+1}_加速度"])
            else:
                headers.extend([f"{prefix_name}{i+1}_偏航X", f"{prefix_name}{i+1}_偏航Y", f"{prefix_name}{i+1}_偏航Z"])

        rows = []
        for t_idx in range(len(time_data)):
            row = [round(time_data[t_idx], 2)]
            for dev_idx in range(num_devices):
                if t_idx < len(data_matrix) and dev_idx < len(data_matrix[t_idx]) and len(data_matrix[t_idx][dev_idx]) >= 3:
                    row.extend(round(v, 4) for v in data_matrix[t_idx][dev_idx][:3])
                else:
                    row.extend([0.0, 0.0, 0.0])
            rows.append(row)

        with open(filepath, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow(headers)
            writer.writerows(rows)

    # ---------- 视图控制 ----------
    def get_curve_data_bounds(self):
        x_vals, y_vals = [], []
        for curve_set in self.ui.curves:
            for curve in curve_set:
                if curve.isVisible():
                    data = curve.getData()
                    if data and data[0] is not None and len(data[0]) > 0:
                        x_vals.extend([min(data[0]), max(data[0])])
                        if data[1] is not None and len(data[1]) > 0:
                            y_vals.extend([min(data[1]), max(data[1])])
        if x_vals and y_vals:
            return {'x_min': min(x_vals), 'x_max': max(x_vals),
                    'y_min': min(y_vals), 'y_max': max(y_vals)}
        return None

    def auto_focus(self):
        bounds = self.get_curve_data_bounds()
        if bounds:
            self._apply_range(bounds['x_min'], bounds['x_max'], bounds['y_min'], bounds['y_max'])

    def auto_focus_axis(self, axis='both'):
        bounds = self.get_curve_data_bounds()
        if not bounds:
            return
        if axis in ('x', 'both'):
            self._apply_x_range(bounds['x_min'], bounds['x_max'])
        if axis in ('y', 'both'):
            self._apply_y_range(bounds['y_min'], bounds['y_max'])

    def reset_view(self):
        bounds = self.get_curve_data_bounds()
        if bounds:
            x_range = bounds['x_max'] - bounds['x_min'] or 100
            y_range = bounds['y_max'] - bounds['y_min'] or 20
            x_min = max(0, bounds['x_min'] - x_range * 0.1)
            x_max = bounds['x_max'] + x_range * 0.1
            y_min = bounds['y_min'] - y_range * 0.2
            y_max = bounds['y_max'] + y_range * 0.2
        else:
            x_min, x_max, y_min, y_max = 0, 100, -10, 10
        self.ui.plot_widget.setXRange(x_min, x_max, padding=0)
        self.ui.plot_widget.setYRange(y_min, y_max, padding=0)
        self.vline.hide()   # 隐藏虚线，因为视图已变化

    def _apply_range(self, x_min, x_max, y_min, y_max):
        self._apply_x_range(x_min, x_max)
        self._apply_y_range(y_min, y_max)

    def _apply_x_range(self, x_min, x_max):
        margin = (x_max - x_min) * 0.1 if x_max != x_min else 0.5
        x_min -= margin
        x_max += margin
        if x_max - x_min < 1:
            center = (x_min + x_max) / 2
            x_min, x_max = center - 0.5, center + 0.5
        self.ui.plot_widget.setXRange(x_min, x_max, padding=0)

    def _apply_y_range(self, y_min, y_max):
        margin = (y_max - y_min) * 0.1 if y_max != y_min else 0.5
        y_min -= margin
        y_max += margin
        if y_max - y_min < 1:
            center = (y_min + y_max) / 2
            y_min, y_max = center - 0.5, center + 0.5
        self.ui.plot_widget.setYRange(y_min, y_max, padding=0)

    def update_curve_visibility(self):
        for i, chk in enumerate(self.ui.checkboxes):
            vis = chk.isChecked()
            for curve in self.ui.curves[i]:
                curve.setVisible(vis)


    # ---------- 数据更新 ----------
    def update_multi_data(self, time_data, data_matrix):
        if not time_data or not data_matrix:
            return
        self._time_data = time_data
        self._data_matrix = data_matrix

        num_devices = len(data_matrix[0])
        if num_devices != len(self.ui.curves):
            self.ui._recreate_curves_and_checkboxes(num_devices)

        for i in range(min(len(self.ui.curves), num_devices)):
            x_vals = [step[i][0] if i < len(step) and len(step[i]) > 0 else 0.0 for step in data_matrix]
            y_vals = [step[i][1] if i < len(step) and len(step[i]) > 1 else 0.0 for step in data_matrix]
            z_vals = [step[i][2] if i < len(step) and len(step[i]) > 2 else 0.0 for step in data_matrix]
            if len(time_data) == len(x_vals):
                self.ui.curves[i][0].setData(time_data, x_vals)
                self.ui.curves[i][1].setData(time_data, y_vals)
                self.ui.curves[i][2].setData(time_data, z_vals)

    # ---------- 历史文件加载 ----------
    def open_history_dialog(self):
        dialog = HistoryFileDialog(self.ui.is_motor, parent=self.ui)
        if self.main_window:
            dialog.file_selected.connect(lambda path: self.main_window.open_history_graph(path, self.ui.is_motor))
        else:
            dialog.file_selected.connect(self._load_csv_file)  # 降级为在当前窗口加载
        dialog.exec_()

    def load_data_from_csv(self):
        data_dir = self._ensure_data_dir()
        file_path, _ = QFileDialog.getOpenFileName(
            self.ui, "选择历史数据文件", data_dir, "CSV Files (*.csv);;All Files (*)"
        )
        if file_path:
            self._load_csv_file(file_path)

    def _load_csv_file(self, file_path):
        try:
            with open(file_path, 'r', encoding='utf-8-sig') as f:
                reader = csv.reader(f)
                raw_headers = next(reader, [])
                headers = [self._clean_column_name(str(h)) for h in raw_headers if h and str(h).strip()]
                if not headers:
                    raise ValueError("CSV 文件缺少表头")

                rows = list(reader)
                if not rows:
                    raise ValueError("CSV 文件中没有数据行")

                col_groups = self._find_column_groups(headers)
                if not col_groups:
                    target_keys = ('位移', '速度', '加速度') if self.ui.is_motor else ('偏航X', '偏航Y', '偏航Z')
                    preview = ', '.join(headers[:min(8, len(headers))])
                    matched_count = len([h for h in headers if any(k in h for k in target_keys)])
                    raise ValueError(
                        f"未识别到所需数据列。\n"
                        f"窗口类型: {'电机' if self.ui.is_motor else '传感器'}\n"
                        f"表头前几项: [{preview}]\n"
                        f"匹配到的列数: {matched_count} (应为3的倍数)\n"
                        f"请确保列名包含关键词: {'/'.join(target_keys)}"
                    )

                num_devices = len(col_groups)
                print(f"[加载] 识别到 {num_devices} 个设备")

                if num_devices != len(self.ui.curves):
                    print(f"[加载] 设备数量变化，重建曲线...")
                    self.ui._recreate_curves_and_checkboxes(num_devices)
                    if self.ui._controller:
                        for chk in self.ui.checkboxes:
                            chk.stateChanged.connect(self.ui._on_checkbox_changed)

                time_data = []
                data_matrix = []
                for row in rows:
                    try:
                        t = float(row[0])
                        time_data.append(t)
                        step = []
                        for cols in col_groups:
                            vals = [float(row[c]) if c < len(row) and row[c].strip() else 0.0 for c in cols]
                            step.append(vals)
                        data_matrix.append(step)
                    except (ValueError, IndexError):
                        continue

                if not time_data:
                    raise ValueError("未提取到有效的时间数据")

                print(f"[加载] 成功提取 {len(time_data)} 个时间点")
                self.update_multi_data(time_data, data_matrix)
                self.auto_focus()
                self.ui.plot_widget.replot()
                print("[加载] 数据已应用到图表")

        except Exception as e:
            QMessageBox.critical(self.ui, "加载失败", str(e))

    def _clean_column_name(self, name):
        name = unicodedata.normalize('NFKC', name)
        name = ''.join(ch for ch in name if not unicodedata.category(ch).startswith('C') or ch in ('\t', '\n', '\r'))
        name = name.replace('\u3000', ' ')
        return name.strip()
    def _find_column_groups(self, headers):
        if self.ui.is_motor:
            target_keys = ('位移', '速度', '加速度')
        else:
            target_keys = ('偏航X', '偏航Y', '偏航Z')

        matched_indices = []
        for idx, name in enumerate(headers):
            if idx == 0:
                continue
            if any(key in name for key in target_keys):
                matched_indices.append(idx)

        if len(matched_indices) > 0 and len(matched_indices) % 3 == 0:
            groups = []
            for i in range(0, len(matched_indices), 3):
                groups.append(tuple(matched_indices[i:i+3]))
            return groups
        return []

    # ---------- 右键菜单 ----------
    def create_context_menu(self, event):
        menu = QMenu()
        focus_menu = menu.addMenu("🎯 聚焦选项")
        focus_menu.addAction("一键聚焦", self.auto_focus)
        focus_menu.addAction("X轴聚焦", lambda: self.auto_focus_axis('x'))
        focus_menu.addAction("Y轴聚焦", lambda: self.auto_focus_axis('y'))
        focus_menu.addSeparator()
        focus_menu.addAction("重置视图", self.reset_view)
        menu.addSeparator()
        save_menu = menu.addMenu("💾 数据保存")
        save_menu.addAction("保存当前数据", self.save_data)
        menu.exec_(event.screenPos())
        return True

    # ---------- 数据显示 ----------

    def _on_mouse_clicked(self, event):
        if event.button() != Qt.LeftButton:
            return

        pos = event.scenePos()
        vb = self.ui.plot_widget.plotItem.vb
        if not vb.sceneBoundingRect().contains(pos):
            return

        mouse_point = vb.mapSceneToView(pos)
        click_x = mouse_point.x()

        # 寻找有效 X 数据（使用第一条可见曲线的 X 轴）
        x_data = None
        for dev_idx, curve_set in enumerate(self.ui.curves):
            if dev_idx < len(self.ui.checkboxes) and not self.ui.checkboxes[dev_idx].isChecked():
                continue
            if curve_set[0].xData is not None and len(curve_set[0].xData) > 0:
                x_data = curve_set[0].xData
                break

        if x_data is None:
            self.ui.coord_label.setText("点击位置无数据")
            self.vline.hide()
            return

        idx = self._find_nearest_index(x_data, click_x)
        if idx is None:
            self.ui.coord_label.setText("无法定位数据点")
            self.vline.hide()
            return

        actual_x = x_data[idx]

        # 显示垂直虚线
        self.vline.setPos(actual_x)
        self.vline.show()

        prefix = "电机" if self.ui.is_motor else "传感器"

        # 收集位移/偏航X
        disp_items = []
        vel_items = []
        acc_items = []
        for dev_idx, curve_set in enumerate(self.ui.curves):
            if dev_idx < len(self.ui.checkboxes) and not self.ui.checkboxes[dev_idx].isChecked():
                continue
            # 获取三条曲线数据
            y1_data = curve_set[0].yData
            y2_data = curve_set[1].yData
            y3_data = curve_set[2].yData

            v1 = y1_data[idx] if y1_data is not None and idx < len(y1_data) else 0.0
            v2 = y2_data[idx] if y2_data is not None and idx < len(y2_data) else 0.0
            v3 = y3_data[idx] if y3_data is not None and idx < len(y3_data) else 0.0

            if self.ui.is_motor:
                disp_items.append(f"{prefix}{dev_idx+1}: {v1:.3f}")
                vel_items.append(f"{prefix}{dev_idx+1}: {v2:.3f}")
                acc_items.append(f"{prefix}{dev_idx+1}: {v3:.3f}")
            else:
                # 传感器模式同理，可自定义标签
                disp_items.append(f"{prefix}{dev_idx+1}: {v1:.3f}")
                vel_items.append(f"{prefix}{dev_idx+1}: {v2:.3f}")
                acc_items.append(f"{prefix}{dev_idx+1}: {v3:.3f}")

        lines = [f"X = {actual_x:.3f}"]
        if disp_items:
            if self.ui.is_motor:
                lines.append("位移: " + ", ".join(disp_items))
                lines.append("速度: " + ", ".join(vel_items))
                lines.append("加速度: " + ", ".join(acc_items))
            else:
                lines.append("横滚: " + ", ".join(disp_items))
                lines.append("俯仰: " + ", ".join(vel_items))
                lines.append("偏航: " + ", ".join(acc_items))
        else:
            lines.append("(无可见设备)")

        self.ui.coord_label.setText("\n".join(lines))

    def _find_nearest_index(self, x_data, target_x):
        """二分查找最接近目标值的索引"""
        import bisect
        if x_data is None or len(x_data) == 0:
            return None
        idx = bisect.bisect_left(x_data, target_x)
        if idx == 0:
            return 0
        if idx == len(x_data):
            return len(x_data) - 1
        before = x_data[idx - 1]
        after = x_data[idx]
        return idx if (after - target_x) < (target_x - before) else idx - 1

class BendGraphController(QObject):
    def __init__(self, window, data_provider):
        super().__init__()
        self.window = window              # BendGraphWindow 实例
        self.data_provider = data_provider # DeviceTab 实例（或 None）
        self.window.set_controller(self)

        # 用于点击显示的竖线（matplotlib 方式）
        self.vline = self.window.ax.axvline(x=0, color='#FF0000', linestyle='--', linewidth=1, visible=False)
        self._click_cid = self.window.canvas.mpl_connect('button_press_event', self._on_mouse_clicked)
        self._context_cid = self.window.canvas.mpl_connect('button_press_event', self._on_right_click)

        # 自动保存相关（需要窗口提供 chk_auto_save 和 spin_interval）
        self.auto_save_timer = QTimer()
        self.auto_save_timer.timeout.connect(self._auto_save)
        if hasattr(self.window, 'chk_auto_save') and self.window.chk_auto_save is not None:
            self.window.chk_auto_save.toggled.connect(self._on_auto_save_toggled)
        if hasattr(self.window, 'spin_interval') and self.window.spin_interval is not None:
            self.window.spin_interval.valueChanged.connect(self._on_interval_changed)
        # 初始状态根据控件设置
        if hasattr(self.window, 'chk_auto_save') and self.window.chk_auto_save.isChecked():
            self._start_auto_save_timer()

    # ---------- 自动保存 ----------
    def _on_auto_save_toggled(self, checked):
        if checked:
            self._start_auto_save_timer()
        else:
            self.auto_save_timer.stop()

    def _on_interval_changed(self, value):
        if hasattr(self.window, 'chk_auto_save') and self.window.chk_auto_save.isChecked():
            self.auto_save_timer.stop()
            self.auto_save_timer.start(int(value * 1000))

    def _start_auto_save_timer(self):
        interval = int(self.window.spin_interval.value() * 1000) if hasattr(self.window, 'spin_interval') else 5000
        self.auto_save_timer.start(interval)

    def _auto_save(self):
        if not self.window.time_data:
            return
        try:
            filename = self._get_auto_filename()
            self._save_to_csv(filename)
            print(f"[自动保存] 成功写入 {filename}")
        except Exception as e:
            print(f"[自动保存] 失败: {e}")

    def _get_auto_filename(self):
        data_dir = self._ensure_data_dir()
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        return os.path.join(data_dir, f"bend_auto_{timestamp}.csv")

    def _get_data_dir(self):
        base = os.path.expanduser("~/.lqts/analyze_data")
        return os.path.join(base, "bend_data")

    def _ensure_data_dir(self):
        data_dir = self._get_data_dir()
        os.makedirs(data_dir, exist_ok=True)
        return data_dir

    # ---------- 数据保存 ----------
    def save_data(self, use_dialog=True):
        """保存数据，可选择弹出文件对话框"""
        if not self.window.time_data:
            QMessageBox.warning(self.window, "无数据", "没有数据可保存")
            return

        if use_dialog:
            data_dir = self._ensure_data_dir()
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            default_path = os.path.join(data_dir, f"bend_angle_{timestamp}.csv")
            filename, _ = QFileDialog.getSaveFileName(
                self.window, "保存弯曲角度数据", default_path,
                "CSV Files (*.csv);;All Files (*)"
            )
            if not filename:
                return
        else:
            filename = self._get_auto_filename()

        try:
            self._save_to_csv(filename)
            QMessageBox.information(self.window, "保存成功", f"数据已保存至:\n{filename}")
        except Exception as e:
            QMessageBox.critical(self.window, "保存失败", str(e))

    def _save_to_csv(self, filepath):
        """写入 CSV 文件"""
        with open(filepath, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow(["时间(秒)", "第一段目标(deg)", "第一段当前(deg)",
                             "第二段目标(deg)", "第二段当前(deg)"])
            for t, t1, c1, t2, c2 in zip(self.window.time_data,
                                         self.window.target1_data,
                                         self.window.current1_data,
                                         self.window.target2_data,
                                         self.window.current2_data):
                writer.writerow([f"{t:.3f}", f"{t1:.3f}", f"{c1:.3f}", f"{t2:.3f}", f"{c2:.3f}"])

    # ---------- 视图控制 ----------
    def auto_focus(self):
        if self.window.time_data:
            self.window.ax.relim()
            self.window.ax.autoscale_view()
            self.window.canvas.draw_idle()

    def auto_focus_axis(self, axis='both'):
        if axis in ('x', 'both') and self.window.time_data:
            self.window.ax.set_xlim(min(self.window.time_data), max(self.window.time_data))
        if axis in ('y', 'both'):
            all_vals = (self.window.target1_data + self.window.current1_data +
                        self.window.target2_data + self.window.current2_data)
            if all_vals:
                ymin, ymax = min(all_vals), max(all_vals)
                if ymin == ymax:
                    ymin -= 10
                    ymax += 10
                self.window.ax.set_ylim(ymin, ymax)
        self.window.canvas.draw_idle()

    def reset_view(self):
        self.window.ax.set_xlim(auto=True)
        self.window.ax.set_ylim(auto=True)
        self.window.ax.relim()
        self.window.ax.autoscale_view()
        self.window.canvas.draw_idle()

    # ---------- 历史数据加载 ----------
    def load_history_file(self, file_path=None):
        """加载 CSV 文件（若未提供路径则弹出对话框）"""
        if file_path is None:
            data_dir = self._ensure_data_dir()
            file_path, _ = QFileDialog.getOpenFileName(
                self.window, "选择历史数据文件", data_dir,
                "CSV Files (*.csv);;All Files (*)"
            )
            if not file_path:
                return

        try:
            time_vals, t1_vals, c1_vals, t2_vals, c2_vals = [], [], [], [], []
            with open(file_path, 'r', encoding='utf-8-sig') as f:
                reader = csv.reader(f)
                next(reader)  # 跳过表头
                for row in reader:
                    if len(row) >= 5:
                        time_vals.append(float(row[0]))
                        t1_vals.append(float(row[1]))
                        c1_vals.append(float(row[2]))
                        t2_vals.append(float(row[3]))
                        c2_vals.append(float(row[4]))
            if not time_vals:
                QMessageBox.warning(self.window, "无数据", "文件中没有有效数据行")
                return

            self.window.time_data = time_vals
            self.window.target1_data = t1_vals
            self.window.current1_data = c1_vals
            self.window.target2_data = t2_vals
            self.window.current2_data = c2_vals
            self.window.update_plot()
            self.auto_focus()
            QMessageBox.information(self.window, "加载成功", f"已加载 {len(time_vals)} 个数据点")
        except Exception as e:
            QMessageBox.critical(self.window, "加载失败", f"错误详情：{str(e)}")

    # ---------- 鼠标交互 ----------
    def _on_mouse_clicked(self, event):
        """左键点击显示竖线和数据坐标"""
        if event.button != 1:  # 左键
            return
        if event.inaxes != self.window.ax:
            return
        click_x = event.xdata
        if click_x is None or not self.window.time_data:
            return

        # 寻找最近的时间点索引
        idx = self._find_nearest_index(self.window.time_data, click_x)
        if idx is None:
            return

        actual_x = self.window.time_data[idx]
        self.vline.set_xdata([actual_x])
        self.vline.set_visible(True)

        # 获取该索引处的各曲线值
        t1 = self.window.target1_data[idx] if idx < len(self.window.target1_data) else 0
        c1 = self.window.current1_data[idx] if idx < len(self.window.current1_data) else 0
        t2 = self.window.target2_data[idx] if idx < len(self.window.target2_data) else 0
        c2 = self.window.current2_data[idx] if idx < len(self.window.current2_data) else 0

        info_lines = [
            f"X = {actual_x:.3f}",
            f"第一段目标: {t1:.3f} deg",
            f"第一段当前: {c1:.3f} deg",
            f"第二段目标: {t2:.3f} deg",
            f"第二段当前: {c2:.3f} deg"
        ]

        # 如果窗口有 coord_label 控件则更新，否则在标题栏显示
        if hasattr(self.window, 'coord_label') and self.window.coord_label is not None:
            self.window.coord_label.setText("\n".join(info_lines))
        else:
            self.window.setWindowTitle(" | ".join(info_lines))

        self.window.canvas.draw_idle()

    def _on_right_click(self, event):
        """右键菜单"""
        if event.button != 3:  # 右键
            return
        # 不要求点击在axes内，菜单始终弹出
        menu = QMenu()
        focus_menu = menu.addMenu("聚焦选项")
        focus_menu.addAction("一键聚焦", self.auto_focus)
        focus_menu.addAction("X轴聚焦", lambda: self.auto_focus_axis('x'))
        focus_menu.addAction("Y轴聚焦", lambda: self.auto_focus_axis('y'))
        focus_menu.addSeparator()
        focus_menu.addAction("重置视图", self.reset_view)
        menu.addSeparator()
        save_menu = menu.addMenu("数据保存")
        save_menu.addAction("保存当前数据", lambda: self.save_data(True))
        save_menu.addAction("加载历史数据", lambda: self.load_history_file())
        menu.exec_(QCursor.pos())

    def _find_nearest_index(self, x_data, target_x):
        """二分查找最接近目标值的索引"""
        if not x_data:
            return None
        arr = np.asarray(x_data)
        idx = np.searchsorted(arr, target_x)
        if idx == 0:
            return 0
        if idx == len(arr):
            return len(arr) - 1
        return idx if (arr[idx] - target_x) < (target_x - arr[idx - 1]) else idx - 1