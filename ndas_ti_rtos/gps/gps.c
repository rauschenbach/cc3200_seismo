#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <main.h>
#include "twi.h"
#include "status.h"
#include "gps.h"


#define 	NAVPVT_MSG_HEADER 		0x070162B5	/* u32: 0xB5 0x62 0x01 0x07 */

#define 	NEO7_DEVICE_ADDR		0x42
#define 	REG_NUM_BYTES			0xFD
#define 	REG_DATA_STREAM			0xFF

#define 	SUCCESS                 	0
#define 	FAILURE                        -1



/************************************************************************
 * 	����������� ����������
 * 	������������ ��� ���������� SRAM (USE_THE_LOADER)
 * 	��� ������������ � ������ ������  
 ************************************************************************/
static NAV_PVT_Payload nav_pvt_load;
static GPS_COUNT_STRUCT_t gps_cnt;
static NAV_PVT_Payload *pGpsNav = &nav_pvt_load;	/* ������������� ����������  */
static GPS_COUNT_STRUCT_t *pGpsCnt = &gps_cnt;	/* �������� ������ */


static long gps_time_sec;

/************************************************************************
 * ���������, � ��� ��� ����������� ROM ������� ��� ������ ������� � SRAM
 ************************************************************************/
/* Enable NAV-PVT output */
static const u8 CFG_MSG_Message[] = {
	0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0x01, 0x07,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xE2
};


/* Disable all inf messages */
static const u8 CFG_INF_Message[] = {
	0xB5, 0x62, 0x06, 0x02, 0x0A, 0x00, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87,
	0x9A, 0x77
};

/* Disable all but UBX out on I2C */
static const u8 CFG_PRT_Message[] = {
	0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 1, 0x00, 1, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xA0, 0x96
};

/* Enable 1Hz timepulse */
static const u8 CFG_TP5_Message[] = {
	0xB5, 0x62, 0x06, 0x31, 0x20, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x28, 0x5C, 0x8F, 0x02, 0x00, 0x00,
	0x00, 0x00, 0xEF, 0x00, 0x00, 0x00, 0x60, 0x77
};


static void vGpsDataTask(void *);
static void gps_parse_data(u8);
static void gps_rx_end(void);

/* ��������� ������ GPS */
int gps_create_task(void)
{
	int ret;

	/* ������� ������, ������� ����� ���������� ������ */
	ret = osi_TaskCreate(vGpsDataTask, "GpsTask", OSI_STACK_SIZE, NULL, GPS_TASK_PRIORITY, NULL);
	if (ret != OSI_OK) {
		PRINTF("Create GpsTask error!\r\n");
		return -1;
	}

	PRINTF("INFO: Create GpsTask OK!\r\n");
	return ret;
}



/**
 * ������������� GPS
 */
int gps_init(void)
{
	int res;

	do {
		/* Enable NAV-PVT output */
		res = gps_write_data_string((u8 *) CFG_MSG_Message, sizeof(CFG_MSG_Message));
		if (res != SUCCESS) {
			break;
		}

		/* Disable all inf messages */
		res = gps_write_data_string((u8 *) CFG_INF_Message, sizeof(CFG_INF_Message));
		if (res != SUCCESS) {
			break;
		}

		/* Disable all but UBX out on I2C */
		res = gps_write_data_string((u8 *) CFG_PRT_Message, sizeof(CFG_PRT_Message));
		if (res != SUCCESS) {
			break;
		}

		/* Enable 1Hz timepulse */
		res = gps_write_data_string((u8 *) CFG_TP5_Message, sizeof(CFG_TP5_Message));
		if (res != SUCCESS) {
			break;
		}
	} while (0);

	return res;
}


/**
 * ������ ������ GPS ������
 */
static void vGpsDataTask(void *par)
{
	int res;

	/* ������ �� �������� ��� static � ������ ��������??? */
	s16 n = 0;
	/*static */ u8 buf[4];


	while (true) {

		/* ���� */
		if (Osi_twi_obj_lock() == OSI_OK) {

			do {
				if (n >= 2) {
					res = gps_read_data_string(buf, 2);
					for (int i = 0; i < 2; i++) {
						//      PRINTF("%c", a);
						gps_parse_data(buf[i]);
					}
					/* ������ ����� */
					if (res < 0) {
						log_write_log_file("Reset module\r\n");
						gps_init();
					}
				}
				n = gps_check_avail_bytes();
			} while (n > 0);

			Osi_twi_obj_unlock();
		}
		osi_Sleep(100);
	}
}

/**
 * ���� ����� - �������� ��������
 */
static void gps_rx_end(void)
{
	pGpsCnt->ind = 0;	/* ���� ���� ������� */
	pGpsCnt->cnt = 0;	/* ������� ������� - ������ ���� ������� */
	pGpsCnt->hdr = 0;
}

/**
 * ������ ������
 */
