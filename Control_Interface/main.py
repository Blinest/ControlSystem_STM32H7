# main.py
import sys

import matplotlib
matplotlib.use('Qt5Agg')  # 设置 backend（如果有其他 backend 设置，保持兼容）
# 配置中文字体
matplotlib.rcParams['font.sans-serif'] = ['WenQuanYi Micro Hei', 'Noto Sans CJK SC', 'SimHei', 'Microsoft YaHei', 'DejaVu Sans']
matplotlib.rcParams['axes.unicode_minus'] = False
import logging
logging.getLogger('matplotlib').setLevel(logging.WARNING)

from PyQt5.QtWidgets import QApplication, QDialog
from UI.main_window import MainWindow
from UI.log_window import LoginWindow
from Core.auth import AuthService



app = QApplication(sys.argv)
app.setStyle("Fusion")
auth_service = AuthService()

# 显示登录界面
login = LoginWindow()
if login.exec_() == QDialog.Accepted:
    win = MainWindow(auth_service) #传入认证服务
    win.show()
    sys.exit(app.exec_())
else:
    sys.exit(0)
