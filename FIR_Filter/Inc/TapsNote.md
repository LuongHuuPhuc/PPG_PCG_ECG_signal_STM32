## LỢI VÀ HẠI CỦA SỐ TAPS TRONG BỘ LỌC
| Số Tabs | Lợi | Hại |
|---------|-----|-----|
| 32 tabs | - CPU nhanh, mỗi mẫu chỉ cần 32 phép nhân, rất nhẹ cho tính toán <br> - Độ trễ thấp (delay = N/2 = 16 samples ~ 2ms ở 8kHz) <br> - Ít RAM, ít Flash| - Độ dốc lọc kém -> stopband attenuation thấp -> Aliasing dễ lọt vào khi downsample 8x <br> - Transiton band rộng -> khó giữ biên tần chính xác (không sắc cạnh) <br> - Đôi khi không đủ sạch để lọc PCG trước khi decimation|
| 64 tabs | - Lọc sạch hơn nhiều <br> - Stopband attenuation cao hơn (40-70 dB) <br> - Transition band hẹp -> giữ sóng PCG tốt hơn <br> - Anti-aliasing tốt hơn -> Đặc biệt quan trọng khi Decimation 8x | - CPU tốn gấp đôi -> 64 phép nhân tích chập mỗi mẫu <br> - Độ trễ tăng (delay = 32 samples ~4ms) <br> - Tốn RAM gấp đôi |