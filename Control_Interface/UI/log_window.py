# QT 组件类
from PyQt5.QtWidgets import (QDialog, QApplication, QVBoxLayout,
                             QWidget, QLabel, QFormLayout, QLineEdit, QHBoxLayout,
                             QCheckBox, QPushButton, QMessageBox)

# QT 核心类
from PyQt5.QtCore import Qt



# 自定义类
from Core.logger import LogManager
from Core.auth import AuthService
# 前端窗口
class LoginWindow(QDialog):
    """登录窗口"""

    def __init__(self):
        super().__init__()
        self.auth_service = AuthService() # 自动使用 ~/.lqts/auth_data
        self._init_ui()
        self._load_saved_login_info()  # 加载保存的登录信息

    def _init_ui(self):
        """初始化UI"""
        self.setWindowTitle("系统登录 - 柔性臂控制平台")
        screen = QApplication.primaryScreen().availableGeometry()
        self.resize(int(screen.width() * 0.1), int(screen.height() * 0.5))
        self.setMinimumSize(600, 480)   # 防止缩得过小导致控件挤爆
        self.showNormal()               # 确保不是最大化状态
        self.setStyleSheet("background-color: #d9d9d6")

        main_layout = QVBoxLayout(self)

        # 中心登录卡片
        center_widget = QWidget()
        center_widget.setStyleSheet(
            "background-color: #d7d2cb; border-radius: 12px; border: 3px solid black;"
        )
        center_widget.setFixedSize(500, 500)

        layout = QVBoxLayout(center_widget)
        layout.setContentsMargins(40, 40, 40, 40)

        # 标题
        title = QLabel("柔性臂控制平台")
        title.setAlignment(Qt.AlignCenter)
        title.setStyleSheet(
            "font-size: 15pt; font-weight: bold; color: black; "
            "margin-bottom: 20px; border: none;"
        )

        # 表单
        form_layout = QFormLayout()
        self.user_input = QLineEdit()
        self.user_input.setPlaceholderText("请输入账号")
        self.pass_input = QLineEdit()
        self.pass_input.setPlaceholderText("请输入密码")
        self.pass_input.setEchoMode(QLineEdit.Password)

        for w in [self.user_input, self.pass_input]:
            w.setStyleSheet(
                "padding: 10px; font-size: 10pt; "
                "border: 3px solid black; border-radius: 4px;"
            )

        form_layout.addRow(
            QLabel("账号:", styleSheet="font-size: 10pt; border: none;"),
            self.user_input
        )
        form_layout.addRow(
            QLabel("密码:", styleSheet="font-size: 10pt; border: none;"),
            self.pass_input
        )

        # 记住密码复选框
        checkbox_layout = QHBoxLayout()
        self.remember_checkbox = QCheckBox("记住密码")
        self.remember_checkbox.setStyleSheet("""
            QCheckBox {
                font-size: 8pt;
                color: #333;
                spacing: 8px;
                border: none;
            }
            QCheckBox::indicator {
                width: 18px;
                height: 18px;
                border: 2px solid black;
                border-radius: 3px;
                background-color: white;
            }
            QCheckBox::indicator:checked {
                background-color: black;
            }
        """)
        checkbox_layout.addWidget(self.remember_checkbox)
        checkbox_layout.addStretch()

        # 按钮
        btn_layout = QHBoxLayout()

        self.btn_login = QPushButton("登录")
        self.btn_login.setStyleSheet("""
            QPushButton { 
                background-color: black; 
                color: #d9d9d6; 
                font-size: 10pt; 
                padding: 12px; 
                border-radius: 6px;
            } 
            QPushButton:hover { 
                background-color: grey; 
            }
        """)
        self.btn_login.clicked.connect(self.handle_login)

        self.btn_register = QPushButton("注册")
        self.btn_register.setStyleSheet("""
            QPushButton { 
                background-color: #555555; 
                color: white; 
                font-size: 10pt; 
                padding: 12px; 
                border-radius: 6px;
            } 
            QPushButton:hover { 
                background-color: #777777; 
            }
        """)
        self.btn_register.clicked.connect(self.handle_register)

        btn_layout.addWidget(self.btn_login)
        btn_layout.addWidget(self.btn_register)

        # 修改密码链接
        self.link_change_pwd = QPushButton("修改密码")

        self.link_change_pwd.setStyleSheet("""
            QPushButton { 
                background-color: transparent; 
                color: #333; 
                font-size: 10pt;
                border: none;
                text-decoration: none;
                padding: 5px;
            } 
            QPushButton:hover {
                color: #0078D7;
                text-decoration: underline;
                font-weight: bold;
            }
            QPushButton:pressed {
                color: #0056b3;
            }
        """)
        self.link_change_pwd.clicked.connect(self.handle_change_password)

        # 组装布局
        layout.addWidget(title)
        layout.addLayout(form_layout)
        layout.addSpacing(10)
        layout.addLayout(checkbox_layout)
        layout.addSpacing(20)
        layout.addLayout(btn_layout)
        layout.addWidget(self.link_change_pwd)

        main_layout.addWidget(center_widget, alignment=Qt.AlignCenter)

        # 回车键触发登录
        self.user_input.returnPressed.connect(self.handle_login)
        self.pass_input.returnPressed.connect(self.handle_login)

    def _load_saved_login_info(self):
        """通过 backend 加载保存的登录信息"""
        username, password, remember = self.auth_service.backend.get_saved_login_info()
        if remember and username:
            self.user_input.setText(username)
            self.pass_input.setText(password)
            self.remember_checkbox.setChecked(True)

    def _save_login_info(self, username: str, password: str, remember: bool):
        """通过 backend 保存登录信息"""
        self.auth_service.backend.save_login_info(username, password, remember)

    def handle_login(self):
        """处理登录"""
        username = self.user_input.text().strip()
        password = self.pass_input.text()

        if not username or not password:
            QMessageBox.warning(self, "错误", "账号和密码不能为空")
            return

        success, message = self.auth_service.login(username, password)

        if success:
            # 保存登录信息
            remember = self.remember_checkbox.isChecked()
            self._save_login_info(username, password, remember)

            # 移除成功提示框，直接进入主界面
            # QMessageBox.information(self, "成功", message)  # 注释掉这行
            self.accept()  # 直接接受对话框，进入主界面
        else:
            QMessageBox.warning(self, "错误", message)
            self.pass_input.clear()
            self.pass_input.setFocus()
    def handle_register(self):
        """处理注册"""
        dialog = RegisterDialog(self)
        if dialog.exec_() == QDialog.Accepted:
            # 注册成功后自动填充用户名
            if dialog.registered_username:
                self.user_input.setText(dialog.registered_username)
                self.pass_input.clear()
                self.pass_input.setFocus()

    def handle_change_password(self):
        """处理修改密码"""
        dialog = ChangePasswordDialog(self.auth_service, self)
        if dialog.exec_() == QDialog.Accepted:
            # 如果修改密码成功，且当前用户勾选了记住密码，更新保存的密码
            if self.remember_checkbox.isChecked() and self.auth_service.is_logged_in():
                current_user = self.auth_service.get_current_user()
                if current_user == self.user_input.text().strip():
                    new_password = dialog.new_password
                    self._save_login_info(current_user, new_password, True)
                    # 同时更新密码输入框中的显示
                    self.pass_input.setText(new_password)

