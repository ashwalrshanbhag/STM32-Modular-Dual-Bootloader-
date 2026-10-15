#include "bootloader.h"
#include <string.h>

extern UART_HandleTypeDef huart2;
extern CRC_HandleTypeDef hcrc;

/* Supported command list for BL_GET_HELP */
/* Supported command list for BL_GET_HELP */
static const uint8_t supported_commands[] = {
    BL_GET_VER, BL_GET_HELP, BL_GET_CID, BL_GO_TO_ADDR,
    BL_FLASH_ERASE, BL_MEM_WRITE, BL_EN_R_W_PROTECT,
    BL_DIS_R_W_PROTECT, BL_READ_SECTOR_P_STATUS,
    BL_MEM_READ, BL_OTP_READ, BL_MY_NEW_COMMAND
};

/* --- Core Communication & CRC Helpers --- */

void bootloader_send_ack(uint8_t follow_len) {
    uint8_t ack_buf[2] = {BL_ACK, follow_len};
    HAL_UART_Transmit(&huart2, ack_buf, 2, HAL_MAX_DELAY);
}

void bootloader_send_nack(void) {
    uint8_t nack = BL_NACK;
    HAL_UART_Transmit(&huart2, &nack, 1, HAL_MAX_DELAY);
}

uint8_t bootloader_verify_crc(uint8_t *pData, uint32_t len, uint32_t crc_host) {
    uint32_t uwCRCValue = 0;

    __HAL_CRC_DR_RESET(&hcrc);
    uwCRCValue = HAL_CRC_Accumulate(&hcrc, (uint32_t *)pData, len);

    return (uwCRCValue == crc_host) ? 0 : 1;
}

/* --- Flash Option Byte Helpers --- */

uint8_t configure_flash_sector_wrp(uint8_t sector_mask, uint8_t enable) {
    FLASH_OBProgramInitTypeDef OBInit = {0};
    HAL_StatusTypeDef status = HAL_OK;

    if (sector_mask == 0) return 0x01; // Invalid input

    // Extract start page (first set bit) and end page (last set bit)
    uint8_t start_page = 0xFF;
    uint8_t end_page   = 0;

    for (uint8_t i = 0; i < 8; i++) {
        if (sector_mask & (1 << i)) {
            if (start_page == 0xFF) start_page = i;
            end_page = i;
        }
    }

    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();

    OBInit.OptionType = OPTIONBYTE_WRP;
    OBInit.WRPArea    = OB_WRPAREA_BANK1_AREAA;

    if (enable) {
        OBInit.WRPStartOffset = start_page;
        OBInit.WRPEndOffset   = end_page;
    } else {
        /* On STM32G4, setting WRPStartOffset > WRPEndOffset disables protection */
        OBInit.WRPStartOffset = 0x7F;
        OBInit.WRPEndOffset   = 0x00;
    }

    status = HAL_FLASHEx_OBProgram(&OBInit);

    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();

    return (status == HAL_OK) ? 0x00 : 0x01;
}

/* --- Command Handlers --- */

void bootloader_handle_getver_cmd(uint8_t *pBuffer) {
    uint32_t pkt_len = pBuffer[0] + 1;
    uint32_t host_crc = *(uint32_t*)(pBuffer + pkt_len - 4);

    if (bootloader_verify_crc(pBuffer, pkt_len - 4, host_crc) == 0) {
        bootloader_send_ack(1);
        uint8_t ver = BL_VER;
        HAL_UART_Transmit(&huart2, &ver, 1, HAL_MAX_DELAY);
    } else {
        bootloader_send_nack();
    }
}

void bootloader_handle_gethelp_cmd(uint8_t *pBuffer) {
    uint32_t pkt_len = pBuffer[0] + 1;
    uint32_t host_crc = *(uint32_t*)(pBuffer + pkt_len - 4);

    if (bootloader_verify_crc(pBuffer, pkt_len - 4, host_crc) == 0) {
        bootloader_send_ack(sizeof(supported_commands));
        HAL_UART_Transmit(&huart2, (uint8_t*)supported_commands, sizeof(supported_commands), HAL_MAX_DELAY);
    } else {
        bootloader_send_nack();
    }
}

