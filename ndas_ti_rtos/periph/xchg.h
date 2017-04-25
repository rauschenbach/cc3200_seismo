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

/* ������ ������ */
#define 	RX_DATA_OK			0
#define		RX_DATA_ERR			1
#define		RX_CRC_ERR			2
#define		RX_OVR_ERR			3
#define		RX_TIMEOUT			4
#define 	XCHG_BUF_LEN			512


/* ��������� �� ���� ��������� ������ ��� ��������! */
#pragma pack(4)
typedef struct {
    HEADER_t hdr;
    u8 data[XCHG_BUF_LEN];	/* �� ����� */
    u16 cnt;			/* ������� ���������/����������� */
    u8 ind;			/* ������ ������ */
    u8 end;			/* ����� ������ */
} XCHG_DATA_STRUCT_h;

/**
 * �������� ������� �� ����
 */
#pragma pack(4)
typedef struct {
    HEADER_t hdr;
    int rx_pack;		/* �������� ������ */
    int tx_pack;		/* ���������� ������ */
    int rx_cmd_err;		/* ������ � ������� */
    int rx_stat_err;		/* ������ ���������, ����� (��� ������) � �� */
    int rx_crc_err;		/* ������ ����������� ����� */
    int rx_timeout;		/* ������� ������ */
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


/* ��� ���������� */
typedef enum {
    CONNECT_COMM = 0,
    CONNECT_UDP,
    CONNECT_TCP,
    CONNECT_NONE = -1,
} CONNECTION_TYPE;


void xchg_start(GNS_PARAM_STRUCT *);
void com_init(void);
void com_mux_config(void);


/* �������� */
void set_timeout(int);
bool is_timeout(void);
void clr_timeout(void);



#endif				/* xchg_drv.h */
