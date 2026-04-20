/*
 * @file sd_spi.c
 *
 * @date  Mar 12, 2026
 * @author LuongHuuPhuc
 *
 * ref docs:
 * 	- SD Association - Simplified Physical Layer Specification
 * 	  Day la nguon goc cho cac buoc init/thao tac chuan cua SD, gom SPI mode, CMD0, CMD8, ACMD41, CMD58,...
 *
 * 	- Elm-Chan - How to Use MMC/SDC
 * 	  Day la tai lieu thuc chien rat hay cho driver SPI, giai thich ro trinh tu vao SPI Mode, vi sao
 * 	  CMD0/CMD8 can CRC hop le, vi sao sau do co the dung CRC gia, cach xu ly SDSC/SDHC,...
 *
 * 	- ST UM1721 - Developing Applications on STM32Cube with FatFS
 * 	  Dung de khop phan tich hop voi Cube/FatFS, tuc vai tro cua cac file `user_diskio.c/.h`, function FATFS_LinkDriver()
 * 	  diskio callback,...
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "sd_spi.h"
#include "sd_spi_port.h"
#include "sd_spi_internal_macros.h"

/* ============================== INTERNAL GLOBAL STATE VAR ============================== */

static bool g_sd_initialized = false; /* Flag danh dau the da khoi tao hay chua */

static sd_card_type_t g_card_type = SD_CARD_UNKNOWN; /* Loai the da duoc gan nhan */

static uint32_t g_sector_count = 0;  /* Tong so sector cua the */

static uint32_t g_erase_block_size = 8U; /* Kich thuoc erase block (don vi sector) (Co the doc chinh xac hon bang lenh bo sung sau nay) */

extern void uart_printf(const char *fmt,...); // Dung de debug loi

/* ============================== INTERAL HELPER FUNCTION ============================== */

/*-----------------------------------------------------------*/

/**
 * @brief Ham chuyen goi don vi sector (512 byte) thanh Gigabyte (GB)
 *
 * @params[in] sector so sector dau vao
 * @retval gia tri chuyen doi sang don vi GB 16-bit
 */
static inline uint16_t SD_SPI_sectors_GB_convert(uint32_t sectors){
	if(sectors == 0U){
		uart_printf("[SD_SPI] Input sectors for GB converting invalid: %d\r\n", sectors);
		return 0U;
	}

	uint64_t total_bytes = (uint64_t)sectors * (uint64_t)SD_SECTOR_SIZE;
	return (uint16_t)(total_bytes / (1024u * 1024u * 1024u)); // bytes -> GB
}

/*-----------------------------------------------------------*/

static inline const char *SD_SPI_card_name_to_str(sd_card_type_t type){
	switch(type){
	case SD_CARD_UNKNOWN:
		return "SD_CARD_UNKNOWN";
	case SD_CARD_MMC:
		return "SD_CARD_MMC";
	case SD_CARD_SDSC:
	        return "SD_CARD_SDSC";
	case SD_CARD_SDHC:
		return "SD_CARD_SDHC";
	default:
		return "SD_CARD_INVALID";
	}
}

/*-----------------------------------------------------------*/

/**
 * @brief Gui va nhan 1 byte qua SPI (blocking mode - test)
 * Lau dai khong nen dung blocking mode de tranh gian doan chuong trinh
 * Nen gom thanh 1 block lon de ghi 1 lan (cho vao RAM buffer)
 *
 * @param[in] tx Byte can gui
 * @retval byte nhan ve
 */
static uint8_t SD_SPI_TxRx(uint8_t tx){
	uint8_t rx = SD_DUMMY_BYTE;
	(void)HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &tx, &rx, 1U, HAL_MAX_DELAY);
	return rx;
}

/*-----------------------------------------------------------*/

/**
 * @brief Chuyen SPI ve toc do cham de khoi tao the
 */
static void SD_SPI_SetSpeedSlow(void){
	__HAL_SPI_DISABLE(&SD_SPI_HANDLE);
	SD_SPI_HANDLE.Init.BaudRatePrescaler = SD_SPI_PRESCALER_SLOW;
	HAL_SPI_Init(&SD_SPI_HANDLE);
	__HAL_SPI_ENABLE(&SD_SPI_HANDLE);
}

/*-----------------------------------------------------------*/

/**
 * @brief Chuyen SPI ve toc do nhanh hon sau khi init thanh cong
 */
