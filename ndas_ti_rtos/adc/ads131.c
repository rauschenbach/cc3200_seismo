/*************************************************************************************
 * Получение данных с ацп. В версии платы SPI сделано простым чтением
 *************************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "main.h"
#include "log.h"
#include "led.h"
#include "status.h"
#include "circbuf.h"
#include "sintab.h"
#include "userfunc.h"
#include "timtick.h"
#include "myspi.h"
#include "ads131.h"

/* Структура, которая описывает параметры буферов для разной частоты */
#include "struct.h"

/* Не работает с задержкой от systick!!! даже если поставить приоритеты 
 * Заменить на вн. функцию!!!
 */
#define DELAY	delay_ms

/**
 * Дефиниции  
 */
#define  		TEST_BUFFER_SIZE 		80	/* Пакетов */
#define			BYTES_IN_DATA			3	/* Число байт в слове данных АЦП */
#define			MAX_PING_PONG_SIZE		12000	/* Размер буфера - если выделять один раз */



/**
 * адреса регистров 
 */
/* Device settings reg (Read only!) */
#define 	ID		0x00

/* Global settings reg */
#define 	CONFIG1 	0x01
#define 	CONFIG2 	0x02
#define 	CONFIG3 	0x03
#define 	FAULT	 	0x04

/* Channel specific registers */
#define 	CH1SET	 	0x05
#define 	CH2SET	 	0x06
#define 	CH3SET	 	0x07
#define 	CH4SET	 	0x08
#define 	CH5SET	 	0x09
#define 	CH6SET	 	0x0A
#define 	CH7SET	 	0x0B
#define 	CH8SET	 	0x0C

/* Fault detect status regs (read-only!) */
#define 	FAULT_STATP 	0x12
#define 	FAULT_STATN 	0x13

/* GPIO Reg */
#define 	GPIO	 	0x14

/************************************************************************
 * ADS131 commands - обозначения по даташиту
 ************************************************************************/
/* System CMD */
#define 	CMD_WAKEUP		0x02
#define 	CMD_STANDBY	    	0x04
#define 	CMD_RESET		0x06
#define 	CMD_START		0x08
#define 	CMD_STOP		0x0A
#define 	CMD_OFFSETCAL		0x1A	/* смещение нуля */

/* Data read CMD */
#define 	CMD_RDATAC		0x10	/* Непрерывное чтение */
#define 	CMD_SDATAC		0x11	/* Стоп непрерывное чтение  */
#define 	CMD_RDATA		0x12	/* Чтение по команде */


/**
 * Параметры заполнения буферов для АЦП и режим работы АЦП. Командный или осцилограф 
 * Внутреняя структура
 */
static struct {
    ADS131_Regs regs;		/*  Коэффициенты АПЦ */

    u64 long_time0;
    u64 long_time1;
    u32 num_irq;		/* количество прерываний */

    s32 num_sig;		/* Номер сигнала */
    u32 sig_in_time;		/* Как часто записывать часовой файл */

    u16 pack_cmd_cnt;		/* Счетчик командных пакетов */
    u16 pack_work_cnt;		/* Счетчик рабочих пакетов */

    u16 sig_in_min;		/* Как часто записывать заголовок */
    u16 samples_in_ping;	/* Число собранных пакетов за заданное время */

    u16 sample_us;		/* Уход периода следования прерываний в микросекундах */
    u8 sps_code;		/* Код частоты дискретизации */
    u8 data_size;		/* Размер пакета данных со всех каналов 3..6..9..12 */

    u8 bitmap;			/* Карта каналов */
    u8 decim;			/* Прореживать? */
    u8 ping_pong_flag;		/* Флаг записи - пинг или понг  */
    u8 mode;			/* Режим работы - test, work или cmd */

    bool handler_write_flag;	/* Флаг того, что запись удалась */
    bool handler_sig_flag;	/* Для лампочек */
    bool is_set;		/* Установлен */
    bool is_init;		/* Используем для "Инициализирован" */
    bool is_run;		/* Используем для "Работает" */
} adc_pars_struct;


/* Параметры АЦП нового протокола */
static ADCP_h adcp;
static CircularBuffer cb;

/**
 * Ошыпки АЦП пишем сюда
 */
static ADS131_ERROR_STRUCT adc_error;	/* Ошыпки АЦП пишем сюда */
static ADS131_ERROR_STRUCT *pAdc_error = &adc_error;

static ADS131_DATA_h adc_pack;	/* Пакет с данными на отправление  - статический буфер  - 272 байта или 160 */
static ADS131_DATA_h *pack = &adc_pack;	/* Пакет с данными на отправление */

static u8 ADC_ping_buf[MAX_PING_PONG_SIZE];	/* Статический буфер выберем  */
static u8 ADC_pong_buf[MAX_PING_PONG_SIZE];

static u8 *ADC_DATA_ping = ADC_ping_buf;	/* Пользуемся указателями  */
static u8 *ADC_DATA_pong = ADC_pong_buf;

/* 
 * Синк объекты для записи на SD - будет ожидать сообщение  которое приходит из прерывания.
 * Изначально как указатели на NULL
 */
static OsiSyncObj_t gSdSyncObj;
static OsiTaskHandle gSdTaskHandle;
static OsiSyncObj_t gCmdSyncObj;
static OsiTaskHandle gCmdTaskHandle;
static void vSdTask(void *);
static void vAdcTask(void *);
static void vCmdTask(void *);


/*************************************************************************************
 * 	Статические функции - видны тока здесь
 *************************************************************************************/
static void adc_pin_config(void);
static void adc_reset(void);
static void adc_sync(void);
static void adc_stop(void);
static void adc_mux_config(void);
static void cmd_write(u8);
static void reg_write(u8, u8);
static u8 reg_read(u8);
static int regs_config(ADS131_Regs *);
IDEF void adc_set_run(bool);


static void adc_irq_register(void);
static void adc_irq_unregister(void);


static void cmd_mode_handler(u32 *, int);
static void work_mode_handler(u32 *, int);

/**
 * Расчитать и заполнить размер буфера для частоты и числа байт в пакете
 */
