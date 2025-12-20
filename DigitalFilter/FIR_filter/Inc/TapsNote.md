## ĐÁP ỨNG TẦN SỐ CỦA FIR NHÌN RA SAO ? 
Với FIR lowpass, đáp ứng tần số có 3 phần: 
- (1) Dải thông (Passband): Tần số thấp hơn tần số cắt $f_{c}$. Mục tiêu: giữ nguyên biên độ (~0dB)
- (2) Vùng chuyển tiếp (Transition band): Nằm giữa passband và stopband. Mục tiêu càng hẹp thì filter cắt càng sắt
- (3) Dải chắn: Tần số cao hơn $f_{c}$. Mục tiêu: giảm biên độ (suy hao càng nhiều càng tốt)
- Hình ảnh ví dụ đáp ứng FIR thực tế sẽ như thế này: 

![Alt_text](../../../Images/Frequency-response-of-Lowpass-FIR-filter-4-Filter-Design-using-FDATool-of-MATLAB-The.png)

- Hình ảnh đáp ứng tần số cho tần số 8000Hz với tần số cắt 450Hz kết hợp cửa sổ hamming:

![Alt_text](../../../Images/FrequencyResponse128taps.png)

- Ở passband có ripple nhỏ
- Ở stopband có side-lobes (ripple) dạng răng cưa
- Ở transition band: mở rộng tùy số taps & cửa sổ

### Vì sao lại sinh ra Ripple trong đáp ứng tần số ? 
- Lý do gốc: Do cắt ngắn sinc -> Sinh nhiễu Gibbs 

## FIR TRONG MIỀN THỜI GIAN - TẦN SỐ (CỐT LÕI) 
- Vì FIR (Finite Impluse Response) là bộ lọc số có **đáp ứng xung hữu hạn** nên số lượng giá trị $h[k]$ đầu vào của bộ lọc cũng có giới hạn
- Tức là khi bạn đưa một xung đơn vị $δ[n]$, tín hiệu đầu ra chỉ kéo dài trong N mẫu rồi về 0
- FIR trong **miền thời gian** hoạt động theo **phép tích chập** giữa tín hiệu đầu vào và đáp ứng xung của bộ lọc 

$$y[n] = (x * n)[n] = \sum_{k=0}^{N-1} h[k]x[n - k]$$

- Trong đó: 
	- $x[n]$ là tín hiệu đầu vào
	- $y[n]$ là tín hiệu đầu ra
	- $h[k]$ là hệ số của bộ lọc (cũng chính là đáp ứng xung)
	- N: số taps (số hệ số)
- Nói ngắn gọn: **FIR trong miền thời gian = lấy một đoạn mẫu của tín hiệu vào, nhân với các hệ số $h[k]$ rồi cộng lại**

### Từ tích chập đến nhân phổ (tính chất Fourier) 
- Trong xử lý tín hiệu, có một tính chất cực kỳ quan trọng:
> **Tích chập trong miền thời gian chính là Nhân trong miền tần số**

- Cụ thể nếu ta biến đổi Fourier rời rạc theo thời gian (Discrete Time Fourier Transform) cho các tín hiệu: 
	- $X(e^{j\omega})$ là phổ của $x[n]$
	- $H(e^{j\omega})$ là phổ của $h[n]$
	- $Y(e^{j\omega})$ là phổ của $y[n]$

thì: 

$$Y(e^{j\omega}) = H(e^{j\omega}) \cdot X(e^{j\omega})$$


-　Đây chính là biểu thức biến đổi từ công thức **tích chập** của FIR theo **miền thời gian** sang **miền tần số**.
- Điều này nói rằng:
	- $H(e^{j\omega})$ đóng vai trò như **"mặt nạ tần số"**
	- Tần số nào mà H gần 1 -> **được cho qua**
	- Tần số nào H gần 0 -> **bị triệt** 
