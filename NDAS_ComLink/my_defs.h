#ifndef _MY_DEFS_H
#define _MY_DEFS_H

#include <signal.h>
#include <stdint.h>
#include <time.h>

/* ������� ������� � �����  */
#define 	NUM_ADS131_PACK	20

#define		SEC_IN_HOUR		3600

/* ��������� ��� 504 ��������, �� 506!!!!  */
#ifndef _WIN32			/* Embedded platform */
#include <float.h>



#else				/* windows   */
#include <windows.h>
#include <tchar.h>
#endif

#ifndef u8
#define u8 unsigned char
#endif

#ifndef s8
#define s8 char
#endif

#ifndef c8
#define c8 char
#endif

#ifndef u16
#define u16 unsigned short
#endif


#ifndef s16
#define s16 short
#endif

#ifndef i32
#define i32  int
#endif


#ifndef u32
#define u32 unsigned long
#endif


#ifndef s32
#define s32 long
#endif


#ifndef u64
#define u64 uint64_t
#endif


#ifndef s64
#define s64 int64_t
#endif


/* ������� ����� */
#ifndef	time64
#define time64	int64_t
#endif


#ifndef _WIN32			/* Embedded platform */

/* long double �� �������������  */
#ifndef f32
#define f32 float
#endif

#ifndef bool
#define bool u8
#endif


#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#else
/* long double �� �������������  */
#ifndef f32
#define f32 float
#endif

#endif


#ifndef IDEF
#define IDEF static inline
#endif

/* �� ���� ����� ����� �������� ���� ���������� */
#define 	BROADCAST_ADDR	0xff


#define		MAGIC				0x4b495245

/* �������� */
#define		TIMER_NS_DIVIDER	(1000000000UL)
#define		TIMER_US_DIVIDER	(1000000)
#define		TIMER_MS_DIVIDER	(1000)


/* ����� UNIX � ������������ */
typedef int64_t UNIX_TIME_t;



/* ������ ���������� ����� */
typedef enum {
	CLOCK_RTC_TIME = 0,
	CLOCK_NO_GPS_TIME,
	CLOCK_PREC_TIME,
	CLOCK_NO_TIME = (-1)
} CLOCK_STATUS_EN;


/*******************************************************************************
 * ��������� ��������� ��� �������� State machine
 *******************************************************************************/
typedef enum {
    DEV_POWER_ON_STATE = 0,
    DEV_CHOOSE_MODE_STATE = 1,
    DEV_INIT_STATE,
    DEV_TUNE_WITHOUT_GPS_STATE,	/*������ ������� ��� GPS */
    DEV_GET_3DFIX_STATE,
    DEV_TUNE_Q19_STATE,		/* ��������� ��������� ������ 19 ��� */
    DEV_SLEEP_AND_DIVE_STATE,	/* ���������� � ��� ����� ������� ������ */
    DEV_WAKEUP_STATE,
    DEV_REG_STATE,
    DEV_FINISH_REG_STATE,
    DEV_BURN_WIRE_STATE,
    DEV_SLEEP_AND_POPUP_STATE,
    DEV_WAIT_GPS_STATE,
    DEV_HALT_STATE,
    DEV_COMMAND_MODE_STATE,
    DEV_POWER_OFF_STATE,
    DEV_ERROR_STATE = -1	/* ������ */
} DEV_STATE_ENUM;


/** 
 * ������ � ������ ��������� �� ����������� 64 ����. 
 * �������� ������ ���������� ��� ������ (len + st_main + 2 ����� CRC16)
 */
