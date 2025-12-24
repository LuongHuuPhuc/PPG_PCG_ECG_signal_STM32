import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.signal import butter, filtfilt

def bandpass_filter(signal, fs, lowcut, highcut, order=4):
    nyq = 0.5 * fs
    b, a = butter(order, [lowcut/nyq, highcut/nyq], btype='band')
    return filtfilt(b, a, signal)

def compute_fft(signal, fs, remove_dc=True):
    signal = np.asarray(signal, dtype=int)

    if remove_dc:
        signal = signal - np.mean(signal)  # Loai bo DC (lech baseline) de ve FFT cho Raw Signal

    N = len(signal)
    window = np.hanning(N)
    signal_w = signal * window

    fft_vals = np.fft.rfft(signal_w)    # 1-sided FFT
    mag = np.abs(fft_vals) / N
    mag[1:-1] *= 2                      # Scale 1-sided (Tru DC va Nyquist)
    freqs = np.fft.rfftfreq(N, d=1/fs)
    return freqs, mag

def plot_signal_and_fft(
    csv_path,
    column_name="ECG",   # <<< Default
    fs=1000,
    lowcut=5,
    highcut=40,
    title_prefix="Signal"
):
    # ===== ĐỌC CSV (CÓ HEADER) =====
    data = pd.read_csv(csv_path)

    if column_name not in data.columns:
        raise ValueError(f"Không có cột '{column_name}'. Các cột có: {list(data.columns)}")

    raw_signal = data[column_name].to_numpy(dtype=float)
    time = np.arange(len(raw_signal)) / fs

    # ===== LỌC =====
    filtered_signal = bandpass_filter(raw_signal, fs, lowcut, highcut)

    # ===== FFT =====
    f_raw, fft_raw = compute_fft(raw_signal, fs, remove_dc=True)
    f_filt, fft_filt = compute_fft(filtered_signal, fs, remove_dc=True)

    # ===== VẼ 2x2 =====
    plt.figure(figsize=(14, 8))

    # RAW SIGNAL
    plt.subplot(2, 2, 1)
    plt.plot(time, raw_signal, linewidth=0.7)
    plt.title(f"{title_prefix} – Raw ({column_name})")
    plt.xlabel("Time (s)")
    plt.grid(True)

    # FILTERED SIGNAL
    plt.subplot(2, 2, 2)
    plt.plot(time, filtered_signal, linewidth=0.7)
    plt.title(f"{title_prefix} – Filtered ({column_name})")
    plt.xlabel("Time (s)")
    plt.grid(True)

    # FFT RAW SIGNAL
    plt.subplot(2, 2, 3)
    plt.plot(f_raw, fft_raw, linewidth=0.7)
    plt.title("FFT – Raw")
    plt.xlabel("Frequency (Hz)")
    plt.grid(True)

    # FFT FILTERED SIGNAL
    plt.subplot(2, 2, 4)
    plt.plot(f_filt, fft_filt, linewidth=0.7)
    plt.title("FFT – Filtered")
    plt.xlabel("Frequency (Hz)")
    plt.grid(True)

    plt.tight_layout()
    plt.show()

# ===== GỌI HÀM =====
INPUT_PATH = r'D:\Esp-idf\Test_source\max30102_test\data_text\ECG_PPG_STM32\BuiNgocDat_ECG_PPG1.csv'

plot_signal_and_fft(
    INPUT_PATH,
    column_name="ECG",     # đổi cột ở đây
    fs=1000,
    lowcut=1,
    highcut=20,
    title_prefix="Signal"
)
