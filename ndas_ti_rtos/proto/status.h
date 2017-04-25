#ifndef _STATUS_H
#define _STATUS_H

#include "my_defs.h"
#include "xchg.h"
#include "proto.h"


/* Причины сброса прибора - перечислены здесь. Из запишем в eeprom  */
//! This function returns the reason(s) for a reset. The reset reason are:-
//! -\b PRCM_POWER_ON  - Device is powering up.
//! -\b PRCM_LPDS_EXIT - Device is exiting from LPDS.
//! -\b PRCM_CORE_RESET - Device is exiting soft core only reset
//! -\b PRCM_MCU_RESET - Device is exiting soft subsystem reset.
//! -\b PRCM_WDT_RESET - Device was reset by watchdog.
//! -\b PRCM_SOC_RESET - Device is exting SOC reset.
//! -\b PRCM_HIB_EXIT - Device is exiting hibernate.


/** 
 * Данные с датчиков - 16 байт 
 */
typedef struct {
    s16 temper;			/* tenth of Celsiuc degree plus or minus */
    s16 voltage;		/* miniVolts */
    s32 press;			/* Давление  */
    s16 humid;			/* Влажность */
    s16 pitch;			/* Углы наклон, поворот, азимут */
    s16 roll;
    s16 head;
} SENSOR_DATE_t;



/**
 * INIT_STATE - ошибки во время инициализации - 4 байта
 */
typedef struct {
    u8 no_time		:1;		/* Нет времени RTC (обычных часов) */
    u8 no_const		:1;		/* Нет констант в EEPROM */
    u8 no_reg_file	:1;		/* Не найден файл параметров на SD карте */
    u8 reg_fault	:1;		/* Регистратор неисправен */
    u8 self_test_on	:1;		/* Идет самотестирование и подстройка таймера - мигает */
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


#define   TEST_STATE_RTC_OK		1	
#define   TEST_STATE_PRESS_OK		2	
#define   TEST_STATE_ACCEL_OK		4
#define   TEST_STATE_COMP_OK        	8
#define   TEST_STATE_HUMID_OK		16
#define   TEST_STATE_EEPROM_OK		32
#define   TEST_STATE_SD_OK		64
#define   TEST_STATE_GPS_OK		128



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

/* Структуры внутри стуктур STATUS и GPS */
void status_init(void);
void status_get_full_status(STATUS_h*);
void status_get_init_state(INIT_STATE_t*);
void status_set_init_state(INIT_STATE_t*);
void status_set_dsp_time(s32);
s32  status_get_dsp_time(void);
s64  status_get_gps_time(void);
void status_set_gps_data(s32, s32, s32, s32, u8, bool);
void status_get_gps_data(GPS_DATA_t *);

void status_get_sensor_date(SENSOR_DATE_t*);
void status_set_sensor_date(SENSOR_DATE_t*);

void status_set_test_state(TEST_STATE_t*);
void status_get_test_state(TEST_STATE_t*);
void status_sync_time(void);
void status_set_drift(s32);

void status_get_runtime_state(RUNTIME_STATE_t*);
void status_set_runtime_state(RUNTIME_STATE_t*);

void status_self_test_on(void);
void status_self_test_off(void);

#endif				/* status.h */
