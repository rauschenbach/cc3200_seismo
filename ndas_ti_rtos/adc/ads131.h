#ifndef _ADS131_H
#define _ADS131_H

#include "proto.h"
#include "my_defs.h"
#include "userfunc.h"


#define 	NUM_ADS131_CHAN		4	/* ����� �������  */
#define 	MAX_ADS131_CHAN		8	/* ������������ ����� �������  */

#define 	NUM_ADS131_PACK		20 	/* ������ �������� */


/**
 * ��������� ��� ��� �� ADS131
 * ��� ������� ����� 
 */
/* 1. ���� �������� ���������� DRDY �� GPIO_06 � ������ 0 */
#define ADS131_DRDY_PIN			PIN_61
#define ADS131_DRDY_MODE		PIN_MODE_0
#define ADS131_DRDY_BASE		GPIOA0_BASE
#define ADS131_DRDY_GPIO		GPIO_PIN_6
#define ADS131_DRDY_INT			GPIO_INT_PIN_6 
#define ADS131_DRDY_BIT			0x40
#define ADS131_DRDY_GROUP_INT		INT_GPIOA0			


/* 2.  ����� RST_IN �� GPIO_07 */
#define ADS131_RESET_PIN		PIN_62
#define ADS131_RESET_MODE		PIN_MODE_0
#define ADS131_RESET_BASE		GPIOA0_BASE
#define ADS131_RESET_BIT		0x80


/* 3.  ����� START / SYNC_IN �� GPIO_08 */
#define ADS131_START_PIN		PIN_63
#define ADS131_START_MODE		PIN_MODE_0
#define ADS131_START_BASE		GPIOA1_BASE
#define ADS131_START_BIT		0x01


/* ������ � ������� - ������ � ����� ���������� ����� �������� ���   */
typedef enum {
	SPS125 = 0,
	SPS250,			//1
	SPS500,			//2
	SPS1K,			//3
	SPS2K,			//4
	SPS4K			//5
} ADS131_FreqEn;


/* ����� ������ ��� */
typedef enum {
	TEST_MODE = 0,
	WORK_MODE,
	CMD_MODE
} ADS131_ModeEn;

/* �������� ������ - ������ � ����� ���������� ����� �������� ���   */
typedef enum {
	PGA1 = 1,
	PGA2,
	PGA_RSVD,
	PGA4,
	PGA8,
	PGA12
} ADS131_PgaEn;

/* �������� ������  */
typedef enum {
	TEST_EXT,
	TEST_INT	
} ADS131_TestEn;

/* ������� ��������� ������� */
typedef enum {
	TEST_FREQ_0, //  F clk / 2^21 (default)
	TEST_FREQ_1, //  F clk / 2^20 
	TEST_FREQ_NONE,    // �� ���
	TEST_AT_DC	   // at dc	
} ADS131_TestFreqEn;


/* ���� ������, ��� ���� ����� �������� */
typedef enum {
	MUX0 = 0,
#define MUX_NORM	MUX0
	MUX1,
#define MUX_SHRT	MUX1
	MUX_RSVD,
	MUX3,
#define	MUX_MVDD	MUX3
	MUX4,
#define MUX_TEMP	MUX4	
	MUX5,
#define MUX_TEST	MUX5
} ADS131_MuxEn;


/**
 * �������� ��� ��� ���� 4-� ��� 8-�� �������.
 */
typedef struct {
	u8 id;
	u8 config1;
	u8 config2;
	u8 config3;
	u8 fault;
	u8 chxset[MAX_ADS131_CHAN];/* ����� ������������!!! ����� ������� */
	u8 fault_statp;
	u8 fault_statn;
	u8 gpio;
} ADS131_Regs;

/* ��������� ������ ��������� ��� ���� ��� */
typedef struct {
	ADS131_ModeEn 		mode;		/* ����� ������ �������� */
	ADS131_FreqEn 		sps;		/* �������  */
	ADS131_PgaEn  		pga[MAX_ADS131_CHAN];		/* PGA ��� 8-� ������� */
	ADS131_MuxEn  		mux;     	/* ������������� �� ����� */
	ADS131_TestEn 		test_sign;	/* �������� ������  */
	ADS131_TestFreqEn	test_freq;	/* ������� ��������� ������� */
	u8 bitmap;				/* ���������� ������: 1 ����� �������, 0 - �������� */
	u8 file_len;				/* ����� ����� ������ � ����� */
} ADS131_Params;


/**
 * ������ ��� ����� ����
 */
