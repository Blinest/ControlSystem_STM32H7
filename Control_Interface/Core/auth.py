# 工具类
import os
from datetime import datetime
from typing import Tuple, Optional
import json
# 加密解密
import base64
import hashlib

# ==========================================
# 1. 全局数据与日志中心
# ==========================================
class GlobalAuth:
    USER = "TH"
    PASS = "th"

class GlobalHistory:
    """全局操作记录管理器"""

    _records = []

    @classmethod
    def add_record(cls, port, action, data, packet):
        """添加操作记录"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        cls._records.append([timestamp, port, action, data, packet])
        if len(cls._records) > 10000:
            cls._records = cls._records[-10000:]

    @classmethod
    def get_records(cls):
        """获取所有记录"""
        return cls._records.copy()

    @classmethod
    def get_record_count(cls):
        """获取记录数量"""
        return len(cls._records)

    @classmethod
    def has_records(cls):
        """检查是否有记录"""
        return len(cls._records) > 0

    @classmethod
    def clear_records(cls):
        """清空所有记录"""
        cls._records.clear()

    @classmethod
    def export_to_csv(cls, filename):
        """导出记录到CSV文件"""
        if not cls._records:
            return False, "没有操作记录可导出"

        try:
            with open(filename, 'w', encoding='utf-8') as file:
                file.write("时间,串口,操作动作,详细数据,数据包(Hex)\n")
                for record in cls._records:
                    escaped_record = []
                    for item in record:
                        item_str = str(item)
                        if ',' in item_str or '"' in item_str:
                            escaped_item = item_str.replace('"', '""')
                            escaped_record.append(f'"{escaped_item}"')
                        else:
                            escaped_record.append(item_str)
                    file.write(','.join(escaped_record) + '\n')
            return True, filename
        except Exception as error:
            return False, str(error)

# ==========================================
# 2. 登录窗口
# ==========================================
class SimpleAuthBackend:
    """简单的文件认证后端"""
    def __init__(self, data_dir: str = None):
        """
        初始化认证后端
        :param data_dir: 存储用户数据的文件夹，默认为 ./lqts/auth_data
        """
        if data_dir is None:
            data_dir = os.path.expanduser("~/.lqts/auth_data")
        self.data_dir = data_dir
        self.users_file = os.path.join(data_dir, "users.json")
        self.config_file = os.path.join(data_dir, "config.json")
        self._init_storage()

    def _init_storage(self):
        """初始化文件夹与默认文件"""
        # 创建文件夹
        if not os.path.exists(self.data_dir):
            os.makedirs(self.data_dir)

        # 如果用户文件不存在，创建默认用户
        if not os.path.exists(self.users_file):
            default_users = {
                "TH": {
                    "password_hash": self._hash_password("th"),
                    "role": "admin",
                    "created_at": "2026-04-01"
                }
            }
            self._save_users(default_users)

        # 初始化配置文件，如果没有创建，则默认使用TH，密码是th
        if not os.path.exists(self.config_file):
            default_config = {
                "remember_me": False,
                "last_username": "TH",
                "last_password": self._encrypt_password("th")  # 注意：实际存储的是加密后的密码
            }
            self._save_config(default_config)

    @staticmethod
    def _hash_password(password: str) -> str:
        """使用SHA256加密密码"""
        return hashlib.sha256(password.encode('utf-8')).hexdigest()

    @staticmethod
    def _encrypt_password(password: str) -> str:
        """简单的密码加密存储（用于记住密码功能）"""
        # 使用base64进行简单编码（实际项目中建议使用更安全的方式）
        return base64.b64encode(password.encode('utf-8')).decode('utf-8')

    @staticmethod
    def _decrypt_password(encrypted: str) -> str:
        """解密密码"""
        try:
            return base64.b64decode(encrypted.encode('utf-8')).decode('utf-8')
        except:
            return ""

    def _load_users(self) -> dict:
        """从文件加载用户数据"""
        try:
            with open(self.users_file, 'r', encoding='utf-8') as file:
                return json.load(file)
        except Exception as e:
            print(f"加载用户数据失败: {e}")
            return {}

    def _save_users(self, users: dict):
        """保存用户数据到文件"""
        try:
            with open(self.users_file, 'w', encoding='utf-8') as f:
                json.dump(users, f, indent=4, ensure_ascii=False)
            return True
        except Exception as e:
            print(f"保存用户数据失败: {e}")
            return False

    def _load_config(self) -> dict:
        """加载配置文件"""
        try:
            with open(self.config_file, 'r', encoding='utf-8') as file:
                return json.load(file)
        except Exception as e:
            print(f"加载配置文件失败: {e}")
            return {"remember_me": False, "last_username": "", "last_password": ""}

    def _save_config(self, config: dict):
        """保存配置文件"""
        try:
            with open(self.config_file, 'w', encoding='utf-8') as file:
                json.dump(config, file, indent=4, ensure_ascii=False)
            return True
        except Exception as e:
            print(f"保存配置文件失败: {e}")
            return False

    def save_login_info(self, username: str, password: str, remember: bool):
        """保存登录信息"""
        config = self._load_config()
        config["remember_me"] = remember
        if remember:
            config["last_username"] = username
            config["last_password"] = self._encrypt_password(password)
        else:
            config["last_username"] = ""
            config["last_password"] = ""
        self._save_config(config)

    def get_saved_login_info(self) -> tuple:
        """获取保存的登录信息"""
        config = self._load_config()
        if config.get("remember_me", False):
            username = config.get("last_username", "")
            encrypted_password = config.get("last_password", "")
            password = self._decrypt_password(encrypted_password)
            return username, password, True
        return "", "", False

    def login(self, username: str, password: str) -> Tuple[bool, str]:
        """
        登录验证
        :return: (是否成功, 消息, 角色)
        """
        if not username or not password:
            return False, "用户名和密码不能为空", None

        users = self._load_users()

        if username not in users:
            return False, "用户不存在", None

        password_hash = self._hash_password(password)
        if users[username]["password_hash"] != password_hash:
            return False, "密码错误", None

        role = users[username].get("role", "user")

        return True, f"欢迎，{username}！", role

    def register(self, username: str, password: str, confirm_password: str = None) -> Tuple[bool, str]:
        """
        注册新用户
        :return: (是否成功, 消息)
        """
        # 验证输入
        if not username or not password:
            return False, "用户名和密码不能为空"

        if len(username) < 3:
            return False, "用户名至少3个字符"

        if len(password) < 3:
            return False, "密码至少3个字符"

        if confirm_password is not None and password != confirm_password:
            return False, "两次输入的密码不一致"

        users = self._load_users()

        # 检查用户名是否已存在
        if username in users:
            return False, "用户名已存在"

        # 创建新用户
        from datetime import datetime
        users[username] = {
            "password_hash": self._hash_password(password),
            "role": "user",
            "created_at": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        }

        # 保存到文件
        if self._save_users(users):
            return True, "注册成功！"
        else:
            return False, "注册失败，请稍后重试"

    def change_password(self, username: str, old_password: str, new_password: str) -> Tuple[bool, str]:
        """
        修改密码
        :return: (是否成功, 消息)
        """
        if not old_password or not new_password:
            return False, "密码不能为空"

        if len(new_password) < 3:
            return False, "新密码至少3个字符"

        users = self._load_users()

        if username not in users:
            return False, "用户不存在"

        # 验证旧密码
        old_password_hash = self._hash_password(old_password)
        if users[username]["password_hash"] != old_password_hash:
            return False, "旧密码错误"

        # 更新密码
        users[username]["password_hash"] = self._hash_password(new_password)

        # 保存到文件
        if self._save_users(users):
            return True, "密码修改成功！"
        else:
            return False, "密码修改失败，请稍后重试"

    def user_exists(self, username: str) -> bool:
        """检查用户是否存在"""
        users = self._load_users()
        return username in users

    def get_user_role(self, username: str) -> str:
        """获取用户角色"""
        users = self._load_users()
        if username in users:
            return users[username].get("role", "user")
        return "user"

class AuthService:
    """认证服务（单例模式）"""
    _instance = None
    _current_user = None    # 添加角色
    _current_role = None  # 添加角色属性

    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def __init__(self, data_dir: str = None):
        if data_dir is None:
            # 默认存储到用户主目录下的 .lqts 文件夹
            data_dir = os.path.expanduser("~/.lqts/auth_data")
        if not hasattr(self, 'backend'):
            self.backend = SimpleAuthBackend(data_dir)

    def login(self, username: str, password: str) -> Tuple[bool, str]:
        """登录"""
        success, message, role = self.backend.login(username, password)
        if success:
            self._current_user = username
            self._current_role = role
        return success, message

    def register(self, username: str, password: str, confirm_password: str = None) -> Tuple[bool, str]:
        """注册"""
        return self.backend.register(username, password, confirm_password)

    def logout(self):
        """登出"""
        self._current_user = None
        self._current_role = None

    def change_password(self, old_password: str, new_password: str) -> Tuple[bool, str]:
        """修改当前用户密码"""
        if not self._current_user:
            return False, "请先登录"
        return self.backend.change_password(self._current_user, old_password, new_password)

    def get_current_user(self) -> Optional[str]:
        """获取当前登录用户"""
        return self._current_user

    def get_current_role(self) -> str:
        """获取当前用户角色"""
        return self._current_role or "user"

    def is_admin(self) -> bool:
        """检查是否为管理员"""
        return self._current_role == "admin"

    def is_logged_in(self) -> bool:
        """是否已登录"""
        return self._current_user is not None
