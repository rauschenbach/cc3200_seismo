#ifndef _PROTO_H
#define _PROTO_H


#include "my_defs.h"

/***************************************************************************
 *   ����� ��������
 ***************************************************************************/
#define			NDAS		(17486)	/* 0x444E */
#define			DEV_VERSION	0x78563412



/* ���� �������.����� ��������� �������� 0 (REQUEST), 1 (ACK), 2 (NAK)  - 2 ����� */
typedef enum {
    PACK_REQUEST,
    PACK_ACK,
    PACK_NAK,
    PACK_NO_DATA,	// 3 �� ������������, ������������ EMPTY
    PACK_EMPTY,	// 4 ���� ������ ���, ������������ ���� ���
    PACK_RSVD = 257
} PACK_TYPE;

/* �������, ������� ������������ COM ���� ��� Socket - 2 ����� */
typedef enum {
    CMD_QUERY_SIGNAL = 2,   /* !!! 2 �� ������������, ������ �� 12 */
    CMD_RECEIVE_PARAMETERS,	//3
    CMD_START_ACQUIRING,	/* !!! 4 - �� ������������. ������ ��� 11 */
    CMD_STOP_ACQUIRING,		//5
    CMD_WRITE_PARAMETERS,	//6
    CMD_APPLY_PARAMETERS,	//7
    CMD_QUERY_DEVICE,		//8
    CMD_QUERY_STAT,		//9
    CMD_QUERY_INFO,		//10
    CMD_START_PREVIEW,		/** 11 ����� ����� ������� � ������ preview */
    CMD_QUERY_PREVIEW,		/** 12 �������� ������������ � ������ preview */

    CMD_SELECT_SD_TO_CR = 21,   /* 21 - ��������� ��������� � ����� card reader */
    CMD_SELECT_SD_TO_MCU,	/* 22 - ��������� ��������� */
    CMD_RESET_MCU,		/* 23 - ����� CPU */

    CMD_GET_COUNT = 49,		/* ������ �������� �������  */

    CMD_RSVD = 257
} USER_CMD;


/**
 * ���������, ����������� ��������� ���������. 20 ����  
 */
typedef struct {
    u16 ndas;			/* ������ ������������ NDAS ==  0x444E */
    u16 crc16;			/* CRC16 ������� ����� HEADER_t ���� ���� */
    u32 dev_id;			/* ����� ������� */
    PACK_TYPE pack_type;	/* request/ack/nak */
    USER_CMD cmd;		/* ������� */
    u32 ver;			/* ������ - ���� 0 */
    u16 size;			/* ������, 0 ���� ������ */
    u16 rsvd0;
} HEADER_t;



/**
 * ��������� ����������� ����� - 8 ���� 
 */
typedef struct {
    unsigned board_1_n_of_ch:4;
    unsigned board_1_type:4;

    unsigned board_2_n_of_ch:4;
    unsigned board_2_type:4;

    unsigned board_3_n_of_ch:4;
    unsigned board_3_type:4;

    unsigned board_4_n_of_ch:4;
    unsigned board_4_type:4;

    unsigned board_5_n_of_ch:4;
    unsigned board_5_type:4;

    unsigned board_6_n_of_ch:4;
    unsigned board_6_type:4;

    unsigned board_7_n_of_ch:4;
    unsigned board_7_type:4;

    unsigned board_8_n_of_ch:4;
    unsigned board_8_type:4;
} BOARD_t;


/**
 * ��������� ��� - 4 �����
 */
typedef struct {
    u16 int_adc1_1:1;
    u16 int_adc1_2:1;
    u16 int_adc1_3:1;
    u16 int_adc1_4:1;
    u16 int_adc1_5:1;		/* ��������� ����� ���������� ������ - �� ������� */
    u16 int_adc1_6:1;		/* ������� ������ �������� � ��������� ��������� */
    u16 int_adc1_7:1;
    u16 int_adc1_8:1;
    u16 int_adc2_1:1;
    u16 int_adc2_2:1;
    u16 int_adc2_3:1;
    u16 int_adc2_4:1;
    u16 int_adc2_5:1;
    u16 int_adc2_6:1;
    u16 int_adc2_7:1;
    u16 int_adc2_8:1;
} ADC_INT_CH_SEL_bit_t;

