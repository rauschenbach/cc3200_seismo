/**
  * Модуль расчета углов. 
  * Алгоритм iNemo STMicroelectronics
  */

#include <math.h>
#include "userfunc.h"
#include "compass.h"

/* Абсолютное значение  */
#define FABS(x)  (x > 0.0? x : -x)
#define M_PI    (3.1415925635f)

static f32 AccSensorNorm(f32 *);
//static void MagSensorCalibration(void);



/**
 * Расчет углов вращений и поворота
 * На входе указатель на данные компаса, акселерометра
 * На выходе указатель на расчитаные углы в углах (радианах) * 10 -> в акселерометре
 */
void calc_angles(lsm303_data *acc, lsm303_data *mag)
{
	f32 fTemp;
	f32 fSinRoll, fCosRoll, fSinPitch, fCosPitch;
	f32 fTiltedX, fTiltedY;
	f32 fAcc[3]; /* Акселерометр */
	f32 fMag[3]; /* Компас */
	
        
	fAcc[0] = (f32)acc->x / 100.0;
	fAcc[1] = (f32)acc->y / 100.0;
	fAcc[2] = (f32)acc->z / 100.0;


	fMag[0] = (f32)mag->x;// / 100.0;
	fMag[1] = (f32)mag->y;// / 100.0;
	fMag[2] = (f32)mag->z;// / 100.0;

	/* Нормализовать векторы ускорений */
        fTemp = AccSensorNorm(fAcc);

	/* Вычисляем промежуточные параметры матрицы поворота */
	fSinRoll = fAcc[1] / sqrt(pow(fAcc[1], 2) + pow(fAcc[2], 2));
	fCosRoll = sqrt(1.0 - fSinRoll * fSinRoll);
	fSinPitch = -fAcc[0] * fTemp;
	fCosPitch = sqrt(1.0 - fSinPitch * fSinPitch);
	
	/* Матрица поворота -> к магнитному полю для компонентов X и Y */
	fTiltedX = fMag[0] * fCosPitch + fMag[2] * fSinPitch;
	fTiltedY = fMag[0] * fSinRoll * fSinPitch + fMag[1] * fCosRoll - fMag[2] * fSinRoll * fCosPitch;

	/* Получаем угол head  */
        fTemp = -atan2(fTiltedY, fTiltedX) * 180.0 / M_PI;
        acc->z = (int)(fTemp * 10.0);

	/* и углы roll и pitch */
	fTemp = atan2(fSinRoll, fCosRoll) * 180 / M_PI;
        acc->x = (int)(fTemp * 10.0);        
        
	fTemp = atan2(fSinPitch, fCosPitch) * 180 / M_PI;
        acc->y = (int)(fTemp * 10.0);                
}

/* Вариант Neo от STM */
void CalcAngles(f32 *pfMagXYZ, f32 *pfAccXYZ, f32 *pfRPH)
{
	f32 fNormAcc;
	f32 fSinRoll, fCosRoll, fSinPitch, fCosPitch;
	f32 fTiltedX, fTiltedY;
	f32 fAcc[3];
	int i;

	for (i = 0; i < 3; i++) {
		fAcc[i] = pfAccXYZ[i] / 100.0;
	}

	/* Нормализовать векторы ускорений */
        fNormAcc = AccSensorNorm(fAcc);

	/* Вычисляем промежуточные параметры матрицы поворота */
	fSinRoll = fAcc[1] / sqrt(pow(fAcc[1], 2) + pow(fAcc[2], 2));
	fCosRoll = sqrt(1.0 - fSinRoll * fSinRoll);
	fSinPitch = -fAcc[0] * fNormAcc;
	fCosPitch = sqrt(1.0 - fSinPitch * fSinPitch);
	
	/* Матрица поворота -> к магнитному полю для компонентов X и Y */
	fTiltedX = pfMagXYZ[0] * fCosPitch + pfMagXYZ[2] * fSinPitch;
	fTiltedY = pfMagXYZ[0] * fSinRoll * fSinPitch + pfMagXYZ[1] * fCosRoll - pfMagXYZ[2] * fSinRoll * fCosPitch;

	/* Получаем угол head  */
	pfRPH[2] = -atan2(fTiltedY, fTiltedX) * 180 / M_PI;

	/* и углы roll и pitch */
	pfRPH[0] = atan2(fSinRoll, fCosRoll) * 180 / M_PI;
	pfRPH[1] = atan2(fSinPitch, fCosPitch) * 180 / M_PI;
}

/**
 * Нормализуем векторы ускорений, для того, чтобы считать от единиц G
 */
static f32 AccSensorNorm(f32 *pfAccXYZ)
{
         /* Функция обратного кв. корня */
	return     inv_sqrt(pow(pfAccXYZ[0], 2) + pow(pfAccXYZ[1], 2) + pow(pfAccXYZ[2], 2));
}