static void SD_SPI_SetSpeedFast(void){
	__HAL_SPI_DISABLE(&SD_SPI_HANDLE);
	SD_SPI_HANDLE.Init.BaudRatePrescaler = SD_SPI_PRESCALER_FAST;
	HAL_SPI_Init(&SD_SPI_HANDLE);
	__HAL_SPI_ENABLE(&SD_SPI_HANDLE);

	// Cap clock idle sau khi toc do thay doi
	SD_CS_HIGH();
	for(uint8_t i = 0; i < 4; i++){ // 2 bytes = 16 xung
		SD_SPI_TxRx(SD_DUMMY_BYTE);
	}
}

/*-----------------------------------------------------------*/

/**
 * @brief Nhan nhieu byte lien tiep tu SPI
 *
 * @param[in] buf Bo dem luu du lieu nhan duoc
 * @param[in] len Chieu dai (kich thuoc) so byte can nhan
 */
static void SD_SPI_MultiRx(uint8_t *buf, uint32_t len){
	for(uint32_t i = 0U; i < len; i++){
		buf[i] = SD_SPI_TxRx(SD_DUMMY_BYTE);
	}
}

/*-----------------------------------------------------------*/

/**
 * @brief Gui nhieu byte lien tiep qua SPI
 *
 * @param[in] buf Bo dem chua du lieu can gui
 * @param[in] len Chieu dai (kich thuoc) so byte can gui
 */
static void SD_SPI_MultiTx(const uint8_t *buf, uint32_t len){
	for(uint32_t i = 0u; i < len; i++){
		(void)SD_SPI_TxRx(buf[i]);
	}
}

/*-----------------------------------------------------------*/

/**
 * @brief Cho den khi the SD tra ve 0xFF, nghia la san sang
 */
static bool SD_SPI_WaitReady(uint32_t timeout_ms){
	uint32_t tick_start = HAL_GetTick();
	do{
		if(SD_SPI_TxRx(SD_DUMMY_BYTE) == SD_SPI_IDLE_BYTE){
			return true;
		}
	} while((HAL_GetTick() - tick_start) < timeout_ms);

	return false;
}

/*-----------------------------------------------------------*/

/**
 * @brief Bo chon the SD va tao them 8 xung clock
 */
static void SD_SPI_Deselect(void){
	SD_CS_HIGH();
	(void)SD_SPI_TxRx(SD_DUMMY_BYTE);
}

/*-----------------------------------------------------------*/

/**
 * @brief Chon the SD bang cach keo CS xuong muc thap
 *
 * @retval
 * - true: Neu the da san sang giao tiep
 * - false: Neu khong san sang giao tiep
 */
static bool SD_SPI_Select(void){
	SD_CS_LOW();
	(void)SD_SPI_TxRx(SD_DUMMY_BYTE); // Doc Dummy de dong bo (it nhat 8 clock)

	// Thu nhieu lan de dam bao Card ready
	if(SD_SPI_WaitReady(SD_TIMEOUT_READY_MS)) return true;

	SD_SPI_Deselect();
	return false;
}

/*-----------------------------------------------------------*/

/**
 * @brief Gui lenh toi the SD trong SPI Mode
 *
 * @param[in] cmd Lenh CMDx hoac ACMDx
 * @param[in] arg Tham so 32-bit
 * @param[in] crc Byte CRC, quan trong voi CMD0/CMD8 o giai doan init
 *
 * @retval Byte response R1
 */
