#ifndef _SENSOR_H
#define _SENSOR_H

#include "main.h"



/* ������ ������������� ��� ������ ������� */
typedef struct   {
        int x;
	int y;
	int z;
	int t; /* + ����������� */
} lsm303_data;



int sensor_create_task(int);
int sensor_init(void);

#endif /*  sensor.h */