class RegisterDialog(QDialog):
    """注册对话框"""

    def __init__(self, parent=None, is_admin=False):
        super().__init__(parent)
        self.auth_service = AuthService()
        self.registered_username = None
        self.is_admin_context = is_admin  # 是否是管理员在创建用户
        self._init_ui()

    def _init_ui(self):
        self.setWindowTitle("用户注册")
        self.setFixedSize(500, 450 if self.is_admin_context else 400)
        self.setStyleSheet("background-color: #d7d2cb;")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(30, 30, 30, 30)
        layout.setSpacing(15)

        # 标题
        title = QLabel("创建新账号" if not self.is_admin_context else "创建用户账号")
        title.setAlignment(Qt.AlignCenter)
        title.setStyleSheet("font-size: 12pt; font-weight: bold; border: none;")
        layout.addWidget(title)

        # 表单
        form_widget = QWidget()
        form_layout = QFormLayout(form_widget)
        form_layout.setSpacing(10)

        self.username_input = QLineEdit()
        self.username_input.setPlaceholderText("至少3个字符")
        self.password_input = QLineEdit()
        self.password_input.setPlaceholderText("至少3个字符")
        self.password_input.setEchoMode(QLineEdit.Password)
        self.confirm_input = QLineEdit()
        self.confirm_input.setPlaceholderText("再次输入密码")
        self.confirm_input.setEchoMode(QLineEdit.Password)

        input_style = "padding: 8px; font-size: 12pt; border: 2px solid black; border-radius: 4px;"
        for w in [self.username_input, self.password_input, self.confirm_input]:
            w.setStyleSheet(input_style)

        form_layout.addRow(QLabel("用户名:", styleSheet="font-size: 12pt;"), self.username_input)
        form_layout.addRow(QLabel("密码:", styleSheet="font-size: 12pt;"), self.password_input)
        form_layout.addRow(QLabel("确认密码:", styleSheet="font-size: 12pt;"), self.confirm_input)

        # 如果是管理员创建用户，显示角色选择
        if self.is_admin_context:
            self.role_combo = QComboBox()
            self.role_combo.addItems(["user", "admin"])
            self.role_combo.setStyleSheet("""
                QComboBox {
                    padding: 8px;
                    font-size: 12pt;
                    border: 2px solid black;
                    border-radius: 4px;
                    background-color: white;
                }
            """)
            form_layout.addRow(QLabel("角色:", styleSheet="font-size: 12pt;"), self.role_combo)

        layout.addWidget(form_widget)

        # 按钮
        btn_layout = QHBoxLayout()
        btn_register = QPushButton("注册")
        btn_register.setStyleSheet("""
            QPushButton { 
                background-color: black; 
                color: white; 
                padding: 10px 20px; 
                border-radius: 4px;
                font-size: 12pt;
            } 
            QPushButton:hover { 
                background-color: grey; 
            }
        """)
        btn_register.clicked.connect(self.handle_register)

        btn_cancel = QPushButton("取消")
        btn_cancel.setStyleSheet("""
            QPushButton { 
                background-color: #999; 
                color: white; 
                padding: 10px 20px; 
                border-radius: 4px;
                font-size: 12pt;
            } 
            QPushButton:hover { 
                background-color: #777; 
            }
        """)
        btn_cancel.clicked.connect(self.reject)

        btn_layout.addWidget(btn_register)
        btn_layout.addWidget(btn_cancel)
        layout.addLayout(btn_layout)

    def handle_register(self):
        """处理注册"""
        username = self.username_input.text().strip()
        password = self.password_input.text()
        confirm = self.confirm_input.text()

        # 获取角色
        role = "user"
        if self.is_admin_context and hasattr(self, 'role_combo'):
            role = self.role_combo.currentText()

        success, message = self.auth_service.register(username, password, confirm)

        # 如果是管理员创建用户，需要更新角色
        if success and self.is_admin_context and role == "admin":
            # 更新用户角色
            users = self.auth_service.backend._load_users()
            if username in users:
                users[username]["role"] = "admin"
                self.auth_service.backend._save_users(users)

        if success:
            QMessageBox.information(self, "成功", message)
            self.registered_username = username
            self.accept()
        else:
            QMessageBox.warning(self, "错误", message)