static void gps_parse_data(u8 rx_byte)
{
	if (pGpsCnt->ind == 0 && rx_byte == 0xb5) {
		pGpsCnt->ind = 1;	/* ���� ���� ������� */
		pGpsCnt->cnt = 1;	/* ������� ������� - ������ ���� ������� */
		pGpsCnt->hdr = rx_byte;
	} else {
		if (pGpsCnt->cnt == 1) {
			pGpsCnt->hdr |= (u16) rx_byte << 8;
		} else if (pGpsCnt->cnt == 2) {
			pGpsCnt->hdr |= (u32) rx_byte << 16;
		} else if (pGpsCnt->cnt == 3) {
			pGpsCnt->hdr |= (u32) rx_byte << 24;
			if (pGpsCnt->hdr != NAVPVT_MSG_HEADER) {
				gps_rx_end();
			}
		} else if (pGpsCnt->cnt == 4) {
			pGpsCnt->len = rx_byte;
			if (rx_byte != 0x54) {
				gps_rx_end();
			}
		} else {
			*((u8 *) pGpsNav + pGpsCnt->cnt - 6) = rx_byte;

		}
		pGpsCnt->cnt++;
		if (pGpsCnt->cnt >= 0x54) {
			struct tm t0;
			t0.tm_isdst = 0;	/* ����������� - ����� ����� ������ ��� */
			t0.tm_hour = pGpsNav->hour;
			t0.tm_min = pGpsNav->min;
			t0.tm_sec = pGpsNav->sec;
			t0.tm_mon = pGpsNav->month - 1;
			t0.tm_mday = pGpsNav->day;
			t0.tm_year = pGpsNav->year - 1900;
			gps_time_sec = tm_to_sec(&t0);
			status_set_gps_data(gps_time_sec, pGpsNav->lat, pGpsNav->lon, 
                                            pGpsNav->hAcc, pGpsNav->numSV, pGpsNav->fixType == 3 ? true : false);

			/* ����� ������� ����� PPS � ������ ��������� �������  */
			status_set_drift(timer_get_drift());
			gps_rx_end();
                        
		}
	}
}


/**
 * ���������� ��������� ���� � ������ �� ������ ������
 */
u16 gps_check_avail_bytes(void)
{
	u8 reg = 0;
	u8 data[2] = { 0, 0 };


	/* Get the register offset address */
	reg = REG_NUM_BYTES;

	/* Write the register address to be read from.
	 * Stop bit implicitly assumed to be 0. */
	if (TWI_IF_Write(NEO7_DEVICE_ADDR, &reg, 1, 0) != SUCCESS) {
		//PRINTF("ERROR: I2C check bytes in buf\n\r");
		return (u16) - 1;
	}
	// Read the specified length of data
	if (TWI_IF_Read(NEO7_DEVICE_ADDR, &data[0], 2) != SUCCESS) {
		//PRINTF("ERROR: I2C Read\n\r");
		return (u16) - 1;
	}


	return ((u16) data[0] << 8) | (u16) data[1];
}

/**
 *  ��������� ����� ������
 */
int gps_read_data_string(u8 * data, u16 len)
{
	u8 reg = 0;
	int res = -1;

	/* Get the register offset address */
	reg = REG_DATA_STREAM;

	/* Get the length of data to be read */
	len = 2;

	do {
		/* Write the register address to be read from.
		 * Stop bit implicitly assumed to be 0. - ����� ����� ����? */
		if (TWI_IF_Write(NEO7_DEVICE_ADDR, &reg, 1, 0) != SUCCESS) {
			//PRINTF("ERROR: I2C Write\n\r");
			//break;
		}

		/* Read the specified length of data */
		if (TWI_IF_Read(NEO7_DEVICE_ADDR, data, len) != SUCCESS) {
			//PRINTF("ERROR: I2C Read\n\r");
		}
		res = 0;
	} while (0);
	return res;
}


/**
 * �������� ����� ������
 */
int gps_write_data_string(u8 * data, u16 len)
{
	int res = -1;

	/* Write the register address to be read from.
	 * Stop bit implicitly assumed to be 0. - ����� ����� ����? */
	if (TWI_IF_Write(NEO7_DEVICE_ADDR, data, len, 0) == SUCCESS) {
		res = 0;
	}
	return res;
}

/**
 * ��� ������� - ������� ����� GPS
 */
void gps_print_data(void)
{
	PRINTF("NOW: (%s) %02d:%02d:%02d %02d-%02d-%02d\r\n",  
		(pGpsNav->fixType == 3) ? "3dfix" : "Nofix",
		pGpsNav->hour, pGpsNav->min, pGpsNav->sec, pGpsNav->month,
	        pGpsNav->day, pGpsNav->year - 2000);
}

/**
 * ������ ����� GPS
 */
long gps_get_gps_time(void)
{
   return gps_time_sec;	
}

