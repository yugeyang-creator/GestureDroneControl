/*
@company: Hiwonder
@date:    2024-03-01
@version:  2.0
@description: 体感手套控制程序
*/

#include <SoftwareSerial.h> //软串口库
#include "LobotServoController.h" //机器人控制信号库
#include "MPU6050.h" //MPU6050库
#include "Wire.h" //IIC库

// 蓝牙TX、RX引脚
#define BTH_RX 11
#define BTH_TX 12

// 创建变位器最小值和最大值存储
float min_list[5] = {0, 0, 0, 0, 0};
float max_list[5] = {255, 255, 255, 255, 255};
// 各个手指读取到的数据变量
float sampling[5] = {0, 0, 0, 0, 0}; 
// 手指相关舵机变量
float data[5] = {1500, 1500, 1500, 1500, 1500};
//uint16_t ServePwm[5] = {1500, 1500, 1500, 1500, 1500};
//uint16_t ServoPwmSet[5] = {1500, 1500, 1500, 1500, 1500};
// 电位器校准标志位
bool turn_on = true;

// 蓝牙通讯串口初始化
SoftwareSerial Bth(BTH_RX, BTH_TX);
// 机器人控制对象
//LobotServoController lsc(Bth);

// K3按钮相关变量
int key_change_count = 0; // 记录key按钮状态变化的次数
bool key_state_changed = false; // 标记key按钮状态是否刚发生变化

// 浮点数映射函数
float float_map(float in, float left_in, float right_in, float left_out, float right_out)
{
  return (in - left_in) * (right_out - left_out) / (right_in - left_in) + left_out;
}

// MPU6050相关变量
MPU6050 accelgyro;
int16_t ax, ay, az;
int16_t gx, gy, gz;
float ax0 = 0, ay0 = 0, az0 = 0;
float gx0 = 0, gy0 = 0, gz0 = 0;
float ax1 = 0, ay1 = 0, az1 = 0;
float gx1 = 0, gy1 = 0, gz1 = 0;

// 加速度校准变量
int ax_offset, ay_offset, az_offset, gx_offset, gy_offset, gz_offset;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  //功能按键初始化
  pinMode(7, INPUT_PULLUP);
  //各手指电位器配置
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  pinMode(A6, INPUT);
  //LED 灯配置
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);

  //蓝牙配置
  Bth.begin(9600);
  Bth.print("AT+ROLE=M");  //蓝牙配置为主模式
  delay(100);
  Bth.print("AT+RESET");  //软重启蓝牙模块
  delay(250);

  //MPU6050 配置
  Wire.begin();
  Wire.setClock(20000);
  accelgyro.initialize();
  accelgyro.setFullScaleGyroRange(3); //设定角速度量程
  accelgyro.setFullScaleAccelRange(1); //设定加速度量程
  delay(200);
  accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);  //获取当前各轴数据以校准
  ax_offset = ax;  //X轴加速度校准数据
  ay_offset = ay;  //Y轴加速度校准数据
  az_offset = az - 8192;  //Z轴加速度校准数据
  gx_offset = gx; //X轴角速度校准数据
  gy_offset = gy; //Y轴角速度校准数据
  gz_offset = gz; //Z轴角速度校准数据
}

