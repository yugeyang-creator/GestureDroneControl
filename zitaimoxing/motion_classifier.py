import numpy as np
import tensorflow as tf
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Conv1D, MaxPooling1D, Flatten, Dense, Dropout
from tensorflow.keras.utils import to_categorical
import serial
import time
import keyboard
import threading

# 状态标签定义
LABELS = {
    'w': 0,  # 前倾
    's': 1,  # 后倾
    'a': 2,  # 左倾
    'd': 3,  # 右倾
    '': 4    # 不变
}

# 反向映射，用于显示结果
REVERSE_LABELS = {
    0: '前倾',
    1: '后倾',
    2: '左倾',
    3: '右倾',
    4: '不变'
}

# 数据缓冲区大小
BUFFER_SIZE = 10
# 数据采集频率
SAMPLE_RATE = 100  # Hz

class DataCollector:
    def __init__(self, port='COM12', baudrate=115200):
        self.ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS,
            timeout=0.1
        )
        self.buffer = ""
        self.raw_data = []  # 存储原始数据点和时间戳
        self.raw_labels = []  # 存储原始标签和时间戳
        self.data_buffer = []  # 存储滑动窗口样本
        self.labels = []  # 存储样本标签
        self.current_label = ''
        self.last_label_change_time = time.time()
        self.collecting = False
        self.lock = threading.Lock()
        
        # 启动键盘监听线程
        keyboard_thread = threading.Thread(target=self._keyboard_listener)
        keyboard_thread.daemon = True
        keyboard_thread.start()
    
    def _keyboard_listener(self):
        """监听键盘输入，更新当前标签"""
        while True:
            event = keyboard.read_event()
            if event.event_type == keyboard.KEY_DOWN:
                if event.name in LABELS:
                    self.current_label = event.name
                    self.last_label_change_time = time.time()
            elif event.event_type == keyboard.KEY_UP:
                if event.name == self.current_label:
                    self.current_label = ''
                    self.last_label_change_time = time.time()
    
    def parse_ascii_frame(self, line):
        """解析ASCII帧，提取x、y倾角值"""
        try:
            if 'HEX:' in line:
                hex_part = line.split('HEX:')[1].strip()
                hex_values = hex_part.split()
                if len(hex_values) >= 8:
                    data = [int(h, 16) for h in hex_values[:8]]
                    if data[0] == 0x55 and data[1] == 0x55:
                        x_low = data[4]
                        x_high = data[5]
                        y_low = data[6]
                        y_high = data[7]
                        x = (x_high << 8) | x_low
                        y = (y_high << 8) | y_low
                        if x >= 0x8000:
                            x -= 0x10000
                        if y >= 0x8000:
                            y -= 0x10000
                        x = x / 10
                        y = y / 10
                        return x, y
        except Exception as e:
            print(f"解析错误: {e}")
        return None
    
    def collect_data(self, duration=10):
        """采集数据，持续指定时间"""
        print(f"开始采集数据，持续 {duration} 秒...")
        print("按 w:前倾, s:后倾, a:左倾, d:右倾, 松开:不变")
        print("实时倾角值: ")
        
        self.collecting = True
        start_time = time.time()
        sample_interval = 1.0 / SAMPLE_RATE
        
        # 清空数据
        self.raw_data = []
        self.raw_labels = []
        self.data_buffer = []
        self.labels = []
        
        while time.time() - start_time < duration:
            try:
                data = self.ser.read(1024)
                if data:
                    try:
                        text = data.decode('utf-8', errors='ignore')
                        self.buffer += text
                        
                        lines = self.buffer.split('\r\n')
                        for line in lines[:-1]:
                            if line.strip():
                                result = self.parse_ascii_frame(line)
                                if result:
                                    x, y = result
                                    current_time = time.time()
                                    
                                    # 实时显示倾角值
                                    print(f"x: {x:.2f}, y: {y:.2f}, 状态: {REVERSE_LABELS[LABELS.get(self.current_label, 4)]}", end='\r')
                                    
                                    # 存储原始数据和标签
                                    self.raw_data.append((x, y, current_time))
                                    self.raw_labels.append((self.current_label, current_time))
                        self.buffer = lines[-1]
                    except UnicodeDecodeError:
                        pass
                time.sleep(sample_interval)
            except serial.SerialException as e:
                print(f"串口读取错误: {e}")
                time.sleep(1)
        
        self.collecting = False
        print("\n数据采集完成，共采集 {} 个数据点".format(len(self.raw_data)))
        
        # 使用滑动窗口生成样本
        if len(self.raw_data) >= BUFFER_SIZE:
            self._generate_sliding_window_samples()
            print("生成 {} 个样本".format(len(self.data_buffer)))
            
            # 保存样本和标签
            self._save_samples()
        else:
            print("数据点不足，无法生成样本")
    
    def _generate_sliding_window_samples(self):
        """使用滑动窗口生成样本，并计算每个窗口的标签"""
        for i in range(len(self.raw_data) - BUFFER_SIZE + 1):
            # 提取窗口数据
            window_data = []
            window_labels = []
            window_times = []
            
            for j in range(i, i + BUFFER_SIZE):
                x, y, timestamp = self.raw_data[j]
                window_data.append([x, y])
                
                # 查找对应时间戳的标签
                for k in range(len(self.raw_labels)):
                    label, label_time = self.raw_labels[k]
                    if label_time <= timestamp:
                        current_label = label
                    else:
                        break
                window_labels.append(current_label)
                window_times.append(timestamp)
            
            # 计算窗口内各标签的持续时间
            label_durations = {}
            for j in range(len(window_labels)):
                label = window_labels[j]
                if j == 0:
                    start_time = window_times[j]
                else:
                    if window_labels[j] != window_labels[j-1]:
                        duration = window_times[j] - start_time
                        prev_label = window_labels[j-1]
                        if prev_label in label_durations:
                            label_durations[prev_label] += duration
                        else:
                            label_durations[prev_label] = duration
                        start_time = window_times[j]
            
            # 处理最后一个标签
            if window_labels:
                duration = window_times[-1] - start_time
                last_label = window_labels[-1]
                if last_label in label_durations:
                    label_durations[last_label] += duration
                else:
                    label_durations[last_label] = duration
            
            # 选择持续时间最长的标签
            if label_durations:
                max_label = max(label_durations, key=label_durations.get)
            else:
                max_label = ''
            
            # 添加到样本和标签列表
            self.data_buffer.append(window_data)
            self.labels.append(max_label)
    
    def _save_samples(self):
        """保存样本和标签到文件"""
        # 保存原始数据
        with open('raw_data.txt', 'w', encoding='utf-8') as f:
            f.write('时间戳,x,y,状态\n')
            for i, (x, y, timestamp) in enumerate(self.raw_data):
                # 查找对应时间戳的标签
                label = ''
                for k in range(len(self.raw_labels)):
                    l, t = self.raw_labels[k]
                    if t <= timestamp:
                        label = l
                    else:
                        break
                status = REVERSE_LABELS[LABELS.get(label, 4)]
                f.write(f"{timestamp:.3f},{x:.2f},{y:.2f},{status}\n")
        
        # 保存样本数据
        with open('samples.txt', 'w', encoding='utf-8') as f:
            f.write('样本索引,窗口数据,标签\n')
            for i, (sample, label) in enumerate(zip(self.data_buffer, self.labels)):
                data_str = ';'.join([f"({x:.2f},{y:.2f})" for x, y in sample])
                status = REVERSE_LABELS[LABELS.get(label, 4)]
                f.write(f"{i},{data_str},{status}\n")
        
        print("样本和标签已保存到 raw_data.txt 和 samples.txt 文件")
    
    def get_data(self):
        """获取采集的数据"""
        with self.lock:
            # 转换为numpy数组
            X = np.array(self.data_buffer)
            y = np.array([LABELS[label] for label in self.labels])
            # 对标签进行独热编码
            y = to_categorical(y, num_classes=len(LABELS))
            return X, y
    
    def close(self):
        """关闭串口"""
        if self.ser.is_open:
            self.ser.close()

