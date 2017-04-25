/****************************************************************************************
 *	���������� ���� �������� �� GPS, ������� ������� 
 *      � ��������� ������������ ������ � �����
 *****************************************************************************************/
#include "timtick.h"
#include "led.h"

#define         TIMER_COEF      (1953)

/* ������ ������ VCXO */
#define         PREC_TIMER_FREQ              80000000
#define         PREC_TIMER_MODE              PIN_MODE_7
#define         PREC_TIMER_PIN               PIN_50
#define         PREC_TIMER_PRCM              PRCM_TIMERA0
#define         PREC_TIMER_BASE              TIMERA0_BASE
#define         PREC_TIMER_INT               INT_TIMERA0A


/* "�������" ������ ��� �������� ������� ���� GPS �� ��������  */
#define		SIMPLE_TIMER_BASE		TIMERA3_BASE
#define         SIMPLE_TIMER_PRCM               PRCM_TIMERA3
#define         SIMPLE_TIMER_INT		INT_TIMERA3A


/* ���� ��� �������� PPS - gpio22  */
#define         PPS_DRDY_PIN            PIN_15
#define         PPS_DRDY_MODE           PIN_MODE_0
#define         PPS_DRDY_BASE           GPIOA2_BASE
#define         PPS_DRDY_GPIO	        GPIO_PIN_6
#define         PPS_DRDY_INT	        GPIO_INT_PIN_6
#define         PPS_DRDY_BIT            0x40
#define         PPS_DRDY_GROUP_INT      INT_GPIOA2
#define         PPS_PRCM                PRCM_GPIOA2


/* ������� ��������� + ����� ������������� � ��������� ���� ��� ������������� */
static struct {
    s32 sec;			/* ������� � 1970 ���� */
    u32 sample;			/* ������� �������� ������. ��������� ����, ���� ����� �� ���������� �� PPS */
    u32 prev_sample;
    u32 freq;
    i32 tick;
    s64 ms100;			/* � ������� ������� �� 100 ����������� - �.�. 10 ��� � �������  */
    void (*call_back_func) (u32);	/* ��������� �� �������, ������� ����� �������� �� ���������� �������  */
    bool force_sync;
    bool phase_ok;		/* ������ ��������������� OK */
    bool time_ok;		/* ��������� ��������� */
    bool ttt;
} timer_tick_struct;


/**
 * �������� ������ - ����������� � ����� ������ ������, �.�. � ��� ��� RTC
 */
static void timer_simple_isr(void)
{
    unsigned long ulInts;

    ulInts = MAP_TimerIntStatus(SIMPLE_TIMER_BASE, true);	/* Clear the timer interrupt. */
    MAP_TimerIntClear(SIMPLE_TIMER_BASE, ulInts);

    /* ����������� ������� */
    timer_tick_struct.sec++;
}


/**
 * ������������� �������� �����
 */
void timer_simple_init(void)
{
    /* �������� ����� ���������� � ������, ���� ��� RTC � ����� �������� ��������� GPS */
    timer_tick_struct.sec = get_comp_time();

    /* �������� ������������ */
    MAP_PRCMPeripheralClkEnable(SIMPLE_TIMER_PRCM, PRCM_RUN_MODE_CLK);

    /* �������� */
    MAP_PRCMPeripheralReset(SIMPLE_TIMER_PRCM);

    /* Configure the timer - ���� ����� */
    MAP_TimerConfigure(SIMPLE_TIMER_BASE, TIMER_CFG_PERIODIC_UP);

    /* ��������� � 0 */
    MAP_TimerPrescaleSet(SIMPLE_TIMER_BASE, TIMER_A, 0);

    /* Enable timerout event interrupt */
    MAP_TimerIntEnable(SIMPLE_TIMER_BASE, TIMER_TIMA_TIMEOUT);

    /* ������������ ����������  */
    osi_InterruptRegister(SIMPLE_TIMER_INT, timer_simple_isr, INT_PRIORITY_LVL_4);

    /* �� ���� ������� - �� 1 �������  */
    MAP_TimerLoadSet(SIMPLE_TIMER_BASE, TIMER_A, SYSCLK);

    /* Enable Timer */
    MAP_TimerEnable(SIMPLE_TIMER_BASE, TIMER_A);
}

