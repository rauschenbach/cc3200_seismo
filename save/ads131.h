#ifndef _ADS131_H
#define _ADS131_H

#include "proto.h"
#include "my_defs.h"
#include "userfunc.h"


#define 	NUM_ADS131_CHAN		4	/* число каналов  */
#define 	MAX_ADS131_CHAN		8	/* максимальное число каналов  */
#define 	NUM_ADS131_PACK		20

/**
 * Дефиниции для ног на ADS131
 */
#if 0	/* Для отладочной платы */

/* Старт на P2 - 2   */
#define ADS131_START_PIN		PIN_18	/* GPIO_28 */
#define ADS131_START_BIT		0x10
#define ADS131_START_BASE		GPIOA3_BASE
#define ADS131_START_MODE		PIN_MODE_0

/* подобрать pin! reset на P2 - 10 - на плате через R на GND. плохо! */
#define ADS131_RESET_PIN		PIN_15	/* GPIO_22 */
#define ADS131_RESET_BIT		0x40
#define ADS131_RESET_BASE		GPIOA2_BASE
#define ADS131_RESET_MODE		PIN_MODE_0

/* DRDY на P4-8 */
#define ADS131_DRDY_PIN			PIN_60	/* GPIO_5 */
#define ADS131_DRDY_GPIO		GPIO_PIN_5 
#define ADS131_DRDY_INT			GPIO_INT_PIN_5 
#define ADS131_DRDY_BIT			0x20
#define ADS131_DRDY_BASE		GPIOA0_BASE
#define ADS131_DRDY_MODE		PIN_MODE_0

#else	/* Для рабочей платы */


/* 1. Вход внешнего прерывания DRDY на GPIO_06 в режиме 0 */
#define ADS131_DRDY_PIN			PIN_61
#define ADS131_DRDY_MODE		PIN_MODE_0
#define ADS131_DRDY_BASE		GPIOA0_BASE
#define ADS131_DRDY_GPIO		GPIO_PIN_6
#define ADS131_DRDY_INT			GPIO_INT_PIN_6 
#define ADS131_DRDY_BIT			0x40
#define ADS131_DRDY_GROUP_INT		INT_GPIOA0			


/* 2.  Выход RST_IN на GPIO_07 */
#define ADS131_RESET_PIN		PIN_62
#define ADS131_RESET_MODE		PIN_MODE_0
#define ADS131_RESET_BASE		GPIOA0_BASE
#define ADS131_RESET_BIT		0x80


/* 3.  Выход START / SYNC_IN на GPIO_08 */
#define ADS131_START_PIN		PIN_63
#define ADS131_START_MODE		PIN_MODE_0
#define ADS131_START_BASE		GPIOA1_BASE
#define ADS131_START_BIT		0x01


#endif


/* Самплы в секунду - только с этими значениями может работать АЦП   */
typedef enum {
	SPS125 = 0,
	SPS250,			//1
	SPS500,			//2
	SPS1K,			//3
	SPS2K,			//4
	SPS4K			//5
} ADS131_FreqEn;


/* Режим работы АЦП */
typedef enum {
	TEST_MODE = 0,
	WORK_MODE,
	CMD_MODE
} ADS131_ModeEn;

/* Усиление канала - только с этими значениями может работать АЦП   */
typedef enum {
	PGA1 = 1,
	PGA2,
	PGA_RSVD,
	PGA4,
	PGA8,
	PGA12
} ADS131_PgaEn;

/* Тестовый сигнал  */
typedef enum {
	TEST_EXT,
	TEST_INT	
} ADS131_TestEn;

/* Частота тестового сигнала */
typedef enum {
	TEST_FREQ_0, //  F clk / 2^21 (default)
	TEST_FREQ_1, //  F clk / 2^20 
	TEST_FREQ_NONE,    // не исп
	TEST_AT_DC	   // at dc	
} ADS131_TestFreqEn;


/* Вход канала, что туда будем подавать */
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
 * Регистры АЦП для всех 4-х или 8-ми каналов.
 */
typedef struct {
	u8 id;
	u8 config1;
	u8 config2;
	u8 config3;
	u8 fault;
	u8 chxset[MAX_ADS131_CHAN];/* здесь МАКСИМАЛЬНОЕ!!! число каналов */
	u8 fault_statp;
	u8 fault_statn;
	u8 gpio;
} ADS131_Regs;

