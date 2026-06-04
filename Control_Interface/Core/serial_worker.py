import os
import sys
from PyQt5.QtCore import QThread, pyqtSignal
import serial
import serial.tools.list_ports

class SerialWorker(QThread):
    signal_data = pyqtSignal(bytes)
    signal_error = pyqtSignal(str)

    def __init__(self, port):
        super().__init__()
        self.port, self.baud, self.serial, self.is_running = port, 9600, serial.Serial(), False
        self._port_check_counter = 0  # 每 200 次循环检查一次端口状态（约 1 秒）

    def run(self):
        try:
            self.serial.port, self.serial.baudrate, self.serial.timeout = self.port, self.baud, 0.05
            self.serial.open()
            self.is_running = True
            print(f"串口 {self.port} 已打开，波特率 {self.baud}")
        except Exception as e:
            print(f"串口 {self.port} 打开失败: {e}")
            return
        while self.is_running:
            try:
                if self.serial.in_waiting > 0:
                    raw = self.serial.read(self.serial.in_waiting)
                    if raw:
                        self.signal_data.emit(raw)
                else:
                    self._port_check_counter += 1
                    if self._port_check_counter >= 200:
                        self._port_check_counter = 0
                        self._check_port_alive()
                    self.msleep(5)
            except Exception as e:
                print(f"串口读取错误: {e}")
                self.signal_error.emit(str(e))
                break
        if self.serial.is_open:
            self.serial.close()
        print(f"串口 {self.port} 已关闭")

    def _check_port_alive(self):
        """检测串口是否物理断开（低频调用，避免阻塞）"""
        try:
            if sys.platform == 'win32':
                active_ports = [p.device for p in serial.tools.list_ports.comports()]
                if self.port not in active_ports:
                    raise serial.SerialException(f"设备 {self.port} 已物理断开")
            else:
                if not os.path.exists(self.port):
                    raise serial.SerialException(f"设备 {self.port} 已物理断开")
        except serial.SerialException:
            raise
        except Exception:
            pass  # 检测本身出错不影响正常读取

    def send_data(self, data: bytes):
        try:
            if self.is_running and self.serial.is_open:
                self.serial.write(data)
                hex_str = ' '.join(f'{b:02X}' for b in data)
                print(f"发送成功: {hex_str}")
        except Exception as e:
            print(f"串口写入错误: {e}")
            self.signal_error.emit(str(e))

    def stop(self):
        self.is_running = False
        self.wait()