static int calculate_ping_pong_buffer(ADS131_FreqEn freq, u8 bitmap, int len)
{
    int i;
    int res = -1;
    u8 bytes = 0;

    /* Не сделано для 8 итд */
    if (freq > SPS4K) {
	log_write_log_file("ERROR: ADS131 Sample Frequency > 4 kHz not supported!\n");
	return res;
    }

    /* Какие каналы пишем. Размер 1 сампла (число 1 * 3 - со всех работающих АЦП сразу) */
    for (i = 0; i < 4; i++) {
	if ((bitmap >> i) & 0x01) {
	    bytes += 3;
	}
    }

    /* Здесь будет переменный размер буфера ping-pong */
    for (i = 0; i < sizeof(adc_work_struct) / sizeof(ADS131_WORK_STRUCT); i++) {

	if (bytes == 0 || len == 0) {
	    break;
	}

	/* Если такая частота и число байт поддерживается  */
	if (freq == adc_work_struct[i].sample_freq && bytes == adc_work_struct[i].num_bytes) {
	    adc_pars_struct.bitmap = bitmap;	/* Какие каналы пишем  */
	    adc_pars_struct.data_size = adc_work_struct[i].num_bytes;
	    adc_pars_struct.samples_in_ping = adc_work_struct[i].samples_in_ping;	/* число отсчетов в одном ping */
	    adc_pars_struct.sig_in_min = adc_work_struct[i].sig_in_min;	/* вызывать signal раз в минуту */
	    adc_pars_struct.sig_in_time = adc_work_struct[i].sig_in_time;	/* Заголовок пишется столько раз за 1 час */
	    adc_pars_struct.sig_in_time *= len;	/* Сколько часов пишем? **** */
	    adc_pars_struct.decim = adc_work_struct[i].decim_coef;	/* Прореживать */
	    adc_pars_struct.num_sig = 0;
	    adc_pars_struct.ping_pong_flag = 0;
	    adc_pars_struct.handler_sig_flag = false;
	    adc_pars_struct.pack_cmd_cnt = 0;	/* обнуляем счетчики */
	    adc_pars_struct.pack_work_cnt = 0;	/* обнуляем счетчики */
	    adc_pars_struct.sample_us = adc_work_struct[i].period_us;

	    log_write_log_file("INFO: freq:  %d Hz, bytes: %d\n", TIMER_US_DIVIDER / adc_pars_struct.sample_us, bytes);
	    log_write_log_file("INFO: decimation in: %d time(s)\n", adc_pars_struct.decim);
	    log_write_log_file("INFO: data size: %d bytes\n", adc_pars_struct.data_size);
	    log_write_log_file("INFO: %d bytes for ping or pong\n", adc_work_struct[i].ping_pong_size_in_byte);
	    log_write_log_file("INFO: bitmap: %02X\n", adc_pars_struct.bitmap);
	    log_write_log_file("INFO: in ping: %d samples\n", adc_pars_struct.samples_in_ping);
	    log_write_log_file("INFO: bitmap: %02X\n", adc_pars_struct.bitmap);
	    log_write_log_file("INFO: write on SD: %d time(s) in min\n", adc_pars_struct.sig_in_min);
	    res = 0;
	    break;
	}
    }
    /* Уход - макс. 5% */
    adc_pars_struct.sample_us += adc_pars_struct.sample_us / 20;
    return res;
}


/**
 * Конфигурирование АЦП без запуска
 * Готовим АЦП к настройке - создаем необходимые буферы
 * Проверяем правильность параметров, коотрые были переданы
 */
bool ADS131_config(ADS131_Params * par)
{
    int volatile i;
    ADS131_Regs regs;
    u8 tmp, coef;
    bool res = false;

    do {
	if (par == NULL) {
	    log_write_log_file("ERROR: ADS131 parameters can't be NULL!\n");
	    break;
	}
        
        log_write_log_file("---------------ADS131 Configuration--------------- \n");

	/* Очистим регистры */
	mem_set(&regs, 0, sizeof(regs));


	/* Если запущен - выключаем прерывания АЦП */
/*vvvvv		adc_irq_unregister(); */
	adc_pars_struct.mode = par->mode;	/* Сохраняем режим работы */

	/* Первый регистр CONFIG1 - data rate. От 125 до 4000 при 24 битах 
	 * Если неверные настройки. NB: SIVY не поддерживает частоту больше 4 кГц */
	tmp = par->sps;
	if (tmp > SPS4K) {
	    log_write_log_file("ERROR:  ADS131 Sample Frequency > 4 kHz not supported\n");
	    break;
	}

	/* Выбираем частоту  - или все частоты меньше 1 кГц получаютя фильтрацией/децимацией на 8 или снижаем частоту генератора */
	coef = 6;		/* Коэффициент  для частоты 1k при генераторе на 1000 Гц  */
	switch (tmp) {
	case SPS4K:
	    regs.config1 = coef - 4;
	    break;

	case SPS2K:
	    regs.config1 = coef - 3;
	    break;

	case SPS1K:
	    regs.config1 = coef - 2;
	    break;

	    /* частота 4 кГц - децимация на 8 */
	case SPS500:
	    regs.config1 = coef - 1;
	    break;

	    /* частота 2кГц - децимация на 8  */
	case SPS250:
	    regs.config1 = coef;
	    break;

	    /* частота 1 кГц - децимация на 8  */
	case SPS125:
	    regs.config1 = coef;
	    break;

	    /* Все значения < 1K будут получены децимацией */
	default:
	    regs.config1 = coef;
	}

	/* Первый регистр: Sample frequency + daisy chain off */
	regs.config1 |= 0xD0;

	adc_pars_struct.sps_code = par->sps;	/* код частоты сохраняем в структуре */

	/* Второй регистр: не меняем - это подача тестового сигнала */
	regs.config2 = (par->test_freq & 0x3) | ((par->test_sign << 4) & 0x10);
	regs.config2 |= 0xE0;

	/* Третий регистр: VREF_4V - use with a 5-V analog supply */
	regs.config3 = 0xC0;


	/* Настраиваем регистры для МАКСИМАЛЬНОГО числа каналов */
	for (i = 0; i < MAX_ADS131_CHAN; i++) {

	    /* Не может быть 000 011 и 111 */
	    if (par->pga[i] == 0 || par->pga[i] == 0x03 || par->pga[i] > 0x06) {
		log_write_log_file("WARN: ADS131 this gain parameter %d not supported for %d channel!\n", par->pga[i], i);
		log_write_log_file("WARN: Set gain 1 for %d channel!\n", i);
		par->pga[i] = PGA1;
	    }

	    /* Не используем MUX 010 110 и 111 */
	    if (par->mux == 2 || par->mux > 5) {
		log_write_log_file("WARN: ADS131 this mux parameter not supported!\n");
		log_write_log_file("WARN: Set mux parameter on MUX0!\n");
		par->mux = MUX0;
	    }

	    tmp = (par->pga[i] << 4) | (par->mux);
	    regs.chxset[i] = tmp;
	    log_write_log_file("CH%dSET = 0x%02X\r\n", i + 1, tmp);

	    /* Выключаем ненужные каналы */
	    if (i >= NUM_ADS131_CHAN) {
		regs.chxset[i] = 0x90;
	    }
	}

	/* В рабочем режиме проводим эти настройки  */
	if (calculate_ping_pong_buffer(par->sps, par->bitmap, par->file_len) != 0) {
	    log_write_log_file("ERROR: Unable to calculate buffer PING PONG\n");
	    break;
	}

	/* Теперь заполняем заголовок */
	log_fill_adc_header(par->sps, adc_pars_struct.bitmap, adc_pars_struct.data_size);
	log_write_log_file("INFO: change ADS131 header OK\n");

	/* Командный режым - или WiFi или COM порт */
	if (par->mode == CMD_MODE) {

	    adc_pars_struct.bitmap = 0x0F;	/* В командном режиме - все каналы */

	    pack->sample_capacity = 3;	/* По 3 байта в данных всегда */
	    pack->channel_mask = 0x0f;	/* 4 канала всегда */


	    /* подсчитаем децимацию - посмотреть чтобы не был =0! */
	    adc_pars_struct.decim = 1;
	    if (par->sps < SPS1K) {
		adc_pars_struct.decim = 8 / (1 << par->sps);
	    }

	    /* Если буфер не существует создадим его на TEST_BUFFER_SIZE пачек ADS131_DATA_h */
	    log_write_log_file("Try to alloc %d bytes for Circular Buffer\r\n", TEST_BUFFER_SIZE * sizeof(ADS131_DATA_h));
	    PRINTF("Try to alloc %d bytes for Circular Buffer\r\n", TEST_BUFFER_SIZE * sizeof(ADS131_DATA_h));
	    if (!ADS131_is_run() && cb_init(&cb, TEST_BUFFER_SIZE)) {
		adc_set_run(false);
		log_write_log_file("FAIL\r\n");
		break;
	    } else {
		log_write_log_file("SUCCESS\r\n");
	    }
	}
	memset(pAdc_error, 0, sizeof(ADS131_ERROR_STRUCT));

	adc_pars_struct.is_set = true;
	if (regs_config(&regs) == 0) {	/*  Конфигурируем регистры АЦП во всех режимах */
	    adc_pars_struct.is_init = true;	/* Инициализирован  */
	    adc_pars_struct.num_irq = 0;
	    res = true;
	} else {
	    log_write_log_file("Error config regs\n");
	    adc_pars_struct.is_init = false;	/* Инициализирован  */
	}
    } while (0);

    return res;
}


