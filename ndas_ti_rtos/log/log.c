/* Сервисные функции: ведение лога, запись на SD карту */
#include <utils.h>
#include <ff.h>
#include <fs.h>
#include "sdcard.h"
#include "board.h"
#include "status.h"
#include "userfunc.h"
#include "ads131.h"
#include "timtick.h"
#include "log.h"
#include "main.h"
#include "minIni.h"
#include "xchg.h"


#define   	MAX_FILE_SIZE		256
#define   	MAX_START_NUMBER	100	/* Максимальное число запусков */
#define	  	MAX_TIME_STRLEN		26	/* Длина строки со временем  */
#define   	MAX_LOG_FILE_LEN	134217728	/* 128 Мбайт */
#define   	MAX_FILE_NAME_LEN	31	/* Длина файла включая имя директории с '\0' */
#define		LOCK_FILE_NAME		"lock.fil"
#define		PARAM_FILE_NAME		"recparam.ini"
#define 	ERROR_LOG_NAME		"error.log"	/* Файл ошибок  */
#define 	LOADER_FILE_NAME	"ndas.bin"	/* имя файла программы */
#define 	PROGRAM_FILE_NAME	"/sys/mcuimg.bin"	/* имя файла программы в файловой системе FLASH */
#define 	BOOT_LOG_NAME		"boot.log"	/* Файл лога загрузки */


/************************************************************************
 *      Эти статические переменные не видимы в других файлах 
 * 	отображаются или указателем SRAM (USE_THE_LOADER)
 * 	или используются в секции данных  
 ************************************************************************/
static FATFS fatfs;		/* File system object - можно убрать из global? нет! */
static DIR dir;			/* Директория где храница все файло - можно убрать из global? */
static FIL log_file;		/* File object */
static FIL adc_file;		/* File object для АЦП */
static FIL boot_file;		/* File object для АЦП */
static ADC_HEADER adc_hdr;	/* Заголовок перед данными АЦП - 80 байт */

static FATFS *pFatfs = &fatfs;
static DIR *pDir = &dir;
static FIL *pLogfile = &log_file;
static FIL *pAdcfile = &adc_file;
static FIL *pBootfile = &boot_file;
static ADC_HEADER *pAdc_hdr = &adc_hdr;



/**
 * Для записи времени в лог - получить время, если таймер1 не запущен, то от RTC
 */
static void time_to_str(char *str)
{
    TIME_DATE t;
    u64 msec;
    char sym;
    int stat = CLOCK_RTC_TIME;

    msec = timer_get_msec_ticks();	/* Получаем время от таймера */
#if 0
    stat = get_clock_status();
#endif

    if (stat == CLOCK_RTC_TIME)
	sym = 'R';
    else if (stat == CLOCK_NO_GPS_TIME)
	sym = 'G';
    else if (stat == CLOCK_GPS_TIME)
	sym = 'P';
    else
	sym = 'N';		/* Нет времени */

    /* Записываем дату в формате: P: 09-07-2013 - 13:11:39.871  */
    if (sec_to_td(msec / TIMER_MS_DIVIDER, &t) != -1) {
	sprintf(str, "%c %02d-%02d-%04d %02d:%02d:%02d.%03d ", sym, t.day, t.mon, t.year, t.hour, t.min, t.sec, (u32) (msec % TIMER_MS_DIVIDER));
    } else {
	sprintf(str, "set time error ");
    }
}

/**
 * Вывод на экран, для отладки - если нет сбора данных
 */
int log_term_printf(const char *fmt, ...)
{
    char buf[256];
    int ret = 0, i = 0;
    va_list list;
    RUNTIME_STATE_t t;

    /* Получить биты */
    status_get_runtime_state(&t);

    /* Если запущены измерения или режим работы по заданию */
    if (!t.acqis_running /*&& GNS_NORMAL_MODE != get_gns_work_mode() */ ) {

	/* Получаем текущее время - MAX_TIME_STRLEN символов с пробелом - всегда пишем */
	time_to_str(buf);
	va_start(list, fmt);
	ret = vsnprintf(buf + MAX_TIME_STRLEN, sizeof(buf), fmt, list);
	va_end(list);

	if (ret < 0) {
	    return ret;
	}


	while (buf[i] != '\0') {
	    MAP_UARTCharPut(UARTA0_BASE, buf[i++]);
	}
    }
    return ret;
}