- Vì vậy, dải tần **low-pass/high-pass/band-pass/band-stop** thực chất nằm ở hình dạng của mặt nạ $H(e^{j\omega})$ và $H(e^{j\omega})$ lại được quyết định hoàn toàn bởi các hệ số $h[k]$

### Đáp ứng tần số $H(e^{j\omega})$ đến từ đâu ? 
- Trong miền tần số, đáp ứng tần số của FIR được lấy bằng biến đổi **Fourier** rời rạc theo miền thời gian (DTFT) của đáp ứng xung $h[k]$:

$$ H(e^{j\omega})=\sum_{k=0}^{N-1} h[k]e^{(-j\omega k)} $$

- Ý nghĩa: 
	- Mỗi hệ số $h[k]$ đóng góp vào việc tạo hình dáng "mặt nạ" trong miền tần số
	- Đổi $h[k]$ -> đổi $H(e^{j\omega})$ chính là đổi loại lọc và dải tần
	
> FIR không biết đâu là HPF/LPF/BPF/BSF, nó chỉ biết dùng $h[k]$ để tạo ra $H$, rồi nhân với phổ đầu vào

- Bạn có thể hình dung trong miền thời gian như sau: 
	- Ta lấy N mẫu gần nhất của tín hiệu đầu vào: $x[n], x[n - 1], x[n -2],...,x[n - (N- 1)]$
	- Nhân nó với lần lượt các hệ số đáp ứng xung: $h[0], h[1], h[2],...,h[N -1]$
	- Sau đó cộng chúng lại để có được tín hiệu đầu ra $y[n]$
- Nếu $h[k]$ ưu tiên cấu trúc tương ứng với tần số thấp -> Sinh ra bộ lọc **low-pass**
- Nếu $h[k]$ ưu tiên cấu trúc tần số cao -> Sinh ra bộ lọc **high-pass**

### Tại sao cần Window trong thiết kế FIR ? 
- Bộ lọc FIR lý tưởng có đáp ứng xung vô hạn 
- Ví dụ low-pass lý tưởng có $h[n]$ dạng sinc, kéo dài vô hạn
- Nhưng FIR yêu cầu hữu hạn, nên ta phải cắt ngắn sinc: 
	- Cắt ngắn thô -> Gây gợn (ripples) mạnh trong miền tần số (hiện tượng Gibbs)  
	- Giải pháp: Nhân thêm Window để làm mượt việc cắt: 

$$ h_{FIR}[n] = h_{ideal} \cdot w[n] $$

- Trong đó $w[n]$ là hàm cửa sổ (Hann/Hanning, Hamming, Blackman,...)
- **Trade-off** quan trọng:
	- Window càng "mượt" -> Giảm gợn (sibe-lobe thấp)
	- Nhưng thường làm transition band rộng hơn (cắt không gắt)

## LỢI VÀ HẠI CỦA SỐ TAPS TRONG BỘ LỌC
| Số Tabs | Lợi | Hại |
|---------|-----|-----|
| 32 tabs | - CPU nhanh, mỗi mẫu chỉ cần 32 phép nhân, rất nhẹ cho tính toán <br> - Độ trễ thấp (delay = N/2 = 16 samples ~ 2ms ở 8kHz) <br> - Ít RAM, ít Flash| - Độ dốc lọc kém -> stopband attenuation thấp -> Aliasing dễ lọt vào khi downsample 8x <br> - Transiton band rộng -> khó giữ biên tần chính xác (không sắc cạnh) <br> - Đôi khi không đủ sạch để lọc PCG trước khi decimation|
| 64 tabs | - Lọc sạch hơn nhiều <br> - Stopband attenuation cao hơn (40-70 dB) <br> - Transition band hẹp -> giữ sóng PCG tốt hơn <br> - Anti-aliasing tốt hơn -> Đặc biệt quan trọng khi Decimation 8x | - CPU tốn gấp đôi -> 64 phép nhân tích chập mỗi mẫu <br> - Độ trễ tăng (delay = 32 samples ~4ms) <br> - Tốn RAM gấp đôi |
