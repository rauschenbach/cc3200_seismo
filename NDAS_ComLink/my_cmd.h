#ifndef _MY_CMD_H
#define _MY_CMD_H

#include "config.h"
#include "my_defs.h"


#define MAX_FILE_SIZE		1024

/* Беcпроводной USB использовать или нет  */
#define 	ENABLE_WIRELESS_USB
//#undef                ENABLE_WIRELESS_USB

/* Разрешить входить в Sleep режим  */
#define 	ENABLE_PLL_SLEEP
//#undef          ENABLE_PLL_SLEEP


/* Лок фал - для работы без ожидания команд   */
#define 	ENABLE_LOCK_FILE
//#undef          ENABLE_LOCK_FILE


/* В РЕЗЕРВИРОВАННЫЙ канал пишется счетчик миллисекундных тиков */
#define		ENABLE_TEST_CHANNEL
//#undef                ENABLE_TEST_CHANNEL

/* Для теста - взять тестовые данные для всех времен (внизу)-файлы пишутся по RECORD_TIME минут! */
#define 	ENABLE_TEST_TIMES
#undef 		ENABLE_TEST_TIMES

/* Выводить на экран или нет  */
#define 	ENABLE_UART_DEBUG
#undef 		ENABLE_UART_DEBUG

/* Для теста - включающее реле всегда в питании или нет */
#define 	ENABLE_POWER_ON
#undef 		ENABLE_POWER_ON

/* Время ожидания подключения портов по WUSB  */
#define		WAIT_WUSB_TIME			5

/* Число секунд, в течении которое будем ожидать входящей команды с PC - секунды */
//#define       WAIT_PC_TIME                    115
#define 	WAIT_PC_COMMAND			WAIT_PC_TIME

/* Число секунд, в течении которых мы будем ждать синхронизацию */
#define		WAIT_TIM3_SYNC			10

/* Время ожидания 3DFIX - 5 минут */
//#define       WAIT_3DFIX_TIME                 300

/*vvvvvvvvvvvv 600 Секунд время сна между ожиданием 3DFIX - */
#define 	SLEEP_3DFIX_TIME		600

/* Число попыток в режиме слипа получить 3d Fix 255 = (-1) */
#define 	NUM_3DFIX_ATEMPT		255



/* Задержка старта после просыпания */
//#define       TIME_START_AFTER_WAKEUP         180

/*Минимальное время записи - 5 минут  */
//#define       RECORD_TIME                     300

/* Задержка пережига после окончания  регистрации - 2 секунды */
//#define       TIME_BURN_AFTER_FINISH          2

/* Время пережигания проволки - это время будет прибавляться к  времени окончания  */
#define 	RELEBURN_TIME_AIRLINK			60

/* Время всплытия - это время прибавится к времени пережига */
#define 	POPUP_DURATION_AIRLINK		1


/* На этот адрес будет отвечать наше устройство */
#define 	BROADCAST_ADDR	0xff

/* Сколько пакетов в пачке  */
#define 	NUM_ADS1282_PACK	20
#define		MAGIC				0x4b495245
#define 	NMEA_PACK_SIZE 			128	/* Размер буфера для сохранения строк NMEA */


/**
 * Параметры АЦП для всех 4-х каналов.
 * Смещение 0 и усиление 
 */
typedef struct {
    u32 magic;			/* Магич. число */
    struct {
	u32 offset;		/* коэффициент 1 - смещение */
	u32 gain;		/* коэффициент 2 - усиление */
    } chan[4];			/* 4 канала */
} ADS131_Regs;

/* Порты-станции */
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
 * В этих структурах записываются параметры модема
 */
#pragma pack(4)
typedef struct {
    u32 modem_num;
    SYSTEMTIME modem_time;
    SYSTEMTIME alarm_time;	/* аварийное время всплытия от модема   */
    u16 am3_h0_time;
    u16 am3_m0_time;
    u16 am3_h1_time;
    u16 am3_m1_time;
    u32 time_for_release;
    u32 burn_len;
} MODEM_STRUCT;

/* Передаеца внутрь настройки для всех АЦП */
typedef struct {
    u8 mode;			/* режим работы програмы */
    u8 sps;			/* частота  */
    u8 pga;			/* PGA */
    u8 mux;			/* Мультиплексор на входе */
    u8 test_sign;
    u8 test_freq;
    u8  bitmap;			/* Включенные каналы: 1 канал включен, 0 - выключен */
    u8  file_len;		/* Длина файла записи в часах */
} START_ADC_STRUCT;


#pragma pack(4)
typedef struct {
    c8 my_str[0x80];
    u32 start_time;		/* время начала регистрации  */
    u32 finish_time;		/* время окончания регистрации */
    u32 burn_time;		/* Время начала пережига проволки  */
    u32 data_file_len;		/* Размер файла данных */
    s32 am3_modem_type;		/* Тип модема */
    s16 popup_len;		/* длительность всплытия */
    s16 modem_num;		/* Номер модема */
    u32 am3_popup_time;		/* аварийное время всплытия от модема   */

    u16 releburn_time;		// сколько секунд пережигать
    u8 am3_cal_h0;
    u8 am3_cal_m0;

    u8 am3_cal_h1;
    u8 am3_cal_m1;
    u16 sample_freq;		/* Частота оцифровки АЦП (250 Hz, 500 Hz, 1000 Hz) */

    u8 power_cons;		/* Энергопотрбление  */
    u8 pga_adc;			/* PGA АЦП (1, 2, 4, 8, 16, 32, 64) */
    u8 num_bytes;		/* Число байт в слове данных (3 или 4) */
    u8 on_off_channels;		/* Включенные каналы ->(1-й, 2-й, 3-й, 4-й) справа налево. 4 цыфры обязательно! */

    u8 const_reg_flag;
    int pos;			/* Позиция установки */
    float filter_freq;		/* частота среза фильтра */
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

// Ошибки обмена
typedef struct {
    int tx_ok;
    int rx_err;
} XCHG_ERR_COUNT;



/* RTC время */
#pragma pack (1)
typedef struct {
    u8 len;
    u32 time;
    u16 crc16;
} RTC_TIME_STRUCT;


// Приемный буфер для NMEA
#pragma pack(1)
typedef struct {
    u8 len;
    c8 str[NMEA_PACK_SIZE];
    u16 crc16;
} NMEA_RX_BUF;


// Язык
enum myLangID {
    lang_rus,
    lang_eng,
};




#endif				/* my_cmd.h  */
