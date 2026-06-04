# controller.py
class PID:
    """PID 控制器，用于闭环控制"""
    def __init__(self, Kp=1.0, Ki=0.0, Kd=0.0, dt=0.05, output_limits=(None, None), integral_limits=(None, None)):
        self.Kp = Kp
        self.Ki = Ki
        self.Kd = Kd
        self.dt = dt
        self.output_limits = output_limits
        self.integral_limits = integral_limits
        self.reset()

    def reset(self):
        self.integral = 0.0
        self.prev_error = 0.0

    def update(self, setpoint, measurement):
        error = setpoint - measurement
        # 比例项
        P = self.Kp * error
        # 积分项（带限幅）
        self.integral += error * self.dt
        if self.integral_limits[0] is not None:
            self.integral = max(self.integral_limits[0], self.integral)
        if self.integral_limits[1] is not None:
            self.integral = min(self.integral_limits[1], self.integral)
        I = self.Ki * self.integral
        # 微分项
        derivative = (error - self.prev_error) / self.dt
        D = self.Kd * derivative
        # 输出
        output = P + I + D
        if self.output_limits[0] is not None:
            output = max(self.output_limits[0], output)
        if self.output_limits[1] is not None:
            output = min(self.output_limits[1], output)
        self.prev_error = error
        return output