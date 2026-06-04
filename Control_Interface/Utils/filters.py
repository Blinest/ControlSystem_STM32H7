# Core/filters.py
"""
滤波算法工具类
提供中值滤波、限幅滤波以及针对电机/IMU数据的组合滤波功能
"""

class FilterProcessor:
    """滤波处理器（静态方法集合）"""

    @staticmethod
    def median_filter(value_queue, new_value, window_size):
        """
        中值滤波：维护一个定长队列，返回队列的中位数

        Args:
            value_queue: list，用于存储历史值的队列（会被修改）
            new_value: float，新采集的值
            window_size: int，滤波窗口大小（建议奇数）

        Returns:
            float: 滤波后的值
        """
        value_queue.append(new_value)
        if len(value_queue) > window_size:
            value_queue.pop(0)
        # 窗口未满时直接返回原始值
        if len(value_queue) < window_size:
            return new_value
        sorted_vals = sorted(value_queue)
        return sorted_vals[len(sorted_vals) // 2]

    @staticmethod
    def rate_limit_filter(old_value, new_value, max_change):
        """
        限幅滤波：如果变化超过阈值，用旧值+限幅值代替

        Args:
            old_value: float，上一次滤波后的值
            new_value: float，新采集的值
            max_change: float，单次允许的最大变化量

        Returns:
            float: 限幅后的值
        """
        diff = new_value - old_value
        if abs(diff) > max_change:
            # 限制变化率，保留变化方向
            return old_value + (max_change if diff > 0 else -max_change)
        return new_value

    @staticmethod
    def apply_to_motor(motor_index, raw_pos, raw_vel, raw_acc,
                       motor_data, motor_filter_buffers,
                       max_change_rate, filter_window_size):
        """
        对单个电机的三个量分别进行限幅+中值滤波

        Args:
            motor_index: int，电机索引
            raw_pos, raw_vel, raw_acc: float，原始位移、速度、加速度
            motor_data: list，存储各电机当前滤波后数据 [[pos,vel,acc], ...]
            motor_filter_buffers: list，每个电机的三个量各自的历史值队列
            max_change_rate: dict，各物理量的最大变化率 {'pos':, 'vel':, 'acc':}
            filter_window_size: int，中值滤波窗口大小

        Returns:
            tuple: (filtered_pos, filtered_vel, filtered_acc)
        """
        # 获取上次滤波后的值（若无则用原始值）
        if motor_index < len(motor_data):
            last_pos, last_vel, last_acc = motor_data[motor_index]
        else:
            last_pos, last_vel, last_acc = raw_pos, raw_vel, raw_acc

        # 限幅
        limited_pos = FilterProcessor.rate_limit_filter(last_pos, raw_pos, max_change_rate['pos'])
        limited_vel = FilterProcessor.rate_limit_filter(last_vel, raw_vel, max_change_rate['vel'])
        limited_acc = FilterProcessor.rate_limit_filter(last_acc, raw_acc, max_change_rate['acc'])

        # 中值滤波
        if motor_index < len(motor_filter_buffers):
            queues = motor_filter_buffers[motor_index]  # queues[0]=pos, [1]=vel, [2]=acc
            filtered_pos = FilterProcessor.median_filter(queues[0], limited_pos, filter_window_size)
            filtered_vel = FilterProcessor.median_filter(queues[1], limited_vel, filter_window_size)
            filtered_acc = FilterProcessor.median_filter(queues[2], limited_acc, filter_window_size)
        else:
            filtered_pos, filtered_vel, filtered_acc = limited_pos, limited_vel, limited_acc

        return filtered_pos, filtered_vel, filtered_acc

    @staticmethod
    def apply_to_sensor(sensor_index, raw_pitch, raw_roll, raw_yaw,
                        sensor_data, sensor_filter_buffers,
                        max_change_rate_angle, filter_window_size):
        """
        对IMU的三个角度进行限幅+中值滤波

        Args:
            sensor_index: int，传感器索引
            raw_pitch, raw_roll, raw_yaw: float，原始俯仰、横滚、偏航角
            sensor_data: list，存储各传感器当前滤波后数据 [[pitch,roll,yaw], ...]
            sensor_filter_buffers: list，每个传感器的三个量各自的历史值队列
            max_change_rate_angle: float，角度最大变化率 (deg/100ms)
            filter_window_size: int，中值滤波窗口大小

        Returns:
            tuple: (filtered_pitch, filtered_roll, filtered_yaw)
        """
        # 获取上次滤波后的值
        if sensor_index < len(sensor_data):
            last_pitch, last_roll, last_yaw = sensor_data[sensor_index]
        else:
            last_pitch, last_roll, last_yaw = raw_pitch, raw_roll, raw_yaw

        # 限幅
        limited_pitch = FilterProcessor.rate_limit_filter(last_pitch, raw_pitch, max_change_rate_angle)
        limited_roll = FilterProcessor.rate_limit_filter(last_roll, raw_roll, max_change_rate_angle)
        limited_yaw = FilterProcessor.rate_limit_filter(last_yaw, raw_yaw, max_change_rate_angle)

        # 中值滤波
        if sensor_index < len(sensor_filter_buffers):
            queues = sensor_filter_buffers[sensor_index]  # queues[0]=pitch, [1]=roll, [2]=yaw
            filtered_pitch = FilterProcessor.median_filter(queues[0], limited_pitch, filter_window_size)
            filtered_roll = FilterProcessor.median_filter(queues[1], limited_roll, filter_window_size)
            filtered_yaw = FilterProcessor.median_filter(queues[2], limited_yaw, filter_window_size)
        else:
            filtered_pitch, filtered_roll, filtered_yaw = limited_pitch, limited_roll, limited_yaw

        return filtered_pitch, filtered_roll, filtered_yaw