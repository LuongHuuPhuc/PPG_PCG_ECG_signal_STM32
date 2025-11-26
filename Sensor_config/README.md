# More about INMP441 sensor and how to use it
- Datasheet: ![https://www.alldatasheet.com/view.jsp?Searchword=Inmp441%20datasheet&gad_source=1&gad_campaignid=1437346752&gclid=Cj0KCQiA5uDIBhDAARIsAOxj0CFfq9F_HSslDNU-THqFUq_C06z7uZzPhY20R8oG9GdGnRkCX6TjEKwaAhi8EALw_wcB](INMP441 datasheet)

## Features
- Là cảm biến MEMS (Micro-Electro-Mechanical Systems - Hệ thống vi cơ điện tử) microphone hiệu năng cao, năng lượng thấp, đầu ra số
- Chuẩn giao thức I2S với độ chính xác dữ liệu 24-bit. I2S cho phép INMP441 kết nối trực tiếp với bộ xử lý số như DSPs hoặc MCU mà không cần đi qua bộ **Codec**
- SNR (Signal-to-Noise) cao (62 dBA)
- Độ nhạy môi trường -26 dBFS
- Flat Frequency Response (đáp ứng tần số phẳng) từ 60Hz -> 15kHz (khả năng tái tạo âm thanh môi trường lại một cách trung thực mà không làm thay đổi âm sắc gốc). Đây là giới hạn vật lý của màng micro + cấu trúc MEMS mà mic có thể thu tốt một cách tự nhiên mà không bị méo hay biến dạng quá nhiều
	> - Frequency Response (đáp ứng tần số) là phép đo định lượng của phổ tần số đầu ra của một hệ thống hoặc thiết bị khi phản ứng với một kích thích tần số từ môi trường. Có thể hiểu đơn giản là việc thiết bị hoặc hệ thống phản ứng thế nào khi ta đưa vào các tín hiệu có tần số khác nhau (có bị méo, mạnh yếu ra sao)

![../Images/FreqResp.png](Alt text)

- Dòng tiêu thụ thấp (tối đa 1.4mA với chế độ *Normal Operation*)
- Áp lực âm thanh có thể chịu đựng: 160 dB
- PSR (Power Supply Rejection - Loại bỏ nhiễu nguồn) lên đến -75 dBFS

![../Images/PSR.png](Alt text)

- Hỗ trợ bộ lọc số (Digital Signal Processor) phần cứng bên trong cảm biến. Cụ thể:
	- Group Delay (Độ trễ nhóm): 17.2 / fs (s) - Đây là đỗ trễ của tín hiệu khi đi qua bộ lọc số tích hợp bên trong mic (DSP filter). Đây là độ trễ nội bộ của mic, không phải của MCU hay DMA. Nó rất quan trọng khi muốn đồng bộ các cảm biến với nhau. Ví dụ với 2 tần số lấy mẫu:
		- fs = 48 kHz -> Group delay ~ 0.36ms (36us)
		- fs = 16 kHz -> Group delay = 1.078ms (1078us)
	- Pass band Ripple (Độ gợn trong dải thông): ±0.04 dB. Bộ lọc bên trong mic cho tín hiệu trong dải tần số sử dụng méo rất nhỏ, gần như không đáng kể. Tín hiệu được giữ nguyên biên độ gần như hoàn toàn
	- Stop band Attenuation (Độ suy giảm dải dừng): 60 dB. Bộ lọc sẽ giảm ít nhất 60 dB ở các tần số cao hơn dải thông (noise, nhiễu aliasing - nhiễu răng cưa). Tức là tần số nằm ngoài dải mic sẽ bị giảm 1000 lần về biên độ, giúp tín hiệu thu được sau mic sạch hơn và giảm aliasing khi ghi âm
	- Pass band (Dải thông): fs x 0.423. Dải thông này là phạm vi lọc của DSP bên trong mic, tín hiệu đi qua sẽ không bị méo. Nếu vượt qua ngưỡng của Pass band thì nó sẽ bị suy giảm bởi Stop band Attenuation ở trên. Thông dải lọc phụ thuộc vào tần số lấy mẫu. Ví dụ:
		- fs = 48 kHz -> Pass band = 20.3 kHz (Mic cho tín hiệu tốt đến ~20 kHz) -> Thu âm full audio
		- fs = 16 kHz -> Pass band = 6.77 kHz (Mic cho tín hiệu tốt đến ~6.77 kHz) -> Phù hợp cho giọng nói

- Ngoài ra, nhà sản xuất cũng khuyến cáo thiết bị nhạy cảm với tĩnh điện (ESD - Electrostatic Discharge). Tuy bên trong cũng được tích hợp mạch bảo vệ độc quyền nhưng thiết bị vẫn có thể bị ảnh hưởng bởi ESD năng lượng cao

## I2S Digital INPUT/OUTPUT CHARACTERISTICS 
- Digital Input (Đầu vào số) bao gồm các chân L/R, WS, SCK. Phụ thuộc vào điện áp đầu vào (Vdd):
	- Input Voltage High (V_ih): Điện áp tối thiểu để mic hiểu là logic 1. Từ min: 0.7 x Vdd -> max: Vdd
		- Ví dụ: Vdd = 3.3V -> V_ih ≥ 2.31V
	- Input Volatage Low (V_il): Điện áp tối đa để mic hiểu là logic 0. Từ min: 0 -> max: 0.25 x Vdd
		- Ví dụ: Vdd = 3.3V -> V_il ≤ 0.825V
	- MCU phải đảm bảo mức điện áp đầu ra BCLK/LRCLK đạt mức này để INMP441 nhận đúng tín hiệu

- SD digital input (đọc tên hơi nhầm, thực ra là đặc tính output vì mic xuất dữ liệu SD ra cho MCU). Bảng này mô tả:
	- Điện áp output khi ở mức 0 (VOL)
	- Điện áp output khi ở mức 1 (VOH)
	- Ứng với từng mức Vdd
	- Ví dụ: nếu Vdd = 3.3V và tải nhẹ:
		- VOL ≤ 0.3 x Vdd -> Khoảng 0.99V
		- VOH ≥ 0.9 x Vdd -> Khoảng 2.97V
	- Tức là chân SD sẽ xuất chuẩn mức CMOS
	
![../Images/I2S_io_char.png]()

## SERIAL DATA PORT TIMING SPECIFICATIONS
- Là thông số thời gian giao tiếp I2S. Thể hiện thời gian giữ mức cao - thấp của xung clock được cấp cho mic. Cụ thể
	- t_SCH - thời gian SCK HIGH phải ≥ 50ns
	- t_SCL - thời gian SCK LOW phải ≥ 50ns
	- Thời gian SCK tối thiểu (chỉ tính thời gian xung ở HIGH + xung ở LOW): t_SCH + t_SCL ≥ 100ns. 
	- Chu kỳ clock SCK (thời gian hoàn thành 1 chu kỳ xung) t_SCP ~ 312ns. Đây là tần số bit clock mà mic có thể xử lý
	- Tần số cấp cho chân SCK hoạt động nằm trong phạm vị 0.5MHz -> 3.2MHz (MCU làm master clock để tạo xung trong dải này cho cảm biến)
	- t_Wss - WS setup: thời gian chân WS phải ổn định **trước** cạnh đầu của SCK
![../Images/I2S_timing.png]()