/**
 * ��������� ��������� ��������� ����� � �������� ���. 
 * �� ������ ����� �� 6 �������. 4 �����
 */
typedef struct {
    unsigned board_active:1;	/* ������� �� ����� ������ */

    unsigned ch_1_en:1;		/* ������� �� �����        */
    unsigned ch_1_range:1;	/* �������� 2.5 - 10 */
    unsigned ch_1_gain:3;	/* GAIN */

    unsigned ch_2_en:1;
    unsigned ch_2_range:1;
    unsigned ch_2_gain:3;

    unsigned ch_3_en:1;
    unsigned ch_3_range:1;
    unsigned ch_3_gain:3;

    unsigned ch_4_en:1;
    unsigned ch_4_range:1;
    unsigned ch_4_gain:3;

    unsigned ch_5_en:1;
    unsigned ch_5_range:1;
    unsigned ch_5_gain:3;

    unsigned ch_6_en:1;
    unsigned ch_6_range:1;
    unsigned ch_6_gain:3;

    unsigned rsvd0:1;
} ADC_BOARD_CFG_t;


/**
 * ��������� ��� - 2 �����
 */
typedef struct {
    u16 board_type:2;		/* ��� ����� � ���. (�� ������� ������ ����� ��� �������� ��������) */
    u16 power_mode:1;
    u16 sample_rate:3;
    u16 fir_phase_response:1;
    u16 test_signal:1;
    u16 int_adc1_sync:1;
    u16 int_adc1_ckldiv:3;
    u16 int_adc2_sync:1;
    u16 int_adc2_ckldiv:3;
} ADC_PARAM_t;


/* ��������� GPS - 24 ����� */
typedef struct GPS_Time_and_Position {
    UNIX_TIME_t time;	/* ����� UNIX � �� � 1970 ���� */
    u8   fix;		/* 0 - time is not valid, 1 - valid GPS fix */
    u8   numSV;		/* Number of satellites in NAV Solution */
    s16  hi;		/* ������ ��� ������� ���� */
    s32  lon;		/* Longitude, deg */
    s32  lat;		/* Latitude, deg */
    s32  tAcc;		/* Time accuracy estimate */
} GPS_DATA_t;


/****************** �������������� ��������� �� �����/�������� � ���������� ******************/
/* ID ����������  - ������ ��������� � ACK */
typedef struct {
    HEADER_t header;
/*    u32 device_id; */
} DEV_ID_h;


/* ����� (������) */
typedef struct {
    HEADER_t header;
    BOARD_t board_state;
} INFO_h;

#pragma pack(2)
typedef struct {
    HEADER_t header;
    ADC_BOARD_CFG_t adc_board_1;	/* ����� ����������� ��������� �� 8 ���� */
    ADC_BOARD_CFG_t adc_board_2;
    ADC_BOARD_CFG_t adc_board_3;
    ADC_BOARD_CFG_t adc_board_4;
    ADC_BOARD_CFG_t adc_board_5;
    ADC_BOARD_CFG_t adc_board_6;
    ADC_BOARD_CFG_t adc_board_7;
    ADC_BOARD_CFG_t adc_board_8;
    u16 power_mode:1;
    u16 sample_rate:3;
    u16 fir_phase_response:1;
    u16 test_signal:1;
    u16 ext_adc_averaging:10;
} ADCP_h;


/** ��������� ������� START_PREVIEW */
typedef struct {
	HEADER_t header;
	u64 time_stamp;             /* UNIX TIME ������� ��������� */
} START_PREVIEW_h;              


/* ��������� � ���������� - �� ����� � �������� */
void proto_get_info(INFO_h*);
void proto_get_dev_id(DEV_ID_h*);
void proto_save_ip_addr(u32);


#endif				/* proto.h */
