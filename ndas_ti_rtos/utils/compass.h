#ifndef _COMPASS_H
#define _COMPASS_H

#include "my_defs.h"
#include "sensor.h"

void MagSensorCalibration(void);
void CalcAngles(f32 *, f32 *, f32 *);
void calc_angles(lsm303_data *, lsm303_data *);

#endif /* compass.h */