/**
 * ���������� ������ ���� �� ���������� ��� ������
 */
void timer_simple_stop(void)
{
    MAP_TimerDisable(SIMPLE_TIMER_BASE, TIMER_A);
    MAP_TimerIntDisable(SIMPLE_TIMER_BASE, TIMER_TIMA_TIMEOUT);
    MAP_PRCMPeripheralClkDisable(SIMPLE_TIMER_PRCM, PRCM_RUN_MODE_CLK);
}



/* ���������� PPS ��� �������� � ��������� ������� */
static void pps_func_isr(void)
{
    timer_tick_struct.sample = HWREG(PREC_TIMER_BASE + TIMER_O_TAR);	/* ��������� �������� �������� ��� ����� ������� */


    /* �������� ���� �������. ������ ��������! 
     * ������ ����� ISR - 12 ������, ���������� ������� ~ 4 �����
     * �������� ����� ~ 4 �����. �.�. ����� �������� ���� ���, ����� �
     * �������� ���� �������� �������������� 20 ������ ��� 250 ��.
     * 51200 ����� "������" ������� - ��� 0.1 �������
     * 1 ��� - 1.95 us. ��� ��� ���� ����� ������� 0
     */
    if (timer_tick_struct.force_sync) {
	HWREG(PREC_TIMER_BASE + TIMER_O_TAV) = 0;
	timer_tick_struct.ms100 = 0;	/* ����� ���������������� ������ ������� */
	timer_tick_struct.force_sync = false;
	timer_tick_struct.phase_ok = true;
    }

    MAP_GPIOIntClear(PPS_DRDY_BASE, PPS_DRDY_GPIO);	/* ������ ����������  */
/*
	timer_tick_struct.sample = timer_tick_struct.sample << 8;	// �������� �� 8, ����� ��������� ���������������� ����� 0
	timer_tick_struct.freq = ((timer_tick_struct.sample - timer_tick_struct.prev_sample) >> 8);	// �������� �������       
	timer_tick_struct.prev_sample = timer_tick_struct.sample;
*/

    u32 s1;
    s1 = timer_tick_struct.sample << 8;
    timer_tick_struct.freq = ((s1 - timer_tick_struct.prev_sample) >> 8);	// �������� �������               
    timer_tick_struct.prev_sample = s1;

    timer_tick_struct.tick++;
}

/**
 * �������� ���� �������
 */
void timer_shift_phase(void)
{
    timer_tick_struct.force_sync = true;
}

/**
 * ���� 100 �� ����������
 */
static void timer_tick_isr(void)
{
    unsigned long ulInts;

    ulInts = MAP_TimerIntStatus(PREC_TIMER_BASE, true);	/* Clear the timer interrupt. */
    MAP_TimerIntClear(PREC_TIMER_BASE, ulInts);

    timer_tick_struct.ms100++;

    /* ����������� ������� */
    if (timer_tick_struct.ms100 == 10) {
	timer_tick_struct.ms100 = 0;

	/* �������������� ����� � ������� */
	timer_tick_struct.sec++;

	/* �������� ����������, ���� �� ���������� */
	if (timer_tick_struct.call_back_func != NULL) {
	    u32 sec;
	    sec = timer_tick_struct.sec;
	    timer_tick_struct.call_back_func(sec);
	}
    }
}



