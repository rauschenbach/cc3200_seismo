/************************************************************************************************
 *	�������, ����������� �� ���� TWI (I2C). �������� ����� ������ �� ��������, �������
 *      � �������� �� STM ������������� ������ �� 1 ��� 2 ������
 * 	��������� ������������ ��� �� ��� � ������� STMf401-Discovery
 * 	��� ������� ���������� ��� ������� ����� ������� � ��������
 *      ��������� ����� ���������� ��� ������� �������� ���������� ������� (�� �� I2C)
 ***********************************************************************************************/
#include "compass.h"
#include "sensor.h"
#include "status.h"
#include "sdreader.h"
#include "twi.h"
#include "adc.h"

/* ���� ������ - ����� � �������*/
#define         TEST_SENSOR     1
#undef          TEST_SENSOR

/* �������� ������������� ������ */
#define         SENSOR_TASK_SLEEP       (1000)


/* ������ �������� �� ���� I2C */
#define 	LSM303_ACC_ADDR		0x19	/* ������������ � ������� ������������ �����. ���� ���������� */
#define 	LSM303_COMP_ADDR	0x1E	/* ������  */
#define 	BMP085_ADDR		0x77	/* ������ ����������� � �������� */
#define		HTS221_ADDR		0x5F	/* ������ ��������� */


#define 	ACC_SENSITIVITY_2G     (1)	/*!< accelerometer sensitivity with 2 g full scale [LSB/mg] */
#define 	ACC_SENSITIVITY_4G     (2)	/*!< accelerometer sensitivity with 4 g full scale [LSB/mg] */
#define 	ACC_SENSITIVITY_8G     (4)	/*!< accelerometer sensitivity with 8 g full scale [LSB/mg] */
#define 	ACC_SENSITIVITY_16G    (12)	/*!< accelerometer sensitivity with 12 g full scale [LSB/mg] */


#define 	ACC_FULLSCALE_2G            ((uint8_t) 0x00)	/*!< �2 g */
#define 	ACC_FULLSCALE_4G            ((uint8_t) 0x10)	/*!< �4 g */
#define 	ACC_FULLSCALE_8G            ((uint8_t) 0x20)	/*!< �8 g */
#define 	ACC_FULLSCALE_16G           ((uint8_t) 0x30)	/*!< �16 g */


/* �������� �������������  */
#define 	CTRL_REG1_A		0x20
#define 	CTRL_REG4_A		0x23
#define         STATUS_REG_A            0x27	/* Status register acceleration */
#define 	OUT_X_L_A		0x28
#define 	OUT_X_H_A		0x29
#define 	OUT_Y_L_A		0x2a
#define 	OUT_Y_H_A		0x2b
#define 	OUT_Z_L_A		0x2c
#define 	OUT_Z_H_A		0x2d

#define  	LSM303_FS_1_3_GA               ((uint8_t) 0x20)	/*!< Full scale = �1.3 Gauss */
#define  	LSM303_FS_1_9_GA               ((uint8_t) 0x40)	/*!< Full scale = �1.9 Gauss */
#define  	LSM303_FS_2_5_GA               ((uint8_t) 0x60)	/*!< Full scale = �2.5 Gauss */
#define 	LSM303_FS_4_0_GA               ((uint8_t) 0x80)	/*!< Full scale = �4.0 Gauss */
#define  	LSM303_FS_4_7_GA               ((uint8_t) 0xA0)	/*!< Full scale = �4.7 Gauss */
#define 	LSM303_FS_5_6_GA               ((uint8_t) 0xC0)	/*!< Full scale = �5.6 Gauss */
#define  	LSM303_FS_8_1_GA               ((uint8_t) 0xE0)	/*!< Full scale = �8.1 Gauss */

/* �������� ������� */
#define 	CRA_REG_M		0x00	/* data rate reg */
#define 	CRB_REG_M		0x01	/* GAIN reg - �������� */
#define		MR_REG_M		0x02	/* Mode sel reg - ����� ������ */

#define		OUT_X_H_M		0x03
#define		OUT_X_L_M		0x04
#define		OUT_Z_H_M		0x05
#define		OUT_Z_L_M		0x06
#define		OUT_Y_H_M		0x07
#define		OUT_Y_L_M		0x08
#define         SR_REG_M                0x09	/* Status Register magnetic field */

/* �������� ���������� �� ���������� ������� */
#define 	TEMP_OUT_H_M 		0x31
#define 	TEMP_OUT_L_M 		0x32

