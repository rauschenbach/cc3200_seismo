#ifndef _MY_DEFS_H
#define _MY_DEFS_H

#include <rom.h>
#include <rom_map.h>


#include <signal.h>
#include <stdint.h>
#include <time.h>

/* Сколько пакетов в пачке  */
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


#ifndef IDEF
#define IDEF static inline
#endif

/* На этот адрес будет отвечать наше устройство */
#define 	BROADCAST_ADDR	0xff


#define		MAGIC				0x4b495245

/* Делители */
#define 	SYSCLK          	80000000UL

#define		TIMER_NS_DIVIDER	(1000000000UL)
#define		TIMER_US_DIVIDER	(1000000)
#define		TIMER_MS_DIVIDER	(1000)
#define		DATA_FILE_LEN		4 /* 4 часа в файле даных */



/* Стек для задач. размер */
#define OSI_STACK_SIZE          1024

/* Время UNIX в наносекундах */
typedef int64_t UNIX_TIME_t;


/* Статус внутренниъ часов */
typedef enum {
	CLOCK_NO_GPS_TIME,
	CLOCK_GPS_TIME,
	CLOCK_UTC_TIME,
	CLOCK_RTC_TIME,
} CLOCK_STATUS_EN;


/* Режим работы станции/ по проводу, сети или в рабочем режиме */
typedef enum {
	GNS_CMD_MODE,
	GNS_NORMAL_MODE,
} GNS_WORK_MODE;

/*******************************************************************************
 * Состояние устройств для описания State machine
 *******************************************************************************/
typedef enum {
	DEV_POWER_ON_STATE = 0,
	DEV_CHOOSE_MODE_STATE = 1,
	DEV_INIT_STATE,
	DEV_TUNE_WITHOUT_GPS_STATE,	/*Запуск таймера БЕЗ GPS */
	DEV_GET_3DFIX_STATE,
	DEV_TUNE_Q19_STATE,	/* Первичная настройка кварца 19 МГц */
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
	u16 gns_addr;		/* Адрес станции */
	c8  gns_dir_name[18];	/* Название директории для всех файлов */


	/* Времена */
	s32 gns_wakeup_time;	/* время начала подстройки перед регистрацией */
	s32 gns_start_time;	/* время начала регистрации  */
	s32 gns_finish_time;	/* время окончания регистрации */
	s32 gns_burn_time;	/* Время начала пережига проволки  */
	s32 gns_popup_time;	/* Момент начала всплытия */
	s32 gns_gps_time;	/* время включения gps после времени всплытия */

	/* Wlan */
	c8  gns_ssid_name[32];	/* Название AP */
	c8  gns_ssid_pass[32];	/* Пароль */

	s16 gns_ssid_sec_type;	/* Тип шифрования */
	s16 gns_server_udp_port;	/* Порт UDP сервера */

	s16 gns_server_tcp_port;	/* Порт TCP сервера */
	u8  gns_mux;		/* Мультиплексор на входе */
	u8  gns_test_sign;

	u8  gns_adc_pga[8];	/* Усиление АЦП  */

	u8  gns_test_freq;
	u8  gns_adc_bitmap;	/* какие каналы АЦП используются */
	u8  gns_adc_consum;	/* энергопотребление АЦП сюда */
	u8 gns_adc_bytes;	/* Число байт в слове данных  */

	u16 gns_adc_freq;	/* Частота АЦП  */
	u8 gns_work_mode;       /* Режим работы: сеть / com порт/ рабочий режим по времени */

	u8 rsvd[5];
} GNS_PARAM_STRUCT;



