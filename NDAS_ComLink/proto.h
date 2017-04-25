#ifndef ProtoH
#define ProtoH
//---------------------------------------------------------------------------

#include <windows.h>
#include "ComPort.h"
#include "my_defs.h"
#include "my_cmd.h"


/***************************************************************************
 *   Новый протокол
 ***************************************************************************/
#define			NDAS		(17486)	/* 0x444E */
#define			DEV_ADDR	0x4b495245
#define			DEV_VERSION	0x78563412


/* Типы пакетов.может принимать значения 0 (REQUEST), 1 (ACK), 2 (NAK)  - 2 байта */
typedef enum {
    PACK_REQUEST,
    PACK_ACK,
    PACK_NAK,
    PACK_NO_DATA,	// 3 не использовать, использовать EMPTY
    PACK_EMPTY,	// 4 если данных нет, использовать этот тип
    PACK_RSVD = 257
} PACK_TYPE;

/* Команды, которые обрабатывает COM порт или Socket - 2 байта */
typedef enum {
    CMD_QUERY_SIGNAL = 2,   /* !!! 2 Не использовать, вместо неё 12 */
    CMD_RECEIVE_PARAMETERS,	//3
    CMD_START_ACQUIRING,	/* !!! 4 - не использовать. Вместо нее 11 */
    CMD_STOP_ACQUIRING,		//5
    CMD_WRITE_PARAMETERS,	//6
    CMD_APPLY_PARAMETERS,	//7
    CMD_QUERY_DEVICE,		//8
    CMD_QUERY_STAT,		//9
    CMD_QUERY_INFO,		//10
    CMD_START_PREVIEW,		/** 11 старт приёма сигнала в режиме preview */
    CMD_QUERY_PREVIEW,		/** 12 получить сейсмограмму в режиме preview */

    CMD_SELECT_SD_TO_CR = 21,   /* 21 - перевести контролер в режим card reader */
    CMD_SELECT_SD_TO_MCU,	/* 22 - Выключить картридер */
    CMD_RESET_MCU,		/* 23 - Сброс CPU */

    CMD_GET_COUNT = 49,		/* Выдать счетчики обменов  */

    CMD_RSVD = 257
} USER_CMD;

/**
 * Структура, описывающая заголовок сообщения. 20 байт
 */
typedef struct {
    u16 ndas;			/* Всегда отправляется NDAS ==  0x444E */
    u16 crc16;			/* CRC16 посылки ПОСЛЕ HEADER_t если есть */
    u32 dev_id;			/* DEV_ADDR - константа */
    PACK_TYPE 	pack_type;	/* request/ack/nak */
    USER_CMD  cmd;		/* Команда */
    u32 ver;			/* Версия - пока 0 */
    u16 size;			/* размер, 0 если запрос */
    u16 rsvd0;
} HEADER_t;


/** 
 * Данные с датчиков - 16 байт 
 */
typedef struct {
    s16 temp;			/* tenth of Celsiuc degree plus or minus */
    s16 voltage;		/* miniVolts */
    s32 press;			/* Давление  */
    s16 hum;			/* Влажность */
    s16 pitch;			/* Углы наклон, поворот, азимут */
    s16 roll;
    s16 head;
} SENSOR_DATE_t;


/**
 * Структура описывающая плату - 8 байт 
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
 * INIT_STATE - ошибки во время инициализации - 4 байта
 */
typedef struct {
    u8 no_time		:1;		/* Нет времени RTC (обычных часов) */
    u8 no_const		:1;		/* Нет констант в EEPROM */
    u8 no_reg_file	:1;		/* Не найден файл параметров на SD карте */
    u8 reg_fault	:1;		/* Регистратор неисправен */
    u8 self_test_on	:1;		/* Идет самотестирование */
    u8 reset_cause	:3;		/* Причина предыдущего сброса */
    u8 rsvd[3];				/* Прочее если будет добавлено */
} INIT_STATE_t;


