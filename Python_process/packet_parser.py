import serial
import struct
import logging
from datetime import datetime

PORT = "COM7" # USB-UART 
BAUDRATE = 460800

PKT_HEADER = 0xAA
PKT_FOOTER = 0x55
PKT_VERSION = 0x01

HDR_SIZE = 6                                    # Header(1) + Version(1) + Type(1) + seq(1) + payload_len(2)
FTR_SIZE = 3                                    # Bao gom CRC16(2) + Footer(1)
AUDIO_PAYLOAD_SIZE = 128                        # 32 samples x 4 bytes/sample
BIO_PAYLOAD_SIZE = 192                          # 32 samples ECG (2 bytes/sample) + 32 samples PPG (4 bytes/sample)
AUDIO_PKT_SIZE = HDR_SIZE + AUDIO_PAYLOAD_SIZE + FTR_SIZE    # 137 bytes
BIO_PKT_SIZE = HDR_SIZE + BIO_PAYLOAD_SIZE + FTR_SIZE        # 201 bytes
TOTAL_PKT_SIZE = AUDIO_PKT_SIZE + BIO_PKT_SIZE   # 338 bytes (137 bytes audio + 201 bytes bio)

PKT_TYPE_AUDIO = 0x01
PKT_TYPE_BIO = 0x02

filename = r"D:\STM32Cube_project\My_workspace\target\PPG_PCG_ECG_signal\Python_process\DebugLog\packet_parser_debuglog5.txt"
log_file = open(filename, "a", buffering=1)

logging.basicConfig(
    filename = filename,
    filemode = 'a', # append (ghi tiep vao duoi)
    level = logging.INFO,
    format = "%(asctime)s | %(message)s",
    encoding = "utf-8"
)

def log(msg):
    print(msg)
    # logging.info(msg)
    log_file.write(msg + "\n")

def crc16_ccitt(data: bytes) -> int:
    """
    Thuat toan CRC16-CICTT kiem tra checksum cho packet nhan duoc
    (Da thuc sinh ma Hex: 0x1021, gia tri khoi tao 0xFFFF)

    Nguyen ly:
    - Coi chuoi dau vao la 1 so nhi phan rat lon
    - Chia so do cho da thuc sinh 0x1021 (Bin: 10001000000100001)
    - Phan du 16-bit cua phep chia chinh la ma CRC

    Truyen:
    - Ben gui (STM32): tinh CRC tren [header + payload], gan vao footer truoc khi UART TX
    - Ben nhan (Python/ESP32): tinh lai CRC tren phan tuong tu, so sanh voi CRC trong footer 
        - Neu bang nhau -> data toan ven
        - Neu khac nhau -> co loi trong qua trinh truyen

    Returns:
        int: Gia tri 16-bit (0x0000 -> 0xFFFF)
    """
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
        crc &= 0xFFFF
    return crc # Neu tra ve khong 

def parse_stream(ser):
    buf = bytearray()
    audio_count = bio_count = crc_err = 0
    last_audio_seq = -1
    last_bio_seq = -1

    try:
        while True:
            buf += ser.read(ser.in_waiting or TOTAL_PKT_SIZE) # Doc tung byte gay bottleneck khi stream toc do cao -> Doi thanh doc tung khoi lon
            # buf += ser.read(4096)

            # Tim header 0xAA - idx la vi tru cua byte header dau tien ma find() tim thay trong packet
            # idx = buf.find(bytes([PKT_HEADER]))

            # Thay bang check 2 byte: 0xAA 0x01 (header + version)
            # -> Xac suat 0xAA 0x01 xuat hien ngau nhien trong payload thap hon nhieu
            MAGIC = bytes([PKT_HEADER, PKT_VERSION])
            idx = buf.find(MAGIC)

            if idx < 0:
                if len(buf) > 1:
                    buf = buf[-1:] # Giu lai 1 byte cuoi
                # buf.clear()
                continue

            # Neu idx cua header lai khong nam o dau (idx = 0) ma lai nam o dau do phia sau
            # -> Loai bo cac bytes rac (garbage) phia truoc header
            # -> Xem 16 bytes tiep theo cua garbage co idx gia mao header 0xAA
            if idx > 0:
                log(f"[WARN] Skipped {idx} garbage bytes before header")
                log(f">>> Garbage= {idx} next bytes= {buf[idx:idx+16].hex()}")
                buf = buf[idx:] # Xoa cac bytes rac truoc idx

            # Chua du de doc header thi doc tiep
            if len(buf) < HDR_SIZE:
                continue

            # Doc header 
            version = buf[1]
            pkt_type = buf[2]
            seq = buf[3]
            pay_len = struct.unpack_from('<H', buf, 4)[0]

            if version != PKT_VERSION:
                buf = buf[1:]
                continue

            if pkt_type not in(PKT_TYPE_AUDIO, PKT_TYPE_BIO):
                buf = buf[1:]
                continue

            # 128 bytes payload cua PCG
            if pkt_type == 1 and pay_len != AUDIO_PAYLOAD_SIZE:
                buf = buf[1:]
                continue

            # 192 bytes payload cua PPG + ECG
            if pkt_type == 2 and pay_len != BIO_PAYLOAD_SIZE:
                buf = buf[1:]
                continue

            total_size = HDR_SIZE + pay_len + FTR_SIZE
            if len(buf) < total_size:
                continue # Chua du 1 frame packet thi doc tiep

            frame = buf[:total_size]

            # Verify footer
            if frame[-1] != PKT_FOOTER:
                buf = buf[1:]
                log(f"[ERR] Bad footer: 0x{frame[-1]:02X}")
                continue

            # Verify CRC
            crc_recv = struct.unpack_from('<H', frame, HDR_SIZE + pay_len)[0] # Ma CRC16 nhan duoc 
            crc_calc = crc16_ccitt(frame[:HDR_SIZE + pay_len])  # Tinh toan CRC16 tren Header + Payload
            if crc_recv != crc_calc:
                crc_err += 1
                log(f"[ERR] CRC mismatch type={pkt_type} seq={seq} | calc=0x{crc_calc:04X} recv=0x{crc_recv:04X} | crc_err={crc_err}")
                buf = buf[1:]
                continue

            buf = buf[total_size:]

            # Parse payload
            if pkt_type == PKT_TYPE_AUDIO:
                audio_count += 1
                if last_audio_seq >= 0:
                    expected = (last_audio_seq + 1) & 0xFF
                    if seq != expected:
                        lost = (seq - expected) & 0xFF
                        log(f"[LOSS] AUDIO: expected seq={expected} got seq={seq} -> lost={lost} packets")
                last_audio_seq = seq

                samples = struct.unpack_from('<32i', frame, HDR_SIZE)  # 32 x int32
                log(f"[AUDIO] seq={seq} | pcg[0]={samples[0]:8d} pcg[31]={samples[31]:8d} "
                    f"| total={audio_count} | crc_err={crc_err}")

            elif pkt_type == PKT_TYPE_BIO:
                bio_count += 1
                if last_bio_seq >= 0:
                    expected = (last_bio_seq + 1) & 0xFF
                    if seq != expected:
                        lost = (seq - expected) & 0xFF
                        log(f"[LOSS] BIO: expected seq={expected} got seq={seq} -> lost={lost} packets")
                last_bio_seq = seq

                ir  = struct.unpack_from('<32I', frame, HDR_SIZE)         # 32 x uint32
                ecg = struct.unpack_from('<32h', frame, HDR_SIZE + 128)   # 32 x int16
                log(f"[BIO] seq={seq} | ir[0]={ir[0]:8d} ecg[0]={ecg[0]:6d} "
                    f"| total={bio_count} | crc_err={crc_err}")

    except KeyboardInterrupt:
        log(f"\nStopped.")