/* ��� ������� */
#define 	LSM303_TWI_COMP_HI	6
#define 	LSM303_TWI_COMP_LO      7



/* ������ ��������� */
#define		HUM_WHO_AM_I		0x0f
#define		HUM_AM_I_HTS221		0xbc

#define		HUM_AV_CONF		0x10
#define		HUM_CTRL_REG1  		0x20
#define		HUM_CTRL_REG2           0x21
#define		HUM_CTRL_REG3           0x22
#define		HUM_STATUS_REG          0x27
#define		HUM_OUT_L               0x28
#define		HUM_OUT_H               0x29


/* ���������� ��� */
#define		NUM_ADC_CHAN		3
#define 	NUM_ADC_SAMPLES		8


/* ������������� ������������ ���������� BMP185. ����� ���� �������� � eeprom ������� */
static struct bmp085_calibr_pars {
    s16 ac1;
    s16 ac2;

    s16 ac3;
    u16 ac4;

    u16 ac5;
    u16 ac6;

    s16 b1;
    s16 b2;

    s16 mb;
    s16 mc;

    s16 md;
    s16 rsvd;
} bmp085_regs;


/* ������������� ������������ ���������� HTS221 */
static struct hts221_calibr_pars {
    int x0;
    int x1;
    int y0;
    int y1;
} hts221_regs;



/* ������������� ������ ������ �������� �� I2C */
static void vSensorTask(void *);

/* ������������ */
static int lsm303_acc_start(void);
static u8 lsm303_get_acc_status(void);
static int lsm303_get_acc_data(lsm303_data *);

/* ������ */
static int lsm303_comp_start(void);
static u8 lsm303_get_comp_status(void);
static int lsm303_get_comp_data(lsm303_data *);

/* ������ ����������� � �������� */
static int bmp085_init(void);
static int bmp085_data_get(int *, int *);

/* ������ ��������� */
static int hts221_init(void);
static int hts221_data_get(int *);
static u8 hts221_get_hum_status(void);

/* ��� �� �����  */
static int adc_init(void);
static int adc_data_get(void);

/**
 * �������� ������, ������� ������ ����� �� lsm303
 */
int sensor_create_task(int par)
{
    int ret;

    /* ������� ������, ������� ����� �������� ������ �������� */
    ret = osi_TaskCreate(vSensorTask, "SensorTask", OSI_STACK_SIZE, &par, SENSOR_TASK_PRIORITY, NULL);
    if (ret != OSI_OK) {
	PRINTF("ERROR: Create SensorTask error!\r\n");
	return -1;
    }

    PRINTF("INFO: Create SensorTask OK!\r\n");
    return ret;
}


/**
 * ������ ������ ��������. ��� � 10 ������ ����������.
 */
static void vSensorTask(void *par)
{
    static SENSOR_DATE_t sens;	/* 16 ���� �������� */
    TEST_STATE_t status;	/* 4 ����� ����������� ������������ */
    lsm303_data comp, acc;
    int temp, press;
    u8 res;


    while (1) {

	/* ���� */
	if (Osi_twi_obj_lock() == OSI_OK) {
          
        /* �������� ��������� �������� - ��� ������������ �������������� */
            status_get_test_state(&status);
          

	    /* �������� ������ �������� (����� ���� ���) */
	    status_get_sensor_date(&sens);


	     /* ���������� - ��������� ����� */
            sens.voltage = adc_data_get() * 3;

	    /* �� ���������� SENSOR_DATA_t - ����������, ��������� ��� ��� ���! */
	    if (status.press_ok && bmp085_data_get(&temp, &press) == SUCCESS) {
		sens.temper = temp;
		sens.press = press;
#if defined TEST_SENSOR
		PRINTF("Temp: %2.01f �C, Press: %d kPa\r\n", (f32) sens.temper / 10., sens.press);
#endif
	    }


	    /* ���� ���� ������� �������, �������� � ��� ������ � ������� ��������� ������� */
	    if (status.acc_ok) {
		/*  ���� �� �������� - ������������ 0! */
		res = lsm303_get_acc_status();
		if (res & 8) {
		    lsm303_get_acc_data(&acc);
		}
	    }

	    /* ������ ������ ����� ������ ������������� */
	    if (status.comp_ok) {
		res = lsm303_get_comp_status();
		if (res & 1) {
		    lsm303_get_comp_data(&comp);

		    /* ��������� ���� */
		    calc_angles(&acc, &comp);
		    sens.pitch = acc.x;
		    sens.roll = acc.y;
		    sens.head = acc.z;
#if defined TEST_SENSOR
		    {
			int x, y, z;
			x = (acc.x / 10);
			y = (acc.y / 10);
			z = (acc.z / 10);
			PRINTF("P = %d�, R = %d�, Head = %d�\r\n", x, y, z);
		    }
#endif
		}
	    }

	    /* ������ ��������� */
	    if (status.hum_ok) {
		res = hts221_get_hum_status();
		/* ������ ������ */
		if (res & 2) {
		    res = hts221_data_get(&temp);
		    sens.humid = temp;
#if defined TEST_SENSOR
		    PRINTF("Hum = %d%%\r\n", temp);
#endif
		}
	    }


	    status_set_sensor_date(&sens);

 	    /* ������ ������ ��������  */	
	    log_change_adc_header(&sens);
            
           /* �������� USB ������ - � ������ NORMAL � ��� ������ SD ����� ���������� � ���������� */ 
	    if (get_sd_state() == SD_CARD_TO_CR && get_sd_cable() == 0) {
		board_reset();	/* ����� ����� */
	    }
            

	    /* ������ ���������� */
	    Osi_twi_obj_unlock();
	}

	osi_Sleep(SENSOR_TASK_SLEEP);
    }
}


