import os
import logging
from datetime import datetime   # <-- 添加这一行

# ==========================================
# 日志管理器
# ==========================================
class LogManager:
    """日志管理器 - 按照增删改查逻辑组织"""
    def __init__(self):
        self._log_directory = os.path.expanduser("~/.lqts/logs")
        self._current_log_path = None
        self._level_icons = {
            "INFO": "ℹ️",
            "WARNING": "⚠️",
            "ERROR": "❌",
            "DEBUG": "🐛"
        }
        self._initialize_logging()

    # ==================== 创建/初始化 (CREATE) ====================
    def _initialize_logging(self):
        """初始化日志系统"""
        if not os.path.exists(self._log_directory):
            os.makedirs(self._log_directory)

        date_str = datetime.now().strftime("%Y-%m-%d")
        self._current_log_path = os.path.join(
            self._log_directory,
            f"control_system_{date_str}.log"
        )

        # 创建根日志记录器，级别设为 DEBUG（允许所有级别）
        logger = logging.getLogger()
        logger.setLevel(logging.DEBUG)

        # 文件处理器：记录 DEBUG 及以上所有级别
        file_handler = logging.FileHandler(self._current_log_path, encoding='utf-8')
        file_handler.setLevel(logging.DEBUG)
        file_formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
        file_handler.setFormatter(file_formatter)

        # 控制台处理器：仅显示 INFO 及以上，避免刷屏
        stream_handler = logging.StreamHandler()
        stream_handler.setLevel(logging.INFO)
        stream_formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
        stream_handler.setFormatter(stream_formatter)

        logger.addHandler(file_handler)
        logger.addHandler(stream_handler)

    def create_log_entry(self, level, message, port=None, raw_data=None):
        """创建日志条目"""
        if level not in self._level_icons:
            level = "INFO"

        formatted_message = f"{self._level_icons[level]} "
        if port:
            formatted_message += f"[{port}] "
        formatted_message += message

        if level == "INFO":
            logging.info(formatted_message)
        elif level == "WARNING":
            logging.warning(formatted_message)
        elif level == "ERROR":
            logging.error(formatted_message)
        elif level == "DEBUG":
            logging.debug(formatted_message)

        return formatted_message
    def create_log_export(self, content, export_filename=None):
        """创建日志导出文件"""
        if not export_filename:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            export_filename = f"log_export_{timestamp}.txt"

        try:
            with open(export_filename, 'w', encoding='utf-8') as file:
                file.write(content)
            return True, export_filename
        except Exception as error:
            return False, str(error)

    def export_log_with_info(self, filename):
        """导出日志并添加导出信息"""
        content = self.read_log_content()
        if not content.strip():
            return False, "没有日志内容可导出"

        success, result = self.create_log_export(content, filename)
        if not success:
            return False, result

        try:
            with open(filename, 'a', encoding='utf-8') as file:
                export_info = (
                    f"\n\n{'='*60}\n"
                    f"日志导出时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n"
                    f"导出文件: {filename}\n"
                    f"日志记录数: {len(content.splitlines())} 行\n"
                    f"{'='*60}"
                )
                file.write(export_info)
            return True, filename
        except Exception as error:
            return False, str(error)

    # ==================== 读取/查询 (READ) ====================

    def read_log_content(self):
        """读取当前日志内容"""
        try:
            if self._current_log_path and os.path.exists(self._current_log_path):
                with open(self._current_log_path, 'r', encoding='utf-8') as file:
                    return file.read()
            return "暂无日志内容"
        except Exception as error:
            return f"读取日志失败: {error}"

    def get_log_file_path(self):
        """获取当前日志文件路径"""
        return self._current_log_path

    def get_log_directory(self):
        """获取日志目录路径"""
        return self._log_directory

    def get_log_file_size(self):
        """获取当前日志文件大小"""
        try:
            if self._current_log_path and os.path.exists(self._current_log_path):
                return os.path.getsize(self._current_log_path)
            return 0
        except Exception:
            return 0

    def has_log_content(self):
        """检查是否有日志内容"""
        content = self.read_log_content()
        return bool(content and content.strip() and content != "暂无日志内容")

    # ==================== 更新/修改 (UPDATE) ====================

    # def update_log_level(self, new_level):
    #     """更新日志级别"""
    #     level_map = {
    #         "DEBUG": logging.DEBUG,
    #         "INFO": logging.INFO,
    #         "WARNING": logging.WARNING,
    #         "ERROR": logging.ERROR,
    #         "CRITICAL": logging.CRITICAL
    #     }
    #
    #     if new_level.upper() in level_map:
    #         logging.getLogger().setLevel(level_map[new_level.upper()])
    #         return True
    #     return False

    # def append_to_log(self, content):
    #     """追加内容到当前日志文件"""
    #     try:
    #         if self._current_log_path and os.path.exists(self._current_log_path):
    #             with open(self._current_log_path, 'a', encoding='utf-8') as file:
    #                 file.write(content + '\n')
    #             return True
    #         return False
    #     except Exception:
    #         return False

    # ==================== 删除/清除 (DELETE) ====================

    def delete_log_content(self):
        """清除当前日志文件内容"""
        try:
            if self._current_log_path and os.path.exists(self._current_log_path):
                with open(self._current_log_path, 'w', encoding='utf-8') as file:
                    file.write("")
                return True
            return False
        except Exception:
            return False

    # def delete_old_logs(self, days_to_keep=30):
    #     """删除指定天数之前的日志文件"""
    #     try:
    #         import time
    #         current_time = time.time()
    #         cutoff_time = current_time - (days_to_keep * 86400)
    #
    #         for filename in os.listdir(self._log_directory):
    #             file_path = os.path.join(self._log_directory, filename)
    #             if os.path.isfile(file_path) and filename.endswith('.log'):
    #                 file_mod_time = os.path.getmtime(file_path)
    #                 if file_mod_time < cutoff_time:
    #                     os.remove(file_path)
    #         return True
    #     except Exception:
    #         return False

# 在模块底部创建单例实例
default_log_manager = LogManager()