#ifndef _XCHG_H
#define _XCHG_H



#include <hw_memmap.h>
#include <hw_common_reg.h>
#include <hw_uart.h>
#include <hw_types.h>
#include <hw_ints.h>
#include <uart.h>
#include <udma.h>
#include <interrupt.h>
#include <utils.h>
#include <prcm.h>
#include "my_defs.h"
#include "proto.h"

/* Ошибки приема */
#define 	RX_DATA_OK			0
#define		RX_DATA_ERR			1
#define		RX_CRC_ERR			2
#define		RX_OVR_ERR			3
#define		RX_TIMEOUT			4
#define 	XCHG_BUF_LEN			512


/* Указатель на нашу структуру приема или передачи! */
#pragma pack(4)
typedef struct {
    HEADER_t hdr;
    u8 data[XCHG_BUF_LEN];	/* На прием */
    u16 cnt;			/* Счетчик принятого/переданного */
    u8 ind;			/* Начало пакета */
    u8 end;			/* Конец приема */
} XCHG_DATA_STRUCT_h;

/**
 * Счетчики обменов по УАРТ
 */
#pragma pack(4)
typedef struct {
    HEADER_t hdr;
    int rx_pack;		/* Принятые пакеты */
    int tx_pack;		/* Переданные пакеты */
    int rx_cmd_err;		/* Ошибка в команде */
    int rx_stat_err;		/* Ошибки набегания, кадра (без стопов) и пр */
    int rx_crc_err;		/* Ошибки контрольной суммы */
    int rx_timeout;		/* Таймаут приема */
} XCHG_COUNT_STRUCT_h;


/* Application specific status/error codes */
typedef enum {
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    LAN_CONNECTION_FAILED = -0x7D0,
    INTERNET_CONNECTION_FAILED = LAN_CONNECTION_FAILED - 1,
    DEVICE_NOT_IN_STATION_MODE = INTERNET_CONNECTION_FAILED - 1,
    SOCKET_CREATE_ERROR = -0x7D0,
    BIND_ERROR = SOCKET_CREATE_ERROR - 1,
    SEND_ERROR = BIND_ERROR - 1,
    RECV_ERROR = SEND_ERROR - 1,
    SOCKET_CLOSE = RECV_ERROR - 1,
    STATUS_CODE_MAX = -0xBB8
} e_AppStatusCodes;


/* Тип соединения */
typedef enum {
    CONNECT_COMM = 0,
    CONNECT_UDP,
    CONNECT_TCP,
    CONNECT_NONE = -1,
} CONNECTION_TYPE;


void xchg_start(GNS_PARAM_STRUCT *);
void com_init(void);
void com_mux_config(void);


/* Таймауты */
void set_timeout(int);
bool is_timeout(void);
void clr_timeout(void);



#endif				/* xchg_drv.h */