void bootloader_handle_getcid_cmd(uint8_t *pBuffer) {
    uint32_t pkt_len = pBuffer[0] + 1;
    uint32_t host_crc = *(uint32_t*)(pBuffer + pkt_len - 4);

    if (bootloader_verify_crc(pBuffer, pkt_len - 4, host_crc) == 0) {
        bootloader_send_ack(2);
        uint16_t mcu_id = (uint16_t)(DBGMCU->IDCODE & 0x0FFF);
        HAL_UART_Transmit(&huart2, (uint8_t*)&mcu_id, 2, HAL_MAX_DELAY);
    } else {
        bootloader_send_nack();
    }
}

void bootloader_handle_go_to_addr_cmd(uint8_t *pBuffer) {
    uint32_t pkt_len = pBuffer[0] + 1;
    uint32_t host_crc = *(uint32_t*)(pBuffer + pkt_len - 4);

    if (bootloader_verify_crc(pBuffer, pkt_len - 4, host_crc) == 0) {
        bootloader_send_ack(1);
        uint32_t target_addr = *(uint32_t*)&pBuffer[2];
        uint8_t addr_valid = 0x00;

        if ((target_addr >= FLASH_BASE && target_addr <= 0x0807FFFFU) ||
            (target_addr >= SRAM1_BASE && target_addr <= 0x2001FFFFU)) {

            HAL_UART_Transmit(&huart2, &addr_valid, 1, HAL_MAX_DELAY);

            HAL_RCC_DeInit();
            HAL_DeInit();

            uint32_t msp_val = *(volatile uint32_t*)target_addr;
            uint32_t reset_handler = *(volatile uint32_t*)(target_addr + 4);
            void (*app_jump)(void) = (void(*)(void))reset_handler;

            __set_MSP(msp_val);
            app_jump();
        } else {
            addr_valid = 0x01;
            HAL_UART_Transmit(&huart2, &addr_valid, 1, HAL_MAX_DELAY);
        }
    } else {
        bootloader_send_nack();
    }
}

void bootloader_handle_flash_erase_cmd(uint8_t *pBuffer) {
    uint32_t pkt_len = pBuffer[0] + 1;
    uint32_t host_crc = *(uint32_t*)(pBuffer + pkt_len - 4);

    if (bootloader_verify_crc(pBuffer, pkt_len - 4, host_crc) == 0) {
        bootloader_send_ack(1);

        uint8_t initial_page = pBuffer[2];
        uint8_t total_pages  = pBuffer[3];
        uint8_t status_code  = 0x00;

        if (initial_page >= 8 || initial_page == 0xFF) {
            FLASH_EraseInitTypeDef erase_handle;
            uint32_t page_error = 0;

            HAL_FLASH_Unlock();
            erase_handle.TypeErase = FLASH_TYPEERASE_PAGES;

            if (initial_page == 0xFF) {
                erase_handle.Banks   = FLASH_BANK_1;
                erase_handle.Page    = 8;
                erase_handle.NbPages = 248;
            } else {
                erase_handle.Banks   = (initial_page < 128) ? FLASH_BANK_1 : FLASH_BANK_2;
                erase_handle.Page    = initial_page;
                erase_handle.NbPages = total_pages;
            }

            if (HAL_FLASHEx_Erase(&erase_handle, &page_error) != HAL_OK) {
                status_code = 0x01;
            }
            HAL_FLASH_Lock();
        } else {
            status_code = 0x01;
        }
        HAL_UART_Transmit(&huart2, &status_code, 1, HAL_MAX_DELAY);
    } else {
        bootloader_send_nack();
    }
}

