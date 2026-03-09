/*
 * CRC8 校验工具类
 * 用于数据帧完整性校验，提高通信可靠性
 */

#ifndef CRC8_H
#define CRC8_H

#include <stdint.h>

/**
 * @brief 计算 CRC8 校验值 (IBUTTON 标准)
 * 
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return uint8_t CRC8 校验值
 */
uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc = _crc_ibutton_update(crc, data[i]);
    }
    return crc;
}

#endif // CRC8_H
