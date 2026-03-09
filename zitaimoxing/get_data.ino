/**
 * DX-BT24 Bluetooth Module Data Receiver
 * 适用于 ESP32-C3 (Arduino框架)
 * 功能：初始化后，持续监听蓝牙模块。当蓝牙被连接并接收到数据时，将数据帧打印到串口监视器。
 * 基于《DX-BT24蓝牙串口通信模块用户手册》
 */

// 定义DX-BT24模块连接的串口引脚 (请根据实际接线修改)
#define BLUETOOTH_RX_PIN 44  // ESP32-C3的GPIO4 连接 BT24的TX
#define BLUETOOTH_TX_PIN 43  // ESP32-C3的GPIO5 连接 BT24的RX

// 创建与蓝牙模块通信的硬件串口实例 (使用UART1)
HardwareSerial BTSerial(1);

// 定义缓冲区相关常量与变量
const int UNIT_SIZE = 8; // 固定的数据单位大小（字节）
uint8_t dataBuffer[UNIT_SIZE]; // 用于存储一个单位数据的缓冲区
int bufferIndex = 0; // 当前缓冲区中的字节位置
unsigned long packetCounter = 0; // 数据包计数器，用于编号

void setup() {
  // 初始化用于调试信息输出的串口 (波特率115200)
  Serial.begin(115200);

  // 初始化与DX-BT24通信的硬件串口
  // DX-BT24默认串口参数: 9600, 8, N, 1 (手册3.1.8节，默认波特率9600)
  BTSerial.begin(9600, SERIAL_8N1, BLUETOOTH_RX_PIN, BLUETOOTH_TX_PIN);
  
  Serial.println("==========================================");
  Serial.println("DX-BT24 Bluetooth Data Receiver Started");
  Serial.println("Waiting for Bluetooth connection and data...");
  Serial.println("==========================================\n");
  
  // 可选：发送测试指令确认模块通讯正常（模块未被连接时处于命令模式）
  // delay(1000);
  // BTSerial.print("AT\r\n"); // 发送测试指令
  // delay(100);
  // printBTSerialResponse(); // 打印响应
}

void loop() {
  // 持续检查蓝牙串口是否有新字节到达
  while (BTSerial.available() > 0) {
    // 读取一个字节放入缓冲区
    dataBuffer[bufferIndex] = BTSerial.read();
    bufferIndex++;

    // 当缓冲区存满8个字节时，打印这一组数据
    if (bufferIndex >= UNIT_SIZE) {
      printDataUnit();
      // 重置缓冲区和索引，准备接收下一组5字节
      bufferIndex = 0;
      packetCounter++;
    }
  }
  delay(10); // 短暂延时，避免过于频繁的循环
}

/**
 * 辅助函数：打印从蓝牙模块串口接收到的响应（用于AT命令模式）
 * 在透传模式下，接收到的数据是应用数据，而非AT响应。
 */
void printDataUnit() {
  // 打印序号
  Serial.print("[");
  Serial.print(packetCounter);
  Serial.print("] HEX: ");

  // 打印十六进制值，每个字节两位，用空格分隔
  for (int i = 0; i < UNIT_SIZE; i++) {
    if (dataBuffer[i] < 0x10) Serial.print('0'); // 补零
    Serial.print(dataBuffer[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}