//获取各个手指电位器数据
void finger() {
  static uint32_t timer_sampling;
  static uint32_t timer_init;
  static uint8_t init_step = 0;
  if (timer_sampling <= millis())
  {
    for (int i = 14; i <= 18; i++)
    {
      if (i < 18)
        sampling[i - 14] += analogRead(i); //读取各个手指的数据
      else
        sampling[i - 14] += analogRead(A6);  //读取小拇指的数据， 因为IIC 用了 A4,A5 口，所以不能从A0 开始连续读取
      sampling[i - 14] = sampling[i - 14] / 2.0; //取上次和本次测量值的均值
      data[i - 14 ] = float_map( sampling[i - 14],min_list[i - 14], max_list[i - 14], 2500, 500); //将测量值映射到500-2500， 握紧手时为500， 张开时为2500
      data[i - 14] = data[i - 14] > 2500 ? 2500 : data[i - 14];  // 限制最大值为2500
      data[i - 14] = data[i - 14] < 500 ? 500 : data[ i - 14];   //限制最小值为500
    }
    timer_sampling = millis() + 10;
  }

  if (turn_on && timer_init < millis())
  {
    switch (init_step)
    {
      case 0:
        digitalWrite(2, LOW);
        digitalWrite(3, LOW);
        digitalWrite(4, LOW);
        digitalWrite(5, LOW);
        digitalWrite(6, LOW);
        timer_init = millis() + 20;
        init_step++;
        break;
      case 1:
        digitalWrite(2, HIGH);
        digitalWrite(3, HIGH);
        digitalWrite(4, HIGH);
        digitalWrite(5, HIGH);
        digitalWrite(6, HIGH);
        timer_init = millis() + 200;
        init_step++;
        break;
      case 2:
        digitalWrite(2, LOW);
        digitalWrite(3, LOW);
        digitalWrite(4, LOW);
        digitalWrite(5, LOW);
        digitalWrite(6, LOW);
        timer_init = millis() + 50;
        init_step++;
        break;
      case 3:
        digitalWrite(2, HIGH);
        digitalWrite(3, HIGH);
        digitalWrite(4, HIGH);
        digitalWrite(5, HIGH);
        digitalWrite(6, HIGH);
        timer_init = millis() + 500;
        init_step++;
        Serial.print("max_list:");
        for (int i = 14; i <= 18; i++)
        {
          max_list[i - 14] = sampling[i - 14];
          Serial.print(max_list[i - 14]);
          Serial.print("-");
        }
        Serial.println();
        break;
      case 4:
        init_step++;
        break;
      case 5:
        if ((max_list[1] - sampling[1]) > 50)
        {
          init_step++;
          digitalWrite(2, LOW);
          digitalWrite(3, LOW);
          digitalWrite(4, LOW);
          digitalWrite(5, LOW);
          digitalWrite(6, LOW);
          timer_init = millis() + 2000;
        }
        break;
      case 6:
        digitalWrite(2, HIGH);
        digitalWrite(3, HIGH);
        digitalWrite(4, HIGH);
        digitalWrite(5, HIGH);
        digitalWrite(6, HIGH);
        timer_init = millis() + 200;
        init_step++;
        break;
      case 7:
        digitalWrite(2, LOW);
        digitalWrite(3, LOW);
        digitalWrite(4, LOW);
        digitalWrite(5, LOW);
        digitalWrite(6, LOW);
        timer_init = millis() + 50;
        init_step++;
        break;
      case 8:
        digitalWrite(2, HIGH);
        digitalWrite(3, HIGH);
        digitalWrite(4, HIGH);
        digitalWrite(5, HIGH);
        digitalWrite(6, HIGH);
        timer_init = millis() + 500;
        init_step++;
        Serial.print("min_list:");
        for (int i = 14; i <= 18; i++)
        {
          min_list[i - 14] = sampling[i - 14];
          Serial.print(min_list[i - 14]);
          Serial.print("-");
        }
        Serial.println();
        turn_on = false;
        break;

      default:
        break;
    }
  }
}


float radianX;
float radianY;
float radianX_last = 0; //最终获得的X轴倾角
float radianY_last = 0; //最终获得的Y轴倾角

//更新倾角传感器数据
void update_mpu6050()
{
  static uint32_t timer_u;
  if (timer_u < millis())
  {
    // put your main code here, to run repeatedly:
    timer_u = millis() + 20;
    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    // 先减偏置，再滤波
    float ax_temp = (float)(ax) - ax_offset;
    ax0 = ax_temp * 0.3 + ax0 * 0.7;  //对读取到的值进行滤波
    float ay_temp = (float)(ay) - ay_offset;
    ay0 = ay_temp * 0.3 + ay0 * 0.7;
    float az_temp = (float)(az) - az_offset;
    az0 = az_temp * 0.3 + az0 * 0.7;
    ax1 = ax0 / 8192.0;  // 转为重力加速度的倍数
    ay1 = ay0 / 8192.0;
    az1 = az0 / 8192.0;

    float gx_temp = (float)(gx) - gx_offset;
    gx0 = gx_temp * 0.3 + gx0 * 0.7;  //对读取到的角速度的值进行滤波
    float gy_temp = (float)(gy) - gy_offset;
    gy0 = gy_temp * 0.3 + gy0 * 0.7;
    float gz_temp = (float)(gz) - gz_offset;
    gz0 = gz_temp * 0.3 + gz0 * 0.7;
    gx1 = gx0;  //校正角速度
    gy1 = gy0;
    gz1 = gz0;


    //互补计算x轴倾角
    radianX = atan2(ay1, az1);
    radianX = radianX * 180.0 / 3.1415926;
    float radian_temp = (float)(gx1) / 16.4 * 0.02;
    radianX_last = 0.8 * (radianX_last + radian_temp) + (-radianX) * 0.2;

    //互补计算y轴倾角
    radianY = atan2(ax1, az1);
    radianY = radianY * 180.0 / 3.1415926;
    radian_temp = (float)(gy1) / 16.4 * 0.02;
    radianY_last = 0.8 * (radianY_last + radian_temp) + (-radianY) * 0.2;


  }
}