#pragma pack(4)
typedef struct {
	s64 time_now;		/* �����, ���� ���������� ������ - ������ � ����� ����� */
	s64 time_last;
	s32 sample_miss;	/* ������ ������ �� ������� */
	s32 block_timeout;	/* ���� �� ����� ������� ���������� */
	u32 test_counter;	/* ������� �������� ������� */

	struct {
		u8 cfg0_wr;
		u8 cfg0_rd;
		u8 cfg1_wr;
		u8 cfg1_rd;
	} adc[NUM_ADS131_CHAN];
} ADS131_ERROR_STRUCT;


/**
 * ������ ��� ������ ��� � ���� ���������
 */
typedef struct {
	ADS131_FreqEn sample_freq;	/* ������� */
	u8 decim_coef;			/* ����������� ��������� */
	u8 num_bytes;			/* ����� ���� � ����� ������ ��������� */
	u8 time_ping;			/* ����� ������ ������ ping � �������� */
	u16 samples_in_ping;		/* ����� �������� � ����� ping */
	u16 ping_pong_size_in_byte;
	u16 period_us;			/* ������ ������ ������ � ��� */
	u16 sig_in_min;			/* ������� � ������ */
	u16 sig_in_time;		/* ������� �� ����� ������������ 1-�� ����� */
} ADS131_WORK_STRUCT;


/**
 * ���� ����� �� ���� 4-� ������� - 12 ���� 
 */
#pragma pack(1)
typedef struct {
    unsigned x : 24;
    unsigned y : 24;
    unsigned z : 24;
    unsigned h : 24;
} DATA_SIGNAL_t;


/**
 * ����� ������ �� ����������� - 20 ��������� �������������, �������� �� 1!!!
 * ������ ������ 20 + 8 + 4 + 240
 */
typedef struct {
    HEADER_t header;
    UNIX_TIME_t t0;             /* UNIX TIME ������� ��������� */
    u16  adc_freq;              /* ������� ��������� */
    u16  rsvd;			/* ������� ������� ��� ��� �� ������ */
    DATA_SIGNAL_t sig[NUM_ADS131_PACK]; /* 12 * 20  = 240 ���� */
} ADS131_DATA_h_old;

/**
 * ����� ������ �� ����������� - 20 ��������� �������������, �������� �� 1!!!
 * ������ ������ 20 + 2 + 2 + 8 + 8 + 240 - 280
 */
typedef struct {
    HEADER_t header;
    u16  adc_freq;              	/* ������� ��������� */
    u16  sample_capacity;		/* ����������� ������ � ������: 1, 2, 3 ��� 4 ����� �������������� */
    UNIX_TIME_t time_stamp;             /* UNIX TIME ������� ��������� */
    u64 channel_mask;			/* ����� ��������� ������� */
    DATA_SIGNAL_t sig[NUM_ADS131_PACK];
} ADS131_DATA_h;



/*****************************************************************************************
 * ��������� �������. �c� ������� � ���������� void ADS131_ ���������� 
 * �� ������ ������ � ����� ������ �� ��� ������������ ���
 ******************************************************************************************/
bool ADS131_config(ADS131_Params *);
bool ADS131_config_and_start(ADS131_Params *);
void ADS131_start(void);	/* ������ ��� */
void ADS131_stop(void);		/* ���� ��� � POWERDOWN ��� ���� */
bool ADS131_is_run(void);
bool ADS131_get_pack(ADS131_DATA_h *);
void ADS131_get_error_count(ADS131_ERROR_STRUCT *);
void ADS131_standby(void);

bool ADS131_ofscal(void);
bool ADS131_gancal(void);
bool ADS131_get_adc_const(u8, u32 *, u32 *);
bool ADS131_set_adc_const(u8, u32, u32);
bool ADS131_clear_adc_buf(void);

void ADS131_ISR_func(void);		/* ���������� �� ���������� */
bool ADS131_get_handler_flag(void);	/* ��� ������ � ��������� ����� ���� ���! */
void ADS131_reset_handler_flag(void);
void ADS131_start_irq(void);
bool ADS131_is_run(void);

void ADS131_test(ADS131_Params *);

void ADS131_get_adcp(ADCP_h *);
void ADS131_set_adcp(ADCP_h *);

bool ADS131_write_parameters(ADCP_h*);
bool ADS131_start_osciloscope(START_PREVIEW_h*);
bool ADS131_stop_osciloscope(void);

#endif				/* ads131.h */