/* Передаеца внутрь настройки для всех АЦП */
typedef struct {
	ADS131_ModeEn 		mode;		/* режим работы програмы */
	ADS131_FreqEn 		sps;		/* частота  */
	ADS131_PgaEn  		pga;		/* PGA */
	ADS131_MuxEn  		mux;     		/* Мультиплексор на входе */
	ADS131_TestEn 		test_sign;	/* Тестовый сигнал  */
	ADS131_TestFreqEn	test_freq;	/* Частота тестового сигнала */
	u8 bitmap;			/* Включенные каналы: 1 канал включен, 0 - выключен */
	u8 file_len;			/* Длина файла записи в часах */
} ADS131_Params;


/**
 * Ошыпки АЦП пишем сюда
 */
#pragma pack(4)
typedef struct {
	s64 time_now;		/* время, если произойдет ошибка - узнаем в какое время */
	s64 time_last;
	s32 sample_miss;	/* отсчет пришел не вовремя */
	s32 block_timeout;	/* Блок не успел вовремя записаться */
	u32 test_counter;	/* счетчик тестовых пакетов */

	struct {
		u8 cfg0_wr;
		u8 cfg0_rd;
		u8 cfg1_wr;
		u8 cfg1_rd;
	} adc[NUM_ADS131_CHAN];
} ADS131_ERROR_STRUCT;


/**
 * Расчет для буфера АЦП в этой структуре
 */
typedef struct {
	ADS131_FreqEn sample_freq;	/* частота */
	u8 decim_coef;			/* коэффициент децимации */
	u8 num_bytes;			/* число байт в одном пакете измерений */
	u8 time_ping;			/* время записи одного ping в секундах */
	u16 samples_in_ping;		/* число отсчетов в одном ping */
	u16 ping_pong_size_in_byte;
	u16 period_us;			/* период одного сампла в мкс */
	u16 sig_in_min;			/* Записей в минуту */
	u16 sig_in_time;		/* Записей за время формирования 1-го файла */
} ADS131_WORK_STRUCT;


/**
 * Один сампл со всех 4-х каналов - 12 байт 
 */
#pragma pack(1)
typedef struct {
    unsigned x : 24;
    unsigned y : 24;
    unsigned z : 24;
    unsigned h : 24;
} DATA_SIGNAL_t;


/**
 * Пакет данных на отправление - 20 измерений акселерометра, упакован на 1!!!
 * Размер пакета 20 + 8 + 4 + 240
 */
typedef struct {
    HEADER_t header;
    UNIX_TIME_t t0;             /* UNIX TIME первого измерения */
    u16  adc_freq;              /* Частота оцифровки */
    u16  rsvd;			/* Счетчик пакетов или что то другое */
    DATA_SIGNAL_t sig[NUM_ADS131_PACK]; /* 12 * 20  = 240 байт */
} ADS131_DATA_h;


/*****************************************************************************************
 * Прототипы функций. Вcе функции с преффиксом void ADS131_ вызываются 
 * из других файлов и сразу влияют на все подключенные АЦП
 ******************************************************************************************/
bool ADS131_config(ADS131_Params *);
bool ADS131_config_and_start(ADS131_Params *);
void ADS131_start(void);	/* Запуск АЦП */
void ADS131_stop(void);		/* Стоп АЦП и POWERDOWN для всех */
bool ADS131_is_run(void);
bool ADS131_get_pack(ADS131_DATA_h *);
void ADS131_get_error_count(ADS131_ERROR_STRUCT *);
void ADS131_standby(void);

bool ADS131_ofscal(void);
bool ADS131_gancal(void);
bool ADS131_get_adc_const(u8, u32 *, u32 *);
bool ADS131_set_adc_const(u8, u32, u32);
bool ADS131_clear_adc_buf(void);

void ADS131_ISR_func(void);		/* Прерывание по готовности */
bool ADS131_get_handler_flag(void);	/* Для сброса и получения флага пока так! */
void ADS131_reset_handler_flag(void);
void ADS131_start_irq(void);
bool ADS131_is_run(void);

void ADS131_test(ADS131_Params *);

void ADS131_get_adcp(ADCP_h *);
void ADS131_set_adcp(ADCP_h *);

bool ADS131_write_parameters(ADCP_h*);
bool ADS131_start_osciloscope(void);
bool ADS131_stop_osciloscope(void);
bool ADS131_get_data(ADS131_DATA_h*);

#endif				/* ads131.h */
