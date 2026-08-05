# OV5640 摄像头 + LCD 显示修复日志

## 项目概述
- **硬件平台**: STM32H743 (ControlSystem_STM32H7)
- **摄像头**: OV5640 (DCMI 接口, RGB565, DVP 并行)
- **显示屏**: LCD SPI (ST7789-like, 240x240, RGB565)
- **RTOS**: FreeRTOS (CMSIS-RTOS v2)

---

## 问题 #1: HAL_Delay 卡死 (TIM17 不触发)

**现象**: 调用 `HAL_Delay()` 后系统卡死，`uwTick` 不递增。

**根因**: TIM17 的 D2 AXI-AHB 桥写缓冲问题，`HAL_TIM_Base_Start_IT()` 写入 CEN 位后未及时传播到硬件。

**修复**:
- **文件**: `Core/Src/stm32h7xx_hal_timebase_tim.c:92`
- **改动**: 在 `HAL_TIM_Base_Start_IT(&htim17)` 之后添加 `__DSB()` 内存屏障
  ```c
  HAL_StatusTypeDef status = HAL_TIM_Base_Start_IT(&htim17);
  __DSB(); /* Ensure TIM17 CEN write propagates through D2 AXI-AHB bridge */
  return status;
  ```

**状态**: `__DSB()` 添加后 HAL_Delay 仍不可用（根因未完全解决），改用 DWT 延时作为替代方案。

---

## 问题 #2: HAL_Delay 不可用的替代方案 — DWT 延时

**修复**:
- **文件**: `BSP/Camera/lcd_spi_154.c` / `BSP/Camera/lcd_spi_154.h`
- **改动**:
  1. 将 `static void LCD_DWT_Delay_ms()` 改为公开函数 `void LCD_DWT_Delay_ms(uint32_t ms)`
  2. 使用 DWT 周期计数器实现微秒级延时，不依赖中断
  3. 所有 LCD 初始化中的 `HAL_Delay()` 替换为 `LCD_DWT_Delay_ms()`
  4. `main.c` 中的 `HAL_Delay()` 同步替换

---

## 问题 #3: LCD 只显示红色，不显示绿色和蓝色

**现象**: RGB 全屏颜色测试中，仅红色生效。

**根因**:
1. SPI TXDR FIFO 打包规则：写入宽度必须与 DataSize 匹配
2. `LCD_ClearRect()` / `LCD_Clear()` 填充的是 `LCD.BackColor` 而非 `LCD.Color`

**修复**:
- **文件**: `BSP/Camera/lcd_spi_154.c`
- **改动**:
  1. `LCD_SPI_Transmit()` (line 1333): DataSize 感知的 TXDR 写入（8位/16位/32位三路分支）
  2. `LCD_SPI_TransmitBuffer()` (line 1473): 16位/32位两路分支
  3. `LCD_ClearRect()` / `LCD_Clear()`: 填充 `LCD.BackColor`

---

## 问题 #4: 黑屏 — 摄像头帧处理代码被注释

**现象**: 屏幕全黑，UART 输出到 "TASK START" 后无显示。

**根因**: `PeriphCtrlTask.c` 中摄像头帧检测和 LCD 显示的代码全部被注释掉。

**修复**:
- **文件**: `APP/Tasks/PeriphCtrlTask.c:50-58`
- **改动**: 取消注释帧处理代码块
  ```c
  if (OV5640_FrameState == 1) {
      OV5640_FrameState = 0;
      SCB_InvalidateDCache_by_Addr((uint32_t *)Camera_Buffer, Display_BufferSize*4);
      LCD_CopyBuffer(0,0,Display_Width,Display_Height, (uint16_t *)Camera_Buffer);
      LCD_DisplayString(84,200,"FPS:");
      LCD_DisplayNumber(132,200, OV5640_FPS,2);
      LED1_Toggle;
  }
  ```

---

## 问题 #5: D-Cache 一致性问题

