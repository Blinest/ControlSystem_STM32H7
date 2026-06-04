# protocol.py
import struct
from collections import deque
from typing import List, Optional, Tuple

# ===================== 数据类 =====================
class MotorData:
    __slots__ = ('pos', 'vel', 'acc', 'status')
    def __init__(self, pos: float, vel: float, acc: float, status: int):
        self.pos = pos      # mm
        self.vel = vel      # mm/s
        self.acc = acc      # mm/s²
        self.status = status  # 0:停止, 1:运行

class SensorData:
    __slots__ = ('pitch', 'roll', 'yaw')
    def __init__(self, pitch: float, roll: float, yaw: float):
        self.pitch = pitch  # deg
        self.roll = roll    # deg
        self.yaw = yaw      # deg

class DeviceStatus:
    __slots__ = ('num_motors', 'num_sensors', 'motors', 'sensors',
                 'bend_angle1', 'bend_angle2', 'sys_state')
    def __init__(self, num_motors: int, num_sensors: int,
                 motors: List[MotorData], sensors: List[SensorData],
                 bend_angle1: float, bend_angle2: float, sys_state: int):
        self.num_motors = num_motors
        self.num_sensors = num_sensors
        self.motors = motors
        self.sensors = sensors
        self.bend_angle1 = bend_angle1
        self.bend_angle2 = bend_angle2
        self.sys_state = sys_state

# ===================== 滤波器 =====================
class DataFilter:
    """中值滤波 + 限幅滤波"""
    def __init__(self, window_size: int = 3, max_change_rate: dict = None):
        self.window_size = window_size
        self.max_change = max_change_rate or {
            'pos': 20.0, 'vel': 50.0, 'acc': 100.0, 'angle': 30.0
        }
        self._motor_buffers = []   # 每个电机 [pos_queue, vel_queue, acc_queue]
        self._sensor_buffers = []  # 每个传感器 [pitch_queue, roll_queue, yaw_queue]
        self._prev_motor = []      # 上一次滤波后的值，用于限幅
        self._prev_sensor = []

    def _median(self, queue: deque, new_val: float) -> float:
        queue.append(new_val)
        if len(queue) > self.window_size:
            queue.popleft()
        if len(queue) < self.window_size:
            return new_val
        sorted_vals = sorted(queue)
        return sorted_vals[len(sorted_vals)//2]

    def _limit(self, old: float, new: float, max_change: float) -> float:
        diff = new - old
        if abs(diff) > max_change:
            return old + (max_change if diff > 0 else -max_change)
        return new

    def apply_motor(self, idx: int, pos: float, vel: float, acc: float) -> Tuple[float, float, float]:
        while len(self._motor_buffers) <= idx:
            self._motor_buffers.append([deque(maxlen=self.window_size) for _ in range(3)])
            self._prev_motor.append([0.0, 0.0, 0.0])
        buffers = self._motor_buffers[idx]
        prev = self._prev_motor[idx]

        fpos = self._median(buffers[0], pos)
        fvel = self._median(buffers[1], vel)
        facc = self._median(buffers[2], acc)

        fpos = self._limit(prev[0], fpos, self.max_change['pos'])
        fvel = self._limit(prev[1], fvel, self.max_change['vel'])
        facc = self._limit(prev[2], facc, self.max_change['acc'])

        self._prev_motor[idx] = [fpos, fvel, facc]
        return fpos, fvel, facc

    def apply_sensor(self, idx: int, pitch: float, roll: float, yaw: float) -> Tuple[float, float, float]:
        while len(self._sensor_buffers) <= idx:
            self._sensor_buffers.append([deque(maxlen=self.window_size) for _ in range(3)])
            self._prev_sensor.append([0.0, 0.0, 0.0])
        buffers = self._sensor_buffers[idx]
        prev = self._prev_sensor[idx]

        fpitch = self._median(buffers[0], pitch)
        froll  = self._median(buffers[1], roll)
        fyaw   = self._median(buffers[2], yaw)

        max_angle = self.max_change['angle']
        fpitch = self._limit(prev[0], fpitch, max_angle)
        froll  = self._limit(prev[1], froll,  max_angle)
        fyaw   = self._limit(prev[2], fyaw,   max_angle)

        self._prev_sensor[idx] = [fpitch, froll, fyaw]
        return fpitch, froll, fyaw

    def reset(self):
        self._motor_buffers.clear()
        self._sensor_buffers.clear()
        self._prev_motor.clear()
        self._prev_sensor.clear()

# ===================== 协议解析器 =====================
class ProtocolParser:
    @staticmethod
    def parse_frame(frame: bytes, apply_filter: bool = False, filter_obj: DataFilter = None) -> Optional[DeviceStatus]:
        # 最小长度：头2 + 字节数1 + 电机数1 + 传感器数1 + 至少1个电机(7) + 弯曲2+2 + 状态1 + 数据长度1 + 校验1 = 19
        if len(frame) < 19 or frame[0] != 0xBB or frame[1] != 0x02:
            return None

        total_len = frame[2]          # 字节数（从电机数到系统状态）
        # 帧总长度应等于 total_len + 4 （头3字节+校验和1字节）
        if len(frame) != total_len + 4:
            return None

        num_m = frame[3]
        num_s = frame[4]

        # 计算从电机数到系统状态结束的字节数（不包含数据长度和校验和）
        expected_data_len = (1 + 1                     # 电机数+传感器数
                             + num_m * 7
                             + num_s * 6
                             + 2 + 2 + 1)              # 弯曲1+弯曲2+系统状态
        if total_len != expected_data_len:
            return None

        offset = 5   # 跳过 0xBB,0x02,total_len,num_m,num_s

        # 解析电机数据
        motors = []
        for _ in range(num_m):
            if offset + 7 > len(frame):
                break
            pos_raw, vel_raw, acc_raw = struct.unpack_from('>hhh', frame, offset)
            offset += 6
            status = frame[offset]
            offset += 1
            motors.append(MotorData(pos_raw/100.0, vel_raw/100.0, acc_raw/100.0, status))

        # 解析传感器数据
        sensors = []
        for _ in range(num_s):
            if offset + 6 > len(frame):
                break
            pitch_raw, roll_raw, yaw_raw = struct.unpack_from('>hhh', frame, offset)
            offset += 6
            sensors.append(SensorData(pitch_raw/100.0, roll_raw/100.0, yaw_raw/100.0))

        # 弯曲角度1 & 2
        if offset + 4 > len(frame):
            return None
        bend1_raw = struct.unpack_from('>h', frame, offset)[0]
        offset += 2
        bend2_raw = struct.unpack_from('>h', frame, offset)[0]
        offset += 2

        # 系统状态
        sys_state = frame[offset]
        offset += 1

        # 校验和已由调用方验证，无需处理

        # 滤波（可选）
        if apply_filter and filter_obj is not None:
            filtered_motors = []
            for i, m in enumerate(motors):
                fp, fv, fa = filter_obj.apply_motor(i, m.pos, m.vel, m.acc)
                filtered_motors.append(MotorData(fp, fv, fa, m.status))
            motors = filtered_motors

            filtered_sensors = []
            for i, s in enumerate(sensors):
                fp, fr, fy = filter_obj.apply_sensor(i, s.pitch, s.roll, s.yaw)
                filtered_sensors.append(SensorData(fp, fr, fy))
            sensors = filtered_sensors

        return DeviceStatus(
            num_motors=num_m,
            num_sensors=num_s,
            motors=motors,
            sensors=sensors,
            bend_angle1=bend1_raw / 100.0,
            bend_angle2=bend2_raw / 100.0,
            sys_state=sys_state
        )