/* Вывод без времени */
int log_term_out(const char *fmt, ...)
{
    char buf[256];
    int ret = 0, i = 0;
    va_list list;
    RUNTIME_STATE_t t;

    /* Получить биты */
    status_get_runtime_state(&t);

    /* Отладка если НЕ запущены измерения и НЕ в режиме Normal */
    if (!t.acqis_running /*&& GNS_NORMAL_MODE != get_gns_work_mode() */ ) {


	/* Получаем текущее время - MAX_TIME_STRLEN символов с пробелом - всегда пишем */
	time_to_str(buf);
	va_start(list, fmt);
	ret = vsnprintf(buf, sizeof(buf), fmt, list);
	va_end(list);

	if (ret < 0) {
	    return ret;
	}

	while (buf[i] != '\0') {
	    MAP_UARTCharPut(UARTA0_BASE, buf[i++]);
	}
    }
    return ret;
}




/**
 * Инициализация файловой системы
 * При монтировании читаем данные с FLASH
 */
int log_mount_fs(void)
{
    int res;
    sd_card_mux_config();
    res = f_mount(pFatfs, "0", 1);	/* Монтируем ФС */
    return res;
}

/**
 * Карта монтирована или нет?
 */
bool log_check_mounted(void)
{
    return pFatfs->fs_type;
}


/**
 * Проверка наличия лок-файла на SD - для определения-стоит ли ждать WUSB 2 минуты
 */
int log_check_lock_file(void)
{
    FIL lock_file;
    FRESULT res;		/* Result code */

    /* Открываем существующий файл */
    if (f_open(&lock_file, LOCK_FILE_NAME, FA_READ | FA_OPEN_EXISTING)) {
	return RES_NO_LOCK_FILE;	/* Если файл не существует! */
    }

    /* Если файл существует - удалим его */
    res = f_unlink(LOCK_FILE_NAME);
    if (res) {
	return RES_DEL_LOCK_ERR;	/* Не удалось */
    }
    return RES_NO_ERROR;	/* Если файл существует */
}


/**
 * Записать в лог загрузчика
 */
int log_write_boot_file(char *fmt, ...)
{
    char str[256];
    FRESULT res;		/* Result code */
    unsigned bw;		/* Прочитано или записано байт  */
    int i, ret = 0;
    va_list p_vargs;		/* return value from vsnprintf  */

    /* Получаем текущее время - MAX_TIME_STRLEN символов с пробелом - всегда пишем */
    time_to_str(str);		// Получить время всегда
    va_start(p_vargs, fmt);
    i = vsnprintf(str + MAX_TIME_STRLEN, sizeof(str), fmt, p_vargs);
    va_end(p_vargs);
    if (i < 0)			/* formatting error?            */
	return RES_FORMAT_ERR;

    i = strlen(str);
    res = f_write(pBootfile, str, i, &bw);
    if (res) {
	return RES_WRITE_BOOTFILE_ERR;
    }

    /* Обязательно запишем! */
    res = f_sync(pBootfile);
    if (res) {
	return RES_WRITE_BOOTFILE_ERR;
    }

    return ret;
}

/**
 * Закрыть лог загрузчика
 */
int log_close_boot_file(int r)
{
    char *str = "------------------------------\r\n";
    unsigned bw;

    if (r)
	log_write_boot_file("INFO: Umount fs and reboot...\r\n");
    else
	log_write_boot_file("INFO: Umount fs...\r\n");
    f_write(pBootfile, str, strlen(str), &bw);
    f_close(pBootfile);
    f_mount(0, 0, 0);
    return 0;
}


/**
 * Открыть лог загрузчика
 */
int log_open_boot_file(void)
{
    int ret, n;

    /* Файл монтирован, есть файловая система на SD карте. Открыли boot */
    ret = f_open(pBootfile, BOOT_LOG_NAME, FA_WRITE | FA_READ | FA_OPEN_ALWAYS);
    if (ret) {
	PRINTF("ERROR:Can't open file %s\n", BOOT_LOG_NAME);
	return RES_NO_LOCK_FILE;
    }

    /* Определим размер */
    n = f_size(pBootfile);
    if (n > MAX_LOG_FILE_LEN) {
	f_truncate(pBootfile);
	n = 0;
    }

    /* Переставим указатель файла */
    f_lseek(pBootfile, n);
    return 0;
}

/**
 * Если на SD карте есть файл с программой - открыть его и прошить на внутреннюю SPI FLASH 
 */