/**
 * ������������� ��������
 */
int sensor_init(void)
{
    u8 res = 0;


    adc_init();

    /* ��������� ������������  */
    if (lsm303_acc_start() == SUCCESS) {
	res |= TEST_STATE_ACCEL_OK;
    }

    /* ��������� ������ */
    if (lsm303_comp_start() == SUCCESS) {
	res |= TEST_STATE_COMP_OK;
    }
    if (hts221_init() == SUCCESS) {
	res |= TEST_STATE_HUMID_OK;
    }


    /* ��������� ������ �������� */
    if (bmp085_init() == SUCCESS) {
	res |= TEST_STATE_PRESS_OK;
    }


    
    return (int) res;
}

/**
 * ������������� ��� �� ����� 58 59 � 60
 */
int adc_init(void)
{
#ifdef CC3200_ES_1_2_1
    // Enable ADC clocks.###IMPORTANT###Need to be removed for PG 1.32
    HWREG(GPRCM_BASE + GPRCM_O_ADC_CLK_CONFIG) = 0x00000043;
    HWREG(ADC_BASE + ADC_O_ADC_CTRL) = 0x00000004;
    HWREG(ADC_BASE + ADC_O_ADC_SPARE0) = 0x00000100;
    HWREG(ADC_BASE + ADC_O_ADC_SPARE1) = 0x0355AA00;
#endif
    /* Pinmux for the selected ADC input pin */
    MAP_PinTypeADC(PIN_58, PIN_MODE_255);
    MAP_PinTypeADC(PIN_59, PIN_MODE_255);
    MAP_PinTypeADC(PIN_60, PIN_MODE_255);

    /* Configure ADC timer which is used to timestamp the ADC data samples */
    MAP_ADCTimerConfig(ADC_BASE, 2 ^ 17);
    /* Enable ADC timer which is used to timestamp the ADC data samples */
    MAP_ADCTimerEnable(ADC_BASE);
    /* Enable ADC module */
    MAP_ADCEnable(ADC_BASE);
    return 0;
}



int adc_data_get(void)
{
    u32 sample = 0;
    int i = 0;
    int data;

    MAP_ADCChannelEnable(ADC_BASE, ADC_CH_1);	/* �������� ����� */

	//UART_PRINT("\n\rTotal no of 32 bit ADC data printed :4096 \n\r");
	//UART_PRINT("\n\rADC data format:\n\r");
	//UART_PRINT("\n\rbits[13:2] : ADC sample\n\r");
	//UART_PRINT("\n\rbits[31:14]: Time stamp of ADC sample \n\r");
        while (i < NUM_ADC_SAMPLES + 4) {
	    if (MAP_ADCFIFOLvlGet(ADC_BASE, ADC_CH_1)) {
                u32 temp = MAP_ADCFIFORead(ADC_BASE, ADC_CH_1); /* ������ ��� */                

		/* ������� ������ ����� ������� 4-�� ��������� */
                if(i > 4) {                
                  temp = (temp >> 2) & 0xfff;
                  sample += temp;                  /* ������� ������ */ 
		}
              i++;                
	    }
	} 
        
        sample /=  NUM_ADC_SAMPLES;                /* ������� ������� */      
        data = sample * 1400 / 4096;
       
	/* ��������� ����� */
	MAP_ADCChannelDisable(ADC_BASE, ADC_CH_1);
//    PRINTF("Voltage of chan1: %ld\n", data);
      return data;
}


