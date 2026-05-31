import serial
import struct

PORT = "COM7" # USB-UART 
BAUDRATE = 460800

PKT_HEADER = 0xAA
PKT_FOOTER = 0x55
HDR_SIZE = 6        # Header(1) + Version(1) + Type(1) + seq(1) + payload_len(2)
FTR_SIZE = 3        # Bao gom CRC16(2) + Footer(1)

PKT_TYPE_AUDIO = 0x01
PKT_TYPE_BIO = 0x02

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

    while True:
        buf += ser.read(ser.in_waiting or 1)

        # Tim header 
        idx = buf.find(bytes([PKT_HEADER]))
        if idx < 0:
            buf.clear()
            continue
        if idx > 0:
            buf = buf[idx:] # Xoa rac truoc header

        # Chua du de doc header
        if len(buf) < HDR_SIZE:
            continue

        # Doc header 
        version = buf[1]
        pkt_type = buf[2]
        seq = buf[3]
        pay_len = struct.unpack_from('<H', buf, 4)[0]

        if version != 1:
            buf = buf[1:]
            continue

        if pkt_type not in(1, 2):
            buf = buf[1:]
            continue

        # 128 bytes payload cua PCG
        if pkt_type == 1 and pay_len != 128:
            buf = buf[1:]
            continue

        # 192 bytes payload cua PPG + ECG
        if pkt_type == 2 and pay_len != 192:
            buf = buf[1:]
            continue

        total_size = HDR_SIZE + pay_len + FTR_SIZE
        if len(buf) < total_size:
            continue # Chua du 1 frame

        frame = buf[:total_size]
        buf = buf[total_size:]

        # Verify footer
        if frame[-1] != PKT_FOOTER:
            print(f"[ERR] Bad footer: 0x{frame[-1]:02X}")
            continue

        # Verify CRC
        crc_recv = struct.unpack_from('<H', frame, HDR_SIZE + pay_len)[0]
        crc_calc = crc16_ccitt(frame[:HDR_SIZE + pay_len])
        if crc_recv != crc_calc:
            crc_err += 1
            print(f"[ERR] CRC mismatch calc=0x{crc_calc:04X} recv=0x{crc_recv:04X}")
            continue

        # Parse payload
        if pkt_type == PKT_TYPE_AUDIO:
            audio_count += 1
            samples = struct.unpack_from('<32i', frame, HDR_SIZE)  # 32 x int32
            print(f"[AUDIO] seq={seq} | pcg[0]={samples[0]:8d} pcg[31]={samples[31]:8d} "
                  f"| total={audio_count}")

        elif pkt_type == PKT_TYPE_BIO:
            bio_count += 1
            ir  = struct.unpack_from('<32I', frame, HDR_SIZE)         # 32 x uint32
            ecg = struct.unpack_from('<32h', frame, HDR_SIZE + 128)   # 32 x int16
            print(f"[BIO]   seq={seq} | ir[0]={ir[0]:8d} ecg[0]={ecg[0]:6d} "
                  f"| total={bio_count} | crc_err={crc_err}")

if __name__ == "__main__":
    with serial.Serial(PORT, BAUDRATE, timeout=1) as ser:
        print(f"Listening on {PORT} @ {BAUDRATE}...")
        parse_stream(ser)