int log_check_loader_file(void)
{
    FIL file;
    int ret, hdl, offset = 0;
    unsigned bw;		/* Прочитано или записано байт  */
    unsigned long MaxSize = 128 * 1024;
    unsigned long token;
    long lRetVal;
    u32 mode;
    u8 buf[MAX_FILE_SIZE];	/* Буфер для записи файла - 256 */

    /* для BOOT лога  */
    log_open_boot_file();

    /* Открываем существующий файл */
    if (f_open(&file, LOADER_FILE_NAME, FA_READ | FA_OPEN_EXISTING)) {
	log_write_boot_file("WARN: File %s does not exist on SD card\r\n", LOADER_FILE_NAME);
	log_close_boot_file(0);
	return RES_NO_LOCK_FILE;	// если файла нет - то и нет ошибки!
    }
    PRINTF("INFO: File %s on SD card open OK\r\n", LOADER_FILE_NAME);
    log_write_boot_file("INFO: File %s on SD card open OK\r\n", LOADER_FILE_NAME);

    do {
	/* Файл существует, открываем файл на FLASH и пытаемся его удалить. Или сначала переименовать / записать/ удалить / снова переименовать? */
	mode = FS_MODE_OPEN_CREATE(MaxSize, _FS_FILE_OPEN_FLAG_COMMIT | _FS_FILE_PUBLIC_WRITE);
	lRetVal = sl_FsOpen((unsigned char *) PROGRAM_FILE_NAME, mode, &token, (_i32 *) & hdl);
	if (lRetVal < 0) {
	    // File may already be created
	    lRetVal = sl_FsClose(hdl, 0, 0, 0);
	    PRINTF("ERROR:File %s already be created\r\n");
	    break;
	} else {
	    // close the user file
	    lRetVal = sl_FsClose(hdl, 0, 0, 0);
	    //   PRINTF("INFO: Can't open file for commit. Try open for writing\r\n");
	    if (SL_RET_CODE_OK != lRetVal) {
		break;
	    }
	}

	//  open a user file for writing
	lRetVal = sl_FsOpen((unsigned char *) PROGRAM_FILE_NAME, FS_MODE_OPEN_WRITE, &token, (_i32 *) & hdl);
	if (lRetVal < 0) {
	    lRetVal = sl_FsClose(hdl, 0, 0, 0);
	    PRINTF("ERROR:Can't create File %s on SPI Flash\r\n", PROGRAM_FILE_NAME);
	    break;
	}

	PRINTF("INFO: File %s on SPI Flash was created OK\r\n", PROGRAM_FILE_NAME);

	// Читаем с SD карты и пишем на FLASH   
	do {
	    ret = f_read(&file, buf, MAX_FILE_SIZE, &bw);
	    if (ret == FR_OK) {
		// Пишем на FLASH столько прочитали
		sl_FsWrite(hdl, offset, buf, bw);
		offset += bw;	// меняем смещение
	    } else {
		PRINTF("WARN: Nothing to read from SD\r\n");
		break;
	    }
	} while (bw > 0);
	PRINTF("INFO: %d bytes were written onto SPI Flash\r\n", offset);
	log_write_boot_file("INFO: %d bytes were written onto SPI Flash\r\n", offset);

	// Закрыть файл на FLASH
	sl_FsClose(hdl, 0, 0, 0);

	// удалим на SD
	if (f_unlink(LOADER_FILE_NAME)) {
	    PRINTF("ERROR:Can't delete file %s on SD\r\n", LOADER_FILE_NAME);
	    return RES_CLOSE_LOADER_ERR;	/* Не удалось */
	} else {
	    PRINTF("INFO: Delete file %s on SD OK\r\n", LOADER_FILE_NAME);
	    log_write_boot_file("INFO: Delete file %s on SD OK\r\n", LOADER_FILE_NAME);
	    log_close_boot_file(1);
	    return RES_NO_ERROR;	/* Если файл существует */
	}
    } while (0);

    /* Закроем файл - не должны сюда попасть! */
    if (f_close(&file)) {
	return RES_CLOSE_LOADER_ERR;
    }
    log_close_boot_file(0);
    return RES_OPEN_LOADER_ERR;
}


/**
 * Проверка наличия файла регистрации на SD
 */
int log_check_reg_file(void)
{
    FIL file;

    /* Открываем существующий файл */
    if (f_open(&file, PARAM_FILE_NAME, FA_READ | FA_OPEN_EXISTING)) {
	return RES_OPEN_PARAM_ERR;
    }

    /* Закроем файл */
    if (f_close(&file)) {
	return RES_CLOSE_PARAM_ERR;
    }
    return RES_NO_ERROR;	/* Если файл существует */
}


/**
 * Запись строки в лог файл, возвращаем сколько записали. С временем ВСЕГДА!
 * Режем по 10 мБайт?
 */
