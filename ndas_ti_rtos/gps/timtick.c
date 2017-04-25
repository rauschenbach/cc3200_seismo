/****************************************************************************************
 *	Подстройки всех таймеров по GPS, ведение времени 
 *      и установка обработчиков старта и стопа
 *****************************************************************************************/
#include "timtick.h"
#include "led.h"

#define         TIMER_COEF      (1953)

/* Точный таймер VCXO */
#define         PREC_TIMER_FREQ              80000000
#define         PREC_TIMER_MODE              PIN_MODE_7
#define         PREC_TIMER_PIN               PIN_50
#define         PREC_TIMER_PRCM              PRCM_TIMERA0
#define         PREC_TIMER_BASE              TIMERA0_BASE
#define         PREC_TIMER_INT               INT_TIMERA0A


/* "Простой" таймер для подсчета времени если GPS не работает  */
#define		SIMPLE_TIMER_BASE		TIMERA3_BASE
#define         SIMPLE_TIMER_PRCM               PRCM_TIMERA3
#define         SIMPLE_TIMER_INT		INT_TIMERA3A


/* Вход для подсчета PPS - gpio22  */
#define         PPS_DRDY_PIN            PIN_15
#define         PPS_DRDY_MODE           PIN_MODE_0
#define         PPS_DRDY_BASE           GPIOA2_BASE
#define         PPS_DRDY_GPIO	        GPIO_PIN_6
#define         PPS_DRDY_INT	        GPIO_INT_PIN_6
#define         PPS_DRDY_BIT            0x40
#define         PPS_DRDY_GROUP_INT      INT_GPIOA2
#define         PPS_PRCM                PRCM_GPIOA2


/* Ведение календаря + флаги синхронизации и внутрение часы для синхронизации */
static struct {
    s32 sec;			/* Секунды с 1970 года */
    u32 sample;			/* Сколько нащелкал таймер. Буквально фаза, если брать из прерывания по PPS */
    u32 prev_sample;
    u32 freq;
    i32 tick;
    s64 ms100;			/* В таймере щелкаем по 100 миллисекунд - т.е. 10 раз в секунду  */
    void (*call_back_func) (u32);	/* Указатель на функцыю, которую будем вызывать из прерывания таймера  */
    bool force_sync;
    bool phase_ok;		/* Таймер синхорнизирован OK */
    bool time_ok;		/* полностью подстроен */
    bool ttt;
} timer_tick_struct;


/**
 * Неточный таймер - запускается в самом начале работы, т.к. у нас нет RTC
 */
static void timer_simple_isr(void)
{
    unsigned long ulInts;

    ulInts = MAP_TimerIntStatus(SIMPLE_TIMER_BASE, true);	/* Clear the timer interrupt. */
    MAP_TimerIntClear(SIMPLE_TIMER_BASE, ulInts);

    /* увеличиваем секунды */
    timer_tick_struct.sec++;
}


/**
 * Инициализация неточных часов
 */
void timer_simple_init(void)
{
    /* Поставим время компиляции в начале, пока нет RTC и плохо работает батарейка GPS */
    timer_tick_struct.sec = get_comp_time();

    /* Разрешим тактирование */
    MAP_PRCMPeripheralClkEnable(SIMPLE_TIMER_PRCM, PRCM_RUN_MODE_CLK);

    /* Сбросить */
    MAP_PRCMPeripheralReset(SIMPLE_TIMER_PRCM);

    /* Configure the timer - счет вверх */
    MAP_TimerConfigure(SIMPLE_TIMER_BASE, TIMER_CFG_PERIODIC_UP);

    /* Прескалер в 0 */
    MAP_TimerPrescaleSet(SIMPLE_TIMER_BASE, TIMER_A, 0);

    /* Enable timerout event interrupt */
    MAP_TimerIntEnable(SIMPLE_TIMER_BASE, TIMER_TIMA_TIMEOUT);

    /* Регистрируем прерывания  */
    osi_InterruptRegister(SIMPLE_TIMER_INT, timer_simple_isr, INT_PRIORITY_LVL_4);

    /* До чего считать - до 1 секунды  */
    MAP_TimerLoadSet(SIMPLE_TIMER_BASE, TIMER_A, SYSCLK);

    /* Enable Timer */
    MAP_TimerEnable(SIMPLE_TIMER_BASE, TIMER_A);
}

