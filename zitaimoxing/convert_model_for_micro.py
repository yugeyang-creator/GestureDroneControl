#!/usr/bin/env python3
"""
将TensorFlow Lite模型转换为适合微控制器的版本（不使用混合量化）
"""
import tensorflow as tf
import os
import sys
import argparse

def convert_model_for_micro(keras_model_path, output_tflite_path):
    """将Keras模型转换为适合微控制器的TFLite模型"""
    # 加载原始Keras模型
    model = tf.keras.models.load_model(keras_model_path)
    
    # 配置转换器，不使用混合量化
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    
    # 禁用混合量化，使用纯浮点模型
    # 或者使用完全整数量化（如果有校准数据）
    # 这里我们使用纯浮点模型，确保兼容性
    converter.optimizations = []  # 不使用优化
    
    # 转换模型
    tflite_model = converter.convert()
    
    # 保存TFLite模型
    with open(output_tflite_path, 'wb') as f:
        f.write(tflite_model)
    
    print(f"适合微控制器的模型已保存为: {output_tflite_path}")
    
    # 转换为C数组
    convert_script = os.path.join(os.path.dirname(__file__), 'convert_to_c_array.py')
    if os.path.exists(convert_script):
        # 生成model.h文件
        model_h_path = os.path.join(os.path.dirname(output_tflite_path), 'model.h')
        # 执行转换脚本
        import subprocess
        result = subprocess.run(
            [sys.executable, convert_script, output_tflite_path, model_h_path],
            capture_output=True,
            text=True
        )
        if result.returncode == 0:
            print(f"模型已转换为C数组并保存为: {model_h_path}")
        else:
            print(f"转换为C数组失败: {result.stderr}")
    else:
        print("转换脚本不存在，请手动执行转换")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='将Keras模型转换为适合微控制器的TFLite模型')
    parser.add_argument('keras_model_path', nargs='?', default='motion_model.h5', help='Keras模型文件路径')
    parser.add_argument('output_tflite_path', nargs='?', default='motion_model_micro.tflite', help='输出TFLite模型文件路径')
    args = parser.parse_args()
    
    convert_model_for_micro(args.keras_model_path, args.output_tflite_path)