int log_write_log_file(char *fmt, ...)
{
    char str[256];
    FRESULT res;		/* Result code */
    unsigned bw;		/* Прочитано или записано байт  */
    int i, ret = 0;
    va_list p_vargs;		/* return value from vsnprintf  */

    /* Не монтировано (нет фс - работаем от PC), или с карточкой проблемы */
    if (pLogfile->fs == NULL) {
	return RES_MOUNT_ERR;
    }

    /* Получаем текущее время - MAX_TIME_STRLEN символов с пробелом - всегда пишем */
    time_to_str(str);		// Получить время всегда
    va_start(p_vargs, fmt);
    i = vsnprintf(str + MAX_TIME_STRLEN, sizeof(str), fmt, p_vargs);

    va_end(p_vargs);
    if (i < 0)			/* formatting error?            */
	return RES_FORMAT_ERR;


    // Заменим переносы строки на UNIX (не с начала!)
    for (i = MAX_TIME_STRLEN + 4; i < sizeof(str) - 3; i++) {
	if (str[i] == 0x0d || str[i] == 0x0a) {
	    str[i] = 0x0d;	// перевод строки
	    str[i + 1] = 0x0a;	// Windows
	    str[i + 2] = 0;
	    break;
	}
    }

    /* Отключать запись по irq, когда мы пишем в log */
//vvv:

    res = f_write(pLogfile, str, strlen(str), &bw);
    if (res) {
	return RES_WRITE_LOG_ERR;
    }

    /* Обязательно запишем! */
    res = f_sync(pLogfile);
    if (res) {
	return RES_SYNC_LOG_ERR;
    }

    /* Включать запись по irq */
////vvvvv:
    return ret;
}


/**
 * Закрыть лог-файл
 */
int log_close_log_file(void)
{
    FRESULT res;		/* Result code */

    res = f_close(pLogfile);
    if (res) {
	return RES_CLOSE_LOG_ERR;
    }
    return RES_NO_ERROR;
}


/**
 * Создаем заголовок - он будет использоваться для файла данных 
 * Вызывается из файла main
 * структуру будем переделывать!!!
 */
void log_create_adc_header(GPS_DATA_t * par)
{
    if (par != NULL) {
#if defined		ENABLE_NEW_SIVY
	strncpy(pAdc_hdr->DataHeader, "SeismicDat1\0", 12);	/* Заголовок данных SeismicDat1\0 - новый Sivy */
	pAdc_hdr->HeaderSize = sizeof(ADC_HEADER);	/* Размер заголовка     */
	pAdc_hdr->GPSTime = par->time;	/* Время синхронизации: наносекунды */

///vvvv:        pAdc_hdr->Drift = par->tAcc;    /* vvvvvvv: Дрифт от точных часов GPS: миллисекунды или Accuraty */

	pAdc_hdr->Drift = timer_get_drift();
	pAdc_hdr->lat = par->lat;	/* широта: 55417872 = 5541.7872N */
	pAdc_hdr->lon = par->lon;	/* долгота:37213760 = 3721.3760E */
#else
	char str[MAX_TIME_STRLEN];	// 26 байт
	TIME_DATE data;
	long t0;
	u8 ind;

	t0 = par->time / TIMER_NS_DIVIDER;
	sec_to_td(t0, &data);	/* время синхронизации сюда */
	strncpy(pAdc_hdr->DataHeader, "SeismicData\0", 12);	/* Дата ID  */
	pAdc_hdr->HeaderSize = sizeof(ADC_HEADER);	/* Размер заголовка     */

	///vvv: pAdc_hdr->Drift = par->drift * 32768;     /* Дрифт настоящий - выведем в миллисекундах по формуле */

	mem_copy(&pAdc_hdr->GPSTime, &data, sizeof(TIME_DATE));	/* Время получения дрифта по GPS */
	pAdc_hdr->NumberSV = 3;	/* Число спутников: пусть будет 3 */
	pAdc_hdr->params.coord.comp = true;	/* Строка компаса  */

	/* Координаты чудес - где мы находимся */
	if (par->lat == par->lon == 0) {
	    ind = 90;
	} else {
	    ind = 12;		// +554177+03721340000009
	}

	/* Исправлено странное копирование */
	snprintf(str, sizeof(str), "%c%06d%c%07d%06d%02d", (par->lat >= 0) ? '+' : '-', abs(par->lat / 100), (par->lon >= 0) ? '+' : '-', abs(par->lon / 100),
		 0, ind);
	mem_copy(pAdc_hdr->params.coord.pos, str, sizeof(pAdc_hdr->params.coord.pos));

#endif
	log_write_log_file("INFO: Create ADS1282 header, lat: %d lon: %d\n", par->lat, par->lon);
    }
}



/**
 * Изменим заголовок. Вызывается из файла ADS1282
 * Нужно узнать координаты, номер платы и прочее
 */
