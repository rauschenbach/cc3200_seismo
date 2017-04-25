#ifndef _MY_CMD_H
#define _MY_CMD_H

#include "config.h"
#include "my_defs.h"


#define MAX_FILE_SIZE		1024

/* ��c��������� USB ������������ ��� ���  */
#define 	ENABLE_WIRELESS_USB
//#undef                ENABLE_WIRELESS_USB

/* ��������� ������� � Sleep �����  */
#define 	ENABLE_PLL_SLEEP
//#undef          ENABLE_PLL_SLEEP


/* ��� ��� - ��� ������ ��� �������� ������   */
#define 	ENABLE_LOCK_FILE
//#undef          ENABLE_LOCK_FILE


/* � ��������������� ����� ������� ������� �������������� ����� */
#define		ENABLE_TEST_CHANNEL
//#undef                ENABLE_TEST_CHANNEL

/* ��� ����� - ����� �������� ������ ��� ���� ������ (�����)-����� ������� �� RECORD_TIME �����! */
#define 	ENABLE_TEST_TIMES
#undef 		ENABLE_TEST_TIMES

/* �������� �� ����� ��� ���  */
#define 	ENABLE_UART_DEBUG
#undef 		ENABLE_UART_DEBUG

/* ��� ����� - ���������� ���� ������ � ������� ��� ��� */
#define 	ENABLE_POWER_ON
#undef 		ENABLE_POWER_ON

/* ����� �������� ����������� ������ �� WUSB  */
#define		WAIT_WUSB_TIME			5

/* ����� ������, � ������� ������� ����� ������� �������� ������� � PC - ������� */
//#define       WAIT_PC_TIME                    115
#define 	WAIT_PC_COMMAND			WAIT_PC_TIME

/* ����� ������, � ������� ������� �� ����� ����� ������������� */
#define		WAIT_TIM3_SYNC			10

/* ����� �������� 3DFIX - 5 ����� */
//#define       WAIT_3DFIX_TIME                 300

/*vvvvvvvvvvvv 600 ������ ����� ��� ����� ��������� 3DFIX - */
#define 	SLEEP_3DFIX_TIME		600

/* ����� ������� � ������ ����� �������� 3d Fix 255 = (-1) */
#define 	NUM_3DFIX_ATEMPT		255



/* �������� ������ ����� ���������� */
//#define       TIME_START_AFTER_WAKEUP         180

/*����������� ����� ������ - 5 �����  */
//#define       RECORD_TIME                     300

/* �������� �������� ����� ���������  ����������� - 2 ������� */
//#define       TIME_BURN_AFTER_FINISH          2

/* ����� ����������� �������� - ��� ����� ����� ������������ �  ������� ���������  */
#define 	RELEBURN_TIME_AIRLINK			60

/* ����� �������� - ��� ����� ���������� � ������� �������� */
#define 	POPUP_DURATION_AIRLINK		1


/* �� ���� ����� ����� �������� ���� ���������� */
#define 	BROADCAST_ADDR	0xff

/* ������� ������� � �����  */
#define 	NUM_ADS1282_PACK	20
#define		MAGIC				0x4b495245
#define 	NMEA_PACK_SIZE 			128	/* ������ ������ ��� ���������� ����� NMEA */


/**
 * ��������� ��� ��� ���� 4-� �������.
 * �������� 0 � �������� 
 */
typedef struct {
    u32 magic;			/* �����. ����� */
    struct {
	u32 offset;		/* ����������� 1 - �������� */
	u32 gain;		/* ����������� 2 - �������� */
    } chan[4];			/* 4 ������ */
} ADS131_Regs;

/* �����-������� */
typedef struct {
    u32 gns_name;
    u32 baud;
    c8 board;
    u8 port_num;
    u8 v;
    u16 r;
    String ver;
} STATION_PORT_STRUCT;

/**
 * � ���� ���������� ������������ ��������� ������
 */
#pragma pack(4)
typedef struct {
    u32 modem_num;
    SYSTEMTIME modem_time;
    SYSTEMTIME alarm_time;	/* ��������� ����� �������� �� ������   */
    u16 am3_h0_time;
    u16 am3_m0_time;
    u16 am3_h1_time;
    u16 am3_m1_time;
    u32 time_for_release;
    u32 burn_len;
} MODEM_STRUCT;

/* ��������� ������ ��������� ��� ���� ��� */
typedef struct {
    u8 mode;			/* ����� ������ �������� */
    u8 sps;			/* �������  */
    u8 pga;			/* PGA */
    u8 mux;			/* ������������� �� ����� */
    u8 test_sign;
    u8 test_freq;
    u8  bitmap;			/* ���������� ������: 1 ����� �������, 0 - �������� */
    u8  file_len;		/* ����� ����� ������ � ����� */
} START_ADC_STRUCT;


#pragma pack(4)
typedef struct {
    c8 my_str[0x80];
    u32 start_time;		/* ����� ������ �����������  */
    u32 finish_time;		/* ����� ��������� ����������� */
    u32 burn_time;		/* ����� ������ �������� ��������  */
    u32 data_file_len;		/* ������ ����� ������ */
    s32 am3_modem_type;		/* ��� ������ */
    s16 popup_len;		/* ������������ �������� */
    s16 modem_num;		/* ����� ������ */
    u32 am3_popup_time;		/* ��������� ����� �������� �� ������   */

    u16 releburn_time;		// ������� ������ ����������
    u8 am3_cal_h0;
    u8 am3_cal_m0;

    u8 am3_cal_h1;
    u8 am3_cal_m1;
    u16 sample_freq;		/* ������� ��������� ��� (250 Hz, 500 Hz, 1000 Hz) */

    u8 power_cons;		/* ����������������  */
    u8 pga_adc;			/* PGA ��� (1, 2, 4, 8, 16, 32, 64) */
    u8 num_bytes;		/* ����� ���� � ����� ������ (3 ��� 4) */
    u8 on_off_channels;		/* ���������� ������ ->(1-�, 2-�, 3-�, 4-�) ������ ������. 4 ����� �����������! */

    u8 const_reg_flag;
    int pos;			/* ������� ��������� */
    float filter_freq;		/* ������� ����� ������� */
} param_file_struct;

enum TIME_ENUM {
    TIME_10_MIN = 0,
    TIME_60_MIN,
    TIME_1_DAY,
    TIME_1_WEEK,
    TIME_1_MONTH
};


typedef struct {
    SYSTEMTIME utc;
    double lat;
    double lon;
} NMEA_params;

// ������ ������
typedef struct {
    int tx_ok;
    int rx_err;
} XCHG_ERR_COUNT;



/* RTC ����� */
#pragma pack (1)
typedef struct {
    u8 len;
    u32 time;
    u16 crc16;
} RTC_TIME_STRUCT;


// �������� ����� ��� NMEA
#pragma pack(1)
typedef struct {
    u8 len;
    c8 str[NMEA_PACK_SIZE];
    u16 crc16;
} NMEA_RX_BUF;


// ����
enum myLangID {
    lang_rus,
    lang_eng,
};




#endif				/* my_cmd.h  */