#pragma pack(4)
typedef struct {
    u8  len;			/* 1......����� ������ */
    u8  st_main;		/* 2......������� ������: ��� �������, ��� ��������, ������ � �������, ��� �����, ����������, ������������ ������, ����������, ����� ������ */
    u16 ports;                  /* 3-4....���������� ���������� */

    u16  test;			/* 3......���������������� � ������: 0 - �����, 1 - ������ T&P, 2 - �������/ ������, 3 - �����, 4 - GPS, 5 - EEPROM, 6 - ����� SD, 7 - flash */
    u8   reset;			/* 7......������� ������ */
    u8   adc;			/* 8......������ ��� */

    s16 temper0;		/* 9-10...����������� �� �������� ����� * 10 */
    s16 temper1;		/* 11-12..����������� ����� (���� ����) * 10 */ 

    s16 power_mv;    	        /* 13-14..���������� �������, �� */
    s16 pitch;			/* 15-16..���� */

    s16 roll;			/* 17-18..������ */
    s16 head;			/* 19-20..������ */

    u32 press;			/* 21-24..��������, ��� */

    /* ��������� ������� � ������������ (�� ������) */
    s32 lat;			/* 25-28..������ (+ ~N, - S):  55417872 ~ 5541.7872N */
    s32 lon;			/* 29-32..�������(+ ~E, - W): -37213760 ~ 3721.3760W */
    s32 gps_time;		/* 33-36..����� GPS */

    s32 gns_time;		/* 37-40..���������� ����� ������� */

    s32 comp_time;		/* 41-44..����� ���������� ��������� */
    u16 dev_addr;		/* 45-46..����� ����� (����� ����������) */
    u8  ver;			/* 47.....������ ��: 1, 2, 3, 4 ��� */
    u8  rev;                    /* 48.....������� �� 0, 1, 2 ��� */

    u32 rsvd0[3];               /* 49-60 */

    u16 rsvd1;                  /* 61-62  */
    u16 crc16;			/* 63-64..����������� ����� ������  */
} DEV_STATUS_STRUCT;


/**
 * ��������� ������� ������� 
 * � ���� ���������� ������������ ��������� ��������
 * ������������� ����� ������:
 * 16.10.12 17.15.22	// ����� ������ �����������
 * 17.10.12 08.15.20	// ����� ��������� �����������
 * 18.10.12 11.17.00	// ����� ��������, �������� 5 ���. ������� ��������
 * ....
 */
#pragma pack(4)
typedef struct {
    /* ����� ����������� */
    u32 gns110_pos;		/* ������� ��������� */

    char gns110_dir_name[18];	/* �������� ���������� ��� ���� ������ */
    u8   gns110_file_len;		/* ������ ����� ������ � ����� */
    bool gns110_const_reg_flag;	/* ���������� ����������� */


    /* ����� */
    s32 gns110_modem_rtc_time;	/* ����� ����� ������ */
    s32 gns110_modem_alarm_time;	/* ��������� ����� �������� �� ������   */
    s32 gns110_modem_type;	/* ��� ������ */
    u16 gns110_modem_num;	/* ����� ������ */
    u16 gns110_modem_burn_len_sec;	/* ������������ �������� ��������� � �������� �� ������ */
    u8 gns110_modem_h0_time;
    u8 gns110_modem_m0_time;
    u8 gns110_modem_h1_time;
    u8 gns110_modem_m1_time;

    /* ������� */
    s32 gns110_wakeup_time;	/* ����� ������ ���������� ����� ������������ */
    s32 gns110_start_time;	/* ����� ������ �����������  */
    s32 gns110_finish_time;	/* ����� ��������� ����������� */
    s32 gns110_burn_time;	/* ����� ������ �������� ��������  */
    s32 gns110_popup_time;	/* ������ ������ �������� */
    s32 gns110_gps_time;	/* ����� ��������� gps ����� ������� �������� */

    /* ��� */
    f32 gns110_adc_flt_freq;	/* ������� ����� ������� HPF */

    u16 gns110_adc_freq;	/* ������� ���  */
    u8 gns110_adc_pga;		/* �������� ���  */
    u8 gns110_adc_bitmap;	/* ����� ������ ��� ������������ */
    u8 gns110_adc_consum;	/* ����������������� ��� ���� */
    u8 gns110_adc_bytes;	/* ����� ���� � ����� ������  */
    u8 rsvd[2];
} GNS110_PARAM_STRUCT;