void log_fill_adc_header(char sps, u8 bitmap, u8 size)
{
    u32 block;

    pAdc_hdr->ConfigWord = sps + 1;	/* 8 миллисекунд (частота 125 Гц + 3 байт */
    pAdc_hdr->ChannelBitMap = bitmap;	/* Будет 1.2.3.4 канала */
    pAdc_hdr->SampleBytes = size;	/* Размер 1 сампла со всех работающих АЦП сразу */
    block = 7500 * (1 << sps);	/* Столько самплов будет за 1 минуту (SPSxxx * 60) */

#if defined		ENABLE_NEW_SIVY
    pAdc_hdr->BlockSamples = block;	/* В новом Sivy размер блока  - 4 байт */
#else

    pAdc_hdr->BlockSamples = block & 0xffff;
    pAdc_hdr->params.coord.rsvd0 = (block >> 16) & 0xff;
    pAdc_hdr->Board = 777;	/* Номер платы ставим свой */
    pAdc_hdr->Rev = 2;		/* Номер ревизии д.б. 2 */
#endif

    log_write_log_file("INFO: Fill ADS1282 header\n");
}


/**
 * Для изменения даных сенсоров. Подсчитать координаты и напряжения.
 * Заполняется раз в минуту
 */
void log_change_adc_header(SENSOR_DATE_t * sens)
{
    if (sens != NULL) {
#if defined		ENABLE_NEW_SIVY
	pAdc_hdr->power_reg = sens->voltage;	/* Напряжение питания, U mv */
	pAdc_hdr->curr_reg = 0;
	pAdc_hdr->power_mod = 0;
	pAdc_hdr->curr_mod = 0;

	pAdc_hdr->temper_reg = sens->temper;	/* Температура регистратора, десятые доли градуса */
	pAdc_hdr->humid_reg = sens->humid;	/* Влажность */
	pAdc_hdr->press_reg = sens->press;	/* Давление внутри сферы */
	pAdc_hdr->pitch = sens->pitch;
	pAdc_hdr->roll = sens->roll;
	pAdc_hdr->head = sens->head;
#else
	int temp;
	/* Пересчитываем по формулам DataFormatProposal */
	pAdc_hdr->Bat = (int) sens->voltage * 1024 / 50000;	/* Будем считать что батарея 12 вольт */

	/* Если нету датчика */
	if (sens->temper == 0 && sens->press == 0) {
	    temp = 200;		/* 20 градусов */
	} else {
	    temp = sens->temper;
	}
	pAdc_hdr->Temp = ((temp + 600) * 1024 / 5000);	/* Будем считать что температура * 10 */

#endif
    }
/*    PRINTF("INFO: Change ADS1282 header OK\n"); */
}


/**
 * Создавать заголовок каждый час, далее - в нем будет изменяться только время
 * Здесь же открыть файл для записи значений полученный с АЦП
 *
 * Профилировать эту функцию!!!!! 
 */
int log_create_hour_data_file(u64 ns)
{
    char name[32];		/* Имя файла */
    struct tm date;
    int res;			/* Result code */
    u32 sec = ns / TIMER_NS_DIVIDER;

    do {
	/* Если уже есть окрытый файл - закроем его и создадим новый  */
	if (pAdcfile->fs) {
	    if ((res = f_close(pAdcfile)) != 0) {
		res = RES_CLOSE_DATA_ERR;
		break;
	    }
	}

	/* Получили время в нашем формате - это нужно для названия */
	if (sec_to_tm(sec, &date) != -1) {
	    GNS_PARAM_STRUCT par;
	    get_gns_start_params(&par);	/* Получили параметры  */


	    /* Название файла по времени: год-месяц-день-часы.минуты */
	    snprintf(name, MAX_FILE_NAME_LEN, "%s/%02d%02d%02d%02d.%02d",
		     &par.gns_dir_name[0], ((date.tm_year - 100) > 0) ? (date.tm_year - 100) : 0, date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min);

	    /* Откроем новый */
	    res = f_open(pAdcfile, name, FA_WRITE | FA_CREATE_ALWAYS);
	    if (res) {
		res = RES_OPEN_DATA_ERR;
		break;
	    }
	    res = RES_NO_ERROR;	/* Все OK */
	} else {
	    res = RES_FORMAT_TIME_ERR;
	}
    } while (0);
    return res;			/* Все OK */
}


/**
 * Подготовить и сбросывать каждую минуту заголовок на SD карту и  делать F_SYNC!
 * В заголовке меняем только время а также получаем время GPS 
 * Профилировать эту функцию!!!!! 
 */
