# ==========================================
# 7. 日志管理窗口
# ==========================================
from PyQt5.QtWidgets import (QApplication, QDialog, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
                             QTabWidget, QWidget, QTextEdit, QTableWidget, QTableWidgetItem,
                             QHeaderView, QStatusBar, QFileDialog, QMessageBox, QListWidget,
                             QComboBox, QLineEdit, QListWidgetItem)
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QTextCursor, QColor

# 自定义类
from Core.logger import default_log_manager as log_manager
from Core.auth import GlobalHistory
# 工具类
import os
from datetime import datetime

class LogManagerWindow(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("日志管理")
        # 根据分辨率自动动态设置窗口大小
        screen = QApplication.primaryScreen().availableGeometry()
        self.resize(int(screen.width() * 0.6), int(screen.height() * 0.7))
        self.setMinimumSize(640, 480)   # 防止缩得过小导致控件挤爆
        self.showNormal()               # 确保不是最大化状态
        self.current_log_file = None  # 当前查看的日志文件
        self._initialize_ui()
        self._refresh_log_list()  # 刷新历史日志列表
        self._refresh_log_display()
        self._refresh_history_display()

        # 存储搜索结果
        self.search_results = []
        self.current_search_index = -1

    def _initialize_ui(self):
        layout = QVBoxLayout(self)

        title = QLabel("📋 系统日志与操作记录")
        title.setStyleSheet("font-size: 28px; font-weight: bold; margin-bottom: 20px;")
        title.setAlignment(Qt.AlignCenter)
        layout.addWidget(title)

        self.tab_widget = QTabWidget()
        self._create_log_tab()
        self.tab_widget.addTab(self._log_tab, "📝 系统日志")
        self._create_history_tab()
        self.tab_widget.addTab(self._history_tab, "📖 操作记录")
        layout.addWidget(self.tab_widget)

        self.status_bar = QStatusBar()
        layout.addWidget(self.status_bar)

    def _create_log_tab(self):
        self._log_tab = QWidget()
        main_layout = QHBoxLayout(self._log_tab)

        # 左侧面板 - 历史日志列表
        left_panel = QWidget()
        left_panel.setFixedWidth(280)
        left_layout = QVBoxLayout(left_panel)
        left_layout.setContentsMargins(0, 0, 10, 0)

        # 历史日志标题
        history_title = QLabel("📁 历史日志文件")
        history_title.setStyleSheet("font-size: 20px; font-weight: bold; padding: 5px;")
        left_layout.addWidget(history_title)

        # 刷新按钮
        btn_refresh_list = QPushButton("🔄 刷新列表")
        btn_refresh_list.setStyleSheet("""
            QPushButton {
                background-color: #0078D7;
                color: white;
                padding: 6px;
                border-radius: 4px;
                font-size: 20px;
            }
            QPushButton:hover {
                background-color: #0056b3;
            }
        """)
        btn_refresh_list.clicked.connect(self._refresh_log_list)
        left_layout.addWidget(btn_refresh_list)

        # 日志文件列表
        self.log_list_widget = QListWidget()
        self.log_list_widget.setStyleSheet("""
            QListWidget {
                background-color: #F5F5F5;
                border: 2px solid #DDD;
                border-radius: 4px;
                font-size: 20px;
            }
            QListWidget::item {
                padding: 8px;
                border-bottom: 1px solid #DDD;
            }
            QListWidget::item:selected {
                background-color: #0078D7;
                color: white;
            }
            QListWidget::item:hover {
                background-color: #E0E0E0;
            }
        """)
        self.log_list_widget.itemClicked.connect(self._on_log_file_selected)
        left_layout.addWidget(self.log_list_widget)

        # 文件信息
        self.lbl_file_info = QLabel("")
        self.lbl_file_info.setStyleSheet("font-size: 20px; color: #666; padding: 5px;")
        self.lbl_file_info.setWordWrap(True)
        left_layout.addWidget(self.lbl_file_info)

        # 删除历史日志按钮
        btn_delete_log = QPushButton("🗑️ 删除选中日志")
        btn_delete_log.setStyleSheet("""
            QPushButton {
                background-color: #D13438;
                color: white;
                padding: 6px;
                border-radius: 4px;
                font-size: 20px;
            }
            QPushButton:hover {
                background-color: #A80000;
            }
        """)
        btn_delete_log.clicked.connect(self._delete_selected_log)
        left_layout.addWidget(btn_delete_log)

        # 打开日志文件夹按钮
        btn_open_folder = QPushButton("📂 打开日志文件夹")
        btn_open_folder.setStyleSheet("""
            QPushButton {
                background-color: #666;
                color: white;
                padding: 6px;
                border-radius: 4px;
                font-size: 20px;
            }
            QPushButton:hover {
                background-color: #888;
            }
        """)
        btn_open_folder.clicked.connect(self._open_log_folder)
        left_layout.addWidget(btn_open_folder)

        left_layout.addStretch()

        # 右侧面板 - 日志内容
        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        right_layout.setContentsMargins(0, 0, 0, 0)

        # 当前文件信息行
        info_layout = QHBoxLayout()
        info_layout.addWidget(QLabel("当前查看:"))
        self.lbl_current_file = QLabel("今日日志")
        self.lbl_current_file.setStyleSheet("color: #0078D7; font-weight: bold;")
        info_layout.addWidget(self.lbl_current_file)
        info_layout.addStretch()

        # 文件大小信息
        self.lbl_file_size = QLabel("")
        self.lbl_file_size.setStyleSheet("color: #666;")
        info_layout.addWidget(self.lbl_file_size)
        right_layout.addLayout(info_layout)

        # 级别筛选和搜索控件
        filter_layout = QHBoxLayout()
        filter_layout.addWidget(QLabel("日志级别:"))
        self.cmb_log_level = QComboBox()
        self.cmb_log_level.addItems(["全部", "INFO", "WARNING", "ERROR", "DEBUG"])
        self.cmb_log_level.currentIndexChanged.connect(self._refresh_log_display)
        filter_layout.addWidget(self.cmb_log_level)
        filter_layout.addSpacing(20)

        filter_layout.addWidget(QLabel("🔍 搜索:"))
        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText("输入关键字搜索...")
        self.search_input.setFixedWidth(250)
        self.search_input.setStyleSheet("""
            QLineEdit {
                padding: 6px;
                font-size: 20px;
                border: 2px solid #ccc;
                border-radius: 4px;
            }
            QLineEdit:focus {
                border-color: #0078D7;
            }
        """)
        self.search_input.returnPressed.connect(self._search_next)
        filter_layout.addWidget(self.search_input)

        # 搜索控件
        filter_layout = QHBoxLayout()
        filter_layout.addWidget(QLabel("日志级别:"))
        self.cmb_log_level = QComboBox()
        self.cmb_log_level.addItems(["全部", "INFO", "WARNING", "ERROR", "DEBUG"])
        self.cmb_log_level.currentIndexChanged.connect(self._refresh_log_display)
        self.cmb_log_level.setFixedWidth(100)
        filter_layout.addWidget(self.cmb_log_level)
        filter_layout.addSpacing(20)

        filter_layout.addWidget(QLabel("🔍 搜索:"))
        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText("输入关键字搜索...")
        self.search_input.setFixedWidth(200)
        self.search_input.setStyleSheet("""
            QLineEdit {
                padding: 4px 8px;
                font-size: 20px;
                border: 2px solid #ccc;
                border-radius: 4px;
                background-color: white;
            }
            QLineEdit:focus {
                border-color: #0078D7;
            }
        """)
        self.search_input.returnPressed.connect(self._search_next)
        filter_layout.addWidget(self.search_input)

        # 搜索按钮
        btn_search = QPushButton("搜索")
        btn_search.setFixedWidth(60)
        btn_search.setStyleSheet("""
            QPushButton {
                background-color: #0078D7;
                color: white;
                padding: 4px 8px;
                border-radius: 4px;
                font-size: 20px;
                border: none;
            }
            QPushButton:hover {
                background-color: #0056b3;
            }
            QPushButton:pressed {
                background-color: #004a9e;
            }
        """)
        btn_search.clicked.connect(self._search_log)
        filter_layout.addWidget(btn_search)

        # 上一个按钮
        self.btn_prev = QPushButton("◀")
        self.btn_prev.setFixedWidth(40)
        self.btn_prev.setEnabled(False)
        self.btn_prev.setToolTip("上一个匹配项")
        self.btn_prev.setStyleSheet("""
            QPushButton {
                background-color: #666;
                color: white;
                padding: 4px 6px;
                border-radius: 4px;
                font-size: 20px;
                border: none;
            }
            QPushButton:hover {
                background-color: #888;
            }
            QPushButton:pressed {
                background-color: #555;
            }
            QPushButton:disabled {
                background-color: #ccc;
            }
        """)
        self.btn_prev.clicked.connect(self._search_prev)
        filter_layout.addWidget(self.btn_prev)

        # 下一个按钮
        self.btn_next = QPushButton("▶")
        self.btn_next.setFixedWidth(40)
        self.btn_next.setEnabled(False)
        self.btn_next.setToolTip("下一个匹配项")
        self.btn_next.setStyleSheet("""
            QPushButton {
                background-color: #666;
                color: white;
                padding: 4px 6px;
                border-radius: 4px;
                font-size: 20px;
                border: none;
            }
            QPushButton:hover {
                background-color: #888;
            }
            QPushButton:pressed {
                background-color: #555;
            }
            QPushButton:disabled {
                background-color: #ccc;
            }
        """)
        self.btn_next.clicked.connect(self._search_next)
        filter_layout.addWidget(self.btn_next)

        # 搜索结果标签
        self.lbl_search_result = QLabel("")
        self.lbl_search_result.setStyleSheet("""
            QLabel {
                color: #666; 
                font-size: 20px;
                padding: 0 8px;
            }
        """)
        filter_layout.addWidget(self.lbl_search_result)

        filter_layout.addStretch()
        right_layout.addLayout(filter_layout)

        # 日志内容显示区域
        self.text_log_content = QTextEdit()
        self.text_log_content.setReadOnly(True)
        self.text_log_content.setStyleSheet("""
            QTextEdit {
                background-color: #F5F5F5;
                border: 3px solid #DDD;
                border-radius: 4px;
                font-family: 'Consolas', 'Monaco', monospace;
                font-size: 20px;
            }
        """)
        right_layout.addWidget(self.text_log_content)

        # 底部按钮
        btn_layout = QHBoxLayout()

        btn_refresh = QPushButton("🔄 刷新")
        btn_refresh.clicked.connect(self._refresh_log_display)
        btn_layout.addWidget(btn_refresh)

        btn_save = QPushButton("💾 另存为")
        btn_save.setStyleSheet("background-color: #107C10; color: white;")
        btn_save.clicked.connect(self._save_log)
        btn_layout.addWidget(btn_save)

        btn_clear = QPushButton("🗑️ 清空当前日志")
        btn_clear.setStyleSheet("background-color: #D13438; color: white;")
        btn_clear.clicked.connect(self._clear_log)
        btn_layout.addWidget(btn_clear)

        btn_export = QPushButton("📤 导出")
        btn_export.setStyleSheet("background-color: #0078D7; color: white;")
        btn_export.clicked.connect(self._export_log)
        btn_layout.addWidget(btn_export)

        btn_layout.addStretch()
        right_layout.addLayout(btn_layout)

        # 添加到主布局
        main_layout.addWidget(left_panel)
        main_layout.addWidget(right_panel, 1)

    def _create_history_tab(self):

        self._history_tab = QWidget()
        layout = QVBoxLayout(self._history_tab)

        # 搜索控件
        search_layout = QHBoxLayout()
        search_layout.addWidget(QLabel("🔍 搜索操作记录:"))
        self.history_search_input = QLineEdit()
        self.history_search_input.setPlaceholderText("输入关键字搜索...")
        self.history_search_input.setFixedWidth(300)
        self.history_search_input.setStyleSheet("""
            QLineEdit {
                padding: 6px;
                font-size: 20px;
                border: 2px solid #ccc;
                border-radius: 4px;
            }
            QLineEdit:focus {
                border-color: #0078D7;
            }
        """)
        self.history_search_input.textChanged.connect(self._filter_history)
        search_layout.addWidget(self.history_search_input)

        btn_clear_search = QPushButton("清除")
        btn_clear_search.setStyleSheet("""
            QPushButton {
                background-color: #666;
                color: white;
                padding: 4px 6px;
                border-radius: 4px;
                font-size: 24px;
                border: none;
            }
            QPushButton:hover {
                background-color: #888;
            }
            QPushButton:pressed {
                background-color: #555;
            }
            QPushButton:disabled {
                background-color: #ccc;
            }
        """)
        btn_clear_search.clicked.connect(self._clear_history_search)

        search_layout.addWidget(btn_clear_search)

        search_layout.addStretch()
        layout.addLayout(search_layout)

        self.table_history = QTableWidget()
        self.table_history.setColumnCount(5)
        self.table_history.setHorizontalHeaderLabels(["时间", "串口", "操作动作", "详细数据", "数据包(Hex)"])
        self.table_history.horizontalHeader().setSectionResizeMode(4, QHeaderView.Stretch)
        self.table_history.setAlternatingRowColors(True)
        self.table_history.setStyleSheet("""
            QTableWidget {
                background-color: white;
                alternate-background-color: #F5F5F5;
                gridline-color: #DDD;
            }
            QHeaderView::section {
                background-color: #0078D7;
                color: white;
                padding: 8px;
                font-weight: bold;
                border: none;
            }
        """)
        layout.addWidget(self.table_history)

        btn_layout = QHBoxLayout()

        btn_refresh = QPushButton("🔄 刷新记录")
        btn_refresh.clicked.connect(self._refresh_history_display)
        btn_layout.addWidget(btn_refresh)

        btn_export = QPushButton("📤 导出记录")
        btn_export.setStyleSheet("background-color: #0078D7; color: white;")
        btn_export.clicked.connect(self._export_history)
        btn_layout.addWidget(btn_export)

        btn_clear = QPushButton("🗑️ 清空记录")
        btn_clear.setStyleSheet("background-color: #D13438; color: white;")
        btn_clear.clicked.connect(self._clear_history)
        btn_layout.addWidget(btn_clear)

        btn_layout.addStretch()
        layout.addLayout(btn_layout)

    # ==================== 历史日志文件管理 ====================
    def _refresh_log_list(self):
        """刷新历史日志文件列表"""
        self.log_list_widget.clear()

        log_dir = log_manager.get_log_directory()
        if not os.path.exists(log_dir):
            return

        # 获取所有日志文件
        log_files = []
        for filename in os.listdir(log_dir):
            if filename.endswith('.log') and filename.startswith('control_system_'):
                filepath = os.path.join(log_dir, filename)
                # 获取文件修改时间
                mtime = os.path.getmtime(filepath)
                size = os.path.getsize(filepath)
                log_files.append({
                    'name': filename,
                    'path': filepath,
                    'mtime': mtime,
                    'size': size
                })

        # 按修改时间倒序排列（最新的在前）
        log_files.sort(key=lambda x: x['mtime'], reverse=True)

        # 添加到列表
        today = datetime.now().strftime("%Y-%m-%d")
        for log_file in log_files:
            # 格式化显示名称
            date_str = log_file['name'].replace('control_system_', '').replace('.log', '')

            if date_str == today:
                display_name = f"📌 今日日志 ({date_str})"
            else:
                display_name = f"📄 {date_str}"

            # 格式化文件大小
            size_str = self._format_file_size(log_file['size'])

            item = QListWidgetItem(display_name)
            item.setData(Qt.UserRole, log_file['path'])
            item.setToolTip(f"文件: {log_file['name']}\n大小: {size_str}\n修改时间: {datetime.fromtimestamp(log_file['mtime']).strftime('%Y-%m-%d %H:%M:%S')}")
            self.log_list_widget.addItem(item)

        # 默认选中今日日志
        for i in range(self.log_list_widget.count()):
            item = self.log_list_widget.item(i)
            if '今日日志' in item.text():
                self.log_list_widget.setCurrentItem(item)
                break

        self.status_bar.showMessage(f"找到 {len(log_files)} 个日志文件", 2000)

    def _format_file_size(self, size_bytes):
        """格式化文件大小"""
        if size_bytes < 1024:
            return f"{size_bytes} B"
        elif size_bytes < 1024 * 1024:
            return f"{size_bytes / 1024:.2f} KB"
        else:
            return f"{size_bytes / (1024 * 1024):.2f} MB"

    def _on_log_file_selected(self, item):
        """选择日志文件时的处理"""
        filepath = item.data(Qt.UserRole)
        self.current_log_file = filepath

        # 更新文件信息
        if os.path.exists(filepath):
            size = os.path.getsize(filepath)
            mtime = os.path.getmtime(filepath)

            filename = os.path.basename(filepath)
            size_str = self._format_file_size(size)
            mtime_str = datetime.fromtimestamp(mtime).strftime('%Y-%m-%d %H:%M:%S')

            self.lbl_file_info.setText(f"文件: {filename}\n大小: {size_str}\n修改时间: {mtime_str}")
            self.lbl_current_file.setText(os.path.basename(filepath))
            self.lbl_file_size.setText(size_str)

            # 刷新日志显示
            self._refresh_log_display()

    def _delete_selected_log(self):
        """删除选中的日志文件"""
        current_item = self.log_list_widget.currentItem()
        if not current_item:
            QMessageBox.warning(self, "提示", "请先选择要删除的日志文件")
            return

        # 不能删除今日日志
        if '今日日志' in current_item.text():
            QMessageBox.warning(self, "提示", "今日日志正在使用中，不能删除")
            return

        filepath = current_item.data(Qt.UserRole)
        filename = os.path.basename(filepath)

        reply = QMessageBox.question(
            self, "确认删除",
            f"确定要删除日志文件吗？\n\n文件: {filename}\n\n此操作不可恢复！",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No
        )

        if reply == QMessageBox.Yes:
            try:
                os.remove(filepath)
                self._refresh_log_list()

                # 如果删除的是当前查看的文件，切换回今日日志
                if self.current_log_file == filepath:
                    self.current_log_file = None
                    self._load_today_log()

                QMessageBox.information(self, "成功", "日志文件已删除")
                self.status_bar.showMessage(f"已删除: {filename}", 3000)
            except Exception as e:
                QMessageBox.critical(self, "错误", f"删除失败: {str(e)}")

    def _open_log_folder(self):
        """打开日志文件夹"""
        log_dir = log_manager.get_log_directory()
        if os.path.exists(log_dir):
            import subprocess
            import sys

            # 根据操作系统打开文件夹
            if sys.platform == 'win32':
                os.startfile(log_dir)
            elif sys.platform == 'darwin':  # macOS
                subprocess.run(['open', log_dir])
            else:  # Linux
                subprocess.run(['xdg-open', log_dir])
        else:
            QMessageBox.warning(self, "提示", "日志文件夹不存在")

    def _load_today_log(self):
        """加载今日日志"""
        today_file = log_manager.get_log_file_path()
        if today_file and os.path.exists(today_file):
            self.current_log_file = today_file
            self.lbl_current_file.setText("今日日志")

            size = os.path.getsize(today_file)
            self.lbl_file_size.setText(self._format_file_size(size))

    def _read_log_file_content(self, filepath):
        """读取日志文件内容"""
        try:
            if filepath and os.path.exists(filepath):
                with open(filepath, 'r', encoding='utf-8') as f:
                    return f.read()
            return "暂无日志内容"
        except Exception as e:
            return f"读取日志失败: {str(e)}"

    def _refresh_log_display(self):
        """刷新日志显示"""
        # 如果没有选中文件，加载今日日志
        if not self.current_log_file:
            self._load_today_log()

        # 读取日志内容
        if self.current_log_file and os.path.exists(self.current_log_file):
            full_content = self._read_log_file_content(self.current_log_file)
        else:
            full_content = log_manager.read_log_content()

        selected_level = self.cmb_log_level.currentText()

        if selected_level == "全部":
            filtered_content = full_content
        else:
            lines = full_content.splitlines()
            level_pattern = f" - {selected_level} - "
            filtered_lines = [line for line in lines if level_pattern in line]
            filtered_content = "\n".join(filtered_lines)

        self.text_log_content.setText(filtered_content)
        self.text_log_content.moveCursor(self.text_log_content.textCursor().End)

        # 清除搜索结果
        self.search_results = []
        self.current_search_index = -1
        self.btn_prev.setEnabled(False)
        self.btn_next.setEnabled(False)
        self.lbl_search_result.setText("")

        # 状态栏提示
        total_lines = len(full_content.splitlines()) if full_content else 0
        displayed_lines = len(filtered_content.splitlines()) if filtered_content else 0

        if self.current_log_file:
            filename = os.path.basename(self.current_log_file)
            if selected_level == "全部":
                self.status_bar.showMessage(
                    f"当前文件: {filename} | 共 {total_lines} 行", 3000
                )
            else:
                self.status_bar.showMessage(
                    f"当前文件: {filename} | 显示 {displayed_lines} / {total_lines} 行 ({selected_level})",
                    3000
                )


    # 搜索相关方法保持不变
    def _search_log(self):
        """搜索日志内容并高亮所有匹配项"""
        search_text = self.search_input.text().strip()
        if not search_text:
            self.lbl_search_result.setText("请输入搜索关键字")
            self.text_log_content.setExtraSelections([])
            return

        content = self.text_log_content.toPlainText()
        lines = content.split('\n')

        self.search_results = []
        for i, line in enumerate(lines):
            if search_text.lower() in line.lower():
                self.search_results.append(i)

        if self.search_results:
            self.current_search_index = 0
            self.btn_prev.setEnabled(True)
            self.btn_next.setEnabled(True)
            self.lbl_search_result.setText(f"找到 {len(self.search_results)} 个结果")
            self._goto_search_result(0)
        else:
            self.current_search_index = -1
            self.btn_prev.setEnabled(False)
            self.btn_next.setEnabled(False)
            self.lbl_search_result.setText("未找到结果")
            self.text_log_content.setExtraSelections([])

    def _goto_search_result(self, index):
        """跳转到指定索引的搜索结果"""
        if not self.search_results or index < 0 or index >= len(self.search_results):
            return

        line_number = self.search_results[index]
        search_text = self.search_input.text().strip()

        # 获取文档
        document = self.text_log_content.document()

        # 移动到文档开头
        cursor = QTextCursor(document)
        cursor.movePosition(QTextCursor.Start)

        # 向下移动到目标行
        for _ in range(line_number):
            if not cursor.movePosition(QTextCursor.Down):
                break

        # 移动到行首
        cursor.movePosition(QTextCursor.StartOfLine)

        # 使用 document.find 在当前行查找文本
        # 创建一个临时光标用于查找范围
        end_cursor = QTextCursor(cursor)
        end_cursor.movePosition(QTextCursor.EndOfLine, QTextCursor.KeepAnchor)

        # 在当前行范围内查找
        found_cursor = document.find(search_text, cursor)

        # 确保找到的文本在当前行内
        if not found_cursor.isNull() and found_cursor.position() <= end_cursor.position():
            # 设置光标并滚动到可见区域
            self.text_log_content.setTextCursor(found_cursor)
            self.text_log_content.ensureCursorVisible()

            # 创建高亮效果
            extra_selection = QTextEdit.ExtraSelection()
            extra_selection.format.setBackground(QColor(255, 255, 0, 180))  # 亮黄色
            extra_selection.format.setForeground(QColor(0, 0, 0))  # 黑色文字
            extra_selection.cursor = found_cursor
            self.text_log_content.setExtraSelections([extra_selection])

    def _search_next(self):
        """下一个搜索结果"""
        if not self.search_results:
            return

        self.current_search_index = (self.current_search_index + 1) % len(self.search_results)
        self._goto_search_result(self.current_search_index)
        self.lbl_search_result.setText(f"结果 {self.current_search_index + 1}/{len(self.search_results)}")

    def _search_prev(self):
        """上一个搜索结果"""
        if not self.search_results:
            return

        self.current_search_index = (self.current_search_index - 1) % len(self.search_results)
        self._goto_search_result(self.current_search_index)
        self.lbl_search_result.setText(f"结果 {self.current_search_index + 1}/{len(self.search_results)}")

    def _save_log(self):
        content = self.text_log_content.toPlainText()
        if not content.strip():
            QMessageBox.warning(self, "提示", "没有日志内容可保存")
            return

        default_filename = f"log_export_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
        filename, _ = QFileDialog.getSaveFileName(
            self, "保存日志文件", default_filename,
            "Text Files (*.txt);;All Files (*)"
        )

        if filename:
            success, result = log_manager.create_log_export(content, filename)
            if success:
                QMessageBox.information(self, "成功", f"日志已保存到:\n{result}")
                self.status_bar.showMessage(f"日志已保存到 {result}", 5000)
            else:
                QMessageBox.critical(self, "错误", f"保存失败:\n{result}")

    def _export_log(self):
        if not log_manager.has_log_content():
            QMessageBox.warning(self, "提示", "没有日志内容可导出")
            return

        default_filename = f"control_system_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log"
        filename, _ = QFileDialog.getSaveFileName(
            self, "导出日志文件", default_filename,
            "Log Files (*.log);;Text Files (*.txt);;All Files (*)"
        )

        if filename:
            success, result = log_manager.export_log_with_info(filename)
            if success:
                QMessageBox.information(self, "成功", f"日志已导出到:\n{result}")
                self.status_bar.showMessage(f"日志已导出到 {result}", 5000)
            else:
                QMessageBox.critical(self, "错误", f"导出失败:\n{result}")

    def _clear_log(self):
        # 如果是历史日志，不能清空
        if self.current_log_file and self.current_log_file != log_manager.get_log_file_path():
            QMessageBox.warning(self, "提示", "只能清空今日日志，历史日志不能修改")
            return

        reply = QMessageBox.question(
            self, "确认清空",
            "确定要清空今日日志内容吗？此操作不可恢复！",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No
        )

        if reply == QMessageBox.Yes:
            success = log_manager.delete_log_content()
            if success:
                self._refresh_log_display()
                QMessageBox.information(self, "成功", "日志已清空")
                self.status_bar.showMessage("日志已清空", 3000)
            else:
                QMessageBox.critical(self, "错误", "清空日志失败")

    def _refresh_history_display(self):
        records = GlobalHistory.get_records()
        self.table_history.setRowCount(len(records))
        for row, record in enumerate(records):
            for col, item in enumerate(record):
                self.table_history.setItem(row, col, QTableWidgetItem(str(item)))

        self.table_history.scrollToBottom()
        self._filter_history()

    def _filter_history(self):
        """过滤操作记录"""
        search_text = self.history_search_input.text().strip().lower()

        for row in range(self.table_history.rowCount()):
            match = False
            if not search_text:
                match = True
            else:
                for col in range(self.table_history.columnCount()):
                    item = self.table_history.item(row, col)
                    if item and search_text in item.text().lower():
                        match = True
                        break

            self.table_history.setRowHidden(row, not match)

        visible_rows = sum(1 for row in range(self.table_history.rowCount())
                           if not self.table_history.isRowHidden(row))
        total_rows = self.table_history.rowCount()

        if search_text:
            self.status_bar.showMessage(f"显示 {visible_rows}/{total_rows} 条记录（筛选: '{search_text}'）", 3000)
        else:
            self.status_bar.showMessage(f"共 {total_rows} 条记录", 3000)

    def _clear_history_search(self):
        """清除操作记录搜索"""
        self.history_search_input.clear()
        self._filter_history()

    def _export_history(self):
        if not GlobalHistory.has_records():
            QMessageBox.warning(self, "提示", "没有操作记录可导出")
            return

        default_filename = f"operation_history_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        filename, _ = QFileDialog.getSaveFileName(
            self, "导出操作记录", default_filename,
            "CSV Files (*.csv);;Text Files (*.txt);;All Files (*)"
        )

        if filename:
            success, result = GlobalHistory.export_to_csv(filename)
            if success:
                QMessageBox.information(self, "成功", f"操作记录已导出到:\n{result}")
                self.status_bar.showMessage(f"操作记录已导出到 {result}", 5000)
            else:
                QMessageBox.critical(self, "错误", f"导出失败:\n{result}")

    def _clear_history(self):
        if not GlobalHistory.has_records():
            QMessageBox.warning(self, "提示", "没有操作记录可清空")
            return

        reply = QMessageBox.question(
            self, "确认清空",
            f"确定要清空所有操作记录吗？\n当前有{GlobalHistory.get_record_count()}条记录，此操作不可恢复！",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No
        )

        if reply == QMessageBox.Yes:
            GlobalHistory.clear_records()
            self._refresh_history_display()
            QMessageBox.information(self, "成功", "操作记录已清空")
            self.status_bar.showMessage("操作记录已清空", 3000)

