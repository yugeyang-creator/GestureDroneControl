/*
 * MIT License
 *
 * Copyright (c) 2026 于歌洋
 * Copyright (c) 2024 Kouhei Ito
 * Copyright (c) 2024 M5Stack Technology CO LTD
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// Controller for M5Fly with cores3
// #define DEBUG

#include "JoyStick.h"
#include "BluetoothDataParser.h"

TaskHandle_t task_tone_handle = NULL;
esp_now_peer_info_t peerInfo;

float Throttle;
float Phi, Theta, Psi;
uint8_t Mode               = ANGLECONTROL;
uint8_t AltMode            = ALT_CONTROL_MODE;
volatile uint8_t Loop_flag = 0;
float dTime                = 0.01;
uint32_t espnow_version;

char data[140];
uint8_t senddata[25];  // 19->22->23->24->25

volatile uint8_t is_peering                   = 0;
volatile uint8_t auto_up_down_status          = 0;
volatile uint32_t auto_up_down_status_counter = 0;

volatile float fly_bat_voltage = 0.0f;
volatile float roll_angle      = 0.0f;
volatile float pitch_angle     = 0.0f;
volatile float yaw_angle       = 0.0f;
volatile float altitude        = 0.0f;
volatile int16_t tof_front     = 0.0;

volatile uint8_t alt_flag      = 0;
volatile uint8_t fly_mode      = 0;

volatile uint8_t proactive_flag          = 0;
volatile uint32_t proactive_flag_counter = 0;

// StampFly MAC ADDRESS
// 1 F4:12:FA:66:80:54 (Yellow)
// 2 F4:12:FA:66:77:A4
uint8_t Addr1[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t Addr2[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Channel
uint8_t Ch_counter;
volatile uint8_t Received_flag             = 0;
volatile uint8_t Channel                   = CHANNEL;
volatile uint8_t is_fly_flag               = 0;
volatile unsigned long is_fly_flag_counter = 0;

// BluetoothDataParser instance
BluetoothDataParser *bluetoothParser;

// TensorFlow Lite model and arena
#include "model.h"
const uint8_t* model = g_model;
size_t tensorArenaSize = 10 * 1024;
uint8_t tensorArena[10 * 1024];

void rc_init(uint8_t ch, uint8_t *addr);

// 受信コールバック
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
    if (is_peering) {
        if (data[7] == 0xaa && data[8] == 0x55 && data[9] == 0x16 && data[10] == 0x88) {
            Received_flag = 1;
            Channel       = data[0];
            Addr2[0]      = data[1];
            Addr2[1]      = data[2];
            Addr2[2]      = data[3];
            Addr2[3]      = data[4];
            Addr2[4]      = data[5];
            Addr2[5]      = data[6];
            Serial.printf("Receive !\n");
        }
    } else {
        if (data[0] == 88 && data[1] == 88) {
            memcpy((uint8_t *)&roll_angle, &data[2 + 4 * (3 - 1)], 4);
            memcpy((uint8_t *)&pitch_angle, &data[2 + 4 * (4 - 1)], 4);
            memcpy((uint8_t *)&yaw_angle, &data[2 + 4 * (5 - 1)], 4);
            memcpy((uint8_t *)&fly_bat_voltage, &data[2 + 4 * (15 - 1)], 4);
            memcpy((uint8_t *)&altitude, &data[2 + 4 * (25 - 1)], 4);
            alt_flag = data[2 + 4 * (28 - 1)];
            fly_mode = data[2 + 4 * (28 - 1) + 1];
            memcpy((uint8_t *)&tof_front, &data[2 + 4 * (28 - 1) + 2], 2);
            is_fly_flag = 1;
        }
    }
}

#define BUF_SIZE 128
// 打印配对信息到串口
void save_data(void) {
    Serial.printf("配对成功！无人机信息:\n");
    Serial.printf("频道: %d\n", Channel);
    Serial.printf("MAC地址: %02X:%02X:%02X:%02X:%02X:%02X\n", Addr2[0], Addr2[1], Addr2[2], Addr2[3], Addr2[4], Addr2[5]);
    Serial.printf("-----------------------------------\n");
}

// 初始化配对信息（不再使用文件系统）
void load_data(void) {
    // 只在第一次启动时显示提示，不重置配对信息
    // 这样可以保持之前的配对状态
    Serial.println("设备已启动，使用之前的配对信息");
}

void rc_init(uint8_t ch, uint8_t *addr) {
    // ESP-NOW初期化
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() == ESP_OK) {
        esp_now_unregister_recv_cb();
        esp_now_register_recv_cb(OnDataRecv);
        Serial.println("ESPNow Init Success");
    } else {
        Serial.println("ESPNow Init Failed");
        ESP.restart();
    }

    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, addr, 6);
    peerInfo.channel = ch;
    peerInfo.encrypt = false;
    while (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
    }
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

void peering(void) {
    uint8_t break_flag;
    //  ESP-NOWコールバック登録
    esp_now_unregister_recv_cb();
    esp_now_register_recv_cb(OnDataRecv);

    // ペアリング
    Ch_counter = 1;
    while (1) {
        Serial.printf("Try channel %02d.\n\r", Ch_counter);
        peerInfo.channel = Ch_counter;
        peerInfo.encrypt = false;
        while (esp_now_mod_peer(&peerInfo) != ESP_OK) {
            Serial.println("Failed to mod peer");
        }
        esp_wifi_set_channel(Ch_counter, WIFI_SECOND_CHAN_NONE);

        // Wait receive StampFly MAC Address
        for (uint8_t i = 0; i < 100; i++) {
            break_flag = 0;
            if (Received_flag == 1) {
                break_flag = 1;
                break;
            }
            usleep(100);
        }
        if (break_flag) break;
        Ch_counter++;
        if (Ch_counter == 15) Ch_counter = 1;
    }

    save_data();
    is_peering = 0;

    Serial.printf("Channel:%02d\n\r", Channel);
    Serial.printf("MAC2:%02X:%02X:%02X:%02X:%02X:%02X:\n\r", Addr2[0], Addr2[1], Addr2[2], Addr2[3], Addr2[4],
                     Addr2[5]);

    // Peering
    while (esp_now_del_peer(Addr1) != ESP_OK) {
        Serial.println("Failed to delete peer1");
    }
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, Addr2, 6);
    peerInfo.channel = Channel;
    peerInfo.encrypt = false;
    while (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer2");
    }
    esp_wifi_set_channel(Channel, WIFI_SECOND_CHAN_NONE);
    // 移除重启，避免配对信息丢失
    // esp_restart();
}

// 周期カウンタ割り込み関数
hw_timer_t *timer = NULL;
void IRAM_ATTR onTimer() {
    Loop_flag = 1;
}

void setup() {
    Serial.begin(9600);
    M5.begin();
    Wire.begin(38, 39);
    Wire.setClock(400000);
    load_data();

    // 初始化BluetoothDataParser
    bluetoothParser = new BluetoothDataParser(44, 43, model, tensorArena, tensorArenaSize);
    bluetoothParser->begin();
    bluetoothParser->setupTensorFlow();

    if (Addr2[0] == 0xFF && Addr2[1] == 0xFF && Addr2[2] == 0xFF && Addr2[3] == 0xFF &&
        Addr2[4] == 0xFF && Addr2[5] == 0xFF) {
        is_peering = 1;
        rc_init(1, Addr1);
        Serial.printf("Start peering...\n\r");
        peering();
    }
#ifdef DEBUG
    else
        rc_init(Channel, Addr1);
#else
    else
        rc_init(Channel, Addr2);
#endif

    esp_now_get_version(&espnow_version);
    Serial.printf("Version %d\n", espnow_version);

    // 割り込み設定
    timer = timerBegin(10000); // 10kHz频率
    timerAttachInterrupt(timer, &onTimer);
    timerWrite(timer, 0);
    timerAlarm(timer, 100, true, 0); // 10ms中断间隔 (100×100μs)
    delay(100);
}

void loop() {
    while (Loop_flag == 0);
    Loop_flag = 0;
    M5.update();

    // 处理蓝牙数据
    if (bluetoothParser->available()) {
        bluetoothParser->processData();
    }

    // 获取指令
    uint8_t throttleCommand = bluetoothParser->getThrottleCommand();
    uint8_t steeringCommand = bluetoothParser->getSteeringCommand();
    uint8_t moveCommand = bluetoothParser->getMoveCommand();
    
    // 只有当有新数据时才打印指令码
    if (bluetoothParser->hasNewData()) {
        // 打印指令码到串口
        Serial.printf("指令码: 油门=0x%02X, 转向=0x%02X, 移动(倾角)=0x%02X\n", throttleCommand, steeringCommand, moveCommand);
        
        // 重置新数据标志
        bluetoothParser->resetNewDataFlag();
    }

    // 初始化控制值
    Throttle = 0.0;
    Phi = 0.0;
    Theta = 0.0;
    Psi = 0.0;
    auto_up_down_status = 0;
    proactive_flag = 0;

    // 判断油门指令
    if (throttleCommand == 0x01) { // 起飞
        Throttle = 0.0;
        Phi = 0.0;
        Theta = 0.0;
        Psi = 0.0;
        auto_up_down_status = 1;
        proactive_flag = 0;
    } else if (throttleCommand == 0x02) { // 降落
        Throttle = 0.0;
        Phi = 0.0;
        Theta = 0.0;
        Psi = 0.0;
        auto_up_down_status = 1;
        proactive_flag = 0;
    } else if (throttleCommand == 0x03) { // 上升
        Throttle = 0.5;
    } else if (throttleCommand == 0x04) { // 下降
        Throttle = -0.5;
    }

    // 判断转向指令
    if (steeringCommand == 0x05) { // 左转
        Psi = -0.5;
    } else if (steeringCommand == 0x06) { // 右转
        Psi = 0.5;
    }

    // 判断移动指令
    if (moveCommand == 0x07) { // 向前
        Theta = -0.5;
        Phi = 0.0;
    } else if (moveCommand == 0x08) { // 向后
        Theta = 0.5;
        Phi = 0.0;
    } else if (moveCommand == 0x09) { // 向左
        Theta = 0.0;
        Phi = -0.5;
    } else if (moveCommand == 0x0A) { // 向右
        Theta = 0.0;
        Phi = 0.5;
    }

    uint8_t *d_int;

    // ブロードキャストの混信を防止するためこの機体のMACアドレスに送られてきたものか判断する
    senddata[0] = peerInfo.peer_addr[3];
    senddata[1] = peerInfo.peer_addr[4];
    senddata[2] = peerInfo.peer_addr[5];

    d_int       = (uint8_t *)&Psi;
    senddata[3] = d_int[0];
    senddata[4] = d_int[1];
    senddata[5] = d_int[2];
    senddata[6] = d_int[3];

    d_int        = (uint8_t *)&Throttle;
    senddata[7]  = d_int[0];
    senddata[8]  = d_int[1];
    senddata[9]  = d_int[2];
    senddata[10] = d_int[3];

    d_int        = (uint8_t *)&Phi;
    senddata[11] = d_int[0];
    senddata[12] = d_int[1];
    senddata[13] = d_int[2];
    senddata[14] = d_int[3];

    d_int        = (uint8_t *)&Theta;
    senddata[15] = d_int[0];
    senddata[16] = d_int[1];
    senddata[17] = d_int[2];
    senddata[18] = d_int[3];

    senddata[19] = auto_up_down_status;
    senddata[20] = 0; // 翻转按钮状态设为0
    senddata[21] = Mode; // 控制模式为角度控制
    senddata[22] = AltMode; // 高度模式为自动高度
    senddata[23] = proactive_flag; // 主动标志为0

    // checksum
    senddata[24] = 0;
    for (uint8_t i = 0; i < 24; i++) senddata[24] = senddata[24] + senddata[i];

    // 送信
    esp_err_t result = esp_now_send(peerInfo.peer_addr, senddata, sizeof(senddata));
#ifdef DEBUG
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n", peerInfo.peer_addr[0], peerInfo.peer_addr[1],
                     peerInfo.peer_addr[2], peerInfo.peer_addr[3], peerInfo.peer_addr[4], peerInfo.peer_addr[5]);
#endif

    if (auto_up_down_status) {
        auto_up_down_status_counter++;
        if (auto_up_down_status_counter > 20) {
            auto_up_down_status_counter = 0;
            auto_up_down_status         = 0;
        }
    }
    if (proactive_flag) {
        proactive_flag_counter++;
        if (proactive_flag_counter > 20) {
            proactive_flag_counter = 0;
            proactive_flag         = 0;
        }
    }
    if (is_fly_flag) {
        is_fly_flag_counter++;
        if (is_fly_flag_counter > 2000) {
            is_fly_flag_counter = 0;
            is_fly_flag         = 0;
        }
    }
}
