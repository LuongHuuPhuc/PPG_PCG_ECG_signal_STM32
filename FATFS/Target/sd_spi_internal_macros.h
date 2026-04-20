/*
 * @file sd_spi_internal_macros.h
 *
 * @date Mar 17, 2026
 * @author LuongHuuPhuc
 */

#ifndef TARGET_SD_SPI_INTERNAL_MACROS_H_
#define TARGET_SD_SPI_INTERNAL_MACROS_H_

/* ============================== INTERNAL MACRO ============================== */

/* Do dai 1 sector logic chuan ma Fatfs lam viec */
#define SD_SECTOR_SIZE					512U

/* Byte dummy thuong duoc gui tren SPI de tao xung clock, dung khi:
 * - host gui byte rong de tao clock
 * - host gui CRC gia
 * - host doc du lieu bang cach phat 0xFF
 */
#define SD_DUMMY_BYTE 					0xFFU

/* Token bat dau block du lieu khi doc */
#define SD_TOKEN_START_BLOCK		    0xFEU

/* Token bat dau block du lieu khi ghi nhieu Block */
#define SD_TOKEN_MULTI_WRITE		    0xFCU

/* Token ket thuc ghi nhieu block */
#define SD_TOKEN_STOP_TRAN				0xFDU

/* Mask kiem tra response sau khi gui block du lieu */
#define SD_DATA_RESP_MASK 				0x1FU

/* Ma response cho biet the da chap nhan block du lieu */
#define SD_DATA_ACCEPTED				0x05U

/* Thoi gian cho the san sang */
#define SD_TIMEOUT_READY_MS			    500U

/* Thoi gian cho nhan token doc du lieu */
#define SD_TIMEOUT_READ_MS				200U

/* Thoi gian cho ghi du lieu */
#define SD_TIMEOUT_WRITE_MS				1000U

/* Thoi gian cho khoi tao the */
#define SD_TIMEOUT_INIT_MS				3000U

/* Gia tri byte cho thay bus SPI dang o trang thai nha/rong
 * Trong giao tiep SD card qua SPI, gia tri 0xFFU thuong xuat hien khi
 * - Host gui dummy byte de tao xung clock
 * - Card chua tra ve response hop le
 * - Card dang san sang va nha bus
 */
#define SD_SPI_IDLE_BYTE				0xFFU

/* Response R1 */
/**
 * @brief Mask bit7 cua byte response R1
 *
 * @details
 * Trong SPI mode, khi doc response tu the SD, bit 7 se bang 0 neu
 * byte nhan duoc la mot response hop le. Neu bit 7 van bang 1,
 * host can tiep tuc doc them cho den khi nhan duoc response thuc su
 *
 * Vi vay, macro nay duoc dung de kiem tra:
 * - (res & SD_R1_RESP_VALID_MASK) == 0U  -> response hop le
 * - (res & SD_R1_RESP_VALID_MASK) != 0U  -> chua phai response hop le
 */
#define SD_R1_RESP_VALID_MASK			(1UL << 7) // 0x80
#define SD_R1_IDLE_STATE				0x01U 	/* Response R1 khi card dang o trang thai IDLE */
#define SD_R1_READY						0x00U	/* Response R1 khi card da san sang va khong co loi */

/* ============================== SD COMMAND O SPI MODE ============================== */

/**
 * !<Prefix (tien to) cua byte lenh SD trong SPI mode>
 *
 * @details
 * Byte dau tien cua CMD SD trong SPI co dang: 01xxxxxx
 * Trong do:
 * 	- bit 7 = 0
 * 	- bit 6 = 1
 * 	- 6 bit thap [5:0] la chi so lenh
 * Vi vay moi lenh CMDx deu duoc tao bang: SD_SPI_CMD_PREFIX + chi_so_lenh
 */
#define SD_SPI_CMD_PREFIX		0x40U