/**
 * Конфигурировать регистры всех 4 или 8 АЦП, SPI0, чипселект дергаем в этой функции
 * Настраиваем выводы _131_START - выход, _131_DRDY - вход
 * Сразу поставим Data Rate в параметре
 */
static int regs_config(ADS131_Regs * regs)
{
    u8 byte = 0;		// максимум 8 каналов может быть
    int i, n;
    int res = 0;

    adc_pin_config();		/* Выводы DRDY + SPI0 */

    /* Before using the Continuous command or configured in read Data serial interface: reset the serial interface. */
    adc_reset();

    if (!adc_pars_struct.is_set)
	return -1;

    /* Остановим. SDATAC command must be issued before any other 
     * commands can be sent to the device.
     */
    cmd_write(CMD_SDATAC);
    DELAY(50);

    /* В качестве проверки сделать чтение - должны прочитать то же самое что записали */
    log_write_log_file("------------ADC tunning--------\n");

    /* Читаем регистр ID  */
    byte = reg_read(ID);
    if ((byte & 0x03) == 0) {
	log_write_log_file("INFO: 4-channel device\n");
	n = 4;
    } else if ((byte & 0x03) == 1) {
	log_write_log_file("INFO: 6-channel device\n");
	n = 6;
    } else if ((byte & 0x03) == 2) {
	log_write_log_file("INFO: 8-channel device\n");
	n = 8;
    } else {
	log_write_log_file("INFO: unknown-channel device\n");
	n = 8;
    }
    log_write_log_file("INFO: ID = 0x%02x\n", byte);
    PRINTF("INFO: ID = 0x%02x\n", byte);


    /* Пишем регистры */
    reg_write(CONFIG1, regs->config1);
    reg_write(CONFIG2, regs->config2);
    reg_write(CONFIG3, regs->config3);

    /* Первый conf */
    byte = reg_read(CONFIG1);
    if (byte != regs->config1) {
	log_write_log_file("FAIL: CONFIG1 = 0x%02x\n", byte);
	res = -1;
    } else {
	log_write_log_file("SUCCESS: CONFIG1: 0x%02x\n", byte);
    }

    PRINTF("CONFIG1: 0x%02x\n", byte);

    /* Второй conf */
    byte = reg_read(CONFIG2);
    if (byte != regs->config2) {
	log_write_log_file("FAIL: CONFIG2 = 0x%02x\n", byte);
	res = -1;
    } else {
	log_write_log_file("SUCCESS: CONFIG2: 0x%02x\n", byte);
    }

    PRINTF("CONFIG2: 0x%02x\n", byte);

    /* Третий conf - иногда стоит бит 0x01 можно проверить по XOR */
    byte = reg_read(CONFIG3);
    if (byte != regs->config3) {
	log_write_log_file("WARN: CONFIG3: 0x%02X, read 0x%02x \n", regs->config3, byte);
	//vvvvv:  res = -1;
    } else {
	log_write_log_file("SUCCESS: CONFIG3: 0x%02x\n", byte);
    }

    PRINTF("CONFIG3: 0x%02x\n", byte);


    /* Выбираем все АЦП, независимо от карты включенных каналов. Ненужные выключаем */
    for (i = 0; i < n; i++) {
	reg_write(CH1SET + i, regs->chxset[i]);	/* Пишем в регистр */

	/* Читаем проверку  - ошибка если в младших 4-х регистрах */
	byte = reg_read(CH1SET + i);
	if (byte != regs->chxset[i] && i < NUM_ADS131_CHAN) {
	    log_write_log_file("FAIL: CH%iSET = 0x%02x\n", i, byte);
	    res = -1;
	} else {
	    log_write_log_file("SUCCESS: CH%iSET = 0x%02x\n", i, byte);
	}
	PRINTF("CH%dSET: 0x%02x\n", i + 1, byte);
    }

    /* Прочитаем FAULT  */
    byte = reg_read(FAULT);
    log_write_log_file("FAULT: 0x%02x\n", byte);


    /*  Частота дискретизации или код?   */
    adcp.sample_rate = adc_pars_struct.sps_code;

    /* Состояние каналов АЦП и усиление */
    adcp.adc_board_1.board_active = 1;
    adcp.adc_board_1.ch_1_gain = (regs->chxset[0] >> 4) & 7;
    adcp.adc_board_1.ch_2_gain = (regs->chxset[1] >> 4) & 7;
    adcp.adc_board_1.ch_3_gain = (regs->chxset[2] >> 4) & 7;
    adcp.adc_board_1.ch_4_gain = (regs->chxset[3] >> 4) & 7;
    adcp.adc_board_1.ch_1_range = 0;
    adcp.adc_board_1.ch_2_range = 0;
    adcp.adc_board_1.ch_3_range = 0;
    adcp.adc_board_1.ch_4_range = 0;

    if (adc_pars_struct.bitmap & 1) {
	adcp.adc_board_1.ch_1_en = 1;
    }
    if (adc_pars_struct.bitmap & 2) {
	adcp.adc_board_1.ch_1_en = 1;
    }
    if (adc_pars_struct.bitmap & 4) {
	adcp.adc_board_1.ch_1_en = 1;
    }
    if (adc_pars_struct.bitmap & 8) {
	adcp.adc_board_1.ch_1_en = 1;
    }
    log_write_log_file("---------------------------\n");
    return res;
}

