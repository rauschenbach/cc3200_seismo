/*****************************************************************************
 * Отдельные функции для манипуляции со статосом и отдельными битами-полями
 *****************************************************************************/
#include <string.h>
#include <hw_types.h>
#include <rom.h>
#include <rom_map.h>
#include <prcm.h>

#include "status.h"
#include "userfunc.h"
#include "main.h"
#include "log.h"
#include "timtick.h"
#include "version.h"
#include "utils.h"

static STATUS_h status;

/**
 * Установим биты статуса в самом начале работы программы
 */
void status_init(void)
{
    long s;

    /* Структура INIT_STATE */
    status.init_state.no_time = 1;
    status.init_state.no_const = 1;
    status.init_state.no_reg_file = 0;
    status.init_state.reg_fault = 0;
    s = MAP_PRCMSysResetCauseGet();
    status.init_state.reset_cause = s;	/* Причина сброса */

    /* Структура RUNTIME_STATE */
    status.runtime_state.gps_sync_ok = 1;
    status.runtime_state.acqis_running = 0;
    status.runtime_state.init_error = 0;
    status.runtime_state.mem_ovr = 0;

    /* Структура DEVICE_STATE */
    status.sens_date.temper = -1;
    status.sens_date.humid = -1;
    status.sens_date.voltage = -1;
    status.sens_date.press = 0;
    status.sens_date.pitch = -1;
    status.sens_date.roll = -1;
    status.sens_date.head = -1;

    /* 0 - receiver time, 1 - GPS time, 2 - UTC time, 3 - RTC time */
//    status.current_GPS_data.base = CLOCK_NO_GPS_TIME;

    /* 0 - time is not valid, 1 - valid GPS fix */
    status.current_GPS_data.fix = 0;
    status.current_GPS_data.tAcc = 0;	/* 0 - receiver time, 1 - GPS time, 2 - UTC time */
}

/* Вернуть статус устройства - 100 байт */
void status_get_full_status(STATUS_h * par)
{
    if (par != NULL) {
	mem_copy(par, &status, sizeof(status));
    }
}

/* Вернуть подстатус устройства */
void status_get_init_state(INIT_STATE_t * par)
{
    if (par != NULL) {
	mem_copy(par, &status.init_state, sizeof(INIT_STATE_t));
    }
}

/* Установить подстатус */
void status_set_init_state(INIT_STATE_t * par)
{
    if (par != NULL) {
	mem_copy(&status.init_state, par, sizeof(INIT_STATE_t));
    }
}

/* Получить параметры  */
void status_get_runtime_state(RUNTIME_STATE_t * par)
{
    mem_copy(par, &status.runtime_state, sizeof(RUNTIME_STATE_t));
}

/* установить параметры */
void status_set_runtime_state(RUNTIME_STATE_t * par)
{
    mem_copy(&status.runtime_state, par, sizeof(RUNTIME_STATE_t));
}

/* Получить параметры GPS  */
void status_get_gps_data(GPS_DATA_t * par)
{
    if (par != NULL) {
	memcpy(par, &status.current_GPS_data, sizeof(GPS_DATA_t));
    }
}

/**
 * Получить время GPS в статусе
 */
s64 status_get_gps_time(void)
{
    return status.current_GPS_data.time;
}

/* Установить параметры GPS - каждую секунду */
void status_set_gps_data(long time, s32 lat, s32 lon, s32 hi, u8 numSV, bool fix)
{
    status.current_GPS_data.time = (UNIX_TIME_t) time * TIMER_NS_DIVIDER;	/* Время UNIX в нс с 1970 года */
    status.current_GPS_data.fix = fix;	/* 0 - time is not valid, 1 - valid GPS fix */
    status.current_GPS_data.hi = hi;	/* Высота над уровнем моря */
    status.current_GPS_data.lon = lon;	/* Longitude, deg */
    status.current_GPS_data.lat = lat;	/* Latitude, deg */
    status.current_GPS_data.numSV = numSV;	/* Число спутников */

    /* Есть fixx но нет статуса что время установлено. Один раз установить */
/*    
    if (fix && status.init_state.no_time) {
	set_sec_ticks(time);
	status.init_state.no_time = 0;
    }  

    char str[32];
    int t0 = status.current_GPS_data.time / TIMER_NS_DIVIDER;
    sec_to_str(t0, str);

    PRINTF("INFO: now time: %s, fix: %s hi: %4.3f lat: %3.4f lon: %3.4f\n", 
		str, (status.current_GPS_data.fix)?  "3dFix" : "NoFix", 
		(f32)status.current_GPS_data.hi/1000.0, (f32)status.current_GPS_data.lon/10e6, (f32)status.current_GPS_data.lat/10e6);
*/
}

/* Записать дрифт */
void status_set_drift(s32 drift)
{
    status.current_GPS_data.tAcc = drift;
}


/**
 * Мигать самотестированием
 */ 
void status_self_test_on(void)
{
    status.init_state.self_test_on ^= 1;	/* Мигаем */
}

/**
 * Выключить самотестирование
 */ 
void status_self_test_off(void)
{
    status.init_state.self_test_on = 0;	/* Выключаем */
}



/**
 * Принудительно синхронизироваьт часы
 */
void status_sync_time(void)
{
    int sec;

    sec = status.current_GPS_data.time / TIMER_NS_DIVIDER;
    timer_set_sec_ticks(sec);
    status.init_state.no_time = 0;
}

/**
 * Получить данные сенсоров
 */
void status_get_sensor_date(SENSOR_DATE_t * par)
{
    if (par != NULL) {
	memcpy(par, &status.sens_date, sizeof(SENSOR_DATE_t));
    }
}


/**
 * Сохранить и установить данные сенсоров
 */
void status_set_sensor_date(SENSOR_DATE_t * par)
{
    if (par != NULL) {
	memcpy(&status.sens_date, par, sizeof(SENSOR_DATE_t));
    }
}


/**
 * Слова тестирования получить
 * Хотя число и 4 байта, получаю данные так же с помошью memcpy
 * Мы не знаем выровнено оно или нет
 */
void status_get_test_state(TEST_STATE_t * par)
{
    if (par != NULL) {
	memcpy(par, &status.test_state, sizeof(TEST_STATE_t));
    }
}

/**
 * Слова тестирования установить
 * Хотя число и 4 байта, получаю данные так же с помошью memcpy
 */
void status_set_test_state(TEST_STATE_t * par)
{
    if (par != NULL) {
	memcpy(&status.test_state, par, sizeof(TEST_STATE_t));
    }
}
