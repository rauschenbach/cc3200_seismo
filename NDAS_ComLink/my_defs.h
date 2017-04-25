#ifndef _MY_DEFS_H
#define _MY_DEFS_H

#include <signal.h>
#include <stdint.h>
#include <time.h>

/* Сколько пакетов в пачке  */
#define 	NUM_ADS131_PACK	20

#define		SEC_IN_HOUR		3600

/* Включения для 504 блекфина, не 506!!!!  */
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


/* Длинное время */
#ifndef	time64
#define time64	int64_t
#endif


#ifndef _WIN32			/* Embedded platform */

/* long double не поддержываеца  */
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
/* long double не поддержываеца  */
#ifndef f32
#define f32 float
#endif

#endif


#ifndef IDEF
#define IDEF static inline
#endif

/* На этот адрес будет отвечать наше устройство */
#define 	BROADCAST_ADDR	0xff


#define		MAGIC				0x4b495245

/* Делители */
#define		TIMER_NS_DIVIDER	(1000000000UL)
#define		TIMER_US_DIVIDER	(1000000)
#define		TIMER_MS_DIVIDER	(1000)


/* Время UNIX в наносекундах */
typedef int64_t UNIX_TIME_t;



/* Статус внутренниъ часов */
typedef enum {
	CLOCK_RTC_TIME = 0,
	CLOCK_NO_GPS_TIME,
	CLOCK_PREC_TIME,
	CLOCK_NO_TIME = (-1)
} CLOCK_STATUS_EN;


/*******************************************************************************
 * Состояние устройств для описания State machine
 *******************************************************************************/
typedef enum {
    DEV_POWER_ON_STATE = 0,
    DEV_CHOOSE_MODE_STATE = 1,
    DEV_INIT_STATE,
    DEV_TUNE_WITHOUT_GPS_STATE,	/*Запуск таймера БЕЗ GPS */
    DEV_GET_3DFIX_STATE,
    DEV_TUNE_Q19_STATE,		/* Первичная настройка кварца 19 МГц */
    DEV_SLEEP_AND_DIVE_STATE,	/* Погружение и сон перед началом работы */
    DEV_WAKEUP_STATE,
    DEV_REG_STATE,
    DEV_FINISH_REG_STATE,
    DEV_BURN_WIRE_STATE,
    DEV_SLEEP_AND_POPUP_STATE,
    DEV_WAIT_GPS_STATE,
    DEV_HALT_STATE,
    DEV_COMMAND_MODE_STATE,
    DEV_POWER_OFF_STATE,
    DEV_ERROR_STATE = -1	/* Авария */
} DEV_STATE_ENUM;


/** 
 * Статус и ошибки устройств на отправление 64 байт. 
 * Короткий статус посылается при ошибке (len + st_main + 2 байта CRC16)
 */
#pragma pack(4)
typedef struct {
    u8  len;			/* 1......Длина пакета */
    u8  st_main;		/* 2......главный статус: нет времени, нет констант, ошибка в команде, нет файла, неисправен, переполнение памяти, остановлен, режим работы */
    u16 ports;                  /* 3-4....Включенные устройства */

    u16  test;			/* 3......Самотестирование и ошибки: 0 - часов, 1 - датчик T&P, 2 - Акселер/ компас, 3 - модем, 4 - GPS, 5 - EEPROM, 6 - карта SD, 7 - flash */
    u8   reset;			/* 7......Причина сброса */
    u8   adc;			/* 8......Ошибки АЦП */

    s16 temper0;		/* 9-10...Температура от внешнего шнура * 10 */
    s16 temper1;		/* 11-12..Температура платы (если есть) * 10 */ 

    s16 power_mv;    	        /* 13-14..Напряжения питания, мв */
    s16 pitch;			/* 15-16..Крен */

    s16 roll;			/* 17-18..Наклон */
    s16 head;			/* 19-20..Азимут */

    u32 press;			/* 21-24..Давление, кПа */

    /* Положение прибора в пространстве (со знаком) */
    s32 lat;			/* 25-28..широта (+ ~N, - S):  55417872 ~ 5541.7872N */
    s32 lon;			/* 29-32..долгота(+ ~E, - W): -37213760 ~ 3721.3760W */
    s32 gps_time;		/* 33-36..Время GPS */

    s32 gns_time;		/* 37-40..Внутреннее время станции */

    s32 comp_time;		/* 41-44..Время компиляции программы */
    u16 dev_addr;		/* 45-46..Адрес платы (номер устройства) */
    u8  ver;			/* 47.....Версия ПО: 1, 2, 3, 4 итд */
    u8  rev;                    /* 48.....Ревизия ПО 0, 1, 2 итд */

    u32 rsvd0[3];               /* 49-60 */

    u16 rsvd1;                  /* 61-62  */
    u16 crc16;			/* 63-64..Контрольная сумма пакета  */
} DEV_STATUS_STRUCT;


/**
 * Параметры запуска прибора 
 * В этих структурах записываются параметры таймеров
 * соответствует такой записи:
 * 16.10.12 17.15.22	// Время начала регистрации
 * 17.10.12 08.15.20	// Время окончания регистрации
 * 18.10.12 11.17.00	// Время всплытия, включает 5 мин. времени пережига
 * ....
 */
