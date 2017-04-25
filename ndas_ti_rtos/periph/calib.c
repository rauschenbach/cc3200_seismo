/**********************************************************************************************************  
 * Вывод ШИМ на калибровку
 ***********************************************************************************************************/
#include <main.h>
#include "calib.h"
#include "status.h"

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
 * To support steps of duty cycle update from [0, 255]
 *      duty cycle granularity = ceil(40000/255) = 157
 * Based on duty cycle granularity,
 *      New Timer reload interval = 255*157 = 40035
 *      New time period = 0.5004375 ms
 *      Timer match value = (update[0, 255] * duty cycle granularity)
 */
//#define TIMER_INTERVAL_RELOAD   40035	/* =(255*157) */
//#define DUTYCYCLE_GRANULARITY   157


#define TIMER_INTERVAL_RELOAD   80000000	/* =(255*157) */
#define DUTYCYCLE_GRANULARITY   157



/* Калибровочный ШИМ */
#define         CALIB_TIMER_FREQ              80000000
#define         CALIB_TIMER_MODE              PIN_MODE_9
#define         CALIB_TIMER_PIN               PIN_21
#define         CALIB_TIMER_PRCM              PRCM_TIMERA1
#define         CALIB_TIMER_BASE              TIMERA1_BASE
#define         CALIB_TIMER_INT               INT_TIMERA1A




/**
 * Здесь инициализация ШИМ режима для изменения частоты VXCO
 * PIN_64  / GPIO_09 - Таймер2, MUX = 3, GT_PWM05
 * TIMERA1 (TIMER B) GPIO 9 --> PWM_5
 */
void calib_init(void)
{
    /* Затактируем вывод ШИМ */
    MAP_PRCMPeripheralClkEnable(CALIB_TIMER_PRCM, PRCM_RUN_MODE_CLK);

    /* Configure PIN_21 for TIMERPWM2 GT_PWM02 */
    MAP_PinTypeTimer(CALIB_TIMER_PIN, CALIB_TIMER_MODE);

    /* Set GPT - Configured Timer in PWM mode */
    MAP_TimerConfigure(CALIB_TIMER_BASE, (TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PWM));


    MAP_TimerPrescaleSet(CALIB_TIMER_BASE, TIMER_A, 0);

    /* Inverting the timer output if required */
    MAP_TimerControlLevel(CALIB_TIMER_BASE, TIMER_A, 1);

    /* Load value set to ~0.5 ms time period */
    MAP_TimerLoadSet(CALIB_TIMER_BASE, TIMER_A, TIMER_INTERVAL_RELOAD);

    /* Match value set so as to output level 0 */
    MAP_TimerMatchSet(CALIB_TIMER_BASE, TIMER_A, TIMER_INTERVAL_RELOAD);

    /* Разрешим таймер  */
    MAP_TimerEnable(CALIB_TIMER_BASE, TIMER_A);
    
    MAP_TimerMatchSet(CALIB_TIMER_BASE, TIMER_A, TIMER_INTERVAL_RELOAD/100000);    
}

/**
 * Изменить режим ШИМ - изменяем ширину импульса
 */
void calib_set_data(u8 level)
{
    // Match value is updated to reflect the new dutycycle settings
    MAP_TimerMatchSet(CALIB_TIMER_BASE, TIMER_A, (level * DUTYCYCLE_GRANULARITY));
}