/**
 * Остановить таймер если не используем его больше
 */
void timer_simple_stop(void)
{
    MAP_TimerDisable(SIMPLE_TIMER_BASE, TIMER_A);
    MAP_TimerIntDisable(SIMPLE_TIMER_BASE, TIMER_TIMA_TIMEOUT);
    MAP_PRCMPeripheralClkDisable(SIMPLE_TIMER_PRCM, PRCM_RUN_MODE_CLK);
}



/* Прерывание PPS для подсчета и настройки частоты */
static void pps_func_isr(void)
{
    timer_tick_struct.sample = HWREG(PREC_TIMER_BASE + TIMER_O_TAR);	/* считываем значение счетчика как можно быстрее */


    /* Сдвигаем фазу таймера. Расчет уточнить! 
     * Пролог этого ISR - 12 тактов, предыдущая команда ~ 4 такта
     * Проверка флага ~ 4 такта. Т.е. нужно сдвинуть фазу так, чтобы в
     * счетчике была величина соответсвующая 20 тактам или 250 нс.
     * 51200 тиков "нашего" таймера - это 0.1 секунды
     * 1 тик - 1.95 us. Так что можо смело ставить 0
     */
    if (timer_tick_struct.force_sync) {
	HWREG(PREC_TIMER_BASE + TIMER_O_TAV) = 0;
	timer_tick_struct.ms100 = 0;	/* Будем синхорнизировать только секунды */
	timer_tick_struct.force_sync = false;
	timer_tick_struct.phase_ok = true;
    }

    MAP_GPIOIntClear(PPS_DRDY_BASE, PPS_DRDY_GPIO);	/* Чистим прерывания  */
/*
	timer_tick_struct.sample = timer_tick_struct.sample << 8;	// сдвигаем на 8, чтобы нормально переворачивалось через 0
	timer_tick_struct.freq = ((timer_tick_struct.sample - timer_tick_struct.prev_sample) >> 8);	// сдвигаем обратно       
	timer_tick_struct.prev_sample = timer_tick_struct.sample;
*/

    u32 s1;
    s1 = timer_tick_struct.sample << 8;
    timer_tick_struct.freq = ((s1 - timer_tick_struct.prev_sample) >> 8);	// сдвигаем обратно               
    timer_tick_struct.prev_sample = s1;

    timer_tick_struct.tick++;
}

/**
 * Сдвинуть фазу таймера
 */
void timer_shift_phase(void)
{
    timer_tick_struct.force_sync = true;
}

/**
 * счет 100 мс интервалов
 */
static void timer_tick_isr(void)
{
    unsigned long ulInts;

    ulInts = MAP_TimerIntStatus(PREC_TIMER_BASE, true);	/* Clear the timer interrupt. */
    MAP_TimerIntClear(PREC_TIMER_BASE, ulInts);

    timer_tick_struct.ms100++;

    /* увеличиваем секунды */
    if (timer_tick_struct.ms100 == 10) {
	timer_tick_struct.ms100 = 0;

	/* Синхронизируем время в статусе */
	timer_tick_struct.sec++;

	/* Вызываем обработчик, если он установлен */
	if (timer_tick_struct.call_back_func != NULL) {
	    u32 sec;
	    sec = timer_tick_struct.sec;
	    timer_tick_struct.call_back_func(sec);
	}
    }
}



