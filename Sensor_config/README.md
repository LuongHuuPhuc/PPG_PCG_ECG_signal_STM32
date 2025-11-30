# More about INMP441 sensor and how to use it
- Datasheet: ![INMP441 datasheet](https://www.alldatasheet.com/view.jsp?Searchword=Inmp441%20datasheet&gad_source=1&gad_campaignid=1437346752&gclid=Cj0KCQiA5uDIBhDAARIsAOxj0CFfq9F_HSslDNU-THqFUq_C06z7uZzPhY20R8oG9GdGnRkCX6TjEKwaAhi8EALw_wcB)

## FEATURES
- Là cảm biến MEMS (Micro-Electro-Mechanical Systems - Hệ thống vi cơ điện tử) microphone hiệu năng cao, năng lượng thấp, đầu ra số
- Chuẩn giao thức I2S với độ chính xác dữ liệu 24-bit. I2S cho phép INMP441 kết nối trực tiếp với bộ xử lý số như DSPs hoặc MCU mà không cần đi qua bộ **Codec**
- SNR (Signal-to-Noise) cao (62 dBA)
- Độ nhạy môi trường -26 dBFS
- **Flat Frequency Response (đáp ứng tần số phẳng)** từ 60Hz -> 15kHz (khả năng tái tạo âm thanh môi trường lại một cách trung thực mà không làm thay đổi âm sắc gốc). Đây là giới hạn vật lý của màng micro + cấu trúc MEMS mà mic có thể thu tốt một cách tự nhiên mà không bị méo hay biến dạng quá nhiều
	> - Frequency Response (đáp ứng tần số) là phép đo định lượng của phổ tần số đầu ra của một hệ thống hoặc thiết bị khi phản ứng với một kích thích tần số từ môi trường. Có thể hiểu đơn giản là việc thiết bị hoặc hệ thống phản ứng thế nào khi ta đưa vào các tín hiệu có tần số khác nhau (có bị méo, mạnh yếu ra sao)

![Alt text](../Images/FreqResp.png)

- Dòng tiêu thụ thấp (tối đa 1.4mA với chế độ *Normal Operation*)
- Áp lực âm thanh có thể chịu đựng: 160 dB
- PSR (Power Supply Rejection - Loại bỏ nhiễu nguồn) lên đến -75 dBFS

![Alt text](../Images/PSR.png)

- Hỗ trợ bộ lọc số (Digital Signal Processor) phần cứng bên trong cảm biến. Cụ thể:
	- **Group Delay (Độ trễ nhóm)**: *17.2 / fs (s)* - Đây là đỗ trễ của tín hiệu khi đi qua bộ lọc số tích hợp bên trong mic (DSP filter). Đây là độ trễ nội bộ của mic, không phải của MCU hay DMA. Nó rất quan trọng khi muốn đồng bộ các cảm biến với nhau. Ví dụ với 2 tần số lấy mẫu:
		- fs = 48 kHz -> Group delay ~ 0.36ms (36us)
		- fs = 16 kHz -> Group delay = 1.078ms (1078us)
	- **Pass band Ripple (Độ gợn trong dải thông)**: ±0.04 dB. Bộ lọc bên trong mic cho tín hiệu trong dải tần số sử dụng méo rất nhỏ, gần như không đáng kể. Tín hiệu được giữ nguyên biên độ gần như hoàn toàn
	- **Stop band Attenuation (Độ suy giảm dải dừng)**: 60 dB. Bộ lọc sẽ giảm ít nhất 60 dB ở các tần số cao hơn dải thông (noise, nhiễu aliasing - nhiễu răng cưa). Tức là tần số nằm ngoài dải mic sẽ bị giảm 1000 lần về biên độ, giúp tín hiệu thu được sau mic sạch hơn và giảm aliasing khi ghi âm
	- **Pass band (Dải thông)**: *fs x 0.423*. Dải thông này là phạm vi lọc của DSP bên trong mic, tín hiệu đi qua sẽ không bị méo. Nếu vượt qua ngưỡng của Pass band thì nó sẽ bị suy giảm bởi Stop band Attenuation ở trên. Thông dải lọc phụ thuộc vào tần số lấy mẫu. Ví dụ:
		- fs = 48 kHz -> Pass band = 20.3 kHz (Mic cho tín hiệu tốt đến ~20 kHz) -> Thu âm full audio
		- fs = 16 kHz -> Pass band = 6.77 kHz (Mic cho tín hiệu tốt đến ~6.77 kHz) -> Phù hợp cho giọng nói

- Ngoài ra, nhà sản xuất cũng khuyến cáo thiết bị nhạy cảm với tĩnh điện (ESD - Electrostatic Discharge). Tuy bên trong cũng được tích hợp mạch bảo vệ độc quyền nhưng thiết bị vẫn có thể bị ảnh hưởng bởi ESD năng lượng cao
- Theo datasheet, để đạt hiệu suất tốt nhất và tránh các hiện tượng ký sinh tiềm ẩn (Parastic Artifacts), rất khuyến nghị đặt 1 tụ điện gốm loại 0.1uF hoặc tốt hơn ở giữa chân Vcc và chân Gnd. Càng gần càng tốt
## CHUẨN I2S TRONG INMP441 HOẠT ĐỘNG THẾ NÀO ? 
- Cổng nối tiếp kiểu slave là I2S, 24-bits, sử dụng định dạng bù hai (**two complement**). Phải có 64 chu kỳ xung nhịp SCK trong mỗi khung WS stereo hoặc 32 chu kỳ SCK trong nửa khung WS mon cho mỗi từ dữ liệu.
- Chân điều khiển **L/R** xác định liệu INMP441 có xuất dữ liệu ở kênh trái (Left) hay kênh trái (Right)
- Độ dài dữ liệu (data word length): Độ dài từ dữ liệu xuất là 24-bit trên mỗi kênh. INMP441 luôn có 64 chu kỳ xung nhịp cho mỗi từ dữ liệu (**f_sck = 64 x f_ws**)
- Định dạng Từ dữ liệu (Data-word Format): Định dạng dữ liệu mặc định là I2S (bù hai), MSB là bit đầu tiên (từ trái sang phải). 
	- Trong định dạng này, MSB của mỗi từ bị trì hoãn 1 chu kỳ SCK kể từ lúc bắt đầu mỗi nửa khung (trước khi bắt đầu chu kỳ SCK cho MSB thì sẽ bị trễ 1 khung đầu)

![Alt_text](../Images/Format.png)

- I2S có 3 loaị clock chính: 

|Clock | Tên khác| Vai trò|
|------|---------|--------|
|BCLK (SCK)| Bit clock | Clock để dịch từng bit dữ liệu PCM|
|LRCLK (WS)| Frame clock | Xác định mẫu trái/phải -> Chính là sample rate fs|
|MCLK | Master clock | Clock nền tốc độ cao cho ADC/DAC được cấp từ bên ngoài cho thiết bị Audio (vd: MCU)|

- **Master clock (MCLK)** của I2S là clock "gốc" được cấp cho các thiết bị audio như ADC, DAC hoặc Codec để chúng hoạt động ổn định từ đó tạo ra các clock nội bộ (hay còn được gọi là **Oversampling Clock**) để chạy ADC/DAC/Codec hay clock cho BLCK/LRCLK (Bản chất I2S)
- Nhiều thiết bị audio cần 1 clock nhanh và ổn định. Giá trị của MCLK thường là bội số của:
	- 256 x Fs
	- 384 x Fs
	- 512 x Fs
	- Chúng là các hệ số của chuẩn công nghiệp cho clock nội bộ của các ADC/DAC audio. Chúng yêu cầu oversampling và digital filtering, vốn chạy nhanh gấp hàng trăm lần tần số lấy mẫu.
	
- Riêng INMP441 là ngoại lệ. Nó không cần **MCLK truyền thống**. Clock I2S như chân BCLK và LRCLK đến trực tiếp từ MCU để nó hoạt động, bởi vì:
	- INMP441 không phải DAC, không phải audio codec mà lại 1 microphone MEMS với ADC sigma-delta tích hợp sẵn. 
	- Không cần clock nội bộ (oversampling clock) từ bên ngoài, tự tạo bằng mạch điều chế sigma-delta 
	- Chỉ cần biết khi nào xuất dữ liệu -> Do BCLK và LRCLK điều khiển
	- INMP441 chỉ cần MCU cấp tempo (nhịp), nhưng không cần MCU cấp clock xử lý nội bộ

- INMP441 tự sinh ra clock nội bộ (oversampling clock) bên trong:
	- Clock này được MCU cấp trực tiếp qua đầu vào chính BCLK và LRCLK rồi từ đó INMP441 sẽ dùng PLL (Vòng khóa pha) bên trong mạch để tạo ra clock nội bộ (Oversampling clock) cho ADC chạy 
	- Thường là hàng MHz (4 - 6 MHz) đề dùng cho sigma-delta ADC và sau đó là decimate xuống theo Fs, theo LRCLK
	
- Một frame của I2S gồm: 
	- 1 cạnh của LRCLK (Left)
	- N bit trên SCK (BCLK)
	- 1 cạnh của LRCLK (Right)
	- N bit tiếp theo
- Tổng cộng 2 x N bit mỗi mẫu. Với INMP441: N = 24 hoặc 32 bit (thường 32-bits/frame)

### Quan hệ giữa BCLK, LRCLK và Sample rate
- Công thức chuẩn I2S:
```ini
BCLK = LRCLK(Fs) x BitsPerSample x Channels
```

- Với 2 kênh (Left + Right):
```ini
BCLK = Fs x BitsPerSample x 2
```
- Thông thường BitsPerSample = 32
- Ví dụ muốn Fs = 16kHz: 
```makefile
BCLK = 16000 × 32 × 2
     = 1,024,000 Hz (1.024 MHz)
```

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
	
![Alt text](../Images/I2S_io_char.png)

## SERIAL DATA PORT TIMING SPECIFICATIONS
- Là thông số thời gian giao tiếp I2S. Thể hiện thời gian giữ mức cao - thấp của xung clock được cấp cho mic. Cụ thể
	- t_SCH - thời gian SCK HIGH phải ≥ 50ns
	- t_SCL - thời gian SCK LOW phải ≥ 50ns
	- Thời gian SCK tối thiểu (chỉ tính thời gian xung ở HIGH + xung ở LOW): t_SCH + t_SCL ≥ 100ns. 
	- Chu kỳ clock SCK (thời gian hoàn thành 1 chu kỳ xung) t_SCP ~ 312ns. Đây là tần số bit clock mà mic có thể xử lý
	- Tần số cấp cho chân SCK hoạt động (hay BLCK - clock để gửi từng bit dữ liệu I2S) nằm trong phạm vị 0.5MHz -> 3.2MHz (MCU làm master clock để tạo xung trong dải này cho cảm biến)
	- t_WSS (WS setup): thời gian chân WS phải ổn định **trước** cạnh đầu của SCK (giá trị ≥ 0ns)
	- t_WHS (WS Hold time): Thời gian WS phải giữ ổn định sau cạnh SCK -> WS cần giữ 1 chút để mic đọc đúng frame (giá trị ≥ 20ns)
	- f_WS (WS frequency): là tần số LRCLK hay tần số clock của Left/Right (tần số này cũng chính là sample rate). Đây là clock xác định:
		- Xung lên -> Kênh trái
		- Xung xuống -> Kênh phải
		- FS = LRCLK = số frame/giây mà INMP441 gửi ra

![Alt text](../Images/I2S_timing.png)

## PIN FUNCTION DESCRIPTIONS

|PIN|NAME|FUNCTION|
|---|----|--------|
|1|SCK (Serial-Data Clock)|Chân cấp xung clock để dịch bit dữ liệu cho I2S (clock này được cấp từ MCU)|
|2|SD (Serial-Data)|Ngõ dữ liệu xuất ra của I2S. Chân này sẽ chuyển sang trạng thái ba mức (**tri-state**) khi không chủ động xuất ra dữ liệu cho kênh tương ứng. Đường tín hiệu SD nên có 1 điện trở kéo xuống 100k Ohm để xả điện tích dư thừa về GND trong trường hợp trên bus I2S có nhiều microphones cùng lúc vì tại một thời điểm, chỉ có 1 microphones được phép kéo đường SD lên/xuống. Các microphones khác phải đưa chân SD về trạng thái **tri-state** để tránh xung đột tín hiệu|
|3|WS (Word Select)| Chân clock cho việc chọn dữ liệu được đọc kênh trái hay bên phải - cũng chính là LRCLK (clock được cấp từ MCU)|
|4|LR (Left Right)| Chân chọn kênh trái/phải. Khi chân này LOW (nối với GND) -> data sẽ được đọc kênh bên trái. Khi chân này HIGH (nối với VCC) -> data sẽ được đọc kênh ở bên phải|

### Chế độ xuất dữ liệu (Data output mode)
- Chân SD ở trạng thái trở kháng cao (tri-stated hay HIGH-Z) khi nó không tích cực điều khiển dữ liệu I2S. Chân SD chuyển sang trạng thái HighZ ngay lập tức sau khi LSB được xuất, để một microphone khác có thể điều khiển đường dữ liệu chung (nếu có)
- Đường truyền SD nên có 1 điện trở **pull-down** để xả tích điện trong trên đường dây trong khoảng thời gian mà tất cả các microphone trên bus đều chuyển sang trạng thái trở kháng cao, cũng như để tránh hiện tương GPIO *floating* và xác định trạng thái của chân đó

### Trạng thái tri-state là gì ?
- Là trạng thái trở kháng cao hay High Impedence State - viết tắt là **HighZ** hoặc **tri-state**
- Đây là trạng thái thứ ba của một chân digital (ngoài HIGH và LOW)
- Khi một chân ở trạng thái HighZ, mạch điện tử bên trong điều khiển chân đó sẽ ngắt kết nối khỏi đường dây (bus) hoặc chuyển sang 1 trạng thái có trở kháng đầu ra cực kỳ lớn
- Chân đó sẽ **không chủ động** kéo điện áp đường đây lên mức HIGH (nối với Vcc) hoặc xuống mức LOW (nối với GND). Do không bị kéo lên hay kéo xuống bởi thiết bị, đường dây được nối với chân HighZ này sẽ ở trạng thái *floating*. Trạng thái này có nghĩa là điện áp trên đường dây không được xác định rõ ràng và dễ bị ảnh hưởng bởi nhiễu điện từ
- Mục đích là để tránh xung đột bus vì tại 1 thời điểm chỉ có 1 thiết bị được phép truyền dữ liệu -> Khi đó các thiết bị khác phải ở trạng thái HighZ