static uint8_t SD_SPI_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc){
	uint8_t res; // Phan hoi
	uint8_t n;

	/* Neu la ACMD<n> thi phai gui CMD55 truoc */
	if((cmd & SD_CMD_ACMD_FLAG) != 0U){
		cmd &= SD_CMD_INDEX_MASK;
		res = SD_SPI_SendCmd(CMD55, 0U, SD_DUMMY_CRC);
		if(res > 1U) return res;
	}


	// Neu la CMD0 va CMD8 thi khong `WaitReady` nen khong dung SD_SPI_Select()
	if(cmd == CMD0 || cmd == CMD8){
		SD_CS_LOW();
		SD_SPI_TxRx(SD_DUMMY_BYTE);
	}
	else{
		// Select card -> Set low
		if(!SD_SPI_Select()){
			uart_printf("[SD_SPI] Cannot select card for cmd 0x%02X\r\n", cmd);
			return SD_SPI_IDLE_BYTE;
		}
	}

	if(cmd != CMD0 && cmd != CMD8) crc = SD_DUMMY_CRC;

	/* Command packet */
	(void)SD_SPI_TxRx(SD_SPI_CMD_PREFIX | cmd);
	(void)SD_SPI_TxRx((uint8_t)(arg >> 24));
	(void)SD_SPI_TxRx((uint8_t)(arg >> 16));
	(void)SD_SPI_TxRx((uint8_t)(arg >> 8));
	(void)SD_SPI_TxRx((uint8_t)arg);
	(void)SD_SPI_TxRx(crc);

	if(cmd == CMD12){
		(void)SD_SPI_TxRx(SD_DUMMY_BYTE);
	}

	for(n = 0U; n < 10U; n++){
		res = SD_SPI_TxRx(SD_DUMMY_BYTE);
		if((res & SD_R1_RESP_VALID_MASK) == 0U){
			// Giu CS Low khi return thanh cong
			return res;
		}
	}
	SD_SPI_Deselect();
	return SD_SPI_IDLE_BYTE;
}

/*-----------------------------------------------------------*/

/**
 * @brief Cho den khi nhan duoc token (chuoi du lieu) mong muon
 *
 * @param[in] token Token can cho
 * @param[in] timeout_ms Thoi gian cho toi da
 *
 * @retval true Neu nhan duoc token - false neu het thoi gian cho
 */
static bool SD_SPI_WaitToken(uint8_t token, uint32_t timeout_ms){
	uint8_t b;
	uint32_t tick_start = HAL_GetTick();

	do{
		b = SD_SPI_TxRx(SD_DUMMY_BYTE);
		if(b == token) return true;
	} while((HAL_GetTick() - tick_start) < timeout_ms);

	return false;
}

/*-----------------------------------------------------------*/

/**
 * @brief Nhan 1 block (co the la 1 sector 512 byte hoac kich thuoc khac) du lieu tu the SD
 *
 * @param[out] buf Bo dem chua du lieu
 * @param[in] len So byte can nhan
 *
 * @retval true Neu thanh cong
 * @retval false Neu that bai
 */
static bool SD_SPI_ReceiveDataBlock(uint8_t *buf, uint32_t len){
	if(!SD_SPI_WaitToken(SD_TOKEN_START_BLOCK, SD_TIMEOUT_READ_MS)){
		return false;
	}

	SD_SPI_MultiRx(buf, len);

	/* Bo qua 2 byte CRC */
	(void)SD_SPI_TxRx(SD_DUMMY_BYTE);
	(void)SD_SPI_TxRx(SD_DUMMY_BYTE);

	return true;
}

/*-----------------------------------------------------------*/

/**
 * @brief Gui mot sector (co the la 1 sector 512 byte hoac kich thuoc khac) du lieu den the SD
 *
 * @param[in] buf Bo dem chua du lieu can gui
 * @param[in] token Token bat dau block hoac token dung truyen
 *
 * @retval true Neu thanh cong - false Neu that bai
 */
static bool SD_SPI_TransmitDataBlock(const uint8_t *buf, uint8_t token){
	uint8_t resp;

	if(!SD_SPI_WaitReady(SD_TIMEOUT_WRITE_MS)) return false;

	SD_SPI_TxRx(token);

	if(token == SD_TOKEN_STOP_TRAN){
		if(!SD_SPI_WaitReady(SD_TIMEOUT_WRITE_MS)) return false;

		(void)SD_SPI_TxRx(SD_DUMMY_BYTE);
		return true;
	}

	SD_SPI_MultiTx(buf, SD_SECTOR_SIZE);

	/* Gui CRC gia */
	(void)SD_SPI_TxRx(SD_DUMMY_BYTE);
	(void)SD_SPI_TxRx(SD_DUMMY_BYTE);

	resp = SD_SPI_TxRx(SD_DUMMY_BYTE);
	if((resp & SD_DATA_RESP_MASK) != SD_DATA_ACCEPTED) return false;

	/**
	 *  Sau khi data response tra ve data accepted thi card bat dau ghi flash noi bo
	 *  Trong thoi gian do MISO = 0 (BUSY), SPI cmd se bi ignore
	 *  Can WaitReady 2 lan vi:
	 *  - Lan 1 chi de dam bao card san sang nhan data
	 *  - Lan 2 sau khi write flash xong
	 *
	 *  WaitReady duoi day la de doi internal flash thuc su ghi xong
	 */
	if(!SD_SPI_WaitReady(SD_TIMEOUT_WRITE_MS)) return false;
	(void)SD_SPI_TxRx(SD_DUMMY_BYTE); // Them 8 xung sau khi ghi

	return true;
}

