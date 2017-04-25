#ifndef _TIMTICK_H
#define _TIMTICK_H

#include <hw_types.h>
#include <hw_memmap.h>
#include <hw_common_reg.h>
#include <hw_ints.h>
#include <hw_mcspi.h>
#include <gpio.h>
#include <pin.h>
#include <timer.h>
#include <rom.h>
#include <rom_map.h>
#include <utils.h>
#include <prcm.h>
#include <interrupt.h>
#include <systick.h>
#include <timer.h>
#include <hw_timer.h>
#include "my_defs.h"
#include "proto.h"
#include "main.h"


void timer_pps_init(void);
void timer_pps_exit(void);
void timer_counter_init(void);
void timer_tick_init(void);

u32  timer_get_freq(void);
u32  timer_get_sample(void);
u32  timer_get_num_tick(void);
s32  timer_get_drift(void);

void timer_simple_init(void);
void timer_simple_stop(void);

void timer_set_callback(void*);
void timer_del_callback(void);
bool timer_is_callback(void);
bool timer_is_sync(void);

void timer_shift_phase(void);
void timer_time_ok(void);

void timer_set_sec_ticks(s32);
u32  timer_get_sec_ticks(void);
s64  timer_get_msec_ticks(void);
s64  timer_get_long_time(void);



#endif /* timtick.h  */
