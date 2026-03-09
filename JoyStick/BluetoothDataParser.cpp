#include <util/crc16.h>

// CRC8 计算函数
uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc = _crc_ibutton_update(crc, data[i]);
    }
    return crc;
}

#include "BluetoothDataParser.h"

// 构造函数
BluetoothDataParser::BluetoothDataParser(int rxPin, int txPin, const uint8_t* model, uint8_t* tensorArena, size_t tensorArenaSize) {
  _rxPin = rxPin;
  _txPin = txPin;
  _model = model;
  _tensorArena = tensorArena;
  _tensorArenaSize = tensorArenaSize;
  
  _bufferIndex = 0;
  _windowIndex = 0;
  _windowFilled = false;
  _hasNewData = false;
  _throttleCommand = 0;
  _steeringCommand = 0;
  
  // 初始化状态标签
  _LABELS[0] = "前倾";
  _LABELS[1] = "后倾";
  _LABELS[2] = "左倾";
  _LABELS[3] = "右倾";
  _LABELS[4] = "不变";
  
  // 创建硬件串口实例
  _btSerial = new HardwareSerial(1);
}

// 初始化方法
void BluetoothDataParser::begin(unsigned long baudRate) {
  _btSerial->begin(baudRate, SERIAL_8N1, _rxPin, _txPin);
}

// 设置TensorFlow
void BluetoothDataParser::setupTensorFlow() {
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
    tflite::GetModel(_model), resolver, _tensorArena, _tensorArenaSize);
  _interpreter = &static_interpreter;
  
  // 分配内存
  TfLiteStatus allocate_status = _interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    Serial.println("模型分配失败");
    return;
  }
  
  // 获取输入输出张量
  _input = _interpreter->input(0);
  _output = _interpreter->output(0);
  
  Serial.println("模型加载成功");
}

// 检查是否有数据可用
bool BluetoothDataParser::available() {
  return _btSerial->available() > 0;
}

// 处理数据
void BluetoothDataParser::processData() {
  // 持续检查蓝牙串口是否有新字节到达
  while (_btSerial->available() > 0) {
    // 读取一个字节放入缓冲区
    _dataBuffer[_bufferIndex] = _btSerial->read();
    _bufferIndex++;

    // 当缓冲区存满8个字节时，处理这一组数据
    if (_bufferIndex >= UNIT_SIZE) {
      // 检查数据有效性（包含 CRC 校验）
      if (_dataBuffer[0] == 0x55 && _dataBuffer[1] == 0x55) {
        // 验证 CRC 校验位
        uint8_t received_crc = _dataBuffer[10];
        uint8_t calculated_crc = crc8(_dataBuffer, 10);
        
        if (received_crc != calculated_crc) {
          // CRC 校验失败，丢弃数据帧
          Serial.println("CRC 校验失败，丢弃数据帧");
          // 清除缓冲区并重置索引
          memset(_dataBuffer, 0, sizeof(_dataBuffer));
          _bufferIndex = 0;
          return;
        }
        
        // CRC 校验通过，处理数据
        // 提取 x、y 倾角值（大端顺序）
        int16_t x_raw = (_dataBuffer[6] << 8) | _dataBuffer[7];
        int16_t y_raw = (_dataBuffer[8] << 8) | _dataBuffer[9];
        
        float x = x_raw / 10.0;
        float y = y_raw / 10.0;
        
        // 更新滑动窗口
        updateWindow(x, y);
        
        // 打印原始数据
        Serial.printf("原始数据: ");
        for (int i = 0; i < UNIT_SIZE; i++) {
          Serial.printf("0x%02X ", _dataBuffer[i]);
        }
        Serial.println();
        
        // 保存指令码
        _throttleCommand = _dataBuffer[4];
        _steeringCommand = _dataBuffer[5];
        
        // 设置新数据标志
        _hasNewData = true;
      }
      
      // 清除缓冲区并重置索引，准备接收下一组数据
      memset(_dataBuffer, 0, sizeof(_dataBuffer));
      _bufferIndex = 0;
    }
  }
}

// 更新滑动窗口
void BluetoothDataParser::updateWindow(float x, float y) {
  // 更新滑动窗口
  _dataWindow[_windowIndex][0] = x;
  _dataWindow[_windowIndex][1] = y;
  
  _windowIndex = (_windowIndex + 1) % WINDOW_SIZE;
  
  // 检查窗口是否填满
  if (!_windowFilled && _windowIndex == 0) {
    _windowFilled = true;
    Serial.println("滑动窗口已填满，开始预测");
    _btSerial->println("滑动窗口已填满，开始预测");
  }
}

// 预测状态
int BluetoothDataParser::predictState() {
  // 准备输入数据
  for (int i = 0; i < WINDOW_SIZE; i++) {
    int inputIndex = (_windowIndex + i) % WINDOW_SIZE;
    _input->data.f[i * 2] = _dataWindow[inputIndex][0];
    _input->data.f[i * 2 + 1] = _dataWindow[inputIndex][1];
  }
  
  // 运行推理
  TfLiteStatus invoke_status = _interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    Serial.println("推理失败");
    _btSerial->println("推理失败");
    return 4;  // 返回"不变"状态
  }
  
  // 获取预测结果
  float maxProb = 0;
  int predictedClass = 4;
  
  for (int i = 0; i < 5; i++) {
    if (_output->data.f[i] > maxProb) {
      maxProb = _output->data.f[i];
      predictedClass = i;
    }
  }
  
  return predictedClass;
}

// 获取移动指令（模型推断）
uint8_t BluetoothDataParser::getMoveCommand() {
  if (!_windowFilled) {
    return 0x00; // 窗口未填满，返回默认值
  }
  
  int prediction = predictState();
  uint8_t moveCommand = 0;
  
  switch(prediction) {
    case 0: // 前倾
      moveCommand = 0x07;
      break;
    case 1: // 后倾
      moveCommand = 0x08;
      break;
    case 2: // 左倾
      moveCommand = 0x09;
      break;
    case 3: // 右倾
      moveCommand = 0x0A;
      break;
    default: // 不变
      moveCommand = 0x00;
      break;
  }
  
  return moveCommand;
}

// 获取油门指令（低位操作指令）
uint8_t BluetoothDataParser::getThrottleCommand() {
  return _throttleCommand;
}

// 获取转向指令（高位操作指令）
uint8_t BluetoothDataParser::getSteeringCommand() {
  return _steeringCommand;
}

// 检查窗口是否填满
bool BluetoothDataParser::isWindowFilled() {
  return _windowFilled;
}

// 检查是否有新数据
bool BluetoothDataParser::hasNewData() {
  return _hasNewData;
}

// 重置新数据标志
void BluetoothDataParser::resetNewDataFlag() {
  _hasNewData = false;
}