/* ��������� GPIO � ���������� ��� ���� PIN15 */
void timer_pps_init(void)
{
    /* Enable Peripheral Clocks  */
    MAP_PRCMPeripheralClkEnable(PPS_PRCM, PRCM_RUN_MODE_CLK);

    /* ���� DRDY (PIN_15/gpio22 �� ���� - ��������� ��� �� ���������� � ��� */
    MAP_PinTypeGPIO(PPS_DRDY_PIN, PPS_DRDY_MODE, false);
    MAP_GPIODirModeSet(PPS_DRDY_BASE, PPS_DRDY_BIT, GPIO_DIR_MODE_IN);

    /* Set GPIO interrupt type - ���������� �� ��� ����������� - �������� ��� ����������! */
    MAP_GPIOIntTypeSet(PPS_DRDY_BASE, PPS_DRDY_GPIO, GPIO_RISING_EDGE);

    osi_InterruptRegister(PPS_DRDY_GROUP_INT, pps_func_isr, INT_PRIORITY_LVL_2);	/* ������������ ���������� */

    /* ������ ����� */
    MAP_GPIOIntClear(PPS_DRDY_BASE, PPS_DRDY_GPIO);

    /* Enable Interrupt */
    MAP_GPIOIntEnable(PPS_DRDY_BASE, PPS_DRDY_INT);

    MAP_TimerEnable(PREC_TIMER_BASE, TIMER_A);
}

/**
 * ���� ������ VCXO
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

    // ���������
    MAP_TimerPrescaleSet(PREC_TIMER_BASE, TIMER_A, 0xFF);
    MAP_TimerPrescaleMatchSet(PREC_TIMER_BASE, TIMER_A, 0xFF);

    // Set the reload value
    MAP_TimerLoadSet(PREC_TIMER_BASE, TIMER_A, 0xFFFF);
    MAP_TimerMatchSet(PREC_TIMER_BASE, TIMER_A, 0xFFFF);


    // Configure the timer in Input Edge-Count Mode - ����� 9.3.2.2
    MAP_TimerConfigure(PREC_TIMER_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_COUNT_UP);

    // ������� �������� �� ������
    MAP_TimerControlEvent(PREC_TIMER_BASE, TIMER_A, TIMER_EVENT_BOTH_EDGES);

    TimerValueSet(PREC_TIMER_BASE, TIMER_A, 0);
}



/**
 * ��������� ���� PPS
 */
void timer_pps_exit(void)
{
    // MAP_IntUnregister(PPS_DRDY_GROUP_INT);
    // MAP_GPIOIntDisable(PPS_DRDY_BASE, PPS_DRDY_INT);

    //MAP_PRCMPeripheralClkDisable(PREC_TIMER_PRCM, PRCM_RUN_MODE_CLK);    
}



/**
 * ��������� ������������� ������� ��� ������������� ���������� 
 */
void timer_tick_init(void)
{
    MAP_PRCMPeripheralClkEnable(PREC_TIMER_PRCM, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PREC_TIMER_PRCM);
    MAP_PinTypeGPIO(PIN_50, PIN_MODE_0, false);

    MAP_GPIODirModeSet(GPIOA0_BASE, 0x01, GPIO_DIR_MODE_IN);
    MAP_PinTypeTimer(PREC_TIMER_PIN, PREC_TIMER_MODE);
    MAP_PinConfigSet(PREC_TIMER_PIN, PIN_TYPE_STD_PD, PIN_STRENGTH_6MA);

    // ���������
    MAP_TimerPrescaleSet(PREC_TIMER_BASE, TIMER_A, 0);
    MAP_TimerPrescaleMatchSet(PREC_TIMER_BASE, TIMER_A, 0);

    // Configure the timer in Input Edge-Count Mode - ����� 9.3.2.2
    MAP_TimerConfigure(PREC_TIMER_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_COUNT_UP /* | TIMER_CFG_PERIODIC_UP */ );

    // ������� �������� �� ������
    MAP_TimerControlEvent(PREC_TIMER_BASE, TIMER_A, TIMER_EVENT_POS_EDGE);

    /* ������� �� 0.1 ���  */
    TimerValueSet(PREC_TIMER_BASE, TIMER_A, 0);

    ////vvvv:
    MAP_TimerMatchSet(PREC_TIMER_BASE, TIMER_A, 51200 - 1);

    /* �� ���� ������� - �� 0.1 �������  */
    MAP_TimerLoadSet(PREC_TIMER_BASE, TIMER_A, 51200 - 1);

    /* Enable timerout event interrupt */
    MAP_TimerIntEnable(PREC_TIMER_BASE, TIMER_CAPA_MATCH);

    /* ������������ ���������� */
    osi_InterruptRegister(INT_TIMERA0A, timer_tick_isr, INT_PRIORITY_LVL_3);

    /* Enable Timer */
    MAP_TimerEnable(PREC_TIMER_BASE, TIMER_A);
}