/**
 * ������������� ����������. ������ ���������
 */
int hts221_init(void)
{
    int res = FAILURE;
    u8 data0, data1;
    volatile int temp;


    do {
	/* ������ ������ �����? */
	res = twi_read_reg_value(HTS221_ADDR, HUM_WHO_AM_I, &data0);
	if (res != SUCCESS && data0 != HUM_AM_I_HTS221) {
	    PRINTF("Error hts221 init\r\n");
	    break;
	}

	/* ��������� EEPROM � ��������� ������������ � 30h ������ - �� ����� ������ ������!!! */
	res = twi_read_reg_value(HTS221_ADDR, 0x30, &data0);
	if (res != SUCCESS) {
	    PRINTF("Error get hts221 calibr. coefs\r\n");
	    break;
	}
	hts221_regs.y0 = (u16) (data0 / 2);

	res = twi_read_reg_value(HTS221_ADDR, 0x31, &data0);
	if (res != SUCCESS) {
	    PRINTF("Error hts221 init\r\n");
	    break;
	}
	hts221_regs.y1 = (u16) (data0 / 2);


	// x0 LO
	res = twi_read_reg_value(HTS221_ADDR, 0x36, &data0);
	if (res != SUCCESS) {
	    PRINTF("Error get hts221 calibr. coefs\r\n");
	    break;
	}
	// X0 HI
	res = twi_read_reg_value(HTS221_ADDR, 0x37, &data1);
	if (res != SUCCESS) {
	    PRINTF("Error get hts221 calibr. coefs\r\n");
	    break;
	}
	hts221_regs.x0 = (int) (((u16) data1 << 8) | data0);


	// x1 LO
	res = twi_read_reg_value(HTS221_ADDR, 0x3a, &data0);
	if (res != SUCCESS) {
	    PRINTF("Error get hts221 calibr. coefs\r\n");
	    break;
	}
	// X1 HI
	res = twi_read_reg_value(HTS221_ADDR, 0x3b, &data1);
	if (res != SUCCESS) {
	    PRINTF("Error get hts221 calibr. coefs\r\n");
	    break;
	}

	temp = (((u16) data1 << 8) | data0);
	hts221_regs.x1 = temp;

	/* ������������� */
	if (hts221_regs.x1 == hts221_regs.x0 || hts221_regs.y0 == hts221_regs.y1) {
	    PRINTF("Error counting hts221 coefs\r\n");
	    break;
	}



	/* ������� AV_CONF - ��. ���������� �� 64 ������ */
	data0 = 0x04;
	res = twi_write_reg_value(HTS221_ADDR, HUM_AV_CONF, data0);
	if (res != SUCCESS) {
	    PRINTF("Error set humidity AV_CONF\r\n");
	    break;
	}


	/* REG1: �������� continius mode 12 �� */
	data0 = 0x85 + 2;
	res = twi_write_reg_value(HTS221_ADDR, HUM_CTRL_REG1, data0);
	if (res != SUCCESS) {
	    PRINTF("Error set humidity REG1\r\n");
	    break;
	}


	/* REG2: ���������� �����, ��� �������������!!!  */
	data0 = 0x01;
	res = twi_write_reg_value(HTS221_ADDR, HUM_CTRL_REG2, data0);
	if (res != SUCCESS) {
	    PRINTF("Error set humidity REG2\r\n");
	    break;
	}


	/* REG3: Data ready �� ���������� */
	data0 = 0x00;
	res = twi_write_reg_value(HTS221_ADDR, HUM_CTRL_REG3, data0);
	if (res != SUCCESS) {
	    PRINTF("Error set humidity REG3\r\n");
	    break;
	}
    } while (0);
    return res;
}


/**
 * �������� ������ ��������� � �������� � �������� ���������
 */
