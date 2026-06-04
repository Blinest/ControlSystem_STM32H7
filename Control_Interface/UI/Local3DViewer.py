from PyQt5.QtWidgets import (QMainWindow, QApplication, QWidget, QVBoxLayout)
from PyQt5.QtWebEngineWidgets import QWebEngineView

from PyQt5.QtCore import QUrl
class Local3DViewer(QMainWindow):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("三维模型查看器")
        screen = QApplication.primaryScreen().availableGeometry()
        self.resize(int(screen.width() * 0.6), int(screen.height() * 0.7))
        self.setMinimumSize(640, 480)   # 防止缩得过小导致控件挤爆
        self.showNormal()               # 确保不是最大化状态

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)
        layout.setContentsMargins(0, 0, 0, 0)

        # 工具栏
        # toolbar = QWidget()
        # toolbar_layout = QHBoxLayout(toolbar)
        # toolbar_layout.setContentsMargins(5, 5, 5, 5)
        #
        # layout.addWidget(toolbar)

        self.web_view = QWebEngineView()
        layout.addWidget(self.web_view)

        self.current_file = None

    def load_file(self, file_path):
        """加载指定的本地 HTML 文件"""
        if not file_path:
            return
        self.current_file = file_path
        url = QUrl.fromLocalFile(file_path)
        self.web_view.load(url)
        self.setWindowTitle(f"LQTS喷管模型")