void bootloader_handle_mem_write_cmd(uint8_t *pBuffer) {
    uint32_t pkt_len = pBuffer[0] + 1;
    uint32_t host_crc = *(uint32_t*)(pBuffer + pkt_len - 4);

    if (bootloader_verify_crc(pBuffer, pkt_len - 4, host_crc) == 0) {
        bootloader_send_ack(1);

        uint32_t mem_address = *(uint32_t*)&pBuffer[2];
        uint8_t payload_len  = pBuffer[6];
        uint8_t *payload     = &pBuffer[7];
        uint8_t write_status = 0x00;

        if (mem_address >= 0x08004000U && mem_address <= 0x0807FFFFU) {
            HAL_FLASH_Unlock();

            for (uint32_t i = 0; i < payload_len; i += 8) {
                uint64_t data = 0xFFFFFFFFFFFFFFFF;
                memcpy(&data, &payload[i], ((payload_len - i) < 8) ? (payload_len - i) : 8);

                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, mem_address + i, data) != HAL_OK) {
                    write_status = 0x01;
                    break;
                }
            }
            HAL_FLASH_Lock();
        } else {
            write_status = 0x01;
        }
        HAL_UART_Transmit(&huart2, &write_status, 1, HAL_MAX_DELAY);
    } else {
        bootloader_send_nack();
    }
}

void bootloader_handle_en_r_w_protect_cmd(uint8_t *pBuffer) {
    uint32_t pkt_len = pBuffer[0] + 1;
    uint32_t host_crc = *(uint32_t*)(pBuffer + pkt_len - 4);

    if (bootloader_verify_crc(pBuffer, pkt_len - 4, host_crc) == 0) {
        bootloader_send_ack(1);

        uint8_t sector_details  = pBuffer[2];
        uint8_t protection_mode = pBuffer[3];
        uint8_t status = 0x00;

        if (protection_mode == 1) { // WRP Mode
            status = configure_flash_sector_wrp(sector_details, 1);
        } else {
            status = 0x01; // Unsupported mode
        }

        HAL_UART_Transmit(&huart2, &status, 1, HAL_MAX_DELAY);

        if (status == 0x00) {
            HAL_FLASH_Unlock();
            HAL_FLASH_OB_Unlock();
            HAL_FLASH_OB_Launch(); // Forces MCU Reset
        }
    } else {
        bootloader_send_nack();
    }
}

void bootloader_handle_dis_r_w_protect_cmd(uint8_t *pBuffer) {
    uint32_t pkt_len = pBuffer[0] + 1;
    uint32_t host_crc = *(uint32_t*)(pBuffer + pkt_len - 4);

    if (bootloader_verify_crc(pBuffer, pkt_len - 4, host_crc) == 0) {
        bootloader_send_ack(1);

        uint8_t status = configure_flash_sector_wrp(0xFF, 0); // Disable WRP
        HAL_UART_Transmit(&huart2, &status, 1, HAL_MAX_DELAY);

        if (status == 0x00) {
            HAL_FLASH_Unlock();
            HAL_FLASH_OB_Unlock();
            HAL_FLASH_OB_Launch(); // Forces MCU Reset
        }
    } else {
        bootloader_send_nack();
    }
}

void bootloader_handle_read_sector_protection_status_cmd(uint8_t *pBuffer) {
    uint32_t pkt_len = pBuffer[0] + 1;
    uint32_t host_crc = *(uint32_t*)(pBuffer + pkt_len - 4);

    if (bootloader_verify_crc(pBuffer, pkt_len - 4, host_crc) == 0) {
        bootloader_send_ack(2);

        uint16_t wrp_status = (uint16_t)(FLASH->WRP1AR & 0xFFFF);
        HAL_UART_Transmit(&huart2, (uint8_t*)&wrp_status, 2, HAL_MAX_DELAY);
    } else {
        bootloader_send_nack();
    }
}
void bootloader_handle_mem_read_cmd(uint8_t *pBuffer) {
    uint32_t pkt_len = pBuffer[0] + 1;
    uint32_t host_crc = *(uint32_t*)(pBuffer + pkt_len - 4);

    if (bootloader_verify_crc(pBuffer, pkt_len - 4, host_crc) == 0) {
        bootloader_send_ack(1);

        uint32_t mem_address = *(uint32_t*)&pBuffer[2];
        uint8_t read_len     = pBuffer[6];
        uint8_t status_code  = 0x00;

        // Validate address within valid Flash or SRAM space
        if ((mem_address >= FLASH_BASE && mem_address <= 0x0807FFFFU) ||
            (mem_address >= SRAM1_BASE && mem_address <= 0x2001FFFFU)) {

            HAL_UART_Transmit(&huart2, &status_code, 1, HAL_MAX_DELAY);
            HAL_UART_Transmit(&huart2, (uint8_t*)mem_address, read_len, HAL_MAX_DELAY);
        } else {
            status_code = 0x01; // Invalid address
            HAL_UART_Transmit(&huart2, &status_code, 1, HAL_MAX_DELAY);
        }
    } else {
        bootloader_send_nack();
    }
}

