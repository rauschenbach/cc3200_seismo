#include "board.h"
#include "sdcard.h"
#include "proto.h"
#include "calib.h"
#include "pwm.h"
#include "led.h"

#include <hw_common_reg.h>
#include <hw_memmap.h>
#include <uart.h>
#include <interrupt.h>
#include <utils.h>
#include <prcm.h>
#include <string.h>
#include <device.h>

#include "ads131.h"
#include "sensor.h"
#include "pwm.h"
#include "xchg.h"
#include "gps.h"
#include "timtick.h"
#include "log.h"
#include "twi.h"
#include "userfunc.h"
#include "main.h"
#include "proto.h"
#include "status.h"
#include "sdreader.h"
#include "version.h"

#define     TIME_SHIFT		    60
#define		TIME_FOR_TEST		300


static GNS_PARAM_STRUCT gns;
static ADS131_Params par;
static TEST_STATE_t test_state;	/* 4 байта непонятного выравнивания */
static INIT_STATE_t init_state;
static int res = 0;



/* Пока просто флаг окончания записи */
static bool stop_aqcuis = false;

static void Loader(void *par);
static void run_task(void);
static void check_time_task(void *);


/** 
 * Обработчик прерываний таймера точного времени - запуск и останов работы 
 */
static void timer_callback_func(u32 sec)
{
    /* Запуск и останов работы по "часам" */
    if (sec == gns.gns_start_time) {
	ADS131_start_irq();
    } else if (sec == gns.gns_finish_time) {
	stop_aqcuis = true;	/* Окончание регистрации */
    }
}

int main(void)
{
    /* В качестве времени - время компиляции */
    status_init();

    /* Initailizing the board */
    board_init();

    timer_simple_init();

    /* Получаем состояние сенсоров - они установились предварительно vvvvv - перенести вверх, в main будет ставить статусы!!! */
    status_get_test_state(&test_state);

    /* Инициализация терминала на выход. Только на время отладки */
    com_mux_config();
    PRINTF("INFO: Program start OK\n");

    /* Светодиоды */
    led_init();

    /* Инициализация i2c также настройка пинов 2 и 3  + Создадим Lock объект */
    twi_init();

    /* SD карту к процессору */
    set_sd_to_mcu();

    /* 
     * Если у нас карта не монтирована - Подключим SD карту к BF и монтируем ФС  
     * Можно убрать проверку что монтирована - после загрузчика что то остается в памяти
     * и это участок кода не выполняется
     */
    if (!log_check_mounted()) {
	if (log_mount_fs() != RES_NO_ERROR) {
	    PRINTF("ERROR: FATFS mount\n");
	} else {
	    PRINTF("INFO: FATFS mount OK\n");
	}
    }
#if 10
    // Start the SimpleLink Host 
    VStartSimpleLinkSpawnTask(9);
    PRINTF("INFO: StartSimpleLinkTask OK\n");

    /* Попробуем сделать самозагрузку */
    osi_TaskCreate(Loader, (const signed char *) "LoaderTask", OSI_STACK_SIZE * 2, NULL, LOADER_TASK_PRIORITY, NULL);
#else
    run_task();
#endif

    /* Start the task scheduler */
    osi_start();
    return res;
}

