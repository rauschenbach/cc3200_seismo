#include <main.h>
#include <math.h>
#include "userfunc.h"
#include "pwm.h"
#include "timtick.h"
#include "gps.h"


/**
 * The PWM works based on the following settings:
 *     Timer reload interval -> determines the time period of one cycle
 *     Timer match value -> determines the duty cycle 
 *                          range [0, timer reload interval]
 * The computation of the timer reload interval and dutycycle granularity
 * is as described below:
 * Timer tick frequency = 80 Mhz = 80000000 cycles/sec
 * For a time period of 0.5 ms, 
 *      Timer reload interval = 80000000/2000 = 40000 cycles
 * To support steps of duty cycle update from [0, 4095]
 *      duty cycle granularity = ceil(40000/4095) = 10
 * Based on duty cycle granularity,
 *      New Timer reload interval = 4095*10 = 40950
 *      New time period = 0.5004375 ms
 *      Timer match value = (update[0, 4095] * duty cycle granularity)
 */
#define         TIMER_INTERVAL_RELOAD           40950
#define         DUTYCYCLE_GRANULARITY           10

#define         BUF_SIZE                        16
#define         TICKS_FOR_16_SEC                16384000
#define         TIM_MAX_ERROR                   5

/* ���� ������ - ����� � �������*/
#define         TEST_GPS                1
#undef          TEST_GPS


static void vPwmDataTask(void *);

/* ��������� ������ ��� */
int pwm_create_task(void)
{
    int ret;

    pwm_init();
    timer_counter_init();
    timer_pps_init();

    /* ������� ������, ������� ����� ����������� ��������� */
    ret = osi_TaskCreate(vPwmDataTask, "PwmTask", OSI_STACK_SIZE, NULL, PWM_TASK_PRIORITY, NULL);
    if (ret != OSI_OK) {
	PRINTF("Create PwmTask error!\r\n");
	return -1;
    }

    PRINTF("Create PwmTask OK!\r\n");
    return ret;
}

/**
 * ����� ������������� ��� ������ ��� ��������� ������� VXCO
 * PIN_64  / GPIO_09 - ������2, MUX = 3, GT_PWM05
 * TIMERA2 (TIMER B) GPIO 9 --> PWM_5
 */
void pwm_init(void)
{
    /* ����������� ����� ��� */
    MAP_PRCMPeripheralClkEnable(PRCM_TIMERA2, PRCM_RUN_MODE_CLK);

    /* Configure PIN_64 for TIMERPWM5 GT_PWM05 */
    MAP_PinTypeTimer(PIN_64, PIN_MODE_3);

/*    SetupTimerPWMMode(TIMERA2_BASE, TIMER_A, (TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PWM | TIMER_CFG_B_PWM), 1); */

    /* Set GPT - Configured Timer in PWM mode */
    MAP_TimerConfigure(TIMERA2_BASE, (TIMER_CFG_SPLIT_PAIR | TIMER_CFG_B_PWM));

    MAP_TimerPrescaleSet(TIMERA2_BASE, TIMER_B, 0);

    /* Inverting the timer output if required */
    MAP_TimerControlLevel(TIMERA2_BASE, TIMER_B, 1);

    /* Load value set to ~0.5 ms time period */
    MAP_TimerLoadSet(TIMERA2_BASE, TIMER_B, TIMER_INTERVAL_RELOAD);

    /* Match value set so as to output level 0 */
    MAP_TimerMatchSet(TIMERA2_BASE, TIMER_B, TIMER_INTERVAL_RELOAD);

    /* �������� ������  */
    MAP_TimerEnable(TIMERA2_BASE, TIMER_B);
}

/**
 * �������� ����� ��� - �������� ������ ��������
 * Level-0...4095
 */
void pwm_set_data(u16 level)
{
    // Match value is updated to reflect the new dutycycle settings
    MAP_TimerMatchSet(TIMERA2_BASE, TIMER_B, (level * DUTYCYCLE_GRANULARITY));
}


/**
 * ������ ���������� VCXO �����
 */