int log_write_adc_header_to_file(u64 ns)
{
    unsigned bw;		/* Прочитано или записано байт  */
    FRESULT res;		/* Result code */

#if defined		ENABLE_NEW_SIVY
    pAdc_hdr->SampleTime = ns;	/* Время сампла, наносекунды */
    pAdc_hdr->GPSTime = status_get_gps_time();	/* Время GPS в наносекундах */
#else
    TIME_DATE date;

    u64 msec = ns / TIMER_US_DIVIDER;
    mem_copy(&pAdc_hdr->SedisTime, &msec, 8);	/* Пишем миллисекунду начала измерений во время Sedis */

    sec_to_td(msec / TIMER_MS_DIVIDER, &date);	/* Получили секунды и время в нашем формате */
    mem_copy(&pAdc_hdr->SampleTime, &date, sizeof(TIME_DATE));	/* Записали время начала записи данных */

    sec_to_td(status_get_gps_time() / TIMER_NS_DIVIDER, &date);	/* Получили секунды и время в нашем формате */
    mem_copy(&pAdc_hdr->GPSTime, &date, sizeof(TIME_DATE));	/* Записали время GPS */

#endif

    /* Скинем в файл заголовок */
    res = f_write(pAdcfile, pAdc_hdr, sizeof(ADC_HEADER), &bw);
    if (res) {
	return RES_WRITE_HEADER_ERR;
    }

    /* Обязательно запишем! не потеряем файл если выдернем SD карту */
    res = f_sync(pAdcfile);
    if (res) {
	return RES_SYNC_HEADER_ERR;
    }

    return RES_NO_ERROR;
}

/**
 * Для проверки - запись числа прерываний
 */
void log_change_num_irq_to_header(u64 num)
{
#if defined		ENABLE_NEW_SIVY
    pAdc_hdr->rsvd0 = (u32) num;	/* Пока пишем на место rsvd0 */
#else
    pAdc_hdr->params.dword[6] = (u32) num;	/* Пока пишем на это место */
#endif
}


/**
 * Запись в файл данных АЦП: данные и размер в байтах
 * Sync делаем раз в минуту!
 * Профилировать эту функцию!!!!! 
 */
int log_write_adc_data_to_file(void *data, int len)
{
    unsigned bw;		/* Прочитано или записано байт  */
    FRESULT res;		/* Result code */

    res = f_write(pAdcfile, (char *) data, len, &bw);
    if (res) {
	return RES_WRITE_DATA_ERR;
    }
    return RES_NO_ERROR;	/* Записали OK */
}


/**
 * Закрыть файл АЦП-перед этим сбросим буферы на диск 
 */
int log_close_data_file(void)
{
    FRESULT res;		/* Result code */

    if (pAdcfile->fs == NULL)	// нет файла еще
	return RES_CLOSE_DATA_ERR;

    /* Обязательно запишем */
    res = f_sync(pAdcfile);
    if (res) {
	return RES_CLOSE_DATA_ERR;
    }

    res = f_close(pAdcfile);
    if (res) {
	return RES_CLOSE_DATA_ERR;
    }
    return FR_OK;		/* Все нормально! */
}


/**
 * Открыть ini-файл регистрации-пока примитивный разбор,
 * параметр: указатель на даные - параметры запуска  
 * возврат:  успех (0) или нет (-1)
 * Возвращаем заполненную структуру наверх
 */