/**
 * TEST_STATE - тестирование устройств - 4 байт
 * Байт включен - тестирование OK, также причина сброса
 */
typedef struct {
    u8	rtc_ok		:1;		/* RTC */
    u8	press_ok	:1;     	/* Датчик давления + температура */
    u8	acc_ok		:1;     	/* Акселерометр */
    u8	comp_ok		:1;     	/* Компас */
    u8	hum_ok		:1;		/* Влажность */
    u8	eeprom_ok	:1;    	 	/* EEPROM */
    u8	sd_ok		:1;		/* SD карта  */
    u8	gps_ok		:1;		/* Модуль GPS */
    u8	rsvd[3];
} TEST_STATE_t;

/**
 * Состояние во время работы - 4 байт
 */
typedef struct {
    u8	gps_sync_ok	:1;		/* Время синхронизировано по GPS */
    u8	acqis_running	:1;		/* Измерения запущены */
    u8	init_error	:1;		/* Ошибка во время инициализации */
    u8	mem_ovr		:1;		/* Переполнение памяти */
    u8	cmd_error	:1;		/* Ошибка в команде */
    u8  reg_sps		:3;		/* Код частоты дискретизации */
    u8	rsvd[3];
} RUNTIME_STATE_t;



/**
 * Состояние модуля GPS - 1 байт
 */
typedef struct {
    u8 New_NAV_PVT_received:1;
    u8 New_TimeMark_received:1;
    u8 Fix_OK:1;
    u8 Fix_Lost:1;
    u8 VXCO_Adjusted:1;
    u8 VXCO_Adjustment_Lost:1;
    u8 Receiver_Error:1;
    u8 rsvd0:1;
    u8	rsvd1[3];
} GPS_STATE_t;


/* Непонятно (состояние SD ?) - 1 байт. */
typedef struct {
    u8 rsvd1;
} STORAGE_STATE_t;


/**
 * Выбранные АЦП - 4 байта
 */
