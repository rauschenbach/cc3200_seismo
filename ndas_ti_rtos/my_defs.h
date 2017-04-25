#ifndef _MY_DEFS_H
#define _MY_DEFS_H

#include <rom.h>
#include <rom_map.h>


#include <signal.h>
#include <stdint.h>
#include <time.h>

/* ������� ������� � �����  */
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


#ifndef IDEF
#define IDEF static inline
#endif

/* �� ���� ����� ����� �������� ���� ���������� */
#define 	BROADCAST_ADDR	0xff


#define		MAGIC				0x4b495245

/* �������� */
#define 	SYSCLK          	80000000UL

#define		TIMER_NS_DIVIDER	(1000000000UL)
#define		TIMER_US_DIVIDER	(1000000)
#define		TIMER_MS_DIVIDER	(1000)
#define		DATA_FILE_LEN		4 /* 4 ���� � ����� ����� */



/* ���� ��� �����. ������ */
#define OSI_STACK_SIZE          1024

/* ����� UNIX � ������������ */
typedef int64_t UNIX_TIME_t;


/* ������ ���������� ����� */
typedef enum {
	CLOCK_NO_GPS_TIME,
	CLOCK_GPS_TIME,
	CLOCK_UTC_TIME,
	CLOCK_RTC_TIME,
} CLOCK_STATUS_EN;


/* ����� ������ �������/ �� �������, ���� ��� � ������� ������ */
typedef enum {
	GNS_CMD_MODE,
	GNS_NORMAL_MODE,
} GNS_WORK_MODE;

/*******************************************************************************
 * ��������� ��������� ��� �������� State machine
 *******************************************************************************/
typedef enum {
	DEV_POWER_ON_STATE = 0,
	DEV_CHOOSE_MODE_STATE = 1,
	DEV_INIT_STATE,
	DEV_TUNE_WITHOUT_GPS_STATE,	/*������ ������� ��� GPS */
	DEV_GET_3DFIX_STATE,
	DEV_TUNE_Q19_STATE,	/* ��������� ��������� ������ 19 ��� */
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
	u16 gns_addr;		/* ����� ������� */
	c8  gns_dir_name[18];	/* �������� ���������� ��� ���� ������ */


	/* ������� */
	s32 gns_wakeup_time;	/* ����� ������ ���������� ����� ������������ */
	s32 gns_start_time;	/* ����� ������ �����������  */
	s32 gns_finish_time;	/* ����� ��������� ����������� */
	s32 gns_burn_time;	/* ����� ������ �������� ��������  */
	s32 gns_popup_time;	/* ������ ������ �������� */
	s32 gns_gps_time;	/* ����� ��������� gps ����� ������� �������� */

	/* Wlan */
	c8  gns_ssid_name[32];	/* �������� AP */
	c8  gns_ssid_pass[32];	/* ������ */

	s16 gns_ssid_sec_type;	/* ��� ���������� */
	s16 gns_server_udp_port;	/* ���� UDP ������� */

	s16 gns_server_tcp_port;	/* ���� TCP ������� */
	u8  gns_mux;		/* ������������� �� ����� */
	u8  gns_test_sign;

	u8  gns_adc_pga[8];	/* �������� ���  */

	u8  gns_test_freq;
	u8  gns_adc_bitmap;	/* ����� ������ ��� ������������ */
	u8  gns_adc_consum;	/* ����������������� ��� ���� */
	u8 gns_adc_bytes;	/* ����� ���� � ����� ������  */

	u16 gns_adc_freq;	/* ������� ���  */
	u8 gns_work_mode;       /* ����� ������: ���� / com ����/ ������� ����� �� ������� */

	u8 rsvd[5];
} GNS_PARAM_STRUCT;



