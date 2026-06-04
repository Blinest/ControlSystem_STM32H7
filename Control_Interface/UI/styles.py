# UI/styles.py
"""统一管理所有界面样式表，高级水母蓝主题（无白色背景，高对比度文字）"""

def get_main_window_style():
    """主窗口全局样式表（全蓝半透明，文字高对比度，输入区淡蓝）"""
    return """
    /* ========== 全局 ========== */
    QMainWindow, QWidget {
        background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0a2f4a, stop:1 #1a4d6e);
        color: #f0f8ff;
        font-family: 'Segoe UI', '微软雅黑';
    }

    /* ========== 通用按钮 ========== */
    QPushButton {
        background-color: rgba(30, 80, 120, 0.7);
        color: #ffffff;
        border: 1px solid #2e8bc0;
        border-radius: 6px;
        padding: 4px 10px;
        font-weight: normal;
    }
    QPushButton:hover {
        background-color: rgba(50, 120, 170, 0.85);
        border-color: #5bb4e0;
        color: #ffffff;
    }
    QPushButton:pressed {
        background-color: rgba(20, 60, 90, 0.9);
        border-color: #1e6f9f;
    }

    /* 主要操作按钮 (class="primary") 深蓝渐变 */
    QPushButton[class="primary"] {
        background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                    stop:0 #2c7eb6, stop:1 #1a5a8a);
        color: white;
        border: none;
        font-weight: bold;
        border-radius: 20px;
        padding: 6px 16px;
    }
    QPushButton[class="primary"]:hover {
        background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                    stop:0 #3e94ce, stop:1 #2c7eb6);
    }
    QPushButton[class="primary"]:pressed {
        background: #0f4a6e;
    }

    /* 危险按钮 */
    QPushButton[class="danger"] {
        background-color: rgba(180, 70, 60, 0.8);
        color: #fff5f0;
        border: 1px solid #c97a6c;
        border-radius: 15px;
        padding: 4px 12px;
    }
    QPushButton[class="danger"]:hover {
        background-color: rgba(200, 80, 70, 0.9);
        border-color: #e07a5a;
    }
    QPushButton[class="danger"]:pressed {
        background-color: #a05242;
    }

    /* 成功按钮 */
    QPushButton[class="success"] {
        background-color: rgba(30, 100, 100, 0.8);
        color: #eafffa;
        border: 1px solid #2c9c8c;
        border-radius: 15px;
    }
    QPushButton[class="success"]:hover {
        background-color: rgba(40, 130, 120, 0.9);
        border-color: #3ebfae;
    }
    QPushButton[class="success"]:pressed {
        background-color: #1a6b6b;
    }

    /* 紧急停止按钮 */
    QPushButton[class="emergency"] {
        background-color: rgba(200, 70, 40, 0.85);
        color: #ffffff;
        font-weight: bold;
        border-radius: 15px;
        border: 1px solid #e07a5a;
    }
    QPushButton[class="emergency"]:hover {
        background-color: rgba(220, 90, 50, 0.95);
        border-color: #ff8c6a;
    }
    QPushButton[class="emergency"]:pressed {
        background-color: #c0392b;
    }

    /* 页面导航按钮 */
    QPushButton[class="page-btn"] {
        background-color: rgba(40, 90, 130, 0.7);
        color: #e6f5ff;
        font-weight: bold;
        border: 1px solid #2e8bc0;
        border-radius: 10px;
        padding: 5px 12px;
    }
    QPushButton[class="page-btn"]:hover {
        background-color: rgba(60, 120, 160, 0.85);
        border-color: #5bb4e0;
    }
    QPushButton[class="page-btn"]:pressed {
        background-color: rgba(25, 70, 100, 0.9);
    }

    /* ========== 分组框 ========== */
    QGroupBox {
        background: rgba(20, 55, 85, 0.6);
        border: 1px solid #2e6a8f;
        border-radius: 12px;
        margin-top: 16px;
        padding: 8px;
        font-weight: bold;
        color: #e6f5ff;
    }
    QGroupBox::title {
        subcontrol-origin: margin;
        left: 12px;
        padding: 0 8px;
        color: #b0e0ff;
        background-color: transparent;
    }

    /* ========== 输入与数值控件（淡蓝背景 + 深色文字）========== */
    QLineEdit, QTextEdit, QDoubleSpinBox, QSpinBox {
        background-color: rgba(210, 225, 240, 0.85);
        color: #0a2f4a;
        border: 1px solid #2e8bc0;
        border-radius: 8px;
        padding: 6px;
        selection-background-color: #2e8bc0;
        selection-color: #ffffff;
    }
    QLineEdit:focus, QTextEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus {
        background-color: rgba(220, 235, 245, 0.95);
        border-color: #4aa4d0;
        color: #0a2f4a;
    }

    /* ========== 下拉框（淡蓝背景 + 深色文字）========== */
    QComboBox {
        background-color: rgba(210, 225, 240, 0.85);
        color: #0a2f4a;
        border: 1px solid #2e8bc0;
        border-radius: 8px;
        padding: 4px 8px;
    }
    QComboBox:hover {
        background-color: rgba(220, 235, 245, 0.9);
        border-color: #5bb4e0;
    }
    QComboBox::drop-down {
        subcontrol-origin: padding;
        subcontrol-position: top right;
        width: 22px;
        border-left: 1px solid #2e6a8f;
        border-top-right-radius: 6px;
        border-bottom-right-radius: 6px;
    }
    QComboBox QAbstractItemView {
        background-color: #f0f6fc;            /* 浅蓝白背景 */
        color: #0a2f4a;                       /* 深蓝文字 */
        selection-background-color: #2e8bc0;
        selection-color: #ffffff;
        border: 1px solid #2e8bc0;
        border-radius: 6px;
        outline: none;
    }
    QComboBox QAbstractItemView::item {
        padding: 4px 8px;
    }

    /* ========== 状态栏 ========== */
    QStatusBar {
        background-color: #0a2f4a;
        color: #e6f5ff;
        font-weight: bold;
        border-top: 1px solid #2e6a8f;
    }
    QStatusBar::item {
        border: none;
    }

    /* ========== 标签页 ========== */
    QTabWidget::pane {
        border: 1px solid #2e6a8f;
        background-color: rgba(15, 45, 70, 0.5);
        border-radius: 12px;
    }
    QTabBar::tab {

        color: #d4ecff;
        border: 1px solid #2e6a8f;
        border-bottom: none;
        border-top-left-radius: 8px;
        border-top-right-radius: 8px;
        padding: 6px 20px;
        margin-right: 4px;
    }
    QTabBar::tab:selected {

        color: #ffffff;
        font-weight: bold;
        border-bottom: 2px solid #7bc4f0;
    }
    QTabBar::tab:hover:!selected {
        background: #2a6a90;
        color: #ffffff;
    }

    /* ========== 工具栏 ========== */
    QToolBar {
        
        border-bottom: 1px solid #2e6a8f;
        spacing: 8px;
        padding: 4px;
    }
    QToolBar QLabel {
        color: #e6f5ff;
        font-weight: normal;
    }

    /* ========== 滚动条 ========== */
    QScrollBar:vertical {
        background: #0a2f4a;
        width: 10px;
        margin: 0;
        border-radius: 5px;
    }
    QScrollBar::handle:vertical {
        background: #2e8bc0;
        min-height: 20px;
        border-radius: 5px;
    }
    QScrollBar::handle:vertical:hover {
        background: #5bb4e0;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0px;
    }

    /* ========== 停靠窗口 ========== */
    QDockWidget {
        border: 1px solid #2e6a8f;
        titlebar-close-icon: url();
        titlebar-normal-icon: url();
    }
    QDockWidget::title {
        background: #0e3a55;
        padding: 4px;
        color: #e6f5ff;
    }

    /* ========== 消息框 ========== */
    QMessageBox {
        background-color: #1a4d6e;
        color: #f0f8ff;
        border: 1px solid #2e8bc0;
        border-radius: 16px;
    }
    QMessageBox QLabel {
        color: #f0f8ff;
    }
    QMessageBox QPushButton {
        min-width: 80px;
        min-height: 28px;
        background-color: rgba(40, 100, 140, 0.8);
        border-radius: 8px;
        border: 1px solid #2e8bc0;
        color: white;
    }
    QMessageBox QPushButton:hover {
        background-color: rgba(60, 130, 170, 0.9);
    }
    """