/* Запуск остальных задач  */
static void run_task(void)
{
    char str[32];
    int i;
    u8 tmp;

    status_get_init_state(&init_state);

    /* Открыть файл регистрации и получить все времена */
    res = log_read_reg_file(&gns);
    if (res != RES_NO_ERROR) {
	PRINTF("ERROR:log file create. error # %d\n", res);
        init_state.no_reg_file = 1; /* Нет файла регистрации */
    } else {
	log_write_log_file("INFO: log file create OK\n");
	test_state.sd_ok = 1;	/* Запишем что sd OK */
    }


    log_write_log_file("INFO: Params file was read and parsed OK\n");
    log_write_log_file("INFO: Device ID: %04d with %s CPU\n", gns.gns_addr, get_cpu_endian() == 1 ? "Big-endian" : "Little-endian");
    log_write_log_file("INFO: Work DIR: %s\n", gns.gns_dir_name);	/* Название директории для всех файлов */
    log_write_log_file("INFO: Freq %d, pga0 %d, pga1 %d, pga2 %d, pga3 %d\n", 
			gns.gns_adc_freq, 
			gns.gns_adc_pga[0],
			gns.gns_adc_pga[1],
			gns.gns_adc_pga[2],
			gns.gns_adc_pga[3]);	/* Частота АЦП   Усиление АЦП  */

    log_write_log_file("INFO: bitmap 0x%02X, consum %s\n", gns.gns_adc_bitmap, gns.gns_adc_consum ? "Hi" : "Lo");	/* энергопотребление сюда */
    log_write_log_file("INFO: Work mode: %d\n", gns.gns_work_mode);
    log_write_log_file("INFO: This program can properly work until 2038\n");

    /* Получить версию */
    get_str_ver(str);
    log_write_log_file("INFO: Prog. ver: %s\n", str);
    delay_ms(50);


    /* GPS */
    res = gps_init();
    if (res == SUCCESS) {
	log_write_log_file("INFO: GPS module init OK\n");
	test_state.gps_ok = 1;
    }

    /* Сенсоры не в задаче. Будем смотреть, какие датчики запустились - такие и будем опрашивать */
    res = sensor_init();
    if (res & TEST_STATE_ACCEL_OK) {
	log_write_log_file("INFO: Accelerometer init OK\n");
	test_state.acc_ok = 1;
    }
    /* компас */
    if (res & TEST_STATE_COMP_OK) {
	log_write_log_file("INFO: Magnetometer init OK\n");
	test_state.comp_ok = 1;
    }
    /* датчик давления */
    if (res & TEST_STATE_PRESS_OK) {
	log_write_log_file("INFO: Press and temp sensor init OK\n");
	test_state.press_ok = 1;
    }
    if (res & TEST_STATE_HUMID_OK) {
	log_write_log_file("INFO: Humidity sensor init OK\n");
	test_state.hum_ok = 1;
    }

    /* Инициализация компаса и акселерометра датчика давления и температуры. нет некоторых сенсоров!  */
    sensor_create_task(res);

    /* В этой задаче происходит подстройка под GPS  */
    gps_create_task();

    /*  Запустим простой таймер */
    timer_set_sec_ticks(gps_get_gps_time());

    /* Задача регулировки генератора */
    pwm_create_task();

    /* Таймер в качестве точных часов PRTC */
    test_state.rtc_ok = 1;

    /* Запишем что EEPROM OK */
    test_state.eeprom_ok = 1;

    /* Cтавим статусы */
    status_set_test_state(&test_state);

     /* Нет файла рег - если проверяем  */
    status_set_init_state(&init_state);

    /*  Включение таймера PWM калибровки */
    /* calib_init();  используемый таймер! */
    /* calib_set_data(120); */

    /* Проверим файл lock.fil - он будет стираться при каждом запуске */
    res = log_check_lock_file();

    /* В рабочем режиме запускаем по времени записанном на SD карте. Только если есть файл lock.fil */
    if (GNS_NORMAL_MODE == gns.gns_work_mode && RES_NO_ERROR == res) {

	/* Запустим задачу ожидания старта */
	osi_TaskCreate(check_time_task, (const signed char *) "CheckTimeTask", OSI_STACK_SIZE, NULL, CHECK_TIME_TASK_PRIORITY, NULL);

	/* mode=2 sps=2 pga=5 mux=5 sign=0 freq=2 bitmap=F len=1  */
	par.mode = WORK_MODE;	/* режим работы програмы */

	/* частота */
	if (gns.gns_adc_freq == 4000) {
	    par.sps = SPS4K;
	} else if (gns.gns_adc_freq == 2000) {
	    par.sps = SPS2K;
	} else if (gns.gns_adc_freq == 1000) {
	    par.sps = SPS1K;
	} else if (gns.gns_adc_freq == 500) {
	    par.sps = SPS500;
	} else if (gns.gns_adc_freq == 125) {
	    par.sps = SPS125;
	} else {
	    par.sps = SPS250;
	}

	/* PGA */
	for (i = 0; i < 8; i++) {
	    if (gns.gns_adc_pga[i] == 12) {
		tmp = PGA12;
	    } else if (gns.gns_adc_pga[i] == 8) {
		tmp = PGA8;
	    } else if (gns.gns_adc_pga[i] == 4) {
		tmp = PGA4;
	    } else if (gns.gns_adc_pga[i] == 2) {
		tmp = PGA2;
	    } else {
		tmp = PGA1;
	    }
	    par.pga[i] = (ADS131_PgaEn)tmp;
	}

  
	/* Включенные каналы: 1 канал включен, 0 - выключен */
	par.bitmap = gns.gns_adc_bitmap;
	par.mux = (ADS131_MuxEn) gns.gns_mux;
	par.test_sign = (ADS131_TestEn) gns.gns_test_sign;
	par.test_freq = (ADS131_TestFreqEn) gns.gns_test_freq;
	par.file_len = DATA_FILE_LEN;	/* По 4 часа пишем  */

	/* Прежде чем начать запись в рабочем режиме - настроить часы и таймер! 
	 * и проверить все заданные времена */
	ADS131_test(&par);
    } else {			/*if (gns.gns_work_mode == GNS_CMD_MODE) */
	xchg_start(&gns);
    }
}