int hts221_data_get(int *hum)
{
    int res = FAILURE;
    u8 lo, hi;
    int temp;

    do {
	if (NULL == hum) {
	    break;
	}

	/* ������ L �������� */
	res = twi_read_reg_value(HTS221_ADDR, HUM_OUT_L, &lo);
	if (res != SUCCESS) {
	    break;
	}

	/* ������ Hi */
	res = twi_read_reg_value(HTS221_ADDR, HUM_OUT_H, &hi);
	if (res != SUCCESS) {
	    break;
	}
	temp = ((u16) hi << 8) | lo;

	if (hts221_regs.x1 != hts221_regs.x0) {
	    *hum = ((temp - hts221_regs.x0) * (hts221_regs.y1 - hts221_regs.y0)) /
		(hts221_regs.x1 - hts221_regs.x0) + hts221_regs.y0;
	}

    } while (0);
    return res;
}


/* Read STATUS register */
u8 hts221_get_hum_status(void)
{
    u8 temp;
    int res;

    res = twi_read_reg_value(HTS221_ADDR, HUM_STATUS_REG, &temp);
    if (res == SUCCESS) {
	return temp;
    }
    return 0;
}


/**
 * ������������� ����������. ������ ����������� � ��������
 */
int bmp085_init(void)
{
    int res = FAILURE;
    do {
	/* ������ pars ��� ���������� � 0 */
	memset(&bmp085_regs, 0, sizeof(struct bmp085_calibr_pars));
	/* ������ ������������ �� ������� �����������  - 24 ����� */
	res = twi_read_block_data(BMP085_ADDR, 0xAA, (u8 *) & bmp085_regs, 24);
	if (res != SUCCESS) {
	    PRINTF("Error get bmp085 calibr. coefs\r\n");
	    break;
	}

	/* �� ��������� ����� ������� - �������� ����� */
	bmp085_regs.ac1 = byteswap2(bmp085_regs.ac1);
	bmp085_regs.ac2 = byteswap2(bmp085_regs.ac2);
	bmp085_regs.ac3 = byteswap2(bmp085_regs.ac3);
	bmp085_regs.ac4 = byteswap2(bmp085_regs.ac4);
	bmp085_regs.ac5 = byteswap2(bmp085_regs.ac5);
	bmp085_regs.ac6 = byteswap2(bmp085_regs.ac6);
	bmp085_regs.b1 = byteswap2(bmp085_regs.b1);
	bmp085_regs.b2 = byteswap2(bmp085_regs.b2);
	bmp085_regs.mb = byteswap2(bmp085_regs.mb);
	bmp085_regs.mc = byteswap2(bmp085_regs.mc);
	bmp085_regs.md = byteswap2(bmp085_regs.md);
	PRINTF("--------------------------------\r\n");
	PRINTF("BMP085 Coefs:\r\n");
	PRINTF("AC1: %d, AC2: %d, AC3: %d, AC4: %u\r\n", bmp085_regs.ac1, bmp085_regs.ac2, bmp085_regs.ac3,
	       bmp085_regs.ac4);
	PRINTF("AC5: %u, AC6: %u,  B1: %d,  B2: %u\r\n", bmp085_regs.ac5, bmp085_regs.ac6, bmp085_regs.b1,
	       bmp085_regs.b2);
	PRINTF("MB: %d, MC: %d, MD: %d,  RSVD: %u\r\n", bmp085_regs.mb, bmp085_regs.mc, bmp085_regs.md,
	       bmp085_regs.rsvd);
	PRINTF("--------------------------------\r\n");
	res = SUCCESS;
    } while (0);
    return res;
}


/**
 * �������� ������ � ������� �������� � ����������� 
 * NB - ����� ������ ������ ���� �������� �� ����� 5 ��, ����� ����� ����� �������!
 */
