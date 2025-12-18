import pandas as pd
import matplotlib.pyplot as plt

FILE_INPUT = r"D:\Esp-idf\Test_source\max30102_test\data_text\ECG_PPG_STM32\NguyenDangKhenh_ECG1.csv" 

data = pd.read_csv(FILE_INPUT, header=None)

# Lay cot dau tien 
y = data.iloc[:, 0]

# Ve
plt.figure(figsize=(12, 6))
plt.xlabel("Sample Index")
plt.ylabel("Value")
plt.title("Plot one column from CSV")
plt.grid(True)
plt.plot(y)
plt.show()