/**
 * Запустить все АЦП с заданным коэффициентом
 * Все действия с задачами выполняем 1 раз!
 */
void ADS131_start(void)
{
    RUNTIME_STATE_t t;
    OsiReturnVal_e ret;


    if (ADS131_is_run()) {	/* Уже запускали */
	return;
    }

    /* Получить биты */
    status_get_runtime_state(&t);

    cmd_write(CMD_RDATAC);	/* Set the data mode. Запустили все */
    adc_sync();			/* Синхронизируем АЦП */
    adc_set_run(true);		/* Уже запускали */

    /* Ставим сигнал на IVG14 если в рабочем режиме. На pga не смотрим. */
    adc_pars_struct.handler_write_flag = true;	/* В первый раз говорим, что можно писать */

    /* Объект синхронизации (event или что то подобное) */
    if (gSdSyncObj == NULL) {
	ret = osi_SyncObjCreate(&gSdSyncObj);
	if (ret != OSI_OK) {
	    log_write_log_file("ERROR: Create gSdSyncObj!\r\n");
	    while (1);
	} else {
	    log_write_log_file("INFO: Create gSdSyncObj OK!\r\n");
	}
    } else {
	log_write_log_file("WARN: gSdSyncObj already exists!\r\n");
    }

    /* Создаем задачу, которая будет передавать данные */
    if (gSdTaskHandle == NULL) {
	ret = osi_TaskCreate(vSdTask, "SdTask", OSI_STACK_SIZE, NULL, SD_TASK_PRIORITY, &gSdTaskHandle);
	if (ret != OSI_OK) {
	    log_write_log_file("ERROR: Create SdTask!\r\n");
	    while (1);
	} else {
	    log_write_log_file("INFO: Create SdTask OK!\r\n");
	}
    } else {
	log_write_log_file("WARN: gSdTask already exists!\r\n");
    }

    if (adc_pars_struct.mode == CMD_MODE) {

	/* Объект синхронизации (event или что то подобное) */
	if (gCmdSyncObj == NULL) {
	    ret = osi_SyncObjCreate(&gCmdSyncObj);
	    if (ret != OSI_OK) {
		log_write_log_file("ERROR: Create gCmdSyncObj!\r\n");
		while (1);
	    } else {
		log_write_log_file("INFO: Create gCmdSyncObj OK!\r\n");
	    }
	} else {
	    log_write_log_file("WARN: gCmdSyncObj already exists!\r\n");
	}

	/* Создаем задачу, которая будет принимать данные для кольцевого буфера */
	if (gCmdTaskHandle == NULL) {
	    ret = osi_TaskCreate(vCmdTask, "CmdTask", OSI_STACK_SIZE, NULL, CMD_TASK_PRIORITY, &gCmdTaskHandle);
	    if (ret != OSI_OK) {
		log_write_log_file("ERROR: Create CmdTask!\r\n");
		while (1);
	    } else {
		log_write_log_file("INFO: Create CmdTask OK!\r\n");
	    }
	} else {
	    log_write_log_file("WARN: CmdTask already exist!\r\n");
	}
    }

    /* Регистрируем обработчик для АЦП на 2-й высокий приоритет но не запускаем */
    adc_irq_register();

    t.acqis_running = 1;
    status_set_runtime_state(&t);	/* Установить биты */
}


/**
 * Стоп АЦП из режима CONTINIOUS в PowerDown, Выключение SPI
 */
void ADS131_stop(void)
{
    RUNTIME_STATE_t t;

    adc_irq_unregister();	/* Если запущен - выключаем прерывания АЦП */
    adc_pin_config();		/* Выводы DRDY и OWFL + мультиплексор + SPI */
    adc_stop();			/* Сигнал стоп вниз */

    DELAY(50);

    /* Получить биты */
    status_get_runtime_state(&t);
    t.acqis_running = 0;

    /* Посылаем команду Stop data continous  */
    cmd_write(CMD_SDATAC);

    log_write_log_file("------------------ADC stop------------------\n");
    log_write_log_file("CFG1: 0x%02x\n", reg_read(CONFIG1));	/* Прочитаем регистры для проверки */
    log_write_log_file("CFG2: 0x%02x\n", reg_read(CONFIG2));
    log_write_log_file("CFG3: 0x%02x\n", reg_read(CONFIG3));
    cmd_write(CMD_STANDBY);

    adc_set_run(false);		/* не запущен  */

    adc_pars_struct.is_init = false;	/* Требуется инициализация  */
    spi_stop();			/* Отрубить SPI */

    status_set_runtime_state(&t);	/* Установить биты */
}

/**
 * Прерывание в командном режиме - байты наверх в Little Endian
 */