/* ���������, ����������� �����, �������� c8 �� u8 - ����� �� �.�. ������������� */
#pragma pack(1)
typedef struct {
	u8 sec;
	u8 min;
	u8 hour;
	u8 week;		/* ���� ������...�� ����������� */
	u8 day;
	u8 month;		/* 1 - 12 */
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
	c8 DataHeader[12];	/* ��������� ������ SeismicDat0\0  */

	c8 HeaderSize;		/* ������ ��������� - 80 ���� */
	u8 ConfigWord;		/* ������������ */
	c8 ChannelBitMap;	/* ���������� ������: 1 ����� ������� */
	c8 SampleBytes;		/* ����� ���� ����� ������ */

	u32 BlockSamples;	/* ������ 1 ��������� ����� � ������ */

	s64 SampleTime;		/* ����� ��������� ������: ����������� ������� UNIX */
	s64 GPSTime;		/* ����� �������������: ����������� ������� UNIX  */
	s64 Drift;		/* ����� �� ������ ����� GPS: �����������  */

	u32 rsvd0;		/* ������. �� ������������ */

	/* ��������� �����: �����������, ���������� � �� */
	u16 power_reg;		/* ���������� �������, U mv */
	u16 curr_reg;		/* ��� �������, U ma */
	u16 power_mod;		/* ���������� ������, U mv */
	u16 curr_mod;		/* ��� ������, U ma */

	s16 temper_reg;		/* ����������� ������������, ������� ���� �������: 245 ~ 24.5 */
	s16 humid_reg;		/* ��������� - �������� */
	u32 press_reg;		/* �������� ������ ����� ��� */

	/* ��������� ������� � ������������ (�� ������) */
	s32 lat;		/* ������ (+ ~N, - S):   55417872 ~ 5541.7872N */
	s32 lon;		/* �������(+ ~E, - W): -37213760 ~ 3721.3760W */

	u16 hi;			/* ������ ��� ����� */
	s16 pitch;		/* ����, ������� ���� �������: 12 ~ 1.2 */

	s16 roll;		/* ������, ������� ���� ������� 2 ~ 0.2 */
	s16 head;		/* ������, ������� ���� ������� 2487 ~ 248.7 */
} ADC_HEADER;
#else

#pragma pack(1)
typedef struct {
	c8 DataHeader[12];	/* ��������� ������ SeismicData\0  */

	c8 HeaderSize;		/* ������ ��������� - 80 ���� */
	c8 ConfigWord;
	c8 ChannelBitMap;
	u16 BlockSamples;
	c8 SampleBytes;

	TIME_DATE SampleTime;	/* �������� ����� ������ */

	u16 Bat;
	u16 Temp;
	u8 Rev;			/* ������� = 2 ������  */
	u16 Board;

	u8 NumberSV;
	s16 Drift;

	TIME_DATE SedisTime;	/* �� ������������ ����  */
	TIME_DATE GPSTime;	/* ����� ������������� */

	union {
		struct {
			u8 rsvd0;
			bool comp;	/* ������ */
			c8 pos[23];	/* ������� (����������) */
			c8 rsvd1[3];
		} coord;	/* ���������� */

		u32 dword[7];	/* ������ */
	} params;
} ADC_HEADER;
#endif

/*************************************************************************
 *  			���������� ���� �����
 *
 ************************************************************************/
#define 		LOADER_TASK_PRIORITY		1
#define 		GPS_TASK_PRIORITY		1
#define 		PWM_TASK_PRIORITY		1
#define 		SENSOR_TASK_PRIORITY		1
#define 		CHECK_TIME_TASK_PRIORITY	1
#define 		WLAN_STATION_TASK_PRIORITY	1

#define 		TX_DATA_TASK_PRIORITY		5
#define 		TCP_RX_TASK_PRIORITY		5
#define 		UDP_RX_TASK_PRIORITY		5

#define 		SD_TASK_PRIORITY		1
#define 		CMD_TASK_PRIORITY		1
#define 		ADC_TASK_PRIORITY		1



/*************************************************************************
 * ������ ������� ������� ������������ �� ������ � ������ 0x20000020   
 * �������� ����� ���� ������: GPS_ (gps.c), ����� ����� ��� ����������
 * gps.c xchg.c log.c dma.c
 **************************************************************************/
#define		GPS_DATA_MEM_ADDR	0x20000000    	// �������� �������
#define		GPS_NAV_MEM_ADDR	0x20000020    	// ������������� ������

#define		XCHG_RX_MEM_ADDR	0x20000100	// ����� ������ �� UART
#define		XCHG_TX_MEM_ADDR	0x20000600     	// �������� ������ �� UART
#define		XCHG_COUNT_MEM_ADDR	0x20000B00      // �������� ������� �� UART ��� WiFi

#define		LOG_FATFS_MEM_ADDR	0x20000B20      // ��������� FAT
#define		LOG_DIR_MEM_ADDR	0x20000D60      // ��������� DIR
#define		LOG_LOGFILE_MEM_ADDR	0x20000DA0      // ���� LOG
#define		LOG_ADCFILE_MEM_ADDR	0x20000DD0      // ���� � �������
#define		LOG_BOOTFILE_MEM_ADDR	0x20000E00      // ���� boot - 40 ����
#define		LOG_ADCHDR_MEM_ADDR	0x20000E30      // ��������� ������ - 80 ����

/* ��� DMA - ������ ���� ��������� �� 1024! */
#define		DMA_CTRL_TAB_MEM_ADDR	0x20001000      // DMA ctrl tab
#define		DMA_CALLBACK_MEM_ADDR	0x20001800      // DMA callback - 260 ����

#define 	ADS131_ERROR_MEM_ADDR   0x20002000      // ������ 
#define 	ADS131_PACK_MEM_ADDR    0x20002200  	// ����� 


#endif				/* my_defs.h */
