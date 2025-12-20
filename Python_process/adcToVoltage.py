import pandas as pd
import matplotlib.pyplot as plt

# ======================
# CẤU HÌNH
# ======================
INPUT_CSV  = "D:\Esp-idf\Test_source\max30102_test\data_text\ECG_PPG_STM32\BuiNgocDat_ECG_PPG1.csv" 
OUTPUT_CSV = "D:\Esp-idf\Test_source\max30102_test\data_text\ADCToVoltage\BuiNgocDat_ECG_PPG1_G100.csv"

ADC_BITS = 12
ADC_MAX  = (2 ** ADC_BITS) - 1
VREF_V   = 3.3          # V
GAIN     = 100.0       # gain tổng AD8232 module (100 - 1100)

# ======================
# ĐỌC FILE CSV
# ======================
df = pd.read_csv(INPUT_CSV)

# ======================
# ADC -> ECG (mV)
# ======================
# Điện áp tại ADC (V)
df["Vout_V"] = df["ECG"] * VREF_V / ADC_MAX

# Trừ REFOUT để đưa về AC quanh 0V
df["Vac_out_V"] = df["Vout_V"] - (VREF_V / 2)

# Chia gain để ra ECG thật (mV)
df["ECG_mV"] = (df["Vac_out_V"] / GAIN) * 1000

# ======================
# LƯU FILE MỚI
# ======================
df.to_csv(OUTPUT_CSV, index=False)

print(f"Saved converted ECG file: {OUTPUT_CSV}")

# ======================
# VẼ BIỂU ĐỒ
# ======================
plt.figure(figsize=(12, 4))
plt.plot(df["ECG_mV"], linewidth=0.8)
plt.xlabel("Sample")
plt.ylabel("ECG Voltage (mV)")
plt.title("ECG Signal (Converted from ADC, Gain = 100)")
plt.grid(True)
plt.tight_layout()
plt.show()