/**
 * Выдает параметры наружу
 */
void get_gns_start_params(GNS_PARAM_STRUCT * par)
{
    if (par != NULL) {		/* Передает в параметр Эту структуру */
	mem_copy(par, &gns, sizeof(GNS_PARAM_STRUCT));
    }
}


/**
 * Копируют всю структуру за раз
 */
void set_gns_start_params(GNS_PARAM_STRUCT * par)
{
    if (par != NULL) {		/* Ставит эту структуру из параметра  */
	mem_copy(&gns, par, sizeof(GNS_PARAM_STRUCT));
    }
}

bool is_finish(void)
{
    return stop_aqcuis;
}


/**  
 * Проверить время "сейчас" и заданное.
 * Установить обработчики прерываний если работаем по заданию на SD карте
 */
static void check_time_task(void *par)
{
    long t0;
    GPS_DATA_t date;
    char str0[32];
    char str1[32];
    char str2[32];

    /* Ждать синхронизации и подстройки генератора */
    do {
	osi_Sleep(1000);
    } while (!timer_is_sync());


    /* Создадим заголовок здесь! */
    status_get_gps_data(&date);
    log_create_adc_header(&date);

    PRINTF("INFO: timer  was sync\r\n");

    /* Получим время Сейчас */
    t0 = timer_get_sec_ticks();

    /* Делаем проверки */
    PRINTF("INFO: Checking times...\n");
    sec_to_str(t0, str0);

    sec_to_str(gns.gns_start_time, str1);
    sec_to_str(gns.gns_finish_time, str2);

    /* Если нет хотя бы 10 минут на запись! - сбросим плату */
    if (gns.gns_finish_time <= (t0 + 600)) {
	PRINTF("ERROR: No time for recording. Now: %s Finish: %s. Reset\n", str0, str2);
	log_close_log_file();
	board_reset();
    }

    /* Если время "сейчас" меньше времени старта на 30 секунд - ошибка - 
     * просто время начала и окончания передвинем на время, кратное минуте
     */
    if (gns.gns_start_time - t0 < TIME_SHIFT) {
	long rec;		// Время записи по SD карте
	rec = gns.gns_finish_time - gns.gns_start_time;
	gns.gns_start_time = (t0 - t0 % 60) + TIME_SHIFT;	/* на минуту позже "сейчас" */
	/*      gns.gns_finish_time = gns.gns_start_time + rec;  */
	PRINTF("ERROR: Start time %s can't be less time now %s. Try to shift start time for test\n", str1, str0);
    }


    /* Ставим обработчик прерываний */
    timer_set_callback((void *) timer_callback_func);

    sec_to_str(gns.gns_start_time, str1);
    sec_to_str(gns.gns_finish_time, str2);
    PRINTF("INFO: Set timer callback OK. Time now %s\n", str0);
    PRINTF("INFO: Start on %s, Finish on %s\n", str1, str2);
    for (;;);
}

/**
 * Самозагрузчик
 */
static void Loader(void *par)
{
    int res;

    /* Start SimpleLink device  to access file System */
    if (sl_Start(NULL, NULL, NULL) < 0) {
	PRINTF("ERROR: sl_Start\n\r");
	for (;;);
    }
    /* Делаем паузу и перезагрузку */
    res = log_check_loader_file();
    if (res == RES_NO_ERROR) {
	delay_ms(25);
	board_reset();
    } else if (res == RES_NO_LOCK_FILE) {	// Обычная работа
	PRINTF("========= start==========\n\r");
	run_task();
    }

    PRINTF("INFO: Application Tested Successfully\n\r");
    PRINTF("INFO: Exiting Loader\n\r");
    for (;;);
}


/**
 * В каком режиме запущена станция - нужно доля отладки
 */
int get_gns_work_mode(void)
{
  return gns.gns_work_mode;
}