/*-----------------------------------------------------------*/

/**
 * @brief Doc CSD va tinh tong so sector cua the
 *
 * @retval Tong so sector 512 byte, 0 neu that bai
 *
 * CSD (Card-Specific Data) trong the nho SD la mot thanh ghi du lieu
 * dac biet chua thong tin va ky thuat chi tiet cua the nho SD
 */
static uint32_t SD_SPI_ReadSectorCountFromCSD(void){
	uint8_t csd[16]; // Mang luu 16 byte thong tin du lieu the nho
	uint32_t csize;  // Dung luong vat ly cua Card sau khi doc CSD

	// gui CMD de doc CSD
	if(SD_SPI_SendCmd(CMD9, 0U, SD_DUMMY_CRC) != SD_R1_READY){
		uart_printf("[SD_SPI] CMD9 send failed\r\n");
		SD_SPI_Deselect();
		return 0U;
	}

	// Nhan 16 byte CSD de xac dinh CSD thuoc version nao
	if(!SD_SPI_ReceiveDataBlock(csd, 16U)){
		SD_SPI_Deselect();
		return 0U;
	}

	SD_SPI_Deselect();

	// Nhin csd[0] de biet version
	/* CSD version 2.0 - dung luong duoc ma hoa theo kieu moi, don gian hon */
	if((csd[0] >> 6) == 1U){
		csize = (((uint32_t)csd[7] & 0x3FU) << 16) | ((uint32_t)csd[8] << 8) | (uint32_t)csd[9];
		return ((csize + 1UL) * 1024UL);
	}
	else{
		/* CSD version 1.0 - phai dung them cac field khac de parse ra data */
		uint32_t block_len;
		uint32_t mult;
		uint32_t blocknr;
		uint32_t capacity;
		uint16_t c_size;
		uint8_t c_size_mult;
		uint8_t read_bl_len; // do dai block doc theo luy thua cua 2

		read_bl_len = csd[5] & 0x0FU;
		c_size = (uint16_t)(((uint16_t)(csd[6] & 0x03U) << 10) | ((uint16_t)csd[7] << 2) | ((csd[8] & 0xC0U) >> 6));
		c_size_mult = (uint8_t)(((csd[9] & 0x03U) << 1) | ((csd[10] & 0x80U) >> 7));
		block_len = 1UL << read_bl_len;
		mult = 1UL << (c_size_mult + 2UL);
		blocknr = (uint32_t)(c_size + 1U) * mult;
		capacity = blocknr * block_len;

		return capacity / SD_SECTOR_SIZE; // Tinh ra dung luong roi quy ve sector 512 byte vi FatFS va lop diskio lam viec theo Sector logic chu khong phai B/MB
	}
}

/*-----------------------------------------------------------*/

/**
 * @brief Ham reset SPI bus truoc khi khoi tao
 *
 * @details
 * Theo SD Physical layer Specification(SPI Mode), truoc khi gui CMD0,
 * host phai cung cap it nhat 74 clock + CS = HIGH + MOSI = HIGH (gui byte 0xFF)
 *
 * Muc dich:
 * - Dong bo lai giao tiep SPI sau power-up hoac MCU reset
 * - Ket thuc cac transaction con dang do
 * - Dua SD Card ve trang thai cho nhan cmd (SPI Idle state)
 *
 * @note
 * - Ham nay khong reset phan cung SD Card, chi reset trang thai giao tiep SPI
 * - Phai goi ham nay truoc CMD0 trong qua trinh khoi tao card
 */
static void SD_SPI_PowerOnReset(void){
	SD_SPI_Deselect();

	for(uint8_t i = 0; i < 10U; i++){
		(void)SD_SPI_TxRx(SD_DUMMY_BYTE); // 160 bytes (>74 clocks)
	}
}

/*-----------------------------------------------------------*/

