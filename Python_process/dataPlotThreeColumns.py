import matplotlib.pyplot as plt
import pandas as pd

# ===== ĐỌC CSV =====
file_path = r"D:\Esp-idf\Test_source\max30102_test\data_text\ECG_PPG_PCG1.csv"
data = pd.read_csv(file_path)

# Ép kiểu số
for col in ['ECG', 'IR', 'PCG']:
    data[col] = pd.to_numeric(data[col], errors='coerce')

data = data.dropna()

ecg = data['ECG'].to_numpy()
ir  = data['IR'].to_numpy()
pcg = data['PCG'].to_numpy()

# ===== VẼ =====
fig, axs = plt.subplots(3, 1, figsize=(14, 8), sharex=True)

axs[0].plot(ecg, color='red', linewidth=0.7)
axs[0].set_title("ECG")
axs[0].set_ylabel("Amplitude")
axs[0].grid(True)

axs[1].plot(ir, color='green', linewidth=0.7)
axs[1].set_title("PPG (IR)")
axs[1].set_ylabel("Amplitude")
axs[1].grid(True)

axs[2].plot(pcg, color='blue', linewidth=0.7)
axs[2].set_title("PCG")
axs[2].set_ylabel("Amplitude")
axs[2].set_xlabel("Sample index")
axs[2].grid(True)

plt.tight_layout()
plt.show()