static void cmd_mode_handler(u32 * data, int num)
{
    /* По первому пакету метка времени */
    if (adc_pars_struct.pack_cmd_cnt == 0) {
	u64 ms = timer_get_msec_ticks();
	pack->time_stamp = ms * 1000000;	/* Секунда + миллисекунды первого пакета. время UNIX_TIME_t */

	/* В командном режиме отмечаем что частота на ступень меньше - нет 125!  */
	pack->adc_freq = adc_pars_struct.sps_code - 1;
    }

    /* Убираем самый младший байт */
    pack->sig[adc_pars_struct.pack_cmd_cnt].x = data[0] >> 8;	// x 3..4..1?..0?..6..
    pack->sig[adc_pars_struct.pack_cmd_cnt].y = data[1] >> 8;
    pack->sig[adc_pars_struct.pack_cmd_cnt].z = data[2] >> 8;
    pack->sig[adc_pars_struct.pack_cmd_cnt].h = data[3] >> 8;	// h
    adc_pars_struct.pack_cmd_cnt++;

#if 0
    if (adc_pars_struct.pack_cmd_cnt % 20 == 0) {
	pack->rsvd++;
    }
#endif

    /* Пишем в круговой буфер и посылаем сигнал на запись */
    if (adc_pars_struct.pack_cmd_cnt >= NUM_ADS131_PACK) {
	OsiReturnVal_e ret;
	adc_pars_struct.pack_cmd_cnt = 0;

	ret = osi_SyncObjSignalFromISR(&gCmdSyncObj);	/* Отправляем сигнал */
	if (ret != OSI_OK) {
	    PRINTF("SyncObjSignalFromISR CmdSyncObj error!\r\n");
//          while (1);
	}
    }
}


/**
 * Прерывание в рабочем режиме 4 канала данных
 */
static void work_mode_handler(u32 * data, int num)
{
    u8 *ptr;
    register u8 i, ch, shift = 0;
    u32 d;
    u64 ns, us;
    OsiReturnVal_e ret;

    ns = timer_get_long_time();	/* время в наносекундах */
    us = ns / 1000;		/* микросекунды */

    pAdc_error->time_last = pAdc_error->time_now;
    pAdc_error->time_now = us;	/* Время сампла */

    /* Смотрим порядок следования прерываний АЦП. Если опоздали на 5% - зафиксировать ошибку АЦП */
    if (pAdc_error->time_last != 0 && pAdc_error->time_now - pAdc_error->time_last > adc_pars_struct.sample_us) {
	pAdc_error->sample_miss++;
    }

    /* Пишем время первого пакета */
    if (0 == adc_pars_struct.pack_work_cnt) {
	adc_pars_struct.long_time0 = ns;	/* Секунды + наносекунды первого пакета */
    }

    /* Собираем пинг или понг за 1 секунду */
    if (0 == adc_pars_struct.ping_pong_flag) {
	ptr = ADC_DATA_ping;	/* 0 собираем пинг */
    } else {
	ptr = ADC_DATA_pong;	/* 1 собираем понг */
    }

    /* Какие каналы включены */
    ch = adc_pars_struct.bitmap;

    /* Если канал включен - перевернем в Big Endian и запишем только 3 байта */
    for (i = 0; i < NUM_ADS131_CHAN; i++) {
	if ((1 << i) & ch) {
	    d = byteswap4(data[i]);
	    mem_copy(ptr + adc_pars_struct.pack_work_cnt * adc_pars_struct.data_size + shift, &d, BYTES_IN_DATA);
	    shift += BYTES_IN_DATA;
	}
    }

    adc_pars_struct.pack_work_cnt++;	/* Счетчик пакетов увеличим  */

    /*  1 раз в секунду за 2 или за 4 секунды будет 1000 пакетов */
    if (adc_pars_struct.pack_work_cnt >= adc_pars_struct.samples_in_ping) {
	adc_pars_struct.pack_work_cnt = 0;	/* обнуляем счетчик */
	adc_pars_struct.ping_pong_flag = !adc_pars_struct.ping_pong_flag;	/* меняем флаг. что собрали то и посылаем */

	/* Если нет флага записи: handler_write_flag, значит запись на SD застопорилась - скидываем на flash  */
	if (!adc_pars_struct.handler_write_flag) {
	    pAdc_error->block_timeout++;	/* Считаем ошибки сброса блока на SD */
	}

	adc_pars_struct.long_time1 = adc_pars_struct.long_time0;	/* Скопируем. иначе можем затереть */

	ret = osi_SyncObjSignalFromISR(&gSdSyncObj);	/* Отправляем сигнал */
	if (ret != OSI_OK) {
	    PRINTF("SyncObjSignalFromISR error!\r\n");
//          while (1);
	}
    }
}


/**
 * Прерывание по переходу из "1" в "0" для АЦП
 * Здесь будет передаваться сообщение в задачу, которая будет обрабатыввать данные
 */
void ADS131_ISR_func(void)
{
    u32 data[NUM_ADS131_CHAN];	/* 4 канала  */

    spi_cs_on();

    spi_write_read_data(NUM_ADS131_CHAN, data);	/* Получаем в формате BIG ENDIAN!!! Сверху вниз */

    do {
	/* Для частоты 125 - беру каждый 2 отсчет. условие в такой последовательности! */
	if ((adc_pars_struct.sps_code == SPS125) && (adc_pars_struct.num_irq % 2 == 1)) {
	    break;
	}
#if 0
	/* Тест синусоидой */
	static int k = 0, j = 0;
	data[0] = get_sin_table(k);
	if (j++ % 10 == 0) {
	    k++;
	}
#endif

	/* Программный режым - запись на SD карту  */
	work_mode_handler(data, NUM_ADS131_CHAN);

	if (adc_pars_struct.mode == CMD_MODE) {

	    /* Делитель до частоты 125 - беру каждый этот отсчет для получения данных для PC */
	    if (adc_pars_struct.num_irq % (1 << adc_pars_struct.sps_code) == 0) {
		cmd_mode_handler(data, NUM_ADS131_CHAN);	/* Для режима ОТ PC */
	    }
	}
    } while (0);

    adc_pars_struct.num_irq++;	/* Считаем прерывания */

    if (adc_pars_struct.num_irq % 125 == 0) {
	led_toggle(LED_GREEN);
    }


    spi_cs_off();		/* Убрали CS  */

    /* Подтвердим в конце, иначе будут вхождение внутрь одного прерывания */
    MAP_GPIOIntClear(ADS131_DRDY_BASE, ADS131_DRDY_GPIO);
}


/**
 * Возвращаем счетчик ошибок
 */
void ADS131_get_error_count(ADS131_ERROR_STRUCT * err)
{
    if (err != NULL) {
	mem_copy(err, pAdc_error, sizeof(ADS131_ERROR_STRUCT));
    }
}

/**
 * Запуск АЦП с установленными настройками
 * Прерывание возникнет по перепаду из единицы в ноль, пин будет читаться как 1 при перепаде в 0 
 */