/* Gui chuoi lenh CMD0 -> CMD8 -> ACMD41 -> CMD58 (ham nay chi thuc hien gui cmd) */
static sd_spi_status_t SD_SPI_InitCard_Internal(void){
	uint32_t tick_start;
	uint8_t cmd8_resp[4];
	uint8_t ocr[4]; // Operating Conditions Register (thanh ghi dieu kien hoat dong) 32-bit cho biet dai dien ap ho tro, power-up status,...
	uint8_t r1; // Response 1 - 8-bit phan hoi trang thai tra ve sau khi gui lenh (cho biet trang thai hien tai cua the)

	g_sd_initialized = false;
	g_card_type = SD_CARD_UNKNOWN;
	g_sector_count = 0U;

	/* Dua the vao trang thai idle */
	r1 = SD_SPI_SendCmd(CMD0, 0U, SD_CMD0_CRC);
	if(r1 != SD_R1_IDLE_STATE){
		uart_printf("[SD_SPI] CMD0 send failed, r1 = 0x%02X\r\n", r1);
		SD_SPI_Deselect();
		return SD_SPI_ERR;
	}

	/* Kiem tra SD co ho tro version 2 */
	r1 = SD_SPI_SendCmd(CMD8, SD_CMD8_ARG, SD_CMD8_CRC);
	if(r1 == SD_R1_IDLE_STATE){ // Neu co ho tro version 2.x
		uart_printf("[SD_SPI] SD Card supported version 2.x\r\n");

		/* CMD8 tra them 4 byte response kieu R7 */
		cmd8_resp[0] = SD_SPI_TxRx(SD_DUMMY_BYTE);
		cmd8_resp[1] = SD_SPI_TxRx(SD_DUMMY_BYTE);
		cmd8_resp[2] = SD_SPI_TxRx(SD_DUMMY_BYTE);
		cmd8_resp[3] = SD_SPI_TxRx(SD_DUMMY_BYTE);

		/* Kiem tra VHS va check pattern */
		if((cmd8_resp[2] == SD_CMD8_RESP_VHS_EXPECTED) && (cmd8_resp[3] == SD_CMD8_RESP_PATTERN_EXPECTED)){
			tick_start = HAL_GetTick();
			do{
				r1 = SD_SPI_SendCmd(ACMD41, SD_ACMD41_HCS, SD_DUMMY_CRC);
				if(r1 == SD_R1_READY) break;
			} while((HAL_GetTick() - tick_start) < SD_TIMEOUT_INIT_MS);

			if(r1 != SD_R1_READY){
				uart_printf("[SD_SPI] ACMD41 send failed, r1 = 0x%02X\r\n", r1);
				SD_SPI_Deselect();
				return SD_SPI_TIMEOUT;
			}

			/* Doc OCR de xac dinh card co phai SDHC/SDXC hay khong */
			if(SD_SPI_SendCmd(CMD58, 0U, SD_DUMMY_CRC) != SD_R1_READY){
				uart_printf("[SD_SPI] CMD58 send failed, r1 = 0x%02X\r\n", r1);
				SD_SPI_Deselect();
				return SD_SPI_ERR;
			}

			ocr[0] = SD_SPI_TxRx(SD_DUMMY_BYTE);
			ocr[1] = SD_SPI_TxRx(SD_DUMMY_BYTE);
			ocr[2] = SD_SPI_TxRx(SD_DUMMY_BYTE);
			ocr[3] = SD_SPI_TxRx(SD_DUMMY_BYTE);

			g_card_type = ((ocr[0] & OCR_CCS_BIT) != 0U) ? SD_CARD_SDHC : SD_CARD_SDSC;
		}
		else{
			uart_printf("[SD_SPI] CMD8 send failed, r1 = 0x%02X\r\n", r1);
			SD_SPI_Deselect();
			return SD_SPI_BADRESP;
		}
	}
	else{
		// Khong ho tro version 2.x
		uart_printf("[SD_SPI] Old card found...\r\n");

		/* SDSC cu hoac MMC */
		tick_start = HAL_GetTick();

		do{
			r1 = SD_SPI_SendCmd(ACMD41, 0U, SD_DUMMY_CRC);
			uart_printf("[SD_SPI] ACMD41(Old) r1 = 0x%02X\r\n", r1);
			if(r1 == SD_R1_READY){
				g_card_type = SD_CARD_SDSC;
				break;
			}

			r1 = SD_SPI_SendCmd(CMD1, 0U, SD_DUMMY_CRC);
			uart_printf("[SD_SPI] CMD1 r1 = 0x%02X\r\n", r1);
			if(r1 == SD_R1_READY){
				g_card_type = SD_CARD_MMC;
				break;
			}
		} while((HAL_GetTick() - tick_start) < SD_TIMEOUT_INIT_MS);

		if(g_card_type == SD_CARD_UNKNOWN){
			SD_SPI_Deselect();
			return SD_SPI_TIMEOUT;
		}

		/* Buoc set block length = 512 cho the khong phai SDHC */
		if(SD_SPI_SendCmd(CMD16, SD_SECTOR_SIZE, SD_DUMMY_CRC) != SD_R1_READY){
			uart_printf("[SD_SPI] CMD16 send failed\r\n");
			SD_SPI_Deselect();
			return SD_SPI_ERR;
		}
	}

	SD_SPI_Deselect();

	g_sector_count = SD_SPI_ReadSectorCountFromCSD();
	if(g_sector_count == 0U) return SD_SPI_ERR;
	uart_printf("[SD_SPI] sector_count = %lu (~%huGB),type:%s\r\n", g_sector_count,
								SD_SPI_sectors_GB_convert(g_sector_count),
								SD_SPI_card_name_to_str(g_card_type));

	g_sd_initialized = true;

	return SD_SPI_OK;
}

