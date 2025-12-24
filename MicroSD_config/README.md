# GIỚI THIỆU VỀ FATFS
- FatFS (File Allocation Table File System) là một thư viện hệ thống tập tin nhỏ gọn, mã nguồn C, được phát triển để chuyên dùng cho MCU và hệ thống nhúng
- FatFS cho phép MCU: 
	- Tạo/mở/ghi/đọc file
	- Làm việc với SD card, USB MSC, NOR/NAND flash
	- Tương thích với PC (Window/Linux)
- FatFS không phải là **driver**, nó không điều khiển phần cứng SD Card.
- Nó chỉ xử lý: 
	- Cấu trúc thư mục, quản lý Cluster, đọc/ghi file logic
	- Nó phụ thuộc vào driver thấp hơn để gửi lệnh SPI/SDIO
	- Đọc/ghi block 512 bytes
	
## Kiến trúc của FATFS trong STM32

```mathematica
┌───────────────────────────┐
│        User Application   │
│   (Logger, Data Recorder) │
└─────────────▲─────────────┘
              │ f_write(), f_open()
┌─────────────┴─────────────┐
│           FATFS           │
│  (File system layer)      │
└─────────────▲─────────────┘
              │ disk_read/write()
┌─────────────┴─────────────┐
│     Disk I/O Layer        │
│   (user_diskio.c)         │
└─────────────▲─────────────┘
              │ SPI / SDIO
┌─────────────┴─────────────┐
│      SD Card Hardware     │
└───────────────────────────┘
```
## 1. FAT FILE SYSTEM (CƠ BẢN)
- FATFS hỗ trợ: 
	- FAT12, FAT16, FAT32 (phổ biến nhất với SD Card)
- Cấu trúc cơ bản:
	- Sector (Vùng): 512 byte (đơn vị thấp nhất). 
		- Là đơn vị lưu trữ dữ liệu nhỏ nhất, giống như những ngăn nhỏ trên ổ cứng hay USB
		- Mỗi sector có địa chỉ riêng để máy tính có thể đọc/ghi dữ liệu 1 cách độc lập.
		- Khi file không lấp đầy một sector, phần còn lại được điền bằng số 0
	- Cluster (Cụm): Nhiều Sector. Là đơn vị quản lý của hệ điều hành
		- Khi lưu file, hệ điều hành sẽ cấp cho nó 1 số lượng cluster nhất định, mỗi cluster có kích thước cố định
		- Kích thước Cluster lớn thường giúp tăng tốc độ truy cập cho các file lớn vì ít phải truy cập nhiều Cluster khác.
		- Tuy nhiên các file nhỏ hơn lại gây lãng phí dung lượng hơn
	- FAT Table: Bảng liên kết các Cluster
	- Directory Entry: Metadata của file
	
## 2. Cơ chế ghi File trong FATFS
1. Mount file system
- Đọc thông tin FAT từ SD card vào RAM
```c
f_mount(&fs, "", 1);
```

2. Mở file 
- Nếu file chưa tồn tại -> tạo mới
- Nếu đã tồn tại -> Mở lại
```c
f_open(&file, "data.csv", FA_OPEN_ALWAYS | FA_WRITE);
```

3. Ghi dữ liệu
- Dữ liệu chưa chắc đã ghi xuống ngay SD card
- FatFS sử dụng Sector buffer (cache) trong RAM
```c
f_write(&file, buffer, len, &bw);
```

4. Đồng bộ dữ liệu (quan trọng)
- Ép cache ghi xuống SD card
- Nếu reset MCU mà không sync -> có thể mất data cuối
```c
f_sync(&file);

```

5. Đóng file
- Tự động gọi `f_sync()`
```c
f_close(&file);

```  

# KIẾN TRÚC CHO MICROSD_CONFIG LIB
- Mục đích của việc viết thư viện này là để: 
	- Che FATFS
	- Dùng thống nhất cho 3 cảm biến 
	- Không phụ thuộc trực tiếp vào `f_open()`, `f_write` khắp project

## 1. Phân tầng 