if __name__ == "__main__":
    with serial.Serial(PORT, BAUDRATE, timeout=0.01) as ser:
        log(f"Listening on {PORT} @ {BAUDRATE}...")
        log("Press Ctrl+C to stop\n")
        parse_stream(ser)

# Debug Note:
# Phia STM32 khong mat packet ma packet bi mat tren duong truyen khi nhan o phia Python
# Ly do: Lien tuc hien thi log ↓↓↓
# [WARN] Skipped 136 garbage bytes before header
# [WARN] Skipped 200 garbage bytes before header
# -> Packet Audio (137 bytes) bi lech 1 bytes, Packet Bio (201 bytes) cung bi lech 1 byte
# Sau do la: 
# [LOSS] AUDIO: expected seq=188 got seq=189 -> lost=1 packets,...
# [LOSS] BIO: expected seq=12 got seq=13 -> lost=1 packets,...
# 
# Nguyen nhan 1 (kha nang cao): 
# Kha nang Parser tu bo qua Packet co the la do Header = 0xAA trong khi data cua Audio
# hoan toan co kha nang chua AA o giua Payload -> Parse sai -> Lech frame
#
# Nguyen nhan 2: 
# CP2102 co RX buffer phan cung = 576 bytes, TX buffer = 640 bytes, su dung USB 2.0 (toc do max 12Mbps) - truyen USB dang Bulk (dam bao tinh toan ven 100%)
# -> Voi baudrate 460800 thi du suc (chi ton ~23.5% bandwidth UART)
# -> Nhung nguyen nhan khong den tu UART baudrate hay buffer, van de thuc su nam o USB Bulk Transfer Lantency:
#   + Tren module CP2102 cua Silicon Labs, tham so quy dinh chu ky gui du lieu `bInterval` duoc cau hinh = 0
#     nghia la thong so nay bi bo qua, Host se lien tuc polling du lieu tren cac cong (endpoints) moi khi co 
#     khung truyen Bulk ranh roi, mang lai toc do truyen nhanh nhat co the.
#   + Tuy nhien Bulk transfer khong cam ket ve mat thoi gian (do tre) vi co co che Time-out
#     -> USB bulk transfer interval (hay polling rate chu ky gui du lieu) co do tre toi thieu 1ms 
#   + Doi khi data bi gom lai roi flush tre
#   + STM32 chia lam 2 packet cho 137 bytes audio + 201 bytes bio = (338 bytes)/32ms -> 10.8KB/s, gui lien tuc
#   + Python `ser.read(ser.in_waiting or 1)` doc theo polling - khi in_waiting = 0 thi doc 1 byte, rat cham, gay bottleneck
#   + Python nhan duoc stream bi dut giua chung
#   + Parser thay buf chua du total_size -> continue
#   + Lan doc tiep theo buf bi append them data toi -> Neu data moi bat dau bang 0xAA -> parser nham header