void bootloader_handle_otp_read_cmd(uint8_t *pBuffer) {
    uint32_t pkt_len = pBuffer[0] + 1;
    uint32_t host_crc = *(uint32_t*)(pBuffer + pkt_len - 4);

    if (bootloader_verify_crc(pBuffer, pkt_len - 4, host_crc) == 0) {
        bootloader_send_ack(1);

        uint8_t status_code = 0x00;
        // STM32G4 OTP memory base address (1024 bytes total space)
        uint8_t *otp_addr = (uint8_t *)0x1FFF7000U;

        // Send status code OK (0x00), then transmit first 64 bytes of OTP
        HAL_UART_Transmit(&huart2, &status_code, 1, HAL_MAX_DELAY);
        HAL_UART_Transmit(&huart2, otp_addr, 64, HAL_MAX_DELAY);
    } else {
        bootloader_send_nack();
    }
}


void bootloader_handle_my_new_cmd(uint8_t *pBuffer) {
    uint32_t pkt_len = pBuffer[0] + 1;
    uint32_t host_crc = *(uint32_t*)(pBuffer + pkt_len - 4);

    if (bootloader_verify_crc(pBuffer, pkt_len - 4, host_crc) == 0) {
        bootloader_send_ack(1);

        // Example custom action: Respond with status code 0x00 (SUCCESS)
        uint8_t status = 0x00;
        HAL_UART_Transmit(&huart2, &status, 1, HAL_MAX_DELAY);
    } else {
        bootloader_send_nack();
    }
}

/* --- Main Command Dispatcher --- */

void bootloader_parse_and_execute(uint8_t *pBuffer) {
    uint8_t command_code = pBuffer[1];

    switch (command_code) {
        case BL_GET_VER:
            bootloader_handle_getver_cmd(pBuffer);
            break;
        case BL_GET_HELP:
            bootloader_handle_gethelp_cmd(pBuffer);
            break;
        case BL_GET_CID:
            bootloader_handle_getcid_cmd(pBuffer);
            break;
        case BL_GO_TO_ADDR:
            bootloader_handle_go_to_addr_cmd(pBuffer);
            break;
        case BL_FLASH_ERASE:
            bootloader_handle_flash_erase_cmd(pBuffer);
            break;
        case BL_MEM_WRITE:
            bootloader_handle_mem_write_cmd(pBuffer);
            break;
        case BL_EN_R_W_PROTECT:
            bootloader_handle_en_r_w_protect_cmd(pBuffer);
            break;
        case BL_MY_NEW_COMMAND:
            bootloader_handle_my_new_cmd(pBuffer);
                    break;
        case BL_OTP_READ:
            bootloader_handle_otp_read_cmd(pBuffer);
                    break;
        case BL_MEM_READ:
            bootloader_handle_mem_read_cmd(pBuffer);
                    break;
        case BL_DIS_R_W_PROTECT:
            bootloader_handle_dis_r_w_protect_cmd(pBuffer);
            break;
        case BL_READ_SECTOR_P_STATUS:
            bootloader_handle_read_sector_protection_status_cmd(pBuffer);
            break;
        default:
            bootloader_send_nack();
            break;
    }
}
