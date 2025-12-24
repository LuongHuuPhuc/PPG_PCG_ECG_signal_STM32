import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.signal import butter, filtfilt

# ======================
# THAM SỐ
# ======================
fs = 1000.0       # Tần số lấy mẫu (Hz) <<< ĐỔI nếu cần
lowcut = 1.0      # Hz
highcut = 20.0    # Hz
order = 4         # Bậc lọc

# ======================
# HÀM BANDPASS FILTER
# ======================
def bandpass_filter(signal, fs, lowcut, highcut, order=4):
    nyq = 0.5 * fs
    low = lowcut / nyq
    high = highcut / nyq
    b, a = butter(order, [low, high], btype='band')
    return filtfilt(b, a, signal)

# ======================
# ĐỌC FILE CSV
# ======================
file_path = r"D:\Esp-idf\Test_source\max30102_test\data_text\ECG_PPG_STM32\NguyenDangKhenh_ECG_PPG1.csv"
data = pd.read_csv(file_path)

# Ép kiểu số
data['ECG'] = pd.to_numeric(data['ECG'], errors='coerce')
data['IR'] = pd.to_numeric(data['IR'], errors='coerce')
data = data.dropna()

# Lấy tín hiệu
ecg = data['ECG'].values
ir  = data['IR'].values

# ======================
# LỌC 5–50 Hz
# ======================
ecg_filt = bandpass_filter(ecg, fs, lowcut, highcut, order)
ir_filt = bandpass_filter(ir, fs, 1, 20, order)
# ======================
# VẼ
# ======================
fig, ax1 = plt.subplots(figsize=(10, 4))

ax1.set_xlabel('Sample index')
ax1.set_ylabel('ECG (filtered)', color='red')
ax1.plot(ecg_filt, color='red', label='ECG filtered')
ax1.tick_params(axis='y', labelcolor='red')

ax2 = ax1.twinx()
ax2.set_ylabel('IR (filtered)', color='green')
ax2.plot(ir_filt, color='green', label='IR filtered')
ax2.tick_params(axis='y', labelcolor='green')

plt.title('ECG & IR Bandpass Filtered (5–50 Hz)')
fig.tight_layout()
plt.show()