/* ============================== PUBLIC API ============================== */

sd_spi_status_t SD_SPI_InitCard(void){
	if(g_sd_initialized) return SD_SPI_OK;

	uint8_t retry = 0;
	sd_spi_status_t st;

	SD_SPI_SetSpeedSlow(); // Ha thap toc do SPI truoc khi Init

	while(retry < 3){
		SD_SPI_PowerOnReset(); // Reset lai SPI bus moi khi duoc init hoac cap nguon lai
		st = SD_SPI_InitCard_Internal();

		if(st == SD_SPI_OK){
			uart_printf("[SD_SPI] InitCard OK!\r\n");
			SD_SPI_SetSpeedFast(); // Set lai toc do cao
			return SD_SPI_OK;
		}

		uart_printf("[SD_SPI] Init failed (retry %d/3)\r\n", retry + 1);
		retry++;
	}

	return SD_SPI_ERR;
}

/*-----------------------------------------------------------*/

sd_spi_status_t SD_SPI_ReadSectors(uint8_t *buf, uint32_t sector, uint32_t count){
	uint32_t addr;

	if(!g_sd_initialized) return SD_SPI_NOINIT;
	if((buf == NULL) || (count == 0U)) return SD_SPI_ERR;

	addr = sector;

	/* SDSC/MMC dung byte addressing, SDHC dung block addressing */
	if(g_card_type != SD_CARD_SDHC){
		addr *= SD_SECTOR_SIZE;
	}

	// Neu chi doc 1 sector
	if(count == 1U){
		if(SD_SPI_SendCmd(CMD17, addr, SD_DUMMY_CRC) != SD_R1_READY){
			uart_printf("[SD_SPI] CMD17 send failed\r\n");
			SD_SPI_Deselect();
			return SD_SPI_ERR;
		}

		if(!SD_SPI_ReceiveDataBlock(buf, SD_SECTOR_SIZE)){
			SD_SPI_Deselect();
			return SD_SPI_ERR;
		}
		SD_SPI_Deselect();
	}

	// Doc nhieu sector
	else{
		// Gui lenh doc nhieu sector lien tiep
		if(SD_SPI_SendCmd(CMD18, addr, SD_DUMMY_CRC) != SD_R1_READY){
			uart_printf("[SD_SPI] CMD18 send failed\r\n");
			SD_SPI_Deselect();
			return SD_SPI_ERR;
		}

		do{
			if(!SD_SPI_ReceiveDataBlock(buf, SD_SECTOR_SIZE)){
				SD_SPI_Deselect();
				return SD_SPI_ERR;
			}
			buf += SD_SECTOR_SIZE;
		} while(--count);

		// Dung qua trinh doc nhieu sector lien tiep
		(void)SD_SPI_SendCmd(CMD12, 0U, SD_DUMMY_CRC);
		SD_SPI_Deselect();
	}
	return SD_SPI_OK;
}

/*-----------------------------------------------------------*/