/* Структура, описывающая время, исправим c8 на u8 - время не м.б. отрицательным */
#pragma pack(1)
typedef struct {
	u8 sec;
	u8 min;
	u8 hour;
	u8 week;		/* день недели...не используеца */
	u8 day;
	u8 month;		/* 1 - 12 */
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
	c8 DataHeader[12];	/* Заголовок данных SeismicDat0\0  */

	c8 HeaderSize;		/* Размер заголовка - 80 байт */
	u8 ConfigWord;		/* Конфигурация */
	c8 ChannelBitMap;	/* Включенные каналы: 1 канал включен */
	c8 SampleBytes;		/* Число байт слове данных */

	u32 BlockSamples;	/* Размер 1 минутного блока в байтах */

	s64 SampleTime;		/* Время минутного сампла: наносекунды времени UNIX */
	s64 GPSTime;		/* Время синхронизации: наносекунды времени UNIX  */
	s64 Drift;		/* Дрифт от точных часов GPS: наносекунды  */

	u32 rsvd0;		/* Резерв. Не используется */

	/* Параметры среды: температура, напряжения и пр */
	u16 power_reg;		/* Напряжение питания, U mv */
	u16 curr_reg;		/* Ток питания, U ma */
	u16 power_mod;		/* Напряжение модема, U mv */
	u16 curr_mod;		/* Ток модема, U ma */

	s16 temper_reg;		/* Температура регистратора, десятые доли градуса: 245 ~ 24.5 */
	s16 humid_reg;		/* Влажность - проценты */
	u32 press_reg;		/* Давление внутри сферы кПа */

	/* Положение прибора в пространстве (со знаком) */
	s32 lat;		/* широта (+ ~N, - S):   55417872 ~ 5541.7872N */
	s32 lon;		/* долгота(+ ~E, - W): -37213760 ~ 3721.3760W */

	u16 hi;			/* Высота над морем */
	s16 pitch;		/* крен, десятые доли градуса: 12 ~ 1.2 */

	s16 roll;		/* наклон, десятые доли градуса 2 ~ 0.2 */
	s16 head;		/* азимут, десятые доли градуса 2487 ~ 248.7 */
} ADC_HEADER;
#else

#pragma pack(1)
typedef struct {
	c8 DataHeader[12];	/* Заголовок данных SeismicData\0  */

	c8 HeaderSize;		/* Размер заголовка - 80 байт */
	c8 ConfigWord;
	c8 ChannelBitMap;
	u16 BlockSamples;
	c8 SampleBytes;

	TIME_DATE SampleTime;	/* Минутное время сампла */

	u16 Bat;
	u16 Temp;
	u8 Rev;			/* Ревизия = 2 всегда  */
	u16 Board;

	u8 NumberSV;
	s16 Drift;

	TIME_DATE SedisTime;	/* Не используется пока  */
	TIME_DATE GPSTime;	/* Время синхронизации */

	union {
		struct {
			u8 rsvd0;
			bool comp;	/* компас */
			c8 pos[23];	/* Позиция (координаты) */
			c8 rsvd1[3];
		} coord;	/* координаты */

		u32 dword[7];	/* Данные */
	} params;
} ADC_HEADER;
#endif

/*************************************************************************
 *  			Приоритеты всех задач
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
 * Адреса буферов которые отображаются на память с адреса 0x20000020   
 * название файла идет первым: GPS_ (gps.c), чтобы знать где исправлено
 * gps.c xchg.c log.c dma.c
 **************************************************************************/
#define		GPS_DATA_MEM_ADDR	0x20000000    	// счетчики обменов
#define		GPS_NAV_MEM_ADDR	0x20000020    	// Навигационные данные

#define		XCHG_RX_MEM_ADDR	0x20000100	// Прием данные по UART
#define		XCHG_TX_MEM_ADDR	0x20000600     	// Передача данные по UART
#define		XCHG_COUNT_MEM_ADDR	0x20000B00      // Счетчики обменов по UART или WiFi

#define		LOG_FATFS_MEM_ADDR	0x20000B20      // Параметры FAT
#define		LOG_DIR_MEM_ADDR	0x20000D60      // Параметры DIR
#define		LOG_LOGFILE_MEM_ADDR	0x20000DA0      // файл LOG
#define		LOG_ADCFILE_MEM_ADDR	0x20000DD0      // файл с данными
#define		LOG_BOOTFILE_MEM_ADDR	0x20000E00      // файл boot - 40 байт
#define		LOG_ADCHDR_MEM_ADDR	0x20000E30      // Заголовок данных - 80 байт

/* Для DMA - должно быть выровнено по 1024! */
#define		DMA_CTRL_TAB_MEM_ADDR	0x20001000      // DMA ctrl tab
#define		DMA_CALLBACK_MEM_ADDR	0x20001800      // DMA callback - 260 байт

#define 	ADS131_ERROR_MEM_ADDR   0x20002000      // ошибки 
#define 	ADS131_PACK_MEM_ADDR    0x20002200  	// пакет 


#endif				/* my_defs.h */