static void adc_irq_register(void)
{
    /* Set GPIO interrupt type */
    MAP_GPIOIntTypeSet(ADS131_DRDY_BASE, ADS131_DRDY_GPIO, GPIO_FALLING_EDGE);
    osi_InterruptRegister(ADS131_DRDY_GROUP_INT, ADS131_ISR_func, INT_PRIORITY_LVL_1);
}


/**
 * Отвяжем прерывание
 */
static void adc_irq_unregister(void)
{
    MAP_GPIOIntDisable(ADS131_DRDY_BASE, ADS131_DRDY_INT);
    MAP_GPIOIntClear(ADS131_DRDY_BASE, ADS131_DRDY_GPIO);
    MAP_IntDisable(ADS131_DRDY_GROUP_INT);
}

/**
 * Старт АЦП - только разрешим прерывания! 
 */
void ADS131_start_irq(void)
{
    /* Чистим флаги */
    MAP_GPIOIntClear(ADS131_DRDY_BASE, ADS131_DRDY_GPIO);

    /* Enable Interrupt */
    MAP_GPIOIntEnable(ADS131_DRDY_BASE, ADS131_DRDY_INT);
}

/**
 * Команда ацп в PD - перед включением
 */
void ADS131_standby(void)
{
    adc_pin_config();
    cmd_write(CMD_SDATAC);
    DELAY(5);
    cmd_write(CMD_STANDBY);
}

/**
 * Команда offset cal
 * OFFSETCAL must be executed every time there is a change in PGA gain settings.
 */
bool ADS131_ofscal(void)
{
    bool res = false;

    /* НЕ когда запущен!  */
    if (!ADS131_is_run()) {
	adc_pin_config();
	cmd_write(CMD_OFFSETCAL);
	res = true;
    }

    return res;
}

/**
 * АЦП запущен?
 */
bool ADS131_is_run(void)
{
    return adc_pars_struct.is_run;
}

/**
 * Установить - АЦП запущен или остановлен
 */
IDEF void adc_set_run(bool run)
{
    adc_pars_struct.is_run = run;
}


/**
 *  Эта функция откачивает данные из пакета
 *  Получить данные - если false, данных нет 
 */
bool ADS131_get_pack(ADS131_DATA_h * buf)
{
    if (!cb_is_empty(&cb) && buf != NULL) {
	RUNTIME_STATE_t t;

	status_get_runtime_state(&t);	/* Получить биты */
	t.mem_ovr = 0;		/* убираем статус - "буфер полон" */
	status_set_runtime_state(&t);	/* Установить биты */
	cb_read(&cb, (ElemType *) buf);
	return true;
    } else {
	return false;		/*  "Данные не готовы" */
    }
}

/**  
 *  Очистить буфер данных
 */
bool ADS131_clear_adc_buf(void)
{
    RUNTIME_STATE_t t;

    status_get_runtime_state(&t);	/* Получить биты */
    t.mem_ovr = 0;		/* убираем статус - "буфер полон" */
    status_set_runtime_state(&t);	/* Установить биты */

    cb_clear(&cb);
    return true;
}


/**
 * Узнать флаг внутри сигнала
 */
bool ADS131_get_handler_flag(void)
{
    return adc_pars_struct.handler_sig_flag;
}

/**
 * Сбросить флаг внутри сигнала
 */
void ADS131_reset_handler_flag(void)
{
    adc_pars_struct.handler_sig_flag = false;
}



/**
 * Подача команды в АЦП 
 * параметр: команда 
 * возврат:  нет
 */
static void cmd_write(u8 cmd)
{
    int volatile i;

    spi_cs_on();

    spi_write_read(cmd);	/* отправка команды */

    /* Пауза в 24 цикла Fclk минимум ~5 микросекунд - ОБЯЗАТЕЛЬНО ДОЛЖНА БЫТЬ!!! 
     * Подобрать длительность паузы по datasheet!!! */
    for (i = 0; i < 25; i++);
    spi_cs_off();
}

/**
 * Запись в 1 регистр  АЦП
 * параметр:  адрес регистра, данные
 * возврат:  нет
 */
static void reg_write(u8 addr, u8 data)
{
    int volatile i, j;
    u8 volatile cmd[3];

    spi_cs_on();

    cmd[0] = 0x40 + addr;
    cmd[1] = 0;
    cmd[2] = data;


    for (i = 0; i < 3; i++) {
	spi_write_read(cmd[i]);

	/* Пауза в ~24 цикла Fclk (5 мкс) - минимум! */
	for (j = 0; j < 25; j++);
    }
    spi_cs_off();
}

/**
 * Прочитать 1 регистр  АЦП: 
 * параметр:  адрес регистра
 * возврат:   данные
 */
static u8 reg_read(u8 addr)
{
    int volatile i, j;
    u8 volatile cmd[2];
    u8 volatile data;

    spi_cs_on();

    cmd[0] = 0x20 + addr;
    cmd[1] = 0;

    for (i = 0; i < 2; i++) {
	spi_write_read(cmd[i]);

	/* Пауза в ~24 цикла Fclk (5 мкс) - минимум! */
	for (j = 0; j < 25; j++);
    }
    data = spi_write_read(0);

    spi_cs_off();
    return data;
}

/**
 * Просто конфирурация ножек DRDY и START и инициализация переключателя АЦП
 * Делаем ее один раз всего!
 */
static void adc_pin_config(void)
{
    spi_init();
    adc_mux_config();
}

/**
 * Конфигурим ноги DRDY и START
 */
static void adc_mux_config(void)
{
    /* Enable Peripheral Clocks  */
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA0, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA1, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA2, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA3, PRCM_RUN_MODE_CLK);

    /*  Ногу reset на выход в "1" */
    MAP_PinTypeGPIO(ADS131_RESET_PIN, ADS131_RESET_MODE, false);
    MAP_GPIODirModeSet(ADS131_RESET_BASE, ADS131_RESET_BIT, GPIO_DIR_MODE_OUT);
    MAP_GPIOPinWrite(ADS131_RESET_BASE, ADS131_RESET_BIT, ADS131_RESET_BIT);	/* Ставим в 1 */

    /* Ногу старт / sync_in (PIN_63 / GPIO_08) на выход в 0 */
    MAP_PinTypeGPIO(ADS131_START_PIN, ADS131_START_MODE, false);
    MAP_GPIODirModeSet(ADS131_START_BASE, ADS131_START_BIT, GPIO_DIR_MODE_OUT);
    MAP_GPIOPinWrite(ADS131_START_BASE, ADS131_START_BIT, ~ADS131_START_BIT);	/* Ставим в 0 */

    /* Ногу DRDY (PIN_61 / GPIO_06) на вход - прицепить так же прерывания к ней */
    MAP_PinTypeGPIO(ADS131_DRDY_PIN, ADS131_DRDY_MODE, false);
    MAP_GPIODirModeSet(ADS131_DRDY_BASE, ADS131_DRDY_BIT, GPIO_DIR_MODE_IN);
}