#pragma pack(4)
typedef struct {
    /* Файлы регистрации */
    u32 gns110_pos;		/* Позиция установки */

    char gns110_dir_name[18];	/* Название директории для всех файлов */
    u8   gns110_file_len;		/* Размер файла данных в часах */
    bool gns110_const_reg_flag;	/* Постоянная регистрация */


    /* Модем */
    s32 gns110_modem_rtc_time;	/* Время часов модема */
    s32 gns110_modem_alarm_time;	/* аварийное время всплытия от модема   */
    s32 gns110_modem_type;	/* Тип модема */
    u16 gns110_modem_num;	/* Номер модема */
    u16 gns110_modem_burn_len_sec;	/* Длительность пережига проволоки в секундах от модема */
    u8 gns110_modem_h0_time;
    u8 gns110_modem_m0_time;
    u8 gns110_modem_h1_time;
    u8 gns110_modem_m1_time;

    /* Времена */
    s32 gns110_wakeup_time;	/* время начала подстройки перед регистрацией */
    s32 gns110_start_time;	/* время начала регистрации  */
    s32 gns110_finish_time;	/* время окончания регистрации */
    s32 gns110_burn_time;	/* Время начала пережига проволки  */
    s32 gns110_popup_time;	/* Момент начала всплытия */
    s32 gns110_gps_time;	/* время включения gps после времени всплытия */

    /* АЦП */
    f32 gns110_adc_flt_freq;	/* Частота среза фильтра HPF */

    u16 gns110_adc_freq;	/* Частота АЦП  */
    u8 gns110_adc_pga;		/* Усиление АЦП  */
    u8 gns110_adc_bitmap;	/* какие каналы АЦП используются */
    u8 gns110_adc_consum;	/* энергопотребление АЦП сюда */
    u8 gns110_adc_bytes;	/* Число байт в слове данных  */
    u8 rsvd[2];
} GNS110_PARAM_STRUCT;



/* Структура, описывающая время, исправим c8 на u8 - время не м.б. отрицательным */
#pragma pack(1)
typedef struct {
    u8 sec;
    u8 min;
    u8 hour;
    u8 week;			/* день недели...не используеца */
    u8 day;
    u8 month;			/* 1 - 12 */
#define mon 	month
    u16 year;
} TIME_DATE;
#define TIME_LEN  8		/* Байт */


/**
 * В эту стуктуру пишется минутный заголовок при записи на SD карту (pack по 1 байту!)
 */
#if defined   ENABLE_NEW_SIVY
/* В новом SIVY паковать по 4 байта */
#pragma pack(4)
typedef struct {
    c8 DataHeader[12];		/* Заголовок данных SeismicDat0\0  */

    c8 HeaderSize;		/* Размер заголовка - 80 байт */
    u8 ConfigWord;		/* Конфигурация */
    c8 ChannelBitMap;		/* Включенные каналы: 1 канал включен */
    c8 SampleBytes;		/* Число байт слове данных */

    u32 BlockSamples;		/* Размер 1 минутного блока в байтах */

    s64 SampleTime;		/* Время минутного сампла: наносекунды времени UNIX */
    s64 GPSTime;		/* Время синхронизации: наносекунды времени UNIX  */
    s64 Drift;			/* Дрифт от точных часов GPS: наносекунды  */

    u32 rsvd0;			/* Резерв. Не используется */

    /* Параметры среды: температура, напряжения и пр */
    u16 u_pow;			/* Напряжение питания, U mv */
    u16 i_pow;			/* Ток питания, U ma */
    u16 u_mod;			/* Напряжение модема, U mv */
    u16 i_mod;			/* Ток модема, U ma */

    s16 t_reg;			/* Температура регистратора, десятые доли градуса: 245 ~ 24.5 */
    s16 t_sp;			/* Температура внешней платы, десятые доли градуса: 278 ~ 27.8  */
    u32 p_reg;			/* Давление внутри сферы кПа */

    /* Положение прибора в пространстве (со знаком) */
    s32 lat;			/* широта (+ ~N, - S):   55417872 ~ 5541.7872N */
    s32 lon;			/* долгота(+ ~E, - W): -37213760 ~ 3721.3760W */

    s16 pitch;			/* крен, десятые доли градуса: 12 ~ 1.2 */
    s16 roll;			/* наклон, десятые доли градуса 2 ~ 0.2 */

    s16 head;			/* азимут, десятые доли градуса 2487 ~ 248.7 */
    u16 rsvd1;			/* Резерв для выравнивания */
} ADC_HEADER;
#else

#pragma pack(1)
typedef struct {
    c8 DataHeader[12];		/* Заголовок данных SeismicData\0  */

    c8 HeaderSize;		/* Размер заголовка - 80 байт */
    c8 ConfigWord;
    c8 ChannelBitMap;
    u16 BlockSamples;
    c8 SampleBytes;

    TIME_DATE SampleTime;	/* Минутное время сампла */

    u16 Bat;
    u16 Temp;
    u8  Rev;			/* Ревизия = 2 всегда  */
    u16 Board;

    u8  NumberSV;
    s16 Drift;

    TIME_DATE SedisTime;	/* Не используется пока  */
    TIME_DATE GPSTime;		/* Время синхронизации */

    union {
	struct {
	    u8 rsvd0;
	    bool comp;		/* компас */
	    c8 pos[23];		/* Позиция (координаты) */
	    c8 rsvd1[3];
	} coord;		/* координаты */

	u32 dword[7];		/* Данные */
    } params;
} ADC_HEADER;
#endif



#endif				/* my_defs.h */
