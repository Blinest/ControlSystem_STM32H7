# ui/widgets.py
from PyQt5.QtWidgets import (QPushButton, QGraphicsDropShadowEffect, QDialog, QVBoxLayout, QHBoxLayout,
                             QLabel, QFrame, QWidget)
from PyQt5.QtCore import Qt, QPoint
from PyQt5.QtGui import QColor, QMouseEvent

class AnimatedButton(QPushButton):
    def __init__(self, text, normal_color, hover_color):
        super().__init__(text)
        self.normal_color = normal_color
        self.hover_color = hover_color
        self.update_style(self.normal_color)   # 初始样式
        self._init_shadow()

    def update_style(self, color):
        """更新按钮的背景色，保留其他样式"""
        self.setStyleSheet(f"""
            QPushButton {{
                background-color: {color};
                border-radius: 6px;
                padding: 8px 16px;
                margin: 0px 0px 0px 0px;
                font-weight: bold;
                color: white;
                border: none;
            }}
        """)

    def _init_shadow(self):
        shadow = QGraphicsDropShadowEffect(self)
        shadow.setBlurRadius(10)
        shadow.setOffset(0, 0)
        shadow.setColor(Qt.gray)
        self.setGraphicsEffect(shadow)

    def _get_shadow(self):
        effect = self.graphicsEffect()
        if effect is None or not isinstance(effect, QGraphicsDropShadowEffect):
            self._init_shadow()
            effect = self.graphicsEffect()
        return effect

    def enterEvent(self, event):
        # 改变背景色为 hover_color
        self.update_style(self.hover_color)
        # 阴影浮起效果
        shadow = self._get_shadow()
        shadow.setOffset(0, 3)        # 向下偏移3像素（原代码中写的是10,10，建议改为(0,3)更自然）
        shadow.setBlurRadius(15)
        super().enterEvent(event)

    def leaveEvent(self, event):
        # 恢复背景色为 normal_color
        self.update_style(self.normal_color)
        # 阴影恢复
        shadow = self._get_shadow()
        shadow.setOffset(0, 0)
        shadow.setBlurRadius(10)
        super().leaveEvent(event)
    def set_normal_color(self, color):
        self.normal_color = color
        if not self.underMouse():  # 如果鼠标不在按钮上，立即更新
            self.update_style(color)

    def set_hover_color(self, color):
        self.hover_color = color
        if self.underMouse():
            self.update_style(color)
