import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.signal import butter, filtfilt

# ======================
# THAM SỐ
# ======================
fs = 1000.0        # Tần số lấy mẫu (Hz) <<< ĐỔI nếu cần
order = 4         # Bậc lọc

lowcut_ecg = 1.0
highcut_ecg = 20.0 

lowcut_ppg = 1.0
highcut_ppg = 10.0

lowcut_pcg = 20.0
highcut_pcg = 200.0

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
file_path = r"D:\Esp-idf\Test_source\max30102_test\data_text\ECG_PPG_PCG1.csv"
data = pd.read_csv(file_path)

# Ép kiểu số
data['ECG'] = pd.to_numeric(data['ECG'], errors='coerce')
data['IR'] = pd.to_numeric(data['IR'], errors='coerce')
data['PCG'] = pd.to_numeric(data['PCG'], errors='coerce')

data = data.dropna()

# Lấy tín hiệu
ecg = data['ECG'].values
ir  = data['IR'].values
pcg  = data['PCG'].values

# ======================
# LỌC
# ======================
ecg_filt = bandpass_filter(ecg, fs, lowcut_ecg, highcut_ecg, order)
ir_filt = bandpass_filter(ir, fs, lowcut_ppg, highcut_ppg, order)
pcg_filt = bandpass_filter(pcg, fs, lowcut_pcg, highcut_pcg, order)

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

ax3 = ax1.twinx()
ax3.set_ylabel('PCG (filtered)', color='blue')
ax3.plot(pcg_filt, color='blue', label='PCG filtered')
ax3.tick_params(axis='y', labelcolor='blue')

plt.title('Signal Bandpass Filtered')
fig.tight_layout()
plt.show()
