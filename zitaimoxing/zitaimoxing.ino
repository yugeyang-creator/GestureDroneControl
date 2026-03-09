#include <TensorFlowLite.h>
#include "model.h"  // 包含转换后的模型

// 定义DX-BT24模块连接的串口引脚
#define BLUETOOTH_RX_PIN 44  // ESP32-C3的GPIO4 连接 BT24的TX
#define BLUETOOTH_TX_PIN 43  // ESP32-C3的GPIO5 连接 BT24的RX

// 创建与蓝牙模块通信的硬件串口实例 (使用UART1)
HardwareSerial BTSerial(1);

// 定义缓冲区相关常量与变量
const int UNIT_SIZE = 8; // 固定的数据单位大小（字节）
uint8_t dataBuffer[UNIT_SIZE]; // 用于存储一个单位数据的缓冲区
int bufferIndex = 0; // 当前缓冲区中的字节位置

// 滑动窗口配置
const int WINDOW_SIZE = 10;
float dataWindow[WINDOW_SIZE][2];  // 存储x、y倾角值
int windowIndex = 0;
bool windowFilled = false;

// 状态标签
const char* LABELS[] = {"前倾", "后倾", "左倾", "右倾", "不变"};

// TensorFlow Lite相关
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;
constexpr int kTensorArenaSize = 16 * 1024;  // 增加内存分配
uint8_t tensor_arena[kTensorArenaSize];

void setup() {
  // 初始化串口
  Serial.begin(115200);  // Cores3使用更高的波特率
  delay(1000);
  Serial.println("Cores3 倾角状态检测");
  
  // 初始化与DX-BT24通信的硬件串口
  // DX-BT24默认串口参数: 9600, 8, N, 1
  BTSerial.begin(9600, SERIAL_8N1, BLUETOOTH_RX_PIN, BLUETOOTH_TX_PIN);
  Serial.println("DX-BT24蓝牙模块初始化成功，等待连接...");
  
  // 初始化TensorFlow Lite
  setupTensorFlow();
}

void setupTensorFlow() {
  // 加载模型
  static tflite::MicroMutableOpResolver<7> resolver;
  resolver.AddConv2D();
  resolver.AddMaxPool2D();
  resolver.AddFullyConnected();
  resolver.AddReshape();
  resolver.AddSoftmax();
  resolver.AddQuantize();
  resolver.AddExpandDims();
  
  static tflite::MicroInterpreter static_interpreter(
    tflite::GetModel(g_model), resolver, tensor_arena, kTensorArenaSize);
  interpreter = &static_interpreter;
  
  // 分配内存
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    Serial.println("模型分配失败");
    return;
  }
  
  // 获取输入输出张量
  input = interpreter->input(0);
  output = interpreter->output(0);
  
  Serial.println("模型加载成功");
}

void loop() {
  // 持续检查蓝牙串口是否有新字节到达
  while (BTSerial.available() > 0) {
    // 读取一个字节放入缓冲区
    dataBuffer[bufferIndex] = BTSerial.read();
    bufferIndex++;

    // 当缓冲区存满8个字节时，处理这一组数据
    if (bufferIndex >= UNIT_SIZE) {
      processBluetoothData();
      // 重置缓冲区和索引，准备接收下一组数据
      bufferIndex = 0;
    }
  }
  delay(10); // 短暂延时，避免过于频繁的循环
}

void processBluetoothData() {
  // 检查数据有效性
  if (dataBuffer[0] == 0x55 && dataBuffer[1] == 0x55) {
    // 提取x、y倾角值
    int x_low = dataBuffer[4];
    int x_high = dataBuffer[5];
    int y_low = dataBuffer[6];
    int y_high = dataBuffer[7];
    
    int16_t x_raw = (x_high << 8) | x_low;
    int16_t y_raw = (y_high << 8) | y_low;
    
    float x = x_raw / 10.0;
    float y = y_raw / 10.0;
    
    // 更新滑动窗口
    updateWindow(x, y);
    
    // 当窗口填满后进行预测
    if (windowFilled) {
      int prediction = predictState();
      // 输出结果
      Serial.print("x: ");
      Serial.print(x);
      Serial.print(", y: ");
      Serial.print(y);
      Serial.print(", 状态: ");
      Serial.println(LABELS[prediction]);
      
      // 同时通过蓝牙发送结果
      BTSerial.print("x: ");
      BTSerial.print(x);
      BTSerial.print(", y: ");
      BTSerial.print(y);
      BTSerial.print(", 状态: ");
      BTSerial.println(LABELS[prediction]);
    }
  }
}

void updateWindow(float x, float y) {
  // 更新滑动窗口
  dataWindow[windowIndex][0] = x;
  dataWindow[windowIndex][1] = y;
  
  windowIndex = (windowIndex + 1) % WINDOW_SIZE;
  
  // 检查窗口是否填满
  if (!windowFilled && windowIndex == 0) {
    windowFilled = true;
    Serial.println("滑动窗口已填满，开始预测");
    BTSerial.println("滑动窗口已填满，开始预测");
  }
}

int predictState() {
  // 准备输入数据
  for (int i = 0; i < WINDOW_SIZE; i++) {
    int inputIndex = (windowIndex + i) % WINDOW_SIZE;
    input->data.f[i * 2] = dataWindow[inputIndex][0];
    input->data.f[i * 2 + 1] = dataWindow[inputIndex][1];
  }
  
  // 运行推理
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    Serial.println("推理失败");
    BTSerial.println("推理失败");
    return 4;  // 返回"不变"状态
  }
  
  // 获取预测结果
  float maxProb = 0;
  int predictedClass = 4;
  
  for (int i = 0; i < 5; i++) {
    if (output->data.f[i] > maxProb) {
      maxProb = output->data.f[i];
      predictedClass = i;
    }
  }
  
  return predictedClass;
}