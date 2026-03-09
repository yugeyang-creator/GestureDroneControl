# 体感手套控制无人机系统

## 项目简介

本项目实现了通过体感手套控制无人机飞行的功能。通过手套上的传感器获取用户手部姿态和动作数据，经过蓝牙传输到核心处理器，再通过姿态识别模型转换为飞行指令，最终通过 ESP-NOW 协议控制无人机飞行。

## 项目结构

```
GestureDroneControl/
├── lehand/             # 体感手套源代码
│   ├── I2Cdev.cpp      # I2C设备通信库
│   ├── I2Cdev.h
│   ├── LobotServoController.cpp  # 舵机控制器
│   ├── LobotServoController.h
│   ├── MPU6050.cpp     # MPU6050传感器库
│   ├── MPU6050.h
│   ├── MPU6050_6Axis_MotionApps20.h
│   ├── MPU6050_9Axis_MotionApps41.h
│   ├── README.md
│   ├── helper_3dmath.h
│   ├── lehand.ino      # 体感手套主程序
│   └── LICENSE
├── JoyStick/           # Cores3处理器代码
│   ├── ESP_NOW/        # ESP-NOW通信库
│   ├── M5CoreS3/       # M5CoreS3开发板库
│   ├── MicroTFLite/    # 微型TensorFlow Lite库
│   ├── BluetoothDataParser.cpp  # 蓝牙数据解析器
│   ├── BluetoothDataParser.h
│   ├── JoyStick.h
│   ├── JoyStick.ino    # 核心处理器主程序
│   └── LICENSE
├── zitaimoxing/        # 姿态模型训练源码
│   ├── convert_model_for_micro.py  # 模型转换脚本
│   ├── get_data.ino    # 数据获取示例
│   ├── model.h         # 转换后的模型C数组
│   ├── motion_classifier.py  # 模型训练脚本
│   ├── motion_model.h5  # 训练后的Keras模型
│   ├── motion_model_micro.tflite  # 适合微控制器的TFLite模型
│   ├── raw_data.txt    # 原始采集数据
│   ├── requirements.txt  # Python依赖
│   ├── samples.txt     # 训练样本
│   ├── zitaimoxing.ino  # 模型测试程序
│   ├── LICENSE
│   └── README.md
├── README.md           # 项目总说明
└── LICENSE             # 项目许可证
```

## 系统组成

### 1. 体感手套 (lehand)
- **功能**：获取手部姿态和动作数据，通过蓝牙发送到核心处理器
- **硬件**：MPU6050传感器（获取倾角数据）、电位器（检测手指弯曲程度）、蓝牙模块
- **工作原理**：
  - MPU6050传感器采集手部倾角数据
  - 电位器采集手指弯曲程度
  - 数据通过蓝牙发送到Cores3处理器

### 2. 核心处理器 (JoyStick)
- **功能**：接收并处理体感手套发送的蓝牙数据，运行姿态识别模型，生成飞行指令，通过ESP-NOW与无人机通信
- **硬件**：M5CoreS3开发板
- **工作原理**：
  - 接收蓝牙数据帧
  - 运行TensorFlow Lite模型进行姿态识别
  - 转换为无人机飞行指令
  - 通过ESP-NOW协议发送到无人机

### 3. 姿态模型 (zitaimoxing)
- **功能**：分析倾角值，输出移动指令
- **技术**：基于TensorFlow Lite的深度学习模型
- **工作原理**：
  - 使用滑动窗口技术处理时间序列数据
  - 通过一维卷积神经网络进行姿态分类
  - 输出前倾、后倾、左倾、右倾、不变等姿态指令

## 硬件要求

### 体感手套
- Arduino开发板
- MPU6050传感器
- 5个电位器（用于检测手指弯曲）
- 蓝牙模块（如HC-05/HC-06）
- 按键、LED指示灯

### 核心处理器
- M5CoreS3开发板
- 蓝牙模块（与手套配对）

### 无人机
- 支持ESP-NOW协议的无人机（如M5Stamp Fly）

## 软件要求

- Arduino IDE
- TensorFlow Lite for Microcontrollers库
- ESP32核心库
- Python 3.8+（用于模型训练）
- 必要的Python依赖包（见zitaimoxing/requirements.txt）

## 安装步骤