/**
 * Старт всех АЦП период импульса START минимум 0.5 мкс 
 */
static void adc_sync(void)
{
    /* Включаем функции для START - изначально в нуле - Ставим в 1 */
    MAP_GPIOPinWrite(ADS131_START_BASE, ADS131_START_BIT, ADS131_START_BIT);

    /* Перед разрешение IRQ - clear pending interrupts */
    MAP_IntPendClear(ADS131_DRDY_GROUP_INT);
}

/* Стоп ацп  */
static void adc_stop(void)
{
    /* Ставим в 1 */
    MAP_GPIOPinWrite(ADS131_START_BASE, ADS131_START_BIT, ADS131_START_BIT);
}

/**
 * вызывется каждый раз при заполнении 1-го пакета для кольцевого буфера
 */
static void vCmdTask(void *s)
{
    OsiReturnVal_e ret;

    while (1) {
	/* Wait for a message to arrive. */
	ret = osi_SyncObjWait(&gCmdSyncObj, OSI_WAIT_FOREVER);

	if (ret == OSI_OK) {

	    if (cb_is_full(&cb)) {
		RUNTIME_STATE_t t;

		/* Получить биты */
		status_get_runtime_state(&t);
		t.mem_ovr = 1;	/* ставим статус - "буфер полон". Очищаем при чтении, не здесь! */
		status_set_runtime_state(&t);	/* Установить биты */
	    } else {
		cb_write(&cb, pack);	/* Пишем в буфер */
	    }
	}
    }
}

/**
 * Сигнал - вызывется 1...4 раза в секунду при нашей частоте при заполнения любого PING_PONG
 */
static void vSdTask(void *s)
{
    u8 *ptr;
    u64 ns;
    int res;
    OsiReturnVal_e ret;

    /* Выполнять пока запущена */
    while (1) {

	/* Wait for a message to arrive. */
	ret = osi_SyncObjWait(&gSdSyncObj, OSI_WAIT_FOREVER);
	PRINTF("Enter Task\r\n");

	if (ret == OSI_OK) {

	    ns = adc_pars_struct.long_time1;	/* Получили в наносекундах */
	    adc_pars_struct.handler_write_flag = false;	/* Убираем флаг сигнала - входим в обработчик */

	    /*  Делаем суточные файлы или часовые - по времени измеренном в ISR */
	    if (0 == adc_pars_struct.num_sig % adc_pars_struct.sig_in_time) {
		res = log_create_hour_data_file(ns);
	    }

	    /* 1 раз в минуту скидываем заголовок с изменившимся временем  */
	    if (0 == adc_pars_struct.num_sig % adc_pars_struct.sig_in_min) {
		log_change_num_irq_to_header(adc_pars_struct.num_irq);
		res += log_write_adc_header_to_file(ns);	/* Миллисекунды пишем в заголовок */
	    }


	    /* Cбрасываем буфер на SD карту */
	    if (1 == adc_pars_struct.ping_pong_flag) {	/* пишем массив пинг */
		ptr = ADC_DATA_ping;
	    } else {		/* пишем массив понг */
		ptr = ADC_DATA_pong;
	    }

	    res += log_write_adc_data_to_file(ptr, adc_pars_struct.samples_in_ping * adc_pars_struct.data_size);
	    adc_pars_struct.num_sig++;	/* Сбрасываем данные за 4, 2 или 1 секунду */
	    adc_pars_struct.handler_sig_flag = true;	/* 1 раз в секунду - зеленый светодиод */
	    adc_pars_struct.handler_write_flag = true;	/* Ставим флаг - выходим из обработчика. */
	} else {
	    PRINTF("Error get message!\r\n");
	}
	PRINTF("Leave Task %s\r\n", res == RES_NO_ERROR ? "OK" : "Fail");

	osi_Sleep(10);
    }
    PRINTF("End of SdTask\r\n");
}


/**
 * Сброс АЦП ногой
 */
static void adc_reset(void)
{
    /* Ставим в 0 */
    MAP_GPIOPinWrite(ADS131_RESET_BASE, ADS131_RESET_BIT, (unsigned char) ~ADS131_RESET_BIT);

    /* Задержка  */
    DELAY(5);

    /* Ставим в 1 */
    MAP_GPIOPinWrite(ADS131_RESET_BASE, ADS131_RESET_BIT, ADS131_RESET_BIT);
    DELAY(5);			/* Задержка  */
}


/* Задача, которая будет считать количество времени на АЦП */
static void vAdcTask(void *p)
{
    OsiReturnVal_e ret;
    GPS_DATA_t data;
    int i = 0;

    /* Записываем пока нет флага окончания регистрации и идут прерывания */
    do {

	if (adc_pars_struct.num_irq) {
	    PRINTF("-->(%d) Num ADC irq: %d.\r\n", i++, adc_pars_struct.num_irq);
	}
#if 0
	j++;
	if (j % 60 == 0) {
	    status_get_gps_data(&data);
	    log_write_log_file("GPS %s, drift %d\n", (data.fix) ? "3dfix" : "no fix", timer_get_drift());
	}
#endif
	osi_Sleep(1000);
    } while (!is_finish());


    ADS131_stop();
    log_write_log_file("Stop conversion\r\n");
    log_close_data_file();	/* Закрываем файл данных */
    log_close_log_file();
    PRINTF("Stop conversion\r\n");


    /* Удаляем Sync объект */
    ret = osi_SyncObjDelete(&gSdSyncObj);
    if (ret != OSI_OK) {
	PRINTF("Delete gSdSyncObj error!\r\n");
	while (1);
    } else {
	PRINTF("Delete gSdSyncObj OK!\r\n");
    }

    /* или поставить наоборот - удалить объект синхро, а потом задучу иначе иксепшен */
    /* Удаляем задачу - сами себя */
    osi_TaskDelete(&gSdTaskHandle);
    PRINTF("Delete SdTask OK!\r\n");


    board_reset();		/* Сбросим плату */
}