**现象**: DMA 写入 Camera_Buffer (0x24000000, AXI SRAM, Cacheable) 后，CPU 读取到的是旧的缓存数据。

**根因**: `SCB_EnableDCache()` 开启后，AXI SRAM 被 D-Cache 缓存。DMA 写入不走 Cache，CPU 读取走 Cache，导致数据不一致。

**修复**: 在帧处理中添加 `SCB_InvalidateDCache_by_Addr()` 使 DMA 写入的缓存行失效
- **调用**: `SCB_InvalidateDCache_by_Addr((uint32_t *)Camera_Buffer, Display_BufferSize*4)`

---

## 问题 #6: 花屏 (Garbled Display)

**现象**: 摄像头图像显示为彩色噪点/条纹。

**根因**: VSYNC 极性不匹配。
- OV5640 寄存器 `0x4740 = 0x21`: Bit[4]=0 → VSYNC 实际为**低电平有效** (active LOW)
- DCMI 配置为 `DCMI_VSPOLARITY_HIGH`，导致在垂直消隐期间错误捕获

**修复**:
- **文件**: `Core/Src/dcmi.c:244` `MX_DCMI_OV5640_Init()`
- **改动**: `DCMI_VSPOLARITY_HIGH` → `DCMI_VSPOLARITY_LOW`
  ```c
  hdcmi.Init.VSPolarity = DCMI_VSPOLARITY_LOW;
  ```

---

## 问题 #7: 亮屏无图像 — 错误的 VSYNC 极性方向

**现象**: 问题 #6 中将 VSYNC 改为 HIGH 后，花屏消失但屏幕仅背光亮，无相机图像。

**诊断过程**:
1. 添加 UART 诊断输出，每2秒打印 DCMI 状态:
   ```
   DCMI state:2 FS:0 FC:0 FPS:0 EC:0
   ```
   - `state:2` = BUSY (DCMI 已启动，等待帧同步)
   - `FC:0` = 无帧捕获
2. 在 `OV5640_DMA_Transmit_Continuous()` 中添加前后状态打印:
   ```
   DMA_State:1 DCMI_State:1          // 启动前: READY
   Start_DMA rc:0 DCMI_State:2       // 启动后: BUSY, 返回 OK
   ```
3. 确认 DCMI 启动成功但 VSYNC 从未被检测到

**根因**: `DCMI_VSPOLARITY_HIGH` 导致 DCMI 在错误的边沿等待 VSYNC。OV5640 寄存器 `0x4740=0x21` 的实际含义是 VSYNC 低有效。**回退到 `DCMI_VSPOLARITY_LOW` 解决问题。**

**结论**: 花屏的根因不是 VSYNC 极性，而是 D-Cache 一致性问题（问题 #5）。在 D-Cache 修复之后，VSYNC_LOW 即可正确捕获并显示图像。

---

## 问题 #8: 任务中重复启动 DMA 导致捕获被破坏

**现象**: 移除 `OV5640_DMA_Transmit_Continuous` 从 task 的重复调用前，DCMI 状态显示 READY（未捕获）。

**根因**: `PeriphCtrlTask` 任务中再次调用 `OV5640_DMA_Transmit_Continuous()`，该函数内部调用 `HAL_DMA_Init()` 会禁用一个已在运行的 DMA 流，破坏 `main()` 中已启动的捕获。

**修复**:
- **文件**: `APP/Tasks/PeriphCtrlTask.c:32-33`
- **改动**: 删除 task 启动时的 `OV5640_DMA_Transmit_Continuous()` 调用
- **原因**: `main()` 在 RTOS 启动前已调用该函数启动 DMA/DCMI，任务无需重复启动

---

## 诊断增强

为快速定位 DCMI/DMA 问题，添加了以下诊断：

