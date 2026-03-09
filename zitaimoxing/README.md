# 姿态检测系统（Zitaimoxing）

## 项目简介

本项目是一个基于 ESP32-C3 和 DX-BT24 蓝牙模块的姿态检测系统，使用 TensorFlow Lite 进行模型推理，能够实时检测设备的倾斜状态（前倾、后倾、左倾、右倾、不变）。

## 功能特性

- **实时姿态检测**：通过蓝牙接收倾角数据，实时分析设备姿态
- **TensorFlow Lite 推理**：使用轻量级深度学习模型进行姿态分类
- **滑动窗口技术**：使用 10 个时间步的数据进行预测，提高准确性
- **蓝牙通信**：使用 DX-BT24 蓝牙模块进行数据传输
- **多轮训练**：支持模型的多轮训练，包含早停机制防止过拟合

## 硬件要求

- ESP32-C3 开发板
- DX-BT24 蓝牙串口通信模块
- 倾角传感器（发送数据格式：8字节，前两字节为 0x55 0x55）

## 软件要求

- Arduino IDE
- TensorFlow Lite for Microcontrollers 库
- Python 3.8+
- 必要的 Python 依赖包（见 requirements.txt）

## 安装步骤

1. **安装 Python 依赖**：
   ```bash
   pip install -r requirements.txt
   ```

2. **安装 Arduino 库**：
   - TensorFlow Lite for Microcontrollers
   - ESP32 核心库

3. **硬件连接**：
   - DX-BT24 TX → ESP32-C3 GPIO4 (BLUETOOTH_RX_PIN)
   - DX-BT24 RX → ESP32-C3 GPIO5 (BLUETOOTH_TX_PIN)
   - DX-BT24 VCC → 3.3V
   - DX-BT24 GND → GND
   实际连接视情况而定。

## 使用方法

### 1. 数据采集与模型训练

1. 连接倾角传感器和蓝牙模块
2. 运行数据采集脚本：
   ```bash
   python motion_classifier.py
   ```
3. 按照提示进行数据采集（按 w:前倾, s:后倾, a:左倾, d:右倾, 松开:不变）
4. 脚本会自动训练模型并生成适合微控制器的版本

### 2. 硬件烧录

1. 打开 Arduino IDE
2. 加载 `zitaimoxing.ino` 文件
3. 选择正确的开发板型号和端口
4. 编译并上传代码到 ESP32-C3

### 3. 运行系统

1. 电源开启 ESP32-C3
2. 蓝牙模块会自动开始接收数据
3. 系统会实时分析姿态并通过串口和蓝牙发送结果

## 模型说明

- **输入**：10 个时间步的 x、y 倾角值（形状：10×2）
- **输出**：5 个姿态类别（前倾、后倾、左倾、右倾、不变）
- **模型结构**：一维卷积神经网络（Conv1D）

## 项目结构

```
zitaimoxing/
├── bluetooth_reader.py    # 蓝牙数据读取脚本
├── convert_to_c_array.py  # TFLite模型转C数组脚本
├── convert_model_for_micro.py  # 为微控制器优化模型脚本
├── get_data.ino          # 蓝牙数据获取示例
├── model.h               # 转换后的模型C数组
├── motion_classifier.py   # 模型训练和数据采集脚本
├── motion_model.h5        # 训练后的Keras模型
├── motion_model_micro.tflite  # 适合微控制器的TFLite模型
├── raw_data.txt          # 原始采集数据
├── requirements.txt      # Python依赖包
├── samples.txt           # 生成的训练样本
└── zitaimoxing.ino       # 主程序
```

## 故障排除

- **蓝牙连接问题**：检查蓝牙模块接线是否正确，确保波特率设置为9600
- **模型推理失败**：确保使用的是 `motion_model_micro.tflite` 模型，该模型不使用混合量化
- **数据解析错误**：确保倾角传感器发送的数据格式正确（8字节，前两字节为 0x55 0x55）

## 许可证

本项目采用 MIT 许可证，详见 LICENSE 文件。