/* Создать задачу для теста АЦП */
void ADS131_test(ADS131_Params * par)
{
    bool res;
    GPS_DATA_t date;
    OsiReturnVal_e ret;

    status_get_gps_data(&date);
    log_create_adc_header(&date);

    res = ADS131_config(par);
    PRINTF("ADC freq: %d Hz\r\n", (1 << par->sps) * 125);
    PRINTF("ADC mode: %s\r\n", par->mode == 0 ? "TEST_MODE" : par->mode == 1 ? "WORK_MODE" : "CMD_MODE");

    PRINTF("ADC pga[0]: %s\r\n",
	   par->pga[0] == PGA1 ? "PGA1" : par->pga[0] == PGA2 ? "PGA2" : par->pga[0] == PGA4 ? "PGA4" : par->pga[0] == PGA8 ? "PGA8" : "PGA12");
    PRINTF("ADC pga[1]: %s\r\n",
	   par->pga[1] == PGA1 ? "PGA1" : par->pga[1] == PGA2 ? "PGA2" : par->pga[1] == PGA4 ? "PGA4" : par->pga[1] == PGA8 ? "PGA8" : "PGA12");
    PRINTF("ADC pga[2]: %s\r\n",
	   par->pga[2] == PGA1 ? "PGA1" : par->pga[2] == PGA2 ? "PGA2" : par->pga[2] == PGA4 ? "PGA4" : par->pga[2] == PGA8 ? "PGA8" : "PGA12");
    PRINTF("ADC pga[3]: %s\r\n",
	   par->pga[3] == PGA1 ? "PGA1" : par->pga[3] == PGA2 ? "PGA2" : par->pga[3] == PGA4 ? "PGA4" : par->pga[3] == PGA8 ? "PGA8" : "PGA12");


    PRINTF("ADC bitmap: 0x%02X\r\n", par->bitmap);
    PRINTF("ADC mux: %s\r\n", par->mux == 0 ? "Normal" :
	   par->mux == 1 ? "shorted" : par->mux == 2 ? "reserved" : par->mux == 3 ? "MVDD" : par->mux == 4 ? "temperature" : "test");
    PRINTF("ADC test signal: %s\r\n", par->test_sign == 0 ? "external" : "internal");
    PRINTF("ADC test freq: %d\r\n", par->test_freq);
    PRINTF("File len: 0x%02X\r\n", par->file_len);

    /* Сделать ожидание 3dfix GPS и запускать на заданное время */
    PRINTF("Init test...: result = %s\r\n", res ? "OK" : "FAIL");
    ADS131_start();
    PRINTF("ADS131 start...\n");

/*    ADS131_start_irq(); */

    /* Создаем задачу, которая будет передавать данные */
    ret = osi_TaskCreate(vAdcTask, "AdcTask", OSI_STACK_SIZE, NULL, ADC_TASK_PRIORITY, NULL);
    if (ret != OSI_OK) {
	while (1) {
	    PRINTF("Create AdcTask error!\r\n");
	    delay_ms(1000);
	}
    } else {
	PRINTF("Create AdcTask OK!\r\n");
    }
}

/* параметры ацп */
void ADS131_get_adcp(ADCP_h * par)
{
    if (par != NULL) {
	mem_copy(par, &adcp, sizeof(adcp));
	par->sample_rate -= 1;	/* на 1 меньше */
    }
}


/**
 * параметры ацп установить  
 * + Запуск АЦП в командном режиме 
 */
bool ADS131_write_parameters(ADCP_h * v)
{
    bool res = false;

    if (v != NULL && !ADS131_is_run()) {
	ADS131_Params par;
	mem_copy(&adcp, v, sizeof(ADCP_h));
	par.mode = CMD_MODE;	/* режим работы програмы */
	par.sps = (ADS131_FreqEn) (adcp.sample_rate + 1);	/* частота - будет на 1 больше! */


	/* Проверим в функции настройки АЦП */
	par.pga[0] = (ADS131_PgaEn) adcp.adc_board_1.ch_1_gain;
	par.pga[1] = (ADS131_PgaEn) adcp.adc_board_1.ch_2_gain;
	par.pga[2] = (ADS131_PgaEn) adcp.adc_board_1.ch_3_gain;
	par.pga[3] = (ADS131_PgaEn) adcp.adc_board_1.ch_4_gain;

	/* Остальные 4 канала - чтобы не ругался  */
	par.pga[4] = par.pga[5] = par.pga[6] = par.pga[7] = PGA1;

	/* Если тестовый сигнал */
	if (adcp.test_signal) {
	    par.mux = MUX_TEST;	/* Мультиплексор на входе */
	    par.test_sign = TEST_INT;	/* Тестовый сигнал  */
	    par.test_freq = TEST_FREQ_0;	/* Частота тестового сигнала */
	} else {
	    par.mux = MUX_NORM;	/* Мультиплексор на входе */
	    par.test_sign = TEST_EXT;	/* Внешний сигнал  */
	    par.test_freq = TEST_FREQ_NONE;	/* Частота тестового сигнала не используется */
	}

	par.bitmap = 0x0f;	/* Включенные каналы: 1 канал включен, 0 - выключен */
	par.file_len = DATA_FILE_LEN;	/* Длина файла записи в часах - 4 часа пишем */

	/* Передаем параметры */
	if (ADS131_config(&par) == true) {
	    ADS131_start();	/* Запускаем АЦП с PGA  */
	    res = true;
	}
    }
    return res;
}

/**
 * Запуск АЦП в командном режиме - установка таймера на время старта
 */
bool ADS131_start_osciloscope(START_PREVIEW_h * sv)
{
    bool res = false;
    GPS_DATA_t date;
    RUNTIME_STATE_t t;

    /* Синхронизируем время */
    if (!timer_is_sync() && sv != NULL) {
	timer_set_sec_ticks(sv->time_stamp / TIMER_NS_DIVIDER);
    }

    status_get_gps_data(&date);
    log_create_adc_header(&date);

    /* Получить биты */
    status_get_runtime_state(&t);

    if (t.acqis_running) {
	ADS131_start_irq();	/* Разрешаем IRQ  */
	res = true;
    }
    return res;
}

/**
 * Стоп всех АЦП 
 */
bool ADS131_stop_osciloscope(void)
{
    bool res = false;
    RUNTIME_STATE_t t;

    /* Получить биты */
    status_get_runtime_state(&t);

    if (t.acqis_running) {
	ADS131_stop();
	log_write_log_file("Stop conversion\r\n");
	log_close_data_file();	/* Закрываем файл данных */
	res = true;
    }
    return res;
}