/* Lenh thuong - gui thang cho the SD */
#define CMD0    (0U) 	/* !<Lenh dua the vao trang thai IDLE, dong thoi bat dau vao SPI Mode> */
#define CMD1    (1U)	/* !<Lenh khoi tao kieu cu, thuong duoc dung cho MMC hoac mot so the cu the> */
#define CMD8    (8U)	/* !<Kiem tra dieu kien dien ap va nhan dien SD version 2.x> */
#define CMD9    (9U)	/* !<Lenh doc thanh ghi CSD (Card-Specific Data) cua the SD>*/
#define CMD10   (10U)	/* !<Lenh doc thanh ghi CID (Card Identification Data) cua the SD>*/
#define CMD12   (12U)	/* !<Dung qua trinh doc nhieu block>*/
#define CMD16   (16U)	/* !<Dat do dai block du lieu, thuong la 512 bytes cho SDSC/MMC>*/
#define CMD17   (17U)   /* !<Doc 1 block> */
#define CMD18   (18U)	/* !<Doc nhieu block lien tiep>*/
#define CMD24   (24U)   /* !<Ghi 1 block> */
#define CMD25   (25U)	/* !<Ghi nhieu block lien tiep>*/
#define CMD55   (55U)	/* !<Bao cho biet lenh tiep theo se la ACMD>*/
#define CMD58   (58U)	/* !<Doc thanh ghi OCR (Operation Conditions Register)>*/

/* Lenh ung dung ACMD (Application Command) - khong gui truc tiep ngay duoc ma phai theo sau CMD55 */
#define SD_CMD_ACMD_FLAG	0x80U // 1000 0000			/* Co noi bo danh dau day la lenh ACMD  */
#define SD_CMD_INDEX_MASK	0x7FU // 0111 1111		   	/* Mask xoa bit danh dau ACMD de thu duoc so lenh thuc (bo di 0x80U cua cac lenh ACMD) - Giu 7 bit thap, xoa bit7 */

#define ACMD23  			(SD_CMD_ACMD_FLAG | (23U)) // 0xD7, sau khi mask se thanh 0x57  /* Pre-erase count truoc khi ghi nhieu block */
#define ACMD41  			(SD_CMD_ACMD_FLAG | (41U)) // 0xE9, sau khi mask se thanh 0x69 	/* Dung trong qua trinh khoi tao the SD de thoat IDLE va san sang lam viec */
#define SD_ACMD41_HCS		(1UL << 30)  			   	/* Bit HCS (High Capacity Support) gui trong ACM41 de gui yeu cau ho tro high capacity */

/**
 * @brief Bit CCS trong OCR
 */
#define OCR_CCS_BIT         0x40U

/* CRC dung trong giai doan init */
#define SD_CMD0_CRC                     0x95U /* CRC hop le bat buoc cho CM0 */
#define SD_CMD8_CRC                     0x87U /* CRC hop le bat buoc cho CM8 */
#define SD_DUMMY_CRC                    0x01U /* CRC gia thuong dung cho lenh khac */

/* CMD8 arg va resp mong doi */
#define SD_CMD8_ARG                     0x1AAU  /* Kiem tra dien ap + echo pattern */
#define SD_CMD8_RESP_VHS_EXPECTED       0x01U	/* Byte VHS ky vong trong response CM8 */
#define SD_CMD8_RESP_PATTERN_EXPECTED   0xAAU	/* Byte pattern ky vong trong response CMD8 */

/**
 * Debug macro for SD_SPI API
 * __x__: ham SD_SPI can kiem tra
 * tag: vi tri ham duoc goi o file
 */
#define SD_SPI_ERROR_CHECK(__x__, tag, __err_ret__) 	do { \
	sd_spi_status_t __sd_spi_err__; \
	__sd_spi_err__ = (__x__); \
	if(__sd_spi_err__ != SD_SPI_OK){ \
		/* uart_printf("["#tag"] %s failed: %d\r\n", #__x__, __sd_spi_err__); */ \
		return (__err_ret__); \
	} \
} while(0)

#endif /* TARGET_SD_SPI_INTERNAL_MACROS_H_ */