/* ���������, ����������� �����, �������� c8 �� u8 - ����� �� �.�. ������������� */
#pragma pack(1)
typedef struct {
    u8 sec;
    u8 min;
    u8 hour;
    u8 week;			/* ���� ������...�� ����������� */
    u8 day;
    u8 month;			/* 1 - 12 */
#define mon 	month
    u16 year;
} TIME_DATE;
#define TIME_LEN  8		/* ���� */


/**
 * � ��� �������� ������� �������� ��������� ��� ������ �� SD ����� (pack �� 1 �����!)
 */
#if defined   ENABLE_NEW_SIVY
/* � ����� SIVY �������� �� 4 ����� */
#pragma pack(4)
typedef struct {
    c8 DataHeader[12];		/* ��������� ������ SeismicDat0\0  */

    c8 HeaderSize;		/* ������ ��������� - 80 ���� */
    u8 ConfigWord;		/* ������������ */
    c8 ChannelBitMap;		/* ���������� ������: 1 ����� ������� */
    c8 SampleBytes;		/* ����� ���� ����� ������ */

    u32 BlockSamples;		/* ������ 1 ��������� ����� � ������ */

    s64 SampleTime;		/* ����� ��������� ������: ����������� ������� UNIX */
    s64 GPSTime;		/* ����� �������������: ����������� ������� UNIX  */
    s64 Drift;			/* ����� �� ������ ����� GPS: �����������  */

    u32 rsvd0;			/* ������. �� ������������ */

    /* ��������� �����: �����������, ���������� � �� */
    u16 u_pow;			/* ���������� �������, U mv */
    u16 i_pow;			/* ��� �������, U ma */
    u16 u_mod;			/* ���������� ������, U mv */
    u16 i_mod;			/* ��� ������, U ma */

    s16 t_reg;			/* ����������� ������������, ������� ���� �������: 245 ~ 24.5 */
    s16 t_sp;			/* ����������� ������� �����, ������� ���� �������: 278 ~ 27.8  */
    u32 p_reg;			/* �������� ������ ����� ��� */

    /* ��������� ������� � ������������ (�� ������) */
    s32 lat;			/* ������ (+ ~N, - S):   55417872 ~ 5541.7872N */
    s32 lon;			/* �������(+ ~E, - W): -37213760 ~ 3721.3760W */

    s16 pitch;			/* ����, ������� ���� �������: 12 ~ 1.2 */
    s16 roll;			/* ������, ������� ���� ������� 2 ~ 0.2 */

    s16 head;			/* ������, ������� ���� ������� 2487 ~ 248.7 */
    u16 rsvd1;			/* ������ ��� ������������ */
} ADC_HEADER;
#else

#pragma pack(1)
typedef struct {
    c8 DataHeader[12];		/* ��������� ������ SeismicData\0  */

    c8 HeaderSize;		/* ������ ��������� - 80 ���� */
    c8 ConfigWord;
    c8 ChannelBitMap;
    u16 BlockSamples;
    c8 SampleBytes;

    TIME_DATE SampleTime;	/* �������� ����� ������ */

    u16 Bat;
    u16 Temp;
    u8  Rev;			/* ������� = 2 ������  */
    u16 Board;

    u8  NumberSV;
    s16 Drift;

    TIME_DATE SedisTime;	/* �� ������������ ����  */
    TIME_DATE GPSTime;		/* ����� ������������� */

    union {
	struct {
	    u8 rsvd0;
	    bool comp;		/* ������ */
	    c8 pos[23];		/* ������� (����������) */
	    c8 rsvd1[3];
	} coord;		/* ���������� */

	u32 dword[7];		/* ������ */
    } params;
} ADC_HEADER;
#endif



#endif				/* my_defs.h */