int bmp085_data_get(int *temp, int *press)
{
    u32 ut;
    long x1, x2, b5;
    s32 x3, b3, b6, p;
    u32 b4, b7;
    u16 up;
    u8 data;
    int res = FAILURE;
    do {
	/* � ������ ������ ���������� ��� ������ �� �������� */
	*temp = -1;
	*press = -1;
	/* ����� ��������� �����������! */
	data = 0x2e;
	res = twi_write_reg_value(BMP085_ADDR, 0xf4, data);
	if (res != SUCCESS) {
	    PRINTF("Error get temp. 1 step\r\n");
	    break;
	}

	/* ���� �� ����� 5 �� � ������ 2 ����� ����������� �� F6 � F7 */
	delay_ms(10);
	res = twi_read_block_data(BMP085_ADDR, 0xF6, (u8 *) & ut, 2);
	if (res != SUCCESS) {
	    PRINTF("Error get temp. 2 step\r\n");
	    break;
	}
	/* ���������� ����� ����������� */
	ut = byteswap2(ut);
	/* ����� ��������� ��������! */
	data = 0x34;
	res = twi_write_reg_value(BMP085_ADDR, 0xf4, data);
	if (res != SUCCESS) {
	    PRINTF("Error get press. 1 step\r\n");
	    break;
	}
	/* ���� �� ����� 5 �� � ������ �� F6 � F7 */
	delay_ms(10);
	res = twi_read_block_data(BMP085_ADDR, 0xF6, (u8 *) & up, 2);
	if (res != SUCCESS) {
	    PRINTF("Error get press. 2 step\r\n");
	    break;
	}
	/* ���������� ����� �������� */
	up = byteswap2(up);

	/* ����� �������� �� ����� ���� */
	if (up == 0 || ut == 0 || up == 0xffff || ut == 0xffff) {
	    PRINTF("Data get error\r\n");
	    break;
	}

	/* ����� ��� �� �������� Bosch ������ ����������� */
	x1 = (ut - bmp085_regs.ac6) * bmp085_regs.ac5 >> 15;
	x2 = (bmp085_regs.mc << 11) / (x1 + bmp085_regs.md);
	b5 = (x1 + x2);

	/* ����������� * 10 */
	if (temp != NULL)
	    *temp = (b5 + 8) >> 4;

	/* ������ �������� */
	b6 = b5 - 4000;
	x1 = (bmp085_regs.b2 * (b6 * b6 >> 12)) >> 11;
	x2 = bmp085_regs.ac2 * b6 >> 11;
	x3 = x1 + x2;
	b3 = ((int32_t) bmp085_regs.ac1 * 4 + x3 + 2) >> 2;
	x1 = bmp085_regs.ac3 * b6 >> 13;
	x2 = (bmp085_regs.b1 * (b6 * b6 >> 12)) >> 16;
	x3 = ((x1 + x2) + 2) >> 2;
	b4 = (bmp085_regs.ac4 * (uint32_t) (x3 + 32768)) >> 15;
	b7 = ((uint32_t) up - b3) * 50000;
	p = ((b7 < 0x80000000) ? ((b7 * 2) / b4) : ((b7 / b4) * 2));
	x1 = (p >> 8) * (p >> 8);
	x1 = (x1 * 3038) >> 16;
	x2 = (-7357 * p) >> 16;
	if (press != NULL)
	    *press = p + ((x1 + x2 + 3791) >> 4);
	res = SUCCESS;
    } while (0);
    return res;
}



/* ����� ������������� */
int lsm303_acc_start(void)
{
    int res = FAILURE;
    u8 axis = 0x17;		/*  X+Y+Z enabled, normal mode 1 Hz */
    u8 mode = 0x08;		/* Little endian, scale +- 2G, Hi res */
    u8 data;
    do {
	/* ��������� ������������ - �������� ��� */
	res = twi_write_reg_value(LSM303_ACC_ADDR, CTRL_REG1_A, axis);
	if (res != SUCCESS) {
	    PRINTF("Error set acc axis\r\n");
	    break;
	}

	/* ��������� ��� ��������? ��� ��� ������... */
	res = twi_read_reg_value(LSM303_ACC_ADDR, CTRL_REG1_A, &data);
	if (res != SUCCESS && data != axis) {
	    PRINTF("Error check acc axis settings. Write 0x%02X read 0x%02X\r\n", axis, data);
	    break;
	}

	/* ����� � ����� ������ */
	if (twi_write_reg_value(LSM303_ACC_ADDR, CTRL_REG4_A, mode) != SUCCESS) {
	    PRINTF("Error set acc mode\r\n");
	    break;
	}

	/* ��������� ��� ��������? ��� ��� ������... */
	res = twi_read_reg_value(LSM303_ACC_ADDR, CTRL_REG4_A, &data);
	if (res != SUCCESS && data != mode) {
	    PRINTF("Error check acc mode settings. Write 0x%02X read 0x%02X\r\n", mode, data);
	    break;
	}
	PRINTF("Mode and axis OK: X+Y+Z enabled, normal mode 1 Hz, Little endian, scale +- 2G, Low res\r\n");
    }
    while (0);
    return res;
}


