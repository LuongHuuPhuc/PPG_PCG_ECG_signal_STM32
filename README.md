# Multimodal Cardiovascular Monitoring System (PPG-PCG-ECG signal) using STM32 MCU
→ Hệ thống theo dõi tim mạch đa phương thức sử dụng vi điều khiển STM32

## Hardware
- **MCU**: STM32F401CEUx
- **Sensor**:
  - ECG signal: **AD8232**
    - Khuếch đại tín hiệu điện tim (không có ADC riêng)
    - Data giao tiếp thông qua bộ ADC trong MCU (cụ thể STM32F401CEUx có bộ ADC 12-bits)
    - Tần số lấy mẫu: 1000Hz

  - PPG signal: **MAX30102**
    - Cảm biến sử dụng quang học để thu thập tín hiệu ADC từ môi trường
    - Sử dụng cơ chế FIFO để ghi-đọc dữ liệu (FIFO này tối đa 32 samples)
    - Có thể cấu hình Pulse Width (độ rộng ) của LED (4 mức **69us**, **118us**, **215us**, **411us**): Độ rộng xung càng dài thì ánh sáng phát ra nhiều hơn, tín hiệu phản xạ mạnh hơn, độ nhiễu giảm nhưng bù lại tốc độ lấy mẫu giảm, tăng điện năng tiêu thụ, bộ đệm FIFO đầy nhanh hơn. Project này sử dụng Pulse Width là 118us để có thể cố định được tần số lấy mẫu
    - Độ phân giải ADC (tối đa 18-bits)
    - Tần số lấy mẫu: 1000Hz
   
  - PCG signal: **INMP441**
    - Cảm biến sử dụng bộ mã hõa PCM cho tín hiệu âm thanh ADC từ môi trường thông qua giao thức I2S (Inter Integrate-circuit Sound)
    - Độ phân giải ADC: 24-bits
    - Tần số lấy mẫu: 8000Hz (tối thiểu của phần mềm) sau đó downsample về 1000Hz để đồng bộ
   
  - Logger: **CP2103 USB to TTL**
    - Module giao tiếp UART để xuất dữ liệu từ MCU ra monitor
    - Tốc độ Baud: 230400 baud
   
  - Debugger: **ST-LINK V2**

## Firmware

## SoSoftware
- TIMER, DMA, ... 