/**
 * ���������� �������, ��� ��� �� �������� ��� ������ 1 �������! 
 */
void timer_set_callback(void *ptr)
{
    timer_tick_struct.call_back_func = (void (*)(u32)) ptr;
}


/**
 * ������ callback ������� 
 */
void timer_del_callback(void)
{
    timer_tick_struct.call_back_func = NULL;
}


/**
 * ���� ����� callback
 */
bool timer_is_callback(void)
{
    return ((timer_tick_struct.call_back_func == NULL) ? false : true);
}


/* ������� ������� */
u32 timer_get_sec_ticks(void)
{
    return timer_tick_struct.sec;
}

/* �������� ������� */
void timer_set_sec_ticks(s32 sec)
{
    /* �������� ����������� ������ ��� ������� */
    timer_tick_struct.sec = sec;
/*	timer_tick_struct.ms100 = 0;       */
}

/* ���� ������ �����? */
bool timer_is_sync(void)
{
    return (timer_tick_struct.time_ok);
}


void timer_time_ok(void)
{
    timer_tick_struct.time_ok = true;
}


/** 
 * �������� ���� � ������������� 
 * 1 ��� ������� 80 ��� - 12.5 ��
 */
s64 timer_get_msec_ticks(void)
{
    return timer_get_long_time() / 1000000;
}

/** 
 * ������� ����������� "������ �����"
 * 1 ��� ������� 1953 ��
 */
s64 timer_get_long_time(void)
{
    s64 time;

    if (timer_tick_struct.time_ok) {

	/*  ����� �� "������ �����" */
	time = ((s64) timer_tick_struct.sec * TIMER_NS_DIVIDER)
	    + ((s64) timer_tick_struct.ms100 * TIMER_NS_DIVIDER / 10)
	    + (s64) HWREG(PREC_TIMER_BASE + TIMER_O_TAR) * TIMER_COEF;
    } else {
	time = ((s64) timer_tick_struct.sec * TIMER_NS_DIVIDER + (s64) MAP_TimerValueGet(SIMPLE_TIMER_BASE, TIMER_A) * 25 / 2);
    }

    /* �������� ����������� �� ������� ns = ����� ����� * 12.5 + ������� * 10e9 */
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
 * �������� ���� ������������ ������� PPS � ��������� �����
 * �������, ��� ���� "���" ������ ����������� ������������ PPS
 * �� ��� �������� ������ � 51200, ���� ��������� - �� ������ � 0
 * ��������� �������� �� �������� � �������, 
 * ���� < 25600 - ����� (+) - ������ ������
 * ���� > 25600 - ����� (-) - ������ �������
 * ���� ������� ����� ����� ��������� ������ �� 640 ���
 * ����� �������� ������ ������� ����� �������
 */
s32 timer_get_drift(void)
{
    return timer_tick_struct.sample;
#if 0    
    int sign = 1;

    if (timer_tick_struct.sample > 25600) {
	sign = -1;		/* ����� */
    }

   /* ns = ����� ����� * 12.5 + ������� * 10e9 */
    return (timer_tick_struct.sample * 25 / 2) * sign;
#endif    
}