### 1. 体感手套设置
1. 打开Arduino IDE
2. 加载lehand/lehand.ino文件
3. 安装必要的库：MPU6050、Wire、SoftwareSerial
4. 编译并上传代码到Arduino开发板
5. 连接硬件：MPU6050传感器、电位器、蓝牙模块

### 2. 核心处理器设置
1. 打开Arduino IDE
2. 加载JoyStick/JoyStick.ino文件
3. 安装必要的库：M5CoreS3、TensorFlow Lite for Microcontrollers、ESP-NOW
4. 编译并上传代码到M5CoreS3开发板

### 3. 模型训练（可选）
1. 进入zitaimoxing目录
2. 安装Python依赖：`pip install -r requirements.txt`
3. 运行数据采集和训练脚本：`python motion_classifier.py`
4. 按照提示采集数据并训练模型
5. 生成的模型会自动转换为适合微控制器的格式

## 使用方法

1. **硬件连接**：
   - 连接体感手套的蓝牙模块与核心处理器的蓝牙模块
   - 确保核心处理器与无人机通过ESP-NOW配对

2. **启动系统**：
   - 开启体感手套电源
   - 开启核心处理器电源
   - 开启无人机电源

3. **控制指令**：
   - **起飞/降落**：按动手套上的按键
   - **上升/下降**：通过拇指的弯曲程度控制
   - **左转/右转**：通过其他四指的弯曲程度控制
   - **前后左右移动**：通过手部倾斜控制

4. **姿态识别**：
   - 系统会实时分析手部姿态
   - 通过TensorFlow Lite模型识别手部倾斜方向
   - 自动转换为无人机飞行指令

## 工作原理

1. **数据采集**：
   - 体感手套通过MPU6050传感器采集手部倾角数据
   - 通过电位器采集手指弯曲程度
   - 数据通过蓝牙发送到核心处理器

2. **数据处理**：
   - 核心处理器接收蓝牙数据帧
   - 解析手部倾角和手指弯曲数据
   - 使用滑动窗口技术处理时间序列数据

3. **姿态识别**：
   - 运行TensorFlow Lite模型进行姿态分类
   - 识别手部倾斜方向（前倾、后倾、左倾、右倾、不变）

4. **指令生成**：
   - 根据姿态识别结果生成无人机飞行指令
   - 包括油门、方向、姿态等控制参数

5. **通信控制**：
   - 通过ESP-NOW协议将指令发送到无人机
   - 实时更新飞行状态

## 架构

严格遵循物联网设计“感知+通信+处理+执行”四层架构

## 故障排除

- **蓝牙连接问题**：检查蓝牙模块接线是否正确，确保波特率设置为9600
- **模型推理失败**：确保使用的是转换后的模型文件
- **数据解析错误**：确保手套发送的数据格式正确
- **ESP-NOW配对失败**：检查无人机是否开启，尝试重新配对

## 未来方向

- 优化模型，以精准识别姿态信息
- 本项目指定无人机处于自动高度模式与角度控制模式（适合于无人机操控经验较少人员），但m5stack无人机还支持手动高度与角速度控制模式（需要一定基础），可以对这些模式加以扩展
- 本项目思想与架构适合于其他无人机控制，可以考虑更换无人机，但需考虑通信协议变化
- 核心处理器和体感手套可以集成，这样可以有效避免蓝牙传输不稳定问题
- 体感手套集成的姿态传感器为MPU6050，仅能较精准识别x、y倾角值，z倾角值存在较大零漂误差，不可用；可以考虑更换为MPU9050（含磁力计）
- 理论上，手指握紧程度对于无人机不同的变化速率；但出于操控难度与模型训练时间成本考虑，本项目移动/高度/偏航均设定为额定值
- 无人机室外飞行到一定高度可能出现失控现象，可能由于通信/硬件/操控等原因
- 本项目遵循物联网设计“感知+通信+处理+执行”四层架构，提供了体感控制物体的范式；理论上体感手套可以换成脑机接口，需改动数据处理与模型识别部分


## 许可证

本项目采用 MIT 许可证，详见 LICENSE 文件。

## 作者

- 于歌洋
- Kouhei Ito
- M5Stack Technology CO LTD

## 致谢

感谢所有为项目做出贡献的开发者和支持者。