/**
 * �������� ������ ������������� - ���������� �� ��������, ��� ���������� ���!
 */
int lsm303_get_acc_data(lsm303_data * acc_val)
{
    u8 ctrlx[2];
    u8 lo, hi;
    s16 x, y, z;
    int res = FAILURE;
    int sens = ACC_SENSITIVITY_2G;
    do {
	if (acc_val == NULL) {
	    break;
	}


	/* ������ X �������� */
	twi_read_reg_value(LSM303_ACC_ADDR, OUT_X_L_A, &lo);
	twi_read_reg_value(LSM303_ACC_ADDR, OUT_X_H_A, &hi);
	x = ((s16) ((u16) hi << 8) + lo) >> 4;
	/* ������ Y �������� */
	twi_read_reg_value(LSM303_ACC_ADDR, OUT_Y_L_A, &lo);
	twi_read_reg_value(LSM303_ACC_ADDR, OUT_Y_H_A, &hi);
	y = ((s16) ((u16) hi << 8) + lo) >> 4;
	/* ������ Z */
	twi_read_reg_value(LSM303_ACC_ADDR, OUT_Z_L_A, &lo);
	twi_read_reg_value(LSM303_ACC_ADDR, OUT_Z_H_A, &hi);
	z = ((s16) ((u16) hi << 8) + lo) >> 4;
	/* ������ 2 ����� �� �������� REG4A */
	twi_read_block_data(LSM303_ACC_ADDR, CTRL_REG4_A, ctrlx, 2);
	/* Little Endian Mode or FIFO mode */
	if (ctrlx[1] & 0x40) {
	    sens = 4.0;
	} else {
	    /* normal mode. switch the sensitivity value set in the CRTL4 */
	    switch (ctrlx[0] & 0x30) {
	    case ACC_FULLSCALE_2G:
		sens = ACC_SENSITIVITY_2G;
		break;
	    case ACC_FULLSCALE_4G:
		sens = ACC_SENSITIVITY_4G;
		break;
	    case ACC_FULLSCALE_8G:
		sens = ACC_SENSITIVITY_8G;
		break;
	    case ACC_FULLSCALE_16G:
		sens = ACC_SENSITIVITY_16G;
		break;
	    }
	}

	/* �������� �������� mG �� ���� */
	acc_val->x = (int) x *sens;
	acc_val->y = (int) y *sens;
	acc_val->z = (int) z *sens;
	res = SUCCESS;
    } while (0);
    return res;
}

/* Read Acc STATUS register  - 0 ������������ ��� �� ��������� ��� ��� ���������� !!! */
static u8 lsm303_get_acc_status(void)
{
    u8 temp;
    int res;
    res = twi_read_reg_value(LSM303_ACC_ADDR, STATUS_REG_A, &temp);
    if (res == SUCCESS) {
	return temp;
    }
    return 0;
}

/**
 * ����� �������
 */
int lsm303_comp_start(void)
{
    int res = FAILURE;
    u8 rate = 0x04;		/* ������� 1.5 ��, + ������������� ������ Off */
    u8 gain = 0x20;		/* gn2=0 gn1=0 gn0=1 */
    u8 mode = 0x00;		/* Continiuos mode - ���������, ��� ��� ���������� ����� */
    u8 data;
    do {

	/* ��������� ������� */
	res = twi_write_reg_value(LSM303_COMP_ADDR, CRA_REG_M, rate);
	if (res != SUCCESS) {
	    PRINTF("Error set comp rate\r\n");
	    break;
	}

	/* ��������� ��� ��������? ��� ��� ������... */
	res = twi_read_reg_value(LSM303_COMP_ADDR, CRA_REG_M, &data);
	if (res != SUCCESS && data != rate) {
	    PRINTF("Error check comp rate settings, data error. Write 0x%02X read 0x%02X\r\n", rate, data);
	    break;
	}

	/* ����� ������� */
	res = twi_write_reg_value(LSM303_COMP_ADDR, CRB_REG_M, gain);
	if (res != SUCCESS) {
	    PRINTF("Error set comp rate\r\n");
	    break;
	}

	/* ��������� ��� ��������? ��� ��� ������... */
	res = twi_read_reg_value(LSM303_COMP_ADDR, CRB_REG_M, &data);
	if (res != SUCCESS && data != gain) {
	    PRINTF("Error check comp gain settings, gain error. Write 0x%02X read 0x%02X\r\n", gain, data);
	    break;
	}

	/* ����� ������� */
	res = twi_write_reg_value(LSM303_COMP_ADDR, MR_REG_M, mode);
	if (res != SUCCESS) {
	    PRINTF("Error set comp rate\r\n");
	    break;
	}

	/* ��������� ��� ��������? ��� ��� ������... */
	res = twi_read_reg_value(LSM303_COMP_ADDR, MR_REG_M, &data);
	if (res != SUCCESS && data != mode) {
	    PRINTF("Error check comp mode settings, gain error. Write 0x%02X read 0x%02X\r\n", mode, data);
	    break;
	}

	PRINTF("Mode comp OK: ������� 1.5 ��, + ������������� ������ ON, gn2=0 gn1=0 gn0=1, Continiuos mode\r\n");
    }
    while (0);
    return res;
}