static void vPwmDataTask(void *par)
{
    int i, k, n = 0;
    int sum;
    long err[3] = { 100, 100, 100 };	// �������� ������ ����� �� ���� �����
    GPS_DATA_t st;
    u16 old, data = 1858;


    /* ���� ���� �� �������� Fix � �� ����� �������� ����� */
    do {
	status_get_gps_data(&st);
#if defined TEST_GPS
	gps_print_data();
#endif
	if (st.fix) {
	    n++;
	}
	status_self_test_on();	/* �������  */
	osi_Sleep(1000);
    } while (n < 10);

    timer_set_sec_ticks(status_get_gps_time() / TIMER_NS_DIVIDER);


    /* 1-� ������ - ������� ����������� ��� ����������� ��� 
     * ��������� ������, ���������� ����� 2 �����  
     * ������ ����� ��������� ���, ���� ���� 16-�� ������ ���
     */
    /* ���� 3-4 ������� ���� �� ��������� �������  */
    PRINTF("Wait 4 sec...\n");
    while (timer_get_num_tick() < 4) {
	status_self_test_on();	/* �������  */
	osi_Sleep(100);		/* �������� ���������� ������ ������� */
    }

    while (1) {
	old = data;
	pwm_set_data(old);

	PRINTF("PWM(%d)\n", old);

	// �������� ����� 16 ��������        
	sum = n = i = k = 0;
	while (i < BUF_SIZE) {
	    k = timer_get_num_tick();	// ����� ���������� PPS
	    if (k != n) {
		n = k;
		sum += timer_get_freq();
		PRINTF("-->%d\n", sum);
		i++;
	    }
	    status_self_test_on();	/* �������  */
	    osi_Sleep(1000);
	}

	data = get_pwm_ctrl_value(sum, old);

	err[2] = err[1];
	err[1] = err[0];
	err[0] = sum - TICKS_FOR_16_SEC;	/* ������� ������ - ��������� ��� ����� ������ ������? */


	PRINTF("Err(%d) Sum(%ld) Freq(%d)\n", err[0], sum, sum / BUF_SIZE);

	/* ������� ��� ����� �������  */
	if (ABS(err[0]) < TIM_MAX_ERROR && ABS(err[1]) < TIM_MAX_ERROR && ABS(err[2]) < TIM_MAX_ERROR) {
	    break;
	}
    }

    PRINTF("����������: err[0]=%d err[1]=%d err[2]=%d MAX=%d\n", ABS(err[0]), ABS(err[1]), ABS(err[2]), TIM_MAX_ERROR);
    timer_pps_exit();

    /* 2-� ������ - �������� ������ ����� ��������� ��� �������� ������ */
    timer_tick_init();

    /* 3-� ������ - ������� ���� ����� PPS � "�����"  ��������  */
    PRINTF("Shift phase...\n");
    timer_shift_phase();
    osi_Sleep(1000);

    /*  ��������� ��� ���������� */
    i = 3;
    while (i--) {
	PRINTF("phase %d\n", timer_get_sample());
	status_self_test_on();	/* �������  */
	osi_Sleep(1000);
    }


    /* ���������������� �����  */
    PRINTF("Sync time...\n");
    status_sync_time();

    /* ��������  */
    osi_Sleep(1000);

    /* ����� ���������� - ���� �������� ������ �����!!! */
    timer_time_ok();

    PRINTF("Stop simple timer, run precision timer...\n");
    timer_simple_stop();


    PRINTF("INFO: Exiting PWM\n");

#if 1
    n = 0;
    while (n++ < 10) {
	int time0, time1;
	char str0[32], str1[32];
	gps_print_data();
	time0 = status_get_gps_time() / TIMER_NS_DIVIDER;
	time1 = timer_get_sec_ticks();
	sec_to_str(time0, str0);
	sec_to_str(time1, str1);
	PRINTF("GPS: %s DSP: %s phase %d\n", str0, str1, timer_get_sample());
	status_self_test_on();	/* �������  */
	osi_Sleep(1000);
    }
#endif
    status_self_test_off();	/* ��������� */
    for (;;);
}