int log_read_reg_file(GNS_PARAM_STRUCT * reg)
{
    int n, ret = RES_NO_ERROR;	/* ret - результат выполнения */
    char buf[8];		/* Название домашней директории */
    char temp;
    char str[32];
    struct tm dir_time;
    FRESULT res;		/* Result code */


#if 10
    do {
	/* Прочитаем АДРЕС нашей платы */
	n = ini_gets("Station", "GNS Number", "1", str, sizeof(str), PARAM_FILE_NAME);
	reg->gns_addr = atoi(str);	///ini_getl("Station", "GNS number", 1, PARAM_FILE_NAME);


	/* времени начала регистрации */
	reg->gns_start_time = ini_gettime("Times", "Start recording", 0, PARAM_FILE_NAME);

	/* Время начала подстройки (просыпания) будет ЗА 10 минут до этого  */
	reg->gns_wakeup_time = (int) (reg->gns_start_time) - 120;

	/* Время окончания регистрации */
	reg->gns_finish_time = ini_gettime("Times", "End recording", 0, PARAM_FILE_NAME);

	/* 3. перевести в секунды времени начала пережига */
	reg->gns_burn_time = ini_gettime("Times", "Burn wire start", 0, PARAM_FILE_NAME);

	/* 4 цифры на число 125, 250, 500, 1000, 2000, 4000 */
	n = ini_gets("ADC", "Sampling rate", "125", str, sizeof(str), PARAM_FILE_NAME);
	reg->gns_adc_freq = atoi(str);

	/* Ошибка в задании частоты */
	if ((reg->gns_adc_freq != 125) && (reg->gns_adc_freq != 250) && (reg->gns_adc_freq != 500)
	    && (reg->gns_adc_freq != 1000) && (reg->gns_adc_freq != 2000) && (reg->gns_adc_freq != 4000)) {
	    ret = RES_FREQ_PARAM_ERR;
	    break;
	}

	/* Мультиплексор */
	n = ini_gets("ADC", "Mux", "Normal", str, sizeof(str), PARAM_FILE_NAME);
	if (strnicmp(str, "short", 5) == 0) {
	    reg->gns_mux = 1;
	} else if (strnicmp(str, "mvdd", 4) == 0) {
	    reg->gns_mux = 3;
	} else if (strnicmp(str, "temp", 4) == 0) {
	    reg->gns_mux = 4;
	} else if (strnicmp(str, "test", 4) == 0) {
	    reg->gns_mux = 5;
	} else {
	    reg->gns_mux = 0;
	}


	/* Тест-сигнал */
	n = ini_gets("ADC", "Test signal", "external", str, sizeof(str), PARAM_FILE_NAME);
	if (strnicmp(str, "internal", 8) == 0) {
	    reg->gns_test_sign = 1;
	} else {
	    reg->gns_test_sign = 0;
	}


	/* Частота тест-сигнала */
	n = ini_gets("ADC", "Test freq", "none", str, sizeof(str), PARAM_FILE_NAME);
	if (strnicmp(str, "freq_0", 6) == 0) {
	    reg->gns_test_freq = 0;
	} else if (strnicmp(str, "freq_1", 6) == 0) {
	    reg->gns_test_freq = 1;
	} else if (strnicmp(str, "AT_DC", 5) == 0) {
	    reg->gns_test_freq = 3;
	} else {
	    reg->gns_test_freq = 2;
	}

	/* Что у нас записано в потреблении? */
	n = ini_gets("ADC", "Power consumption", "Hi", str, sizeof(str), PARAM_FILE_NAME);
	if (strnicmp(str, "Hi", 2) == 0) {
	    reg->gns_adc_consum = 1;
	} else if (strnicmp(str, "Lo", 2) == 0) {
	    reg->gns_adc_consum = 0;
	} else {
	    ret = RES_CONSUMP_PARAM_ERR;
	    break;
	}

	/* 2 цифры на число PGA0 */
	n = ini_gets("ADC", "PGA0", "1", str, sizeof(str), PARAM_FILE_NAME);
	temp = atoi(str);
	if ((temp != 1) && (temp != 2) && (temp != 4)
	    && (temp != 8) && (temp != 12)) {
	    ret = RES_PGA_PARAM_ERR;
	    break;
	}
	reg->gns_adc_pga[0] = temp;

	/* 2 цифры на число PGA1 */
	n = ini_gets("ADC", "PGA1", "1", str, sizeof(str), PARAM_FILE_NAME);
	temp = atoi(str);
	if ((temp != 1) && (temp != 2) && (temp != 4)
	    && (temp != 8) && (temp != 12)) {
	    ret = RES_PGA_PARAM_ERR;
	    break;
	}
	reg->gns_adc_pga[1] = temp;

	/* 2 цифры на число PGA2 */
	n = ini_gets("ADC", "PGA2", "1", str, sizeof(str), PARAM_FILE_NAME);
	temp = atoi(str);
	if ((temp != 1) && (temp != 2) && (temp != 4)
	    && (temp != 8) && (temp != 12)) {
	    ret = RES_PGA_PARAM_ERR;
	    break;
	}
	reg->gns_adc_pga[2] = temp;

	/* 2 цифры на число PGA3 */
	n = ini_gets("ADC", "PGA3", "1", str, sizeof(str), PARAM_FILE_NAME);
	temp = atoi(str);
	if ((temp != 1) && (temp != 2) && (temp != 4)
	    && (temp != 8) && (temp != 12)) {
	    ret = RES_PGA_PARAM_ERR;
	    break;
	}
	reg->gns_adc_pga[3] = temp;
	// остальные неиспользуемые = 1
	reg->gns_adc_pga[4] = reg->gns_adc_pga[5] = reg->gns_adc_pga[6] = reg->gns_adc_pga[7] = 1;

	/* Используемые каналы АЦП. Справа налево */
	n = ini_gets("ADC", "Channels ON", "1111", str, sizeof(str), PARAM_FILE_NAME);
	/* 4 цифры на число */
	reg->gns_adc_bitmap = 0;
	strncpy(buf, str, 5);

	/* Смотрим ПО выключеию  */
	if (buf[0] != '0') {
	    reg->gns_adc_bitmap |= 8;
	}
	if (buf[1] != '0') {
	    reg->gns_adc_bitmap |= 4;
	}
	if (buf[2] != '0') {
	    reg->gns_adc_bitmap |= 2;
	}
	if (buf[3] != '0') {
	    reg->gns_adc_bitmap |= 1;
	}


	/* Режим работы станции */
	n = ini_gets("WorkMode", "Mode", "COM", str, sizeof(str), PARAM_FILE_NAME);
	if (n >= 3) {

	    if (strnicmp(str, "Normal", 3) == 0) {
		reg->gns_work_mode = GNS_NORMAL_MODE;
	    } else {
		reg->gns_work_mode = GNS_CMD_MODE;	/* командный режим  */
	    }
	}

	/* В любом режиме определить имя станции */
	//if (reg->gns_work_mode == GNS_CMD_MODE) {

	/* Что у нас записано в названии AP? */
	n = ini_gets("Network", "SSID name", "Test", str, sizeof(str), PARAM_FILE_NAME);
	if (n > 1 && n < 32) {
	    strncpy(reg->gns_ssid_name, str, strlen(str));
	}

	/* Пароль AP? */
	n = ini_gets("Network", "Security key", "Test", str, sizeof(str), PARAM_FILE_NAME);
	if (n >= 4) {
	    strncpy(reg->gns_ssid_pass, str, strlen(str));
	}

	/* Тип шифрования */
	reg->gns_ssid_sec_type = ini_getl("Network", "Security type", 0, PARAM_FILE_NAME);

	/* Номер порта UDP */
	reg->gns_server_udp_port = ini_getl("Network", "UDP port", 1000, PARAM_FILE_NAME);

	/* Номер порта TCP */
	reg->gns_server_tcp_port = ini_getl("Network", "TCP port", 1000, PARAM_FILE_NAME);

	//}


	/* Сначала читаем директории, если она открывается - значит существует - 8 байт */
	sprintf(str, "GNS%04d", reg->gns_addr);
	if (f_opendir(pDir, str)) {	/* Open the directory */

	    /* Не удалось создать папку с названием станции! */
	    if (f_mkdir(str)) {
		ret = RES_MKDIR_PARAM_ERR;
		break;
	    }
	}


	/* Создаем папку с названием времени запуска */
	int reset = 0;
	sec_to_tm(reg->gns_start_time, &dir_time);

	    if (reset == PRCM_WDT_RESET) {	/* Причина ресета - WDT */

		/* Сначала читаем директории, если она открывается - значит существует */
		for (n = MAX_START_NUMBER - 1; n >= 0; n--) {
		    sprintf(reg->gns_dir_name, "%s/%02d%02d%02d%02d",
			    str,
			    ((dir_time.tm_year - 100) > 0) ? (dir_time.tm_year - 100) % MAX_START_NUMBER : 0, 
			     (dir_time.tm_mon + 1) % MAX_START_NUMBER,
			      dir_time.tm_mday % MAX_START_NUMBER, n % MAX_START_NUMBER);

		    res = f_opendir(pDir, reg->gns_dir_name);	/* Open the directory - самую последнюю */
		    if (res == FR_OK) {
			break;
		    }
		}
	    } else {

		/* Сначала читаем директории, если она открывается - значит существует */
		for (n = MAX_START_NUMBER - 1; n >= 0; n--) {
		    sprintf(reg->gns_dir_name, "%s/%02d%02d%02d%02d",
			    str,
			    ((dir_time.tm_year - 100) >
			     0) ? (dir_time.tm_year - 100) % MAX_START_NUMBER : 0, (dir_time.tm_mon + 1) % MAX_START_NUMBER,
			    dir_time.tm_mday % MAX_START_NUMBER, n % MAX_START_NUMBER);

		    res = f_opendir(pDir, reg->gns_dir_name);	/* Open the directory */
		    if (res == FR_OK && n < (MAX_START_NUMBER - 1)) {
			n++;
			sprintf(reg->gns_dir_name, "%s/%02d%02d%02d%02d",
				str,
				((dir_time.tm_year - 100) >
				 0) ? (dir_time.tm_year - 100) % 100 : 0, (dir_time.tm_mon + 1) % MAX_START_NUMBER, dir_time.tm_mday % MAX_START_NUMBER,
				n % MAX_START_NUMBER);
			break;
		    }
		}

		if (n > (MAX_START_NUMBER - 1)) {
		    ret = RES_MAX_RUN_ERR;
		    break;
		}

		/* Создать папку с названием: число и номер запуска */
		if ((res = f_mkdir(reg->gns_dir_name))) {
		    ret = RES_MKDIR_PARAM_ERR;
		    break;
		}
	    }


	    /* Открыть файл лога в этой директории всегда, создать если нет!  */
	    sprintf(str, "%s/GNS110.log", reg->gns_dir_name);
	    if ((res = f_open(pLogfile, str, FA_WRITE | FA_READ | FA_OPEN_ALWAYS))) {
		ret = RES_CREATE_LOG_ERR;
		break;
	    }

	    /* Определим размер */
	    n = f_size(pLogfile);
	    if (n >= MAX_LOG_FILE_LEN)
		f_truncate(pLogfile);

	    /* Переставим указатель файла */
	    f_lseek(pLogfile, n);

	}
	while (0);

#endif
	return ret;		/* Все получили - успех или неудача! */
    }