/* Настроить GPIO и Прерывания для ноги PIN15 */
void timer_pps_init(void)
{
    /* Enable Peripheral Clocks  */
    MAP_PRCMPeripheralClkEnable(PPS_PRCM, PRCM_RUN_MODE_CLK);

    /* Ногу DRDY (PIN_15/gpio22 на вход - прицепить так же прерывания к ней */
    MAP_PinTypeGPIO(PPS_DRDY_PIN, PPS_DRDY_MODE, false);
    MAP_GPIODirModeSet(PPS_DRDY_BASE, PPS_DRDY_BIT, GPIO_DIR_MODE_IN);

    /* Set GPIO interrupt type - посмотреть на что реагировать - падающий или восходящий! */
    MAP_GPIOIntTypeSet(PPS_DRDY_BASE, PPS_DRDY_GPIO, GPIO_RISING_EDGE);

    osi_InterruptRegister(PPS_DRDY_GROUP_INT, pps_func_isr, INT_PRIORITY_LVL_2);	/* Регистрируем прерывания */

    /* Чистим флаги */
    MAP_GPIOIntClear(PPS_DRDY_BASE, PPS_DRDY_GPIO);

    /* Enable Interrupt */
    MAP_GPIOIntEnable(PPS_DRDY_BASE, PPS_DRDY_INT);

    MAP_TimerEnable(PREC_TIMER_BASE, TIMER_A);
}

/**
 * Счет клоков VCXO
 */
void timer_counter_init(void)
{
    // Configure PIN_50 for TIMERCP5 GT_CCP00
    MAP_PRCMPeripheralClkEnable(PREC_TIMER_PRCM, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PREC_TIMER_PRCM);
    MAP_PinTypeGPIO(PIN_50, PIN_MODE_0, false);

    MAP_GPIODirModeSet(GPIOA0_BASE, 0x01, GPIO_DIR_MODE_IN);
    MAP_PinTypeTimer(PREC_TIMER_PIN, PREC_TIMER_MODE);
    MAP_PinConfigSet(PREC_TIMER_PIN, PIN_TYPE_STD_PD, PIN_STRENGTH_6MA);

    // Прескалер
    MAP_TimerPrescaleSet(PREC_TIMER_BASE, TIMER_A, 0xFF);
    MAP_TimerPrescaleMatchSet(PREC_TIMER_BASE, TIMER_A, 0xFF);

    // Set the reload value
    MAP_TimerLoadSet(PREC_TIMER_BASE, TIMER_A, 0xFFFF);
    MAP_TimerMatchSet(PREC_TIMER_BASE, TIMER_A, 0xFFFF);


    // Configure the timer in Input Edge-Count Mode - режим 9.3.2.2
    MAP_TimerConfigure(PREC_TIMER_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_COUNT_UP);

    // Считаем импульсы по фронту
    MAP_TimerControlEvent(PREC_TIMER_BASE, TIMER_A, TIMER_EVENT_BOTH_EDGES);

    TimerValueSet(PREC_TIMER_BASE, TIMER_A, 0);
}



/**
 * Отключаем счет PPS
 */
void timer_pps_exit(void)
{
    // MAP_IntUnregister(PPS_DRDY_GROUP_INT);
    // MAP_GPIOIntDisable(PPS_DRDY_BASE, PPS_DRDY_INT);

    //MAP_PRCMPeripheralClkDisable(PREC_TIMER_PRCM, PRCM_RUN_MODE_CLK);    
}



/**
 * Настройка интервального таймера уже подстроенного генератора 
 */
void timer_tick_init(void)
{
    MAP_PRCMPeripheralClkEnable(PREC_TIMER_PRCM, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PREC_TIMER_PRCM);
    MAP_PinTypeGPIO(PIN_50, PIN_MODE_0, false);

    MAP_GPIODirModeSet(GPIOA0_BASE, 0x01, GPIO_DIR_MODE_IN);
    MAP_PinTypeTimer(PREC_TIMER_PIN, PREC_TIMER_MODE);
    MAP_PinConfigSet(PREC_TIMER_PIN, PIN_TYPE_STD_PD, PIN_STRENGTH_6MA);

    // Прескалер
    MAP_TimerPrescaleSet(PREC_TIMER_BASE, TIMER_A, 0);
    MAP_TimerPrescaleMatchSet(PREC_TIMER_BASE, TIMER_A, 0);

    // Configure the timer in Input Edge-Count Mode - режим 9.3.2.2
    MAP_TimerConfigure(PREC_TIMER_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_COUNT_UP /* | TIMER_CFG_PERIODIC_UP */ );

    // Считаем импульсы по фронту
    MAP_TimerControlEvent(PREC_TIMER_BASE, TIMER_A, TIMER_EVENT_POS_EDGE);

    /* считаем до 0.1 сек  */
    TimerValueSet(PREC_TIMER_BASE, TIMER_A, 0);

    ////vvvv:
    MAP_TimerMatchSet(PREC_TIMER_BASE, TIMER_A, 51200 - 1);

    /* До чего считать - до 0.1 секунды  */
    MAP_TimerLoadSet(PREC_TIMER_BASE, TIMER_A, 51200 - 1);

    /* Enable timerout event interrupt */
    MAP_TimerIntEnable(PREC_TIMER_BASE, TIMER_CAPA_MATCH);

    /* Регистрируем прерывания */
    osi_InterruptRegister(INT_TIMERA0A, timer_tick_isr, INT_PRIORITY_LVL_3);

    /* Enable Timer */
    MAP_TimerEnable(PREC_TIMER_BASE, TIMER_A);
}

