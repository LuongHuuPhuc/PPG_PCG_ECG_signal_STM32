Bạn có bao giờ tự hỏi sao dữ liệu truyền qua UART đôi khi bị mất hoặc sai lệch, mặc dù đã cấu hình đúng tốc độ baud ?

UART (Universal Asynchronous Receiver/Transmitter) là giao tiếp nối tiếp phổ biến nhất trong hệ thống nhúng, nhưng đảm bảo tính toàn vẹn dữ liệu không phải lúc nào cũng đơn giản. Hãy cùng khám phá các kỹ thuật chống mất dữ liệu UART từ cơ bản đến nâng cao

## Các nguyên nhân dẫn đến mất dữ liệu UART
- Sai lệch tốc độ baud (baud rate mismatch)
- Nhiễu điện từ (EMI) trên đường truyền
- Tràn bộ đệm (buffer overflow)
- Mất đồng bộ khung (frame synchronization loss)
- Tín hiệu clock không ổn định

## Các kỹ thuật chống mất dữ liệu cơ bản
1. **PARITY BIT** - Cơ chế kiểm tra lỗi đơn giản
- UART hỗ trợ 3 chế độ parity: even, odd và none. Parity bit giúp phát hiện lỗi bit đơn giản nhưng không sửa được
- Theo tài liệu kỹ thuật của các vi điều khiển như STM32, ARM Cortex-M, việc cấu hình parity cần khớp giữa transmitter và receiver

2. **STOP BITS** và **OVERSAMPLING**
- Sử dụng 2 stop bits thay vì 1 giúp tăng độ tin cậy trong môi trường nhiễu.
- Kỹ thuật oversampling (lấy mẫu quá mức) - lấy nhiều mẫu ở giữa bit để xác định giá trị chính xác, được triển khai trong các UART hiện đại

## Các kỹ thuật nâng cao
1. HARDWARE FLOW CONTROL (RTS/CTS)
- Đây là giải pháp phần cứng hiệu quả nhất. RTS (Request to Send) và CTS (Clear to Send) tạo cơ chế bắt tay, đảm bảo receiver sẵn sàng trước khi transmitter gửi dữ liệu
- Theo datasheet của FTDI và Silicon Labs, flow control ngăn chặn hoàn toàn buffer overflow
	- Cơ chế này xuất phát từ chuẩn RS-232 (chuẩn vật lý do EIA đặt ra dùng để truyền tín hiệu nối tiếp giữa máy tính và thiết bị ngoại vi). Nó thường được dùng trong công nghiệp với mức điện áp cao và logic đảo (trái ngược với TTL), chức năng tương tự như UART.
	- Sau này đã được tinh chỉnh xuống điện áp 3.3V để có thể sử dụng được cho các hệ thống nhúng sử dụng UART
	- RTS: MCU -> thiết bị ngoài (MCU báo "Tôi sẵn sàng nhận dữ liệu)
	- CTS: Thiết bị -> MCU (Thiết bị báo "Tôi sẵn sàng nhận dữ liệu")
	
2. SOFTWARE FLOW CONTROL (X-On/X-Off)
- Khi không có chân RTS/CTS, có thể dùng ký tự điều khiển X-On (0x11) và X-Off (0x13) để tạm dừng/tiếp tục truyền
- Tuy nhiên, phương pháp này không an toàn với dữ liệu nhị phân

3. PROTOCOL LAYER CHECKSUM
- Thêm checksum (CRC, Fletcher hoặc Simple sum) ở lớp ứng dụng.
- Ví dụ mỗi gói dữ liệu kèm theo CRC-16, receiver tính toán và so sánh, nếu không khớp sẽ yêu cầu truyền lại

4. TIMEOUT và RETRANSMISION MECHANISM
- Triển khai bộ điếm thời gian (Timer) để phát hiện mất kết nối. Sau timeout, hệ thống tự động khởi tạo lại truyền thông hoặc gửi lại gói tin

## Thực tế triển khai
Trong hệ thống nhúng thực tế, kết hợp nhiều phương pháp cho độ tin cậy cao:
- Sử dụng Hardware Flow Control khi có thể
- Thêm CRC-16 cho các gói tin quan trọng
- Cấu hình timeout hợp lý (thường 100 - 500ms)
- Sử dụng DMA để giảm tải CPU và tránh mất dữ liệu do interrupt latency

## Kết luận
Không có giải pháp duy nhất cho tất cả trường hợp. Lựa chọn kỹ thuật phù hợp phụ thuộc vào:
- Tốc độ truyền dữ liệu
- Môi trường hoạt động
- Tài nguyên phần cứng có sẵn
- Yêu cầu về độ tin cậy của ứng dụng