```csharp
┌────────────────────────────┐
│ Application (Sensor task)  │
│  - ECG task                │
│  - PPG task                │
│  - PCG task                │
└────────────▲───────────────┘
             │
┌────────────┴───────────────┐
│  SD Storage Library (YOU)  │  ← BẠN VIẾT
│  - MicroSD_Init()          │
│  - MicroSD_Open()          │
│  - MicroSD_WriteLine()     │
│  - MicroSD_Flush()         │
│  - MicroSD_Close()         │
└────────────▲───────────────┘
             │
┌────────────┴───────────────┐
│ FATFS (CubeMX generated)   │
│  f_mount / f_open / f_write│
└────────────▲───────────────┘
             │
┌────────────┴───────────────┐
│ BSP / HAL / SPI / DMA      │
└────────────────────────────┘
```
- Sensor task không bao giờ gọi `f_xxx()` trực tiếp, chỉ cần gọi MicroSD_config
- Không trực tiếp ghi `f_write()` data vào SD từ sensor task mà chỉ 1 task như **Logger task** ghi SD

``` text
ECG task  ─┐
PPG task  ─┼──> Logger task ──> SD_WriteLine()
PCG task  ─┘

```
- Trong hệ thống FreeRTOS, FatFS cần được sử dụng thông qua 1 task duy nhất để tránh xung đột tài nguyên.

## 2. CHÂN CS CỦA SD CARD ĐANG ĐƯỢC KÉO Ở ĐÂU TRONG PROJECT CUBEMX 

### 2.1 CubeMX tạo ra driver FATFS cho SPI, nhưng CS Không nằm trong SPI driver. Chân này do người dùng tự cấu hình GPIO

- Với **FATFS + SPI**, CubeMX tạo ra 1 lớp "diskio" trung gian
- File quan trọng nhất: 

``` swift
FATFS/Target/user_diskio.c

```
- Chính file này là nơi bạn điều khiển chân CS

### 2.2 Trong `user_diskio.c` có gì ? 
- Bạn sẽ thấy các hàm như kiểu: 

```c
DSTATUS disk_initialize(BYTE drv);
DRESULT disk_read(BYTE drv, BYTE *buff, LBA_t sector, UINT count);
DRESULT disk_write(BYTE drv, const BYTE *buff, LBA_t sector, UINT count);
```
- FATFS gọi mấy hàm này 
- Trong các hàm này, gọi SPI, bạn phải kéo CS LOW/HIGH ở đây

### 2.3 Chuẩn nhất: define macro CS trong `user_diskio.c`
- Ở đầu file `user_diskio.c`:
```c
#include "main.h"  // Dung GPIO pin CubeMX tao
```

- Thêm: 
```c
#define SD_CS_LOW()   HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET)
#define SD_CS_HIGH()  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET)
```

### 2.4 Kéo CS ở đâu là ĐÚNG 
- Ví dụ trong `disk_read()`

```c
SD_CS_LOW();

/* SPI read sector */
SD_ReadBlocks(buff, sector, count);

SD_CS_HIGH();

```
- Không kéo CS trong application
- Chỉ kéo CS trong diskio/BSP Layer

# LÀM RÕ VỀ HỆ THỐNG FOLDER TRONG FATFS
- Bạn để ý thì thấy ngoài các thư viện low-level của FATFS nằm trong folder **Middlewares** ra thì còn folder FATFS nữa phải không ?
- Nhìn có vẻ trùng chức năng nhưng thực chất chúng có mối liên hệ với nhau và dưới đây mình sẽ làm mối liên hệ đó và chỉ ra nơi để bạn có thể cho Code của bản thân vào mà không sợ bị ảnh hưởng đến các file hệ thống
- CubeMX thiết kế như vậy vì họ muốn: 
	- FATFS không phụ thuộc vào MCU
	- Không phụ thuộc vào SPI hay SDIO
	- Cho phép thay Storage không đổi application
	
## 1. Tổng quan kiến trúc FATFS trong CubeMX
- Nó chi làm 3 tầng chính:
	- Application Layer - nơi ứng dụng gọi API
	- Disk Driver Layer (Target) - nơi người dùng viết code cho phần cứng
	- Middleware Core (Third-Party) - lõi FATFS thuần, độc lập phần cứng