def get_device_tab_extra_style():
    """
    设备选项卡特有的样式补充（全蓝半透明，强化监控文字对比度）
    输入控件已由全局样式统一为淡蓝色，此处不再覆盖。
    """
    return """
    /* 电机/传感器卡片 - 半透蓝底 */
    QFrame#motorCard, QFrame#sensorCard {
        background: rgba(30, 80, 120, 0.5);
        border: 1px solid #2e8bc0;
        border-radius: 12px;
    }
    QFrame#motorCard:hover, QFrame#sensorCard:hover {
        background: rgba(40, 100, 145, 0.65);
        border-color: #5bb4e0;
    }
    QDoubleSpinBox {
        min-height: 34px;
        font-size: 10pt;
        font-weight: bold;
        border: 1px solid #cfe2f2;
        border-radius: 10px;
        background: #2C3E50; /* 深色背景 */
        padding: 4px 8px;
        color: white; /* 白色文字 */
    }
    QDoubleSpinBox:focus {
        border-color: #1e6f9f;
    }
    QPushButton[addr_btn="true"] {
        border-radius: 8px;
        font-weight: bold;
    }
    QPushButton[addr_btn="true"]:checked {
        background-color: #3498DB;
        color: white;
    }

    /* 卡片内 QLabel 强制白色（卡片背景深，需要浅字） */
    QFrame#motorCard QLabel,
    QFrame#sensorCard QLabel {
        color: #ffffff !important;
        font-weight: normal;
    }
    
    /* 卡片内输入框继承全局淡蓝样式，不做额外覆盖 */
    QFrame#motorCard QLineEdit,
    QFrame#sensorCard QLineEdit,
    QFrame#motorCard QTextEdit,
    QFrame#sensorCard QTextEdit,
    QFrame#motorCard QDoubleSpinBox,
    QFrame#sensorCard QDoubleSpinBox,
    QFrame#motorCard QSpinBox,
    QFrame#sensorCard QSpinBox {
        /* 使用全局样式，这里仅清空可能的冲突 */
        color: #0a2f4a;
        background-color: rgba(210, 225, 240, 0.85);
    }

    /* 扁平卡片（弯曲角度监控） */
    QFrame[flatCard="true"] {
        background: rgba(25, 70, 105, 0.6);
        border: 1px solid #2e8bc0;
        border-radius: 12px;
    }
    QFrame[flatCard="true"]:hover {
        background: rgba(35, 85, 125, 0.75);
    }
    QFrame[flatCard="true"] QLabel {
        color: #ffffff !important;
        font-size: 13pt;
        font-weight: bold;
    }

    /* 定点监测卡片 */
    QFrame#singleValueCard {
        background: rgba(30, 80, 120, 0.5);
        border: 1px solid #2e8bc0;
        border-radius: 6px;
    }
    QFrame#singleValueCard:hover {
        background: rgba(40, 100, 145, 0.65);
        border-color: #5bb4e0;
    }
    QLabel#singleCardTitle {
        color: #ffffff;
        font-weight: bold;
        font-size: 15pt;
        border: none;
    }
    QLabel#singleCardValue {
        color: #ffffff;
        font-size: 15pt;
        font-weight: bold;
        border: none;
    }
    /* 工具栏用户标签 */
    QLabel#toolbarUserLabel {
        font-weight: bold;
        color: #1a3b4f;
        background-color: #e6f0f9;
        border-radius: 18px;
        padding: 5px 15px;
        font-size: 12pt;
        margin-right: 5px;
    }
    
    /* 工具栏登出按钮 */
    QPushButton#toolbarLogoutButton {
        background-color: #555;
        color: white;
        padding: 5px 15px;
        border-radius: 4px;
        font-size: 12pt;
    }
    QPushButton#toolbarLogoutButton:hover {
        background-color: #D13438;
    }

    /* 翻页按钮文字 */
    QPushButton[class="page-btn"] {
        color: #ffffff;
    }

    /* 右侧监控选项卡内容区域背景 */
    QTabWidget QWidget {
        background: transparent;
    }

    /* 电机卡片内部文字 */
    QLabel#motorCardTitle {
        color: #ffffff;
        font-weight: bold;
        font-size: 11pt;
        border: none;
    }
    QLabel#motorStateBall {
        color: #ffffff;
        font-size: 10pt;
        border: none;
    }
    QLabel#motorBlockTitle {
        color: #ffffff;
        font-size: 10pt;
        font-weight: bold;
        border: none;
    }
    QLabel#motorCurValue {
        color: #ffffff;
        font-size: 9pt;
        font-weight: bold;
        border: none;
    }
    QLabel#motorTarValue {
        color: #ffffff;
        font-size: 9pt;
        border: none;
    }
    QFrame#motorLine {
        background-color: rgba(255, 255, 255, 0.3);
        border: none;
        height: 1px;
    }

    /* 传感器卡片内部文字 */
    QLabel#sensorCardTitle {
        color: #ffffff;
        font-weight: bold;
        font-size: 11pt;
        border: none;
    }
    QLabel#sensorAxisLabel {
        color: #ffffff;
        font-size: 9pt;
        font-weight: bold;
        border: none;
    }
    QLabel#sensorAxisValue {
        color: #ffffff;
        font-size: 9pt;
        font-weight: bold;
        border: none;
    }
    QFrame#sensorLine {
        background-color: rgba(255, 255, 255, 0.3);
        border: none;
        height: 1px;
    }
    """