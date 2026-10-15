#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_

#include "main.h"

/* Protocol Responses */
#define BL_ACK                          0xA5U
#define BL_NACK                         0x7FU

/* Command Codes */
#define BL_GET_VER                      0x51U
#define BL_GET_HELP                     0x52U
#define BL_GET_CID                      0x53U
#define BL_GO_TO_ADDR                   0x55U
#define BL_FLASH_ERASE                  0x56U
#define BL_MEM_WRITE                    0x57U
#define BL_EN_R_W_PROTECT               0x58U
#define BL_MEM_READ                     0x59U
#define BL_READ_SECTOR_P_STATUS         0x5AU
#define BL_OTP_READ                     0x5BU
#define BL_DIS_R_W_PROTECT              0x5CU
#define BL_MY_NEW_COMMAND               0x5DU

/* Version Definition */
#define BL_VER                          0x10U

/* Core Function Prototypes */
void bootloader_parse_and_execute(uint8_t *pBuffer);
void bootloader_send_ack(uint8_t follow_len);
void bootloader_send_nack(void);
uint8_t bootloader_verify_crc(uint8_t *pData, uint32_t len, uint32_t crc_host);

/* Option Byte Helper */
uint8_t configure_flash_sector_wrp(uint8_t sector_mask, uint8_t enable);

/* Command Handlers */
void bootloader_handle_getver_cmd(uint8_t *pBuffer);
void bootloader_handle_gethelp_cmd(uint8_t *pBuffer);
void bootloader_handle_getcid_cmd(uint8_t *pBuffer);
void bootloader_handle_go_to_addr_cmd(uint8_t *pBuffer);
void bootloader_handle_flash_erase_cmd(uint8_t *pBuffer);
void bootloader_handle_mem_write_cmd(uint8_t *pBuffer);
void bootloader_handle_en_r_w_protect_cmd(uint8_t *pBuffer);
void bootloader_handle_mem_read_cmd(uint8_t *pBuffer);
void bootloader_handle_read_sector_protection_status_cmd(uint8_t *pBuffer);
void bootloader_handle_otp_read_cmd(uint8_t *pBuffer);
void bootloader_handle_dis_r_w_protect_cmd(uint8_t *pBuffer);
void bootloader_handle_my_new_cmd(uint8_t *pBuffer);

#endif /* INC_BOOTLOADER_H_ */