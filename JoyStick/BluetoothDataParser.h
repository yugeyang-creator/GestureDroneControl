#ifndef BluetoothDataParser_h
#define BluetoothDataParser_h

#include <Arduino.h>
#include <MicroTFLite.h>

// 定义缓冲区相关常量
#define UNIT_SIZE 11 // 固定的数据单位大小（字节，含 CRC 校验位）
#define WINDOW_SIZE 10 // 滑动窗口大小

class BluetoothDataParser {
private:
  // 蓝牙串口配置
  HardwareSerial* _btSerial;
  int _rxPin;
  int _txPin;
  
  // 缓冲区相关变量
  uint8_t _dataBuffer[UNIT_SIZE];
  int _bufferIndex;
  
  // 滑动窗口配置
  float _dataWindow[WINDOW_SIZE][2];
  int _windowIndex;
  bool _windowFilled;
  
  // TensorFlow Lite相关
  tflite::MicroInterpreter* _interpreter;
  TfLiteTensor* _input;
  TfLiteTensor* _output;
  const uint8_t* _model;
  uint8_t* _tensorArena;
  size_t _tensorArenaSize;
  
  // 状态标签
  const char* _LABELS[5];
  
  // 新数据标志
  bool _hasNewData;
  
  // 存储最新的指令码
  uint8_t _throttleCommand;
  uint8_t _steeringCommand;
  
  // 私有方法
  void updateWindow(float x, float y);
  int predictState();
  
public:
  // 构造函数
  BluetoothDataParser(int rxPin, int txPin, const uint8_t* model, uint8_t* tensorArena, size_t tensorArenaSize);
  
  // 初始化方法
  void begin(unsigned long baudRate = 9600);
  void setupTensorFlow();
  
  // 主处理方法
  bool available();
  void processData();
  
  // 获取操作码的方法
  uint8_t getMoveCommand();      // 获取移动指令（模型推断）
  uint8_t getThrottleCommand();  // 获取油门指令（低位操作指令）
  uint8_t getSteeringCommand();  // 获取转向指令（高位操作指令）
  
  // 状态方法
  bool isWindowFilled();
  bool hasNewData();
  void resetNewDataFlag();
};

#endif