class MotionClassifier:
    def __init__(self):
        self.model = self._build_model()
    
    def _build_model(self):
        """构建轻量级一维卷积神经网络模型，用于状态分类"""
        model = Sequential([
            Conv1D(filters=16, kernel_size=3, activation='relu', input_shape=(BUFFER_SIZE, 2)),
            MaxPooling1D(pool_size=2),
            Conv1D(filters=32, kernel_size=3, activation='relu'),
            MaxPooling1D(pool_size=2),
            Flatten(),
            Dense(64, activation='relu'),
            Dropout(0.2),
            Dense(len(LABELS), activation='softmax')
        ])
        
        model.compile(
            optimizer='adam',
            loss='categorical_crossentropy',
            metrics=['accuracy']
        )
        
        return model
    
    def train(self, X, y, epochs=50, batch_size=32, patience=10):
        """训练模型，实现早停功能"""
        # 导入早停回调
        from tensorflow.keras.callbacks import EarlyStopping
        
        # 设置早停回调
        early_stopping = EarlyStopping(
            monitor='val_loss',  # 监控验证集损失
            patience=patience,    # 耐心值
            restore_best_weights=True  # 恢复最佳权重
        )
        
        # 拆分数据：训练集 70%，验证集 15%，测试集 15%
        from sklearn.model_selection import train_test_split
        X_train, X_temp, y_train, y_temp = train_test_split(X, y, test_size=0.3, random_state=42)
        X_val, X_test, y_val, y_test = train_test_split(X_temp, y_temp, test_size=0.5, random_state=42)
        
        print(f"数据拆分完成：")
        print(f"训练集: {X_train.shape[0]} 样本")
        print(f"验证集: {X_val.shape[0]} 样本")
        print(f"测试集: {X_test.shape[0]} 样本")
        
        # 训练模型
        history = self.model.fit(
            X_train, y_train,
            epochs=epochs,
            batch_size=batch_size,
            validation_data=(X_val, y_val),
            callbacks=[early_stopping]
        )
        
        # 用测试集评估模型
        print("\n用测试集评估模型：")
        loss, accuracy = self.model.evaluate(X_test, y_test)
        print(f"测试集评估结果: 损失={loss:.4f}, 准确率={accuracy:.4f}")
        return history
    
    def evaluate(self, X, y):
        """评估模型"""
        loss, accuracy = self.model.evaluate(X, y)
        print(f"模型评估结果: 损失={loss:.4f}, 准确率={accuracy:.4f}")
        return loss, accuracy
    
    def predict(self, X):
        """预测动作"""
        predictions = self.model.predict(X)
        return np.argmax(predictions, axis=1)
    
    def save_model(self, path):
        """保存模型"""
        self.model.save(path)
    
    def load_model(self, path):
        """加载模型"""
        self.model = tf.keras.models.load_model(path)
    
    def optimize_for_edge(self, path):
        """优化模型，使其适合边缘设备"""
        # 直接调用convert_model_for_micro.py脚本生成适合微控制器的模型
        import os
        import sys
        # 确保脚本路径正确
        convert_script = os.path.join(os.path.dirname(__file__), 'convert_model_for_micro.py')
        if os.path.exists(convert_script):
            # 获取Keras模型路径
            keras_model_path = os.path.join(os.path.dirname(path), 'motion_model.h5')
            # 执行转换脚本
            import subprocess
            result = subprocess.run(
                [sys.executable, convert_script, keras_model_path, path],
                capture_output=True,
                text=True
            )
            if result.returncode == 0:
                print("模型已优化并转换为适合微控制器的C数组")
            else:
                print(f"模型优化失败: {result.stderr}")
        else:
            print("转换脚本不存在，请手动执行转换")

def main():
    # 数据采集
    collector = DataCollector()
    try:
        # 采集120秒数据
        collector.collect_data(duration=1200)
        
        # 获取数据
        X, y = collector.get_data()
        print(f"数据形状: X={X.shape}, y={y.shape}")
        
        # 数据预处理
        # 数据已经是正确的形状 (samples, BUFFER_SIZE, 2)，不需要再reshape
        
        # 创建并训练模型
        classifier = MotionClassifier()
        classifier.train(X, y, epochs=50, batch_size=32, patience=10)
        
        # 评估模型 (可选，因为train方法已经包含了测试集评估)
        # classifier.evaluate(X, y)
        
        # 保存模型
        classifier.save_model('motion_model.h5')
        
        # 优化模型用于边缘设备
        classifier.optimize_for_edge('motion_model_micro.tflite')
        
        print("模型训练和优化完成！")
        print("可以将 motion_model_micro.tflite 烧录到开发板上使用")
        
    except KeyboardInterrupt:
        print("程序已手动停止")
    finally:
        collector.close()

if __name__ == "__main__":
    main()