typedef struct {
    u16 int_adc1_1:1;
    u16 int_adc1_2:1;
    u16 int_adc1_3:1;
    u16 int_adc1_4:1;
    u16 int_adc1_5:1;	/* Добавлены новые внутренние каналы - на будущее */
    u16 int_adc1_6:1;	/* Внешние каналы вынесены в отдельную структуру */
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
 * Структура описывает параметры платы с внешними АЦП. 
 * На каждой плате по 6 каналов. 4 байта
 */
typedef struct {
    unsigned board_active:1;	/* Активна ли плата вообще */

    unsigned ch_1_en:1;		/* Активен ли канал        */
    unsigned ch_1_range:1;	/* Диапазон 2.5 - 10 */
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
 * Параметры АЦП - 2 байта
 */
typedef struct {
    u16 board_type:2;		/* Тип платы с АЦП. (на текущий момент всего два варианта возможны) */
    u16 power_mode:1;
    u16 sample_rate:3;
    u16 fir_phase_response:1;
    u16 test_signal:1;
    u16 int_adc1_sync:1;
    u16 int_adc1_ckldiv:3;
    u16 int_adc2_sync:1;
    u16 int_adc2_ckldiv:3;
} ADC_PARAM_t;


/* Параметры GPS - 28 байт */
typedef struct GPS_Time_and_Position1 {
    u16 year;
    u8  month;
    u8  day;

    u8  hour;
    u8  min;
    u8  sec;
    u8  timeBase:2; // 0 - receiver time, 1 - GPS time, 2 - UTC time
    u8  time:1; // 0 - time is not valid, 1 - valid GPS fix
    u8  reserved:5;

    u32 accEst; 	// Accuracy estimate, ns

    s32 lon; 		// Longitude, deg
    s32 lat; 		// Latitude, deg
    s32 hAcc;	 	// Horizontal accuracy estimate, mm
    s32 vAcc; 		// Vertical accuracy estimate, mm
} GPS_DATA1;

/* Параметры GPS - 24 байта */
typedef struct {
    UNIX_TIME_t time;	/* Время UNIX в нс с 1970 года */
    u8   fix;		/* 0 - time is not valid, 1 - valid GPS fix */
    u8   numSV;		/* Number of satellites in NAV Solution */
    s16  hi;		/* Высота над уровнем моря */
    s32  lon;		/* Longitude, deg */
    s32  lat;		/* Latitude, deg */
    u32  tAcc;		/* Time accuracy estimate */
} GPS_DATA_t;


/**
 * Счетчики обменов по УАРТ
 */
#pragma pack(4)
typedef struct XCHG_COUNT_STRUCT_h {
    HEADER_t hdr;
    int rx_pack;		/* Принятые пакеты */
    int tx_pack;		/* Переданные пакеты */
    int rx_cmd_err;		/* Ошибка в команде */
    int rx_stat_err;		/* Ошибки набегания, кадра (без стопов) и пр */
    int rx_crc_err;		/* Ошибки контрольной суммы */
    int rx_timeout;		/* Таймаут приема */
}  UART_COUNTS_t;;



/**
 * Пакет данных на отправление - 100 измерений акселерометра, упакован на 1!!!
 * Размер пакета - 252 байта
 */
/* Сигнал одного канала */
#pragma pack(1)
typedef struct {
    unsigned x : 24;
    unsigned y : 24;
    unsigned z : 24;
    unsigned h : 24;
} DATA_SIGNAL_t;


///////////////////////////////////////////////////////////////////////////////////
/* Сформированные структуры на прием/передачу с заголовком */
/* ID устройства */
typedef struct {
    HEADER_t header;
    u32 device_id;
} DEV_ID_h;


/* Плата (модуль) */
typedef struct {
    HEADER_t header;
    BOARD_t  board_state;
} INFO_h;


#pragma pack(2)
typedef struct {
    HEADER_t header;
    ADC_BOARD_CFG_t adc_board_1;	/* Всего допускается установка до 8 плат */
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



/**
 * Пакет данных на отправление - 10 измерений акселерометра, упакован на 1!!!
 * Размер пакета 
 */
typedef struct {
    HEADER_t header;
    UNIX_TIME_t t0;             /* UNIX TIME первого измерения */
    u16  adc_freq;              /* Частота оцифровки */
    u16  rsvd;			/* Счетчик пакетов или что то другое */
    DATA_SIGNAL_t sig[NUM_ADS131_PACK];
} ADS131_DATA_h;


/* Внутрение часы для синхронизации */
typedef struct {
  HEADER_t header;
  UNIX_TIME_t time;	
} TIMER_CLOCK_h;



/** Структура команды START_PREVIEW */
typedef struct {
	HEADER_t header;
	u64 time_stamp;             /* UNIX TIME первого измерения */
} START_PREVIEW_h;              


/***************************************************************************
 * 	Структура с заголовком. Статус  
 *****************************************************************************/
typedef struct {
    HEADER_t header;       /* 202  */
    INIT_STATE_t 	init_state;	/* 4 байта */
    TEST_STATE_t 	test_state;	/* 4 байт */
    RUNTIME_STATE_t 	runtime_state;	/* 4 байт */
    SENSOR_DATE_t 	sens_date;	/* 16 байт */
    GPS_DATA_t 		current_GPS_data;
} STATUS_h;


bool PortInit(int, u32);
bool PortClose(void);


int CountsGet(UART_COUNTS_t*);
int  ApplyParams(ADCP_h*);
int  ClockTimeSync(TIMER_CLOCK_h *t);
int  StatusGet(STATUS_h *);
int  StopDevice(void*);
int  StartDevice(void);
int  DataGet(void *);
void GetMyCounts(UART_COUNTS_t*);

int  SendAnyCommand(char *);


//---------------------------------------------------------------------------
#endif
