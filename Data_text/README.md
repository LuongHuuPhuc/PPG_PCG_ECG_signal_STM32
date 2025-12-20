# CÁC LOẠI NHIỄU TRONG ECG
## 1. BW - Baseline Wander (Trôi đường nền)
- Đặc điểm: 
	- Là dao động chậm của đường nền ECG
	- Làm cho toàn bộ tín hiệu bị nâng lên hoặc hạ xuống từ từ 
	- Không làm méo hình dạng QSR nhiều nhưng làm sai biên độ & khó phát hiện R-peak 
- Dải tần: 
	- Rất thấp: 0.05 ~ 0.5Hz
- Nguyên nhân: 
	- Hô hấp
	- Cử động cơ thể 
	- Thay đổi tiếp xúc điện cực - da
	- Thay đổi trở kháng da theo thời gian 
	
- Cách xử lý: 
	- High-pass Filter: Cut-off thường dùng 0.5Hz
	- Polynomial fitting/spline
	- Wavelet (DWT loại bỏ scale thấp)

## 2. 