/*******************************************************************************
 * ������������ �� ��������� � �������
 * ���� � ������ ��������� � �������� USB - ����������� �� ���������
 * ��������� � ������ ���������� VBUS - ������ ������������ - ���������� VBUS
 ******************************************************************************/

#include "sdreader.h"

/* ����� ���������� */
#define        	MAX14502_ADDR   	0x70

/* ������ ��������� */
#define		CONTROL_REG		0x00
#define		CONFIG1_REG		0x01
#define		CONFIG2_REG		0x02
#define		CONFIG3_REG		0x03
#define		IE1_REG			0x04
#define		IE2_REG			0x05
#define		USBVIDH_REG		0x06
#define		USBVIDL_REG		0x07
#define		USBPIDH_REG		0x08
#define		USBPIDL_REG		0x09


#define		MAX14502_STATUS		0x12


static int sd_card_state = SD_CARD_UNKNOWN;


/* ���������� ����� � ��������� */
int set_sd_to_cr(void)
{
    int res;
    u8 data = 0x03;

    /* ���� */
    while (Osi_twi_obj_lock() != OSI_OK);

    res = twi_write_reg_value(MAX14502_ADDR, CONTROL_REG, data);
    if (res != SUCCESS) {
	PRINTF("Error set SD to CardReader\r\n");
    } else {
	sd_card_state = SD_CARD_TO_CR;
    }
    Osi_twi_obj_unlock();
    return res;
}

/* ���������� ����� � ���������� */
int set_sd_to_mcu(void)
{
    int res;
    u8 data = 0x18;


    while (Osi_twi_obj_lock() != OSI_OK);
    res = twi_write_reg_value(MAX14502_ADDR, CONTROL_REG, data);
    if (res != SUCCESS) {
	PRINTF("Error set SD to CPU\r\n");
    } else {
	sd_card_state = SD_CARD_TO_MCU;
    }

    Osi_twi_obj_unlock();

    return res;
}


/**
 * USB ������ ��������?
 * ������ ������, ���� ������� �� ������ 0x12, ��� 2 - VBUS.
 * ���� ���������� � ������ ���� ������ � VBUS == FALSE,
 * �� ������������� ������� �� MCU
 * 0 - ����� �����������
 */
int get_sd_cable(void)
{
    int res = -1;
    u8 val;

    res = twi_read_reg_value(MAX14502_ADDR, MAX14502_STATUS, &val);
    if (res == SUCCESS) {
	/* ��������� val */
	if (val & 0x04) {
	    res = 1; // ���� VBUS - ������ ���������
	} else {
	    res = 0;
	}
    }

    return res;
}


/**
 * ���������� ��������� ����������
 */
int get_sd_state(void)
{
    return sd_card_state;
}