1. **dcmi.c**: 添加 `volatile uint32_t DCMI_FrameCountTotal` 帧计数器，在 `HAL_DCMI_FrameEventCallback` 中递增
2. **dcmi.h**: 导出 `DCMI_FrameCountTotal` 和 `DCMI_ErrorCode`
3. **PeriphCtrlTask.c**: 每 2 秒打印 DCMI 诊断信息 (State, FrameState, FrameCount, FPS, ErrorCode)
4. **dcmi_ov5640.c**: `OV5640_DMA_Transmit_Continuous()` 前后打印 DMA/DCMI 状态和启动返回值

---

## 修改文件清单

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| `Core/Src/dcmi.c` | 修改 | VSPolarity 修正为 LOW, 帧计数器, 诊断变量 |
| `Core/Inc/dcmi.h` | 修改 | 导出 DCMI_FrameCountTotal, DCMI_ErrorCode |
| `Core/Src/stm32h7xx_hal_timebase_tim.c` | 修改 | 添加 __DSB() 屏障 |
| `Core/Src/main.c` | 修改 | HAL_Delay→LCD_DWT_Delay_ms, 摄像头初始化流程 |
| `BSP/Camera/lcd_spi_154.c` | 修改 | DWT 延时公开, TXDR 宽度匹配, Clear 颜色修正 |
| `BSP/Camera/lcd_spi_154.h` | 修改 | LCD_DWT_Delay_ms 声明 |
| `BSP/Camera/dcmi_ov5640.c` | 修改 | GPIO 速度修复 (LOW→VERY_HIGH), 诊断打印 |
| `APP/Tasks/PeriphCtrlTask.c` | 修改 | 帧处理恢复, D-Cache 失效, 诊断打印, 去除重复 DMA 启动 |
| `Core/Src/stm32h7xx_it.c` | 无改动 | (DMA2_Stream7 ISR 已正确配置) |

---

---

## 问题 #9: 花屏 — GPIO 输出速度不足

**现象**: 摄像头图像显示为彩色噪点/花屏，帧已正确捕获（~44 FPS），FPS 文字也能正常显示在 LCD 上。

**诊断过程**:
1. 确认帧捕获正常（FC 递增，~44 FPS），说明 VSYNC 极性正确
2. TXT/FPS 文字能通过 LCD_CopyBuffer 路径正确显示，说明 LCD/SPI 显示管道正常
3. 与参考项目对比，确认 DCMI 极性配置一致（PCKPOLARITY_RISING, VSPOLARITY_LOW, HSPOLARITY_LOW）
4. 对比参考项目的 HAL_DCMI_MspInit()，发现关键差异：GPIO 输出速度
   - CubeMX 生成的代码将所有 DCMI 引脚配置为 `GPIO_SPEED_FREQ_LOW`
   - 参考项目所有 DCMI 引脚使用 `GPIO_SPEED_FREQ_VERY_HIGH`
   - OV5640 PCLK 频率约 80MHz，LOW 速度的 GPIO 无法可靠切换 → 数据位错误 → 花屏

**修复**:
- **文件**: `BSP/Camera/dcmi_ov5640.c:86-120` `DCMI_OV5640_Init()`
- **改动**: 在 `MX_DCMI_OV5640_Init()` 之后添加 GPIO 速度重配置代码，将所有 DCMI 引脚（PA4/PA6/PC6/PC7/PD3/PE4/PE5/PE6/PG9/PG10/PG11）的 Speed 设为 `GPIO_SPEED_FREQ_VERY_HIGH`
- **注意**: 不在 dcmi.c 中直接修改（用户要求）

---

## 最终工作状态

- LCD 显示: 正常 (RGB 三色测试通过)
- OV5640 相机: 正常捕获，帧率 ~44 FPS (240x240 RGB565)
- DCMI 配置: 硬件同步模式, VSYNC 低有效, HSYNC 低有效, PCLK 上升沿
- DCMI GPIO: 所有引脚 VERY_HIGH 速度
- DMA: DMA2_Stream7, 循环模式, FIFO 使能
- D-Cache: 帧处理后正确失效缓存行
- HAL_Delay: 不可用（TIM17 问题未完全解决），使用 DWT 延时