sd_spi_status_t SD_SPI_WriteSectors(const uint8_t *buf, uint32_t sector, uint32_t count){
	uint32_t addr;

	if(!g_sd_initialized) return SD_SPI_NOINIT;
	if((buf == NULL) || (count == 0U)) return SD_SPI_ERR;

	addr = sector;

	/* SDSC/MMC dung byte addressing, SDHC dung block addressing */
	if(g_card_type != SD_CARD_SDHC){
		addr *= SD_SECTOR_SIZE;
	}

	// Neu chi ghi 1 sector
	if(count == 1U){
		/* Gui lenh ghi 1 sector */
		if(SD_SPI_SendCmd(CMD24, addr, SD_DUMMY_CRC) != SD_R1_READY){
			uart_printf("[SD_SPI] CMD24 send failed\r\n");
			SD_SPI_Deselect();
			return SD_SPI_ERR;
		}

		if(!SD_SPI_TransmitDataBlock(buf, SD_TOKEN_START_BLOCK)){
			SD_SPI_Deselect();
			return SD_SPI_ERR;
		}

		if(!SD_SPI_WaitReady(SD_TIMEOUT_WRITE_MS)){
			SD_SPI_Deselect();
			return SD_SPI_TIMEOUT;
		}
		SD_SPI_Deselect();
	}

	// Ghi nhieu sector
	else{
		/* ACMD23 dung de bao truoc so sector se ghi */
		if((g_card_type == SD_CARD_SDHC) || (g_card_type == SD_CARD_SDSC)){
			if(SD_SPI_SendCmd(ACMD23, count, SD_DUMMY_CRC) != SD_R1_READY){
				SD_SPI_Deselect();
				return SD_SPI_ERR;
			}
		}

		/* Gui lenh ghi nhieu sector lien tiep */
		if(SD_SPI_SendCmd(CMD25, addr, SD_DUMMY_CRC) != SD_R1_READY){
			uart_printf("[SD_SPI] CMD25 send failed\r\n");
			SD_SPI_Deselect();
			return SD_SPI_ERR;
		}

		do{
			if(!SD_SPI_TransmitDataBlock(buf, SD_TOKEN_MULTI_WRITE)){
				SD_SPI_Deselect();
				return SD_SPI_ERR;
			}
			buf += SD_SECTOR_SIZE;
		} while(--count);

		if(!SD_SPI_TransmitDataBlock(NULL, SD_TOKEN_STOP_TRAN)){
			SD_SPI_Deselect();
			return SD_SPI_ERR;
		}

		if(!SD_SPI_WaitReady(SD_TIMEOUT_WRITE_MS)){
			SD_SPI_Deselect();
			return SD_SPI_TIMEOUT;
		}
		SD_SPI_Deselect();
	}
	return SD_SPI_OK;
}

/*-----------------------------------------------------------*/

sd_spi_status_t SD_SPI_Sync(void){
	if(!g_sd_initialized) return SD_SPI_NOINIT;
	if(!SD_SPI_Select()) return SD_SPI_ERR;
	if(!SD_SPI_WaitReady(SD_TIMEOUT_READY_MS)){
		SD_SPI_Deselect();
		return SD_SPI_TIMEOUT;
	}

	SD_SPI_Deselect();
	return SD_SPI_OK;
}

/*-----------------------------------------------------------*/

uint32_t SD_SPI_GetSectorCount(void){
	return g_sector_count;
}

/*-----------------------------------------------------------*/

uint32_t SD_SPI_GetEraseBlockSize(void){
	return g_erase_block_size;
}

/*-----------------------------------------------------------*/

bool SD_SPI_IsInitialized(void){
	return g_sd_initialized;
}

/*-----------------------------------------------------------*/

sd_card_type_t SD_SPI_GetCardType(void){
	return g_card_type;
}

/*-----------------------------------------------------------*/

bool SD_SPI_IsCardPresent(void){
	// Luu trang thai SPI hien tai
	bool was_init = g_sd_initialized;

	// Tam thoi chuyen sang toc do cham de kiem tra
	if(was_init){
		SD_SPI_SetSpeedSlow();
	}

	SD_CS_LOW();
	HAL_Delay(1);
	uint8_t resp = SD_SPI_TxRx(SD_DUMMY_BYTE);
	SD_CS_HIGH();

	// Khoi phuc lai toc do
	if(was_init){
		SD_SPI_SetSpeedFast();
	}

	return (resp != SD_SPI_IDLE_BYTE);
}

#ifdef __cplusplus
}
#endif