/* Read Magnetometer STATUS register */
u8 lsm303_get_comp_status(void)
{
    u8 temp;
    int res;
    /* Read Mag STATUS register */
    res = twi_read_reg_value(LSM303_COMP_ADDR, SR_REG_M, &temp);
    if (res == SUCCESS) {
	return temp;
    }
    return 0;
}

/**
 * �������� ������ ������� - ���������� �� ��������, ��� ���������� ���!
 * ��������� endian - � ������� �������� �� �������� � ������� �� �������������!
 */
int lsm303_get_comp_data(lsm303_data * data)
{
    u8 lo, hi, gain;
    s32 x, y, z;
    s32 sens_xy = 1100, sens_z = 980;
    int res = 0;
    do {
	if (data == NULL) {
	    break;
	}

	/* ������ X �������� */
	res = twi_read_reg_value(LSM303_COMP_ADDR, OUT_X_H_M, &hi);
	if (res != SUCCESS) {
	    break;
	}

	res = twi_read_reg_value(LSM303_COMP_ADDR, OUT_X_L_M, &lo);
	if (res != SUCCESS) {
	    break;
	}

	x = ((s32) ((u16) hi << 8) + lo);
	/* Z */
	res = twi_read_reg_value(LSM303_COMP_ADDR, OUT_Z_H_M, &hi);
	if (res != SUCCESS) {
	    break;
	}

	res = twi_read_reg_value(LSM303_COMP_ADDR, OUT_Z_L_M, &lo);
	if (res != SUCCESS) {
	    break;
	}

	z = ((s32) ((u16) hi << 8) + lo);
	/* Y  */
	res = twi_read_reg_value(LSM303_COMP_ADDR, OUT_Y_H_M, &hi);
	if (res != SUCCESS) {
	    break;
	}

	res = twi_read_reg_value(LSM303_COMP_ADDR, OUT_Y_L_M, &lo);
	if (res != SUCCESS) {
	    break;
	}

	y = ((s32) ((u16) hi << 8) + lo);
	/* ��������� ������� �������� ��� ���������� ���� */
	res = twi_read_reg_value(LSM303_COMP_ADDR, CRB_REG_M, &gain);
	if (res != SUCCESS) {
	    break;
	}

	switch (gain) {

	    /*!< Full scale = �1.9 Gauss */
	case LSM303_FS_1_9_GA:
	    sens_xy = 855;
	    sens_z = 760;
	    break;
	    /*!< Full scale = �2.5 Gauss */
	case LSM303_FS_2_5_GA:
	    sens_xy = 670;
	    sens_z = 600;
	    break;
	    /*!< Full scale = �4.0 Gauss */
	case LSM303_FS_4_0_GA:
	    sens_xy = 450;
	    sens_z = 400;
	    break;
	    /*!< Full scale = �4.7 Gauss */
	case LSM303_FS_4_7_GA:
	    sens_xy = 400;
	    sens_z = 355;
	    break;
	    /*!< Full scale = �5.6 Gauss */
	case LSM303_FS_5_6_GA:
	    sens_xy = 330;
	    sens_z = 295;
	    break;
	    /*!< Full scale = �8.1 Gauss */
	case LSM303_FS_8_1_GA:
	    sens_xy = 230;
	    sens_z = 205;
	    break;
	    /*!< Full scale = �1.3 Gauss */
	case LSM303_FS_1_3_GA:
	default:
	    break;
	}

	data->x = x * 1000 / sens_xy;
	data->y = y * 1000 / sens_xy;
	data->z = z * 1000 / sens_z;
	res = SUCCESS;
    } while (0);
    return res;
}
