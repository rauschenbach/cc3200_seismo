#ifndef _PWM_H
#define _PWM_H

#include "main.h"

void pwm_init(void);
int pwm_create_task(void);
void pwm_set_data(u16);

#endif /* pwm.h */