/**
 * Установить функцию, это так же означает что таймер 1 запущен! 
 */
void timer_set_callback(void *ptr)
{
    timer_tick_struct.call_back_func = (void (*)(u32)) ptr;
}


/**
 * Убрать callback функцию 
 */
void timer_del_callback(void)
{
    timer_tick_struct.call_back_func = NULL;
}


/**
 * Если стоит callback
 */
bool timer_is_callback(void)
{
    return ((timer_tick_struct.call_back_func == NULL) ? false : true);
}


/* Вернуть секунды */
u32 timer_get_sec_ticks(void)
{
    return timer_tick_struct.sec;
}

/* Записать секунды */
void timer_set_sec_ticks(s32 sec)
{
    /* Вставить критическую секцию или семафор */
    timer_tick_struct.sec = sec;
/*	timer_tick_struct.ms100 = 0;       */
}

/* Есьт точное время? */
bool timer_is_sync(void)
{
    return (timer_tick_struct.time_ok);
}


void timer_time_ok(void)
{
    timer_tick_struct.time_ok = true;
}


/** 
 * Получить тики в миллисекундах 
 * 1 тик частоты 80 МГц - 12.5 нс
 */
s64 timer_get_msec_ticks(void)
{
    return timer_get_long_time() / 1000000;
}

/** 
 * Вернуть наносекунды "точных часов"
 * 1 тик частоты 1953 нс
 */
s64 timer_get_long_time(void)
{
    s64 time;

    if (timer_tick_struct.time_ok) {

	/*  Время по "точным часам" */
	time = ((s64) timer_tick_struct.sec * TIMER_NS_DIVIDER)
	    + ((s64) timer_tick_struct.ms100 * TIMER_NS_DIVIDER / 10)
	    + (s64) HWREG(PREC_TIMER_BASE + TIMER_O_TAR) * TIMER_COEF;
    } else {
	time = ((s64) timer_tick_struct.sec * TIMER_NS_DIVIDER + (s64) MAP_TimerValueGet(SIMPLE_TIMER_BASE, TIMER_A) * 25 / 2);
    }

    /* Получаем наносекунды из формулы ns = число тиков * 12.5 + секунды * 10e9 */
    return time;
}

u32 timer_get_num_tick(void)
{
    return timer_tick_struct.tick;
}

u32 timer_get_freq(void)
{
    return timer_tick_struct.freq;
}

u32 timer_get_sample(void)
{
    return timer_tick_struct.sample;
}


/**
 * Получить фазу относительно сигнала PPS и вычислить дрифт
 * Считаем, что если "наш" таймер запаздывает относительно PPS
 * то его значение близко к 51200, если опережает - то близко к 0
 * разбиваем значение на половине и считаем, 
 * если < 25600 - дрифт (+) - таймер спешит
 * если > 25600 - дрифт (-) - таймер отстает
 * Этим методом дрифт можно вычислить только до 640 мкс
 * потом добавить расчет дробных долей секунды
 */
s32 timer_get_drift(void)
{
    return timer_tick_struct.sample;
#if 0    
    int sign = 1;

    if (timer_tick_struct.sample > 25600) {
	sign = -1;		/* минус */
    }

   /* ns = число тиков * 12.5 + секунды * 10e9 */
    return (timer_tick_struct.sample * 25 / 2) * sign;
#endif    
}
