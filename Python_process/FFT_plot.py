import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

def plot_signal_and_fft(
    csv_path,
    column_index=0,
    fs=1000,
    dtype=np.int16,
    title_prefix="Signal"
):
    # ===== ĐỌC FILE CSV =====
    data = pd.read_csv(csv_path)

    if column_index >= data.shape[1]:
        raise ValueError(f"CSV chỉ có {data.shape[1]} cột, không có cột {column_index}")

    signal = data.iloc[:, column_index].to_numpy(dtype=dtype)

    # ===== THAM SỐ =====
    N = len(signal)
    T = 1 / fs
    time = np.arange(N) * T

    # ===== WINDOWING =====
    window = np.hanning(N)
    windowed_signal = signal * window

    # ===== FFT (1-sided) =====
    fft_values = np.fft.fft(windowed_signal)
    fft_magnitude = np.abs(fft_values) / N
    fft_magnitude = fft_magnitude[:N // 2] * 2

    freqs = np.fft.fftfreq(N, d=T)[:N // 2]

    # ===== VẼ =====
    plt.figure(figsize=(12, 8))

    # Time domain
    plt.subplot(2, 1, 1)
    plt.plot(time, signal, linewidth=0.7, color='blue')
    plt.title(f"{title_prefix} – Time Domain (Column {column_index})")
    plt.xlabel("Time (s)")
    plt.ylabel("Amplitude")
    plt.grid(True)

    # Frequency domain
    plt.subplot(2, 1, 2)
    plt.plot(freqs, fft_magnitude, linewidth=0.7, color='red')
    plt.title(f"{title_prefix} – Frequency Domain (FFT)")
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("Magnitude")
    plt.grid(True)

    plt.tight_layout()
    plt.show()

INPUT_PATH = r'D:\Esp-idf\Test_source\max30102_test\data_text\ECG_PPG_STM32\NgoTuanAnh_ECG_PPG.csv'
TITLE = "FFT for ECG"
plot_signal_and_fft(INPUT_PATH, column_index=0, fs=1000, title_prefix=TITLE)