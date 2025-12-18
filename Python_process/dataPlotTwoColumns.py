import matplotlib.pyplot as plt
import pandas as pd

# Đọc dữ liệu từ file CSV
file_path = r"D:\Esp-idf\Test_source\max30102_test\data_text\ECG_PPG_STM32\Vinh_ECG_PPG.csv"  # Đặt tên file CSV của bạn
data = pd.read_csv(file_path)

# Chuyển đổi các cột RED và IR sang kiểu số
data['ECG'] = pd.to_numeric(data['ECG'], errors='coerce')
data['IR'] = pd.to_numeric(data['IR'], errors='coerce')

# Loại bỏ các hàng chứa giá trị không hợp lệ (nếu có)
data = data.dropna()

# Đọc các cột RED và IR
red_signal = data['ECG']  # Cột 'RED' chứa tín hiệu RED 
ir_signal = data['IR']    # Cột 'IR' chứa tín hiệu IR

# Vẽ biểu đồ
fig, ax1 = plt.subplots()

# Trục thứ nhất cho tín hiệu RED
ax1.set_xlabel('Index')  # Thay trục x thành chỉ số
ax1.set_ylabel('ECG', color='red')
ax1.plot(red_signal, label='ECG', color='red')
ax1.tick_params(axis='y', labelcolor='red')
ax1.set_ylim([min(red_signal) - 50, max(red_signal) + 50])  # Điều chỉnh dải tín hiệu

# Trục thứ hai cho tín hiệu IR
ax2 = ax1.twinx()
ax2.set_ylabel('IR', color='green')
ax2.plot(ir_signal, label='IR', color='green')
ax2.tick_params(axis='y', labelcolor='green')
ax2.set_ylim([min(ir_signal) - 50, max(ir_signal) + 50])  # Điều chỉnh dải tín hiệu

# Thêm tiêu đề
plt.title('Signal Visualization')

# Hiển thị
fig.tight_layout()
plt.show()