class ChangePasswordDialog(QDialog):
    """修改密码对话框"""

    def __init__(self, auth_service: 'AuthService', parent=None):
        super().__init__(parent)
        self.auth_service = auth_service
        self.password_changed = False
        self.new_password = ""
        self._init_ui()

    def _init_ui(self):
        self.setWindowTitle("修改密码")
        self.setFixedSize(500, 450)
        self.setStyleSheet("background-color: #d7d2cb;")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(30, 30, 30, 30)
        layout.setSpacing(15)

        # 标题
        title = QLabel("修改密码")
        title.setAlignment(Qt.AlignCenter)
        title.setStyleSheet("font-size: 10pt; font-weight: bold; border: none;")
        layout.addWidget(title)

        # 表单
        form_widget = QWidget()
        form_layout = QFormLayout(form_widget)
        form_layout.setSpacing(10)

        self.old_password = QLineEdit()
        self.old_password.setPlaceholderText("输入旧密码")
        self.old_password.setEchoMode(QLineEdit.Password)

        self.new_password = QLineEdit()
        self.new_password.setPlaceholderText("至少3个字符")
        self.new_password.setEchoMode(QLineEdit.Password)

        self.confirm_password = QLineEdit()
        self.confirm_password.setPlaceholderText("再次输入新密码")
        self.confirm_password.setEchoMode(QLineEdit.Password)

        input_style = "padding: 8px; font-size: 10pt; border: 2px solid black; border-radius: 4px;"
        for w in [self.old_password, self.new_password, self.confirm_password]:
            w.setStyleSheet(input_style)

        form_layout.addRow(QLabel("旧密码:", styleSheet="font-size: 10pt;"), self.old_password)
        form_layout.addRow(QLabel("新密码:", styleSheet="font-size: 10pt;"), self.new_password)
        form_layout.addRow(QLabel("确认密码:", styleSheet="font-size: 10pt;"), self.confirm_password)

        layout.addWidget(form_widget)

        # 如果是未登录状态，需要输入用户名
        if not self.auth_service.is_logged_in():
            self.username_input = QLineEdit()
            self.username_input.setPlaceholderText("输入用户名")
            self.username_input.setStyleSheet(input_style)
            form_layout.insertRow(0, QLabel("用户名:", styleSheet="font-size: 10pt;"), self.username_input)

        # 按钮
        btn_layout = QHBoxLayout()
        btn_confirm = QPushButton("确认修改")
        btn_confirm.setStyleSheet("""
            QPushButton { 
                background-color: black; 
                color: white; 
                padding: 10px 20px; 
                border-radius: 4px;
                font-size: 10pt;
            } 
            QPushButton:hover { 
                background-color: grey; 
            }
        """)
        btn_confirm.clicked.connect(self.handle_change_password)

        btn_cancel = QPushButton("取消")
        btn_cancel.setStyleSheet("""
            QPushButton { 
                background-color: #999; 
                color: white; 
                padding: 10px 20px; 
                border-radius: 4px;
                font-size: 10pt;
            } 
            QPushButton:hover { 
                background-color: #777; 
            }
        """)
        btn_cancel.clicked.connect(self.reject)

        btn_layout.addWidget(btn_confirm)
        btn_layout.addWidget(btn_cancel)
        layout.addLayout(btn_layout)


    def handle_change_password(self):
        """处理修改密码"""
        old_pwd = self.old_password.text()
        new_pwd = self.new_password.text()
        confirm_pwd = self.confirm_password.text()

        if new_pwd != confirm_pwd:
            QMessageBox.warning(self, "错误", "两次输入的新密码不一致")
            return

        if hasattr(self, 'username_input'):
            # 未登录状态，需要指定用户名
            username = self.username_input.text().strip()
            if not username:
                QMessageBox.warning(self, "错误", "请输入用户名")
                return

            # 直接调用backend的方法
            success, message = self.auth_service.backend.change_password(username, old_pwd, new_pwd)
        else:
            # 已登录状态，修改当前用户密码
            success, message = self.auth_service.change_password(old_pwd, new_pwd)
            if success:
                self.password_changed = True
                self.new_password = new_pwd

        if success:
            QMessageBox.information(self, "成功", message)
            self.accept()
        else:
            QMessageBox.warning(self, "错误", message)