void run(){

};//待扩展

//run1, 发送电位器值。
void run1(int mode)
{
  // 发送处理后的电位器值
  static uint32_t last_send_time = 0;
  if (millis() - last_send_time > 100) { // 每100ms发送一次数据
    last_send_time = millis();
    
    int16_t theta0 = (int16_t)data[0]; //拇指
    int16_t theta1 = (int16_t)data[1]; //食指
    int16_t theta2 = (int16_t)data[2]; //中指
    int16_t theta3 = (int16_t)data[3]; //无名指
    int16_t theta4 = (int16_t)data[4]; //小指
    int16_t theta5 = (int16_t)((theta1+theta2+theta3+theta4)/4);

    // 构建数据帧
    byte buf[23]; // 帧头(2) + 长度(1) + 命令(1) + 操作指令(2) + x倾角(2) + y倾角(2) = 10 bytes
    buf[0] = 0x55;
    buf[1] = 0x55;
    buf[2] = 6; // 数据长度（操作指令2字节 + x倾角2字节 + y倾角2字节）
    buf[3] = 0x42; // 自定义命令码，用于发送传感器数据

    // 检查key按钮状态是否刚发生变化
    if (key_state_changed) {
      // 根据变化次数的奇偶性设置蓝牙数据帧
      if (key_change_count % 2 == 1) {
        // 奇数次变化 - 起飞模式
        buf[4] = 0x01;
        buf[5] = 0xff;
      } else {
        // 偶数次变化 - 降落模式
        buf[4] = 0x02;
        buf[5] = 0xff;
      }
      
      // 添加x、y倾角值
      int16_t x_angle = (int16_t)(radianX_last*10);
      int16_t y_angle = (int16_t)(radianY_last*10);
      buf[6] = x_angle >> 8; // x倾角高8位
      buf[7] = x_angle & 0xff; // x倾角低8位
      buf[8] = y_angle >> 8; // y倾角高8位
      buf[9] = y_angle & 0xff; // y倾角低8位
      
      // 发送数据
      Bth.write(buf, 10);
      key_state_changed = false; // 重置状态变化标记
      return; // 直接返回
    }

    if(theta0>2450){
      //油门增加
      buf[4] = 0x03;
    }else if(theta0<550){
      //油门减少
      buf[4] = 0x04;
    }else{
      //无操作
      buf[4] = 0x00;
    }

    if(theta5>2450){
      //左转
      buf[5] = 0x05;
    }else if(theta5<550){
      //右转
      buf[5] = 0x06;
    }else{
      //无操作
      buf[5] = 0x00;
    }

    // 添加x、y倾角值
    int16_t x_angle = (int16_t)(radianX_last*10);
    int16_t y_angle = (int16_t)(radianY_last*10);
    buf[6] = x_angle >> 8; // x倾角高8位
    buf[7] = x_angle & 0xff; // x倾角低8位
    buf[8] = y_angle >> 8; // y倾角高8位
    buf[9] = y_angle & 0xff; // y倾角低8位
    
    // 发送数据
    Bth.write(buf, 10);
  }
}

void run2(){
  
};//待扩展

void run3(int mode){
  
};//待扩展

int mode = 1;
bool key_state = false;

void loop() {
  finger();  //更新手指电位器数据
  update_mpu6050();  //更新倾角传感器数据

  if (turn_on == false) //启动后电位器校正完毕
  {
    // 若K3按键按下
    if(key_state == true && digitalRead(7) == true)
    {
      delay(30);
      if(digitalRead(7) == true)
        key_state = false;
    }
    if (digitalRead(7) == false && key_state == false)
    {
      delay(30);
      // 若K3被按下,则增加状态变化次数
      if (digitalRead(7) == false)
      {
        key_state = true;
        key_change_count++; // 增加状态变化次数
        key_state_changed = true; // 标记状态刚发生变化
        // 显示状态：奇数次亮LED2，偶数次灭LED2
        if (key_change_count % 2 == 1)
        {
          digitalWrite(2, HIGH); // 奇数次亮LED2
        }
        else
        {
          digitalWrite(2, LOW); // 偶数次灭LED2
        }
      }
    }

    if (mode == 0)
      run();  // 蜘蛛、人形等仿生机器人
    if (mode == 1 || mode == 4)  { //模式1为左手掌,模式2为右手掌
      run1(mode); // 手掌
    }
    if (mode == 2)
      run2(); //小车
    if (mode == 3 || mode == 5)
      run3(mode);  //机械臂  模式3为PWM舵机驱动，模式5为总线舵机驱动
  }
  //print_data();  //打印传感器数据便于调试
}