- Sơ đồ tổng quát:
```java
Application
   ↓
FATFS/App
   ↓
FATFS/Target        ← USER IMPLEMENTATION
   ↓
Middlewares/FatFs   ← FATFS CORE (KHÔNG ĐỤNG)
   ↓
SPI / SDIO / HAL

```

## 2. Folder `Middlewares/Third_Party/FatFs/` - Lõi hệ thống

```css
Middlewares/
└── Third_Party/
    └── FatFs/
        └── src/
            ├── diskio.c       ← GENERIC DISPATCHER
            ├── diskio.h
            ├── ff_gen_drv.c   ← DRIVER MANAGER
            ├── ff_gen_drv.h
            ├── ff.c / ff.h
``` 
- Đây là thư viện FATFS gốc của **ChaN**, đã được STM tích hợp
- Đặc điểm: 
	- Không chứa code phần cứng
	- Không biết SPI, SD card, CS
	- Không chỉnh sửa
- Chức năng từng file 
| File | Vai trò|
|------|--------|
|`ff.c`/`ff.h`| FATFS core (f_open, f_write, f_close,...) chứa toàn bộ logic hệ thống file FAT|
|`diskio.h`| Inteface chuẩn (API contract) cho disk|
|`diskio.c`| Dispatcher - gọi Driver phù hợp|
|`ff_gen_drv.c`| Quản lý & map driver (USER, USB, SDIO,...)|
|`integer.h`| Kiểu dữ liệu cho FATFS|

- `ff.c` và `ff.h` là API mức cao (filesystem layer) để đọc và ghi data lên ổ đĩa theo chuẩn FAT
	- Nó chỉ chứa logic cách xử lý file theo đúng chuẩn FAT chứ không tương tác trực tiếp với phần cứng 
	- Có nhiệm vụ chỉ là xử lý logic và gửi lệnh cho các API cấp thấp để xử lý request của nó
- `diskio.h` và `diskio.c` là lớp trung gian giữa filesystem layer và driver.
	- Nó định nghĩa các interface chuẩn `disk_initialize`, `disk_read`, disk_write`,...
	- Không xử lý file, chỉ có nhiệm vụ duy nhất là lớp giao tiếp giữa driver và chuẩn FAT
	- `diskio.c` không phải SD driver, chỉ chuyển tiếp lời gọi sang driver được đăng ký

## 3. Folder `FATFS/Target/` - DRIVER do người dùng viết
```ccs
FATFS/
├── Target/
│   ├── user_diskio.c   ← BẠN VIẾT Ở ĐÂY
│   └── user_diskio.h
```
- Đây là file quan trọng nhất đối với SD card SPI
- `user_diskio.c` chính là nơi bạn viết driver cho phần cứng cho FATFS
- Là nơi chuyển "Sector-level-request" của FATFS thành "SPI transaction thật với SD card"
- Chức năng:
	- Implement các hàm mà FATFS yêu cầu:
		- `disk_initialize`	
		- `disk_read`
		- `disk_write`
		- `disk_ioctl`
	- Giao tiếp trực tiếp với SPI
	- Điều khiển CS LOW/HIGH
	- Gửi Command SD Card
- Đây là nơi duy nhất bạn được phép thao tác kéo CS, gửi command SD và đọc/ghi data

## 4. Folder `FATFS/App/` - CẦU NỐI GIỮA APP-DRIVER
```css
FATFS/
├── App/
│   ├── fatfs.c
│   └── fatfs.h
```
- Trong này chứa hàm khởi tạo FATFS cho hệ thống do CubeMX cung cấp (thay vì để trong main.c thì nó để ở đây). Trong file main.c bạn sẽ thấy nó được Include.
- Link Driver với FATFS core
- Ví dụ trong `fatfs.c`:
```c
FATFS_LinkDriver(&USER_Driver, USER_Path);
```
- Dòng này nói với FATFS core rằng: Khi dùng Driver USER, hãy gọi các hàm trong `user_diskio.c`

## 5. Luồng gọi thực tế 

```arduino
Application       (MicroSD_config)
   ↓
ff.c              (FATFS core) -> Lo file
   ↓
diskio.c          (dispatcher) -> Lo disk (đĩa)
   ↓
user_diskio.c     ← CS + SPI ở đây (Lo Driver)
   ↓
HAL SPI
   ↓
SD Card
```