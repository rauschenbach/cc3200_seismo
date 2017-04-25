/*************************************************************************************
 * ��������� ������ � ���. � ������ ����� SPI ������� ������� �������
 *************************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "main.h"
#include "log.h"
#include "led.h"
#include "status.h"
#include "circbuf.h"
#include "sintab.h"
#include "userfunc.h"
#include "timtick.h"
#include "myspi.h"
#include "ads131.h"

/* ���������, ������� ��������� ��������� ������� ��� ������ ������� */
#include "struct.h"

/* �� �������� � ��������� �� systick!!! ���� ���� ��������� ���������� 
 * �������� �� ��. �������!!!
 */
#define DELAY	delay_ms

/**
 * ���������  
 */
#define  		TEST_BUFFER_SIZE 		80	/* ������� */
#define			BYTES_IN_DATA			3	/* ����� ���� � ����� ������ ��� */
#define			MAX_PING_PONG_SIZE		12000	/* ������ ������ - ���� �������� ���� ��� */



/**
 * ������ ��������� 
 */
/* Device settings reg (Read only!) */
#define 	ID		0x00

/* Global settings reg */
#define 	CONFIG1 	0x01
#define 	CONFIG2 	0x02
#define 	CONFIG3 	0x03
#define 	FAULT	 	0x04

/* Channel specific registers */
#define 	CH1SET	 	0x05
#define 	CH2SET	 	0x06
#define 	CH3SET	 	0x07
#define 	CH4SET	 	0x08
#define 	CH5SET	 	0x09
#define 	CH6SET	 	0x0A
#define 	CH7SET	 	0x0B
#define 	CH8SET	 	0x0C

/* Fault detect status regs (read-only!) */
#define 	FAULT_STATP 	0x12
#define 	FAULT_STATN 	0x13

/* GPIO Reg */
#define 	GPIO	 	0x14

/************************************************************************
 * ADS131 commands - ����������� �� ��������
 ************************************************************************/
/* System CMD */
#define 	CMD_WAKEUP		0x02
#define 	CMD_STANDBY	    	0x04
#define 	CMD_RESET		0x06
#define 	CMD_START		0x08
#define 	CMD_STOP		0x0A
#define 	CMD_OFFSETCAL		0x1A	/* �������� ���� */

/* Data read CMD */
#define 	CMD_RDATAC		0x10	/* ����������� ������ */
#define 	CMD_SDATAC		0x11	/* ���� ����������� ������  */
#define 	CMD_RDATA		0x12	/* ������ �� ������� */


/**
 * ��������� ���������� ������� ��� ��� � ����� ������ ���. ��������� ��� ���������� 
 * ��������� ���������
 */
static struct {
    ADS131_Regs regs;		/*  ������������ ��� */

    u64 long_time0;
    u64 long_time1;
    u32 num_irq;		/* ���������� ���������� */

    s32 num_sig;		/* ����� ������� */
    u32 sig_in_time;		/* ��� ����� ���������� ������� ���� */
    u16 pack_cnt;		/* ������� ������� */
    u16 sig_in_min;		/* ��� ����� ���������� ��������� */

    u16 samples_in_ping;	/* ����� ��������� ������� �� �������� ����� */
    u16 sample_us;		/* ���� ������� ���������� ���������� � ������������� */

    u8 sps_code;		/* ��� ������� ������������� */
    u8 data_size;		/* ������ ������ ������ �� ���� ������� 3..6..9..12 */
    u8 bitmap;			/* ����� ������� */
    u8 decim;			/* �����������? */
    u8 ping_pong_flag;		/* ���� ������ - ���� ��� ����  */
    u8 mode;			/* ����� ������ - test, work ��� cmd */

    bool handler_write_flag;	/* ���� ����, ��� ������ ������� */
    bool handler_sig_flag;	/* ��� �������� */
    bool is_set;		/* ���������� */
    bool is_init;		/* ���������� ��� "���������������" */
    bool is_run;		/* ���������� ��� "��������" */
} adc_pars_struct;


/* ��������� ��� ������ ��������� */
static ADCP_h adcp;
static CircularBuffer cb;

/**
 * ������ ��� ����� ����
 */
static ADS131_ERROR_STRUCT adc_error;	/* ������ ��� ����� ���� */
static ADS131_ERROR_STRUCT *pAdc_error = &adc_error;

static ADS131_DATA_h adc_pack;	/* ����� � ������� �� �����������  - ����������� �����  - 272 ����� */
static ADS131_DATA_h *pack = &adc_pack;	/* ����� � ������� �� ����������� */

static u8 ADC_ping_buf[MAX_PING_PONG_SIZE];	/* ����������� ����� �������  */
static u8 ADC_pong_buf[MAX_PING_PONG_SIZE];

static u8 *ADC_DATA_ping = ADC_ping_buf;	/* ���������� �����������  */
static u8 *ADC_DATA_pong = ADC_pong_buf;

/* 
 * ���� ������ ��� ������ �� SD - ����� ������� ���������
 * ������� �������� �� ����������.  ������� ��� ��� �� ���
 */
static OsiSyncObj_t gSdSyncObj;
static OsiTaskHandle gSdTaskHandle;
static OsiSyncObj_t gCmdSyncObj;
static OsiTaskHandle gCmdTaskHandle;
static void vSdTask(void *);
static void vAdcTask(void *);
static void vCmdTask(void *);


/*************************************************************************************
 * 	����������� ������� - ����� ���� �����
 *************************************************************************************/
static void adc_pin_config(void);
static void adc_reset(void);
static void adc_sync(void);
static void adc_stop(void);
static void adc_mux_config(void);
static void cmd_write(u8);
static void reg_write(u8, u8);
static u8 reg_read(u8);
static int regs_config(ADS131_Regs *);
IDEF void adc_set_run(bool);


static void adc_irq_register(void);
static void adc_irq_unregister(void);


static void cmd_mode_handler(u32 *, int);
static void work_mode_handler(u32 *, int);

/**
 * ��������� � ��������� ������ ������ ��� ������� � ����� ���� � ������
 */
static int calculate_ping_pong_buffer(ADS131_FreqEn freq, u8 bitmap, int len)
{
    int i;
    int res = -1;
    u8 bytes = 0;

    /* �� ������� ��� 8 ��� */
    if (freq > SPS4K) {
	log_write_log_file("ERROR: ADS131 Sample Frequency > 4 kHz not supported!\n");
	return res;
    }

    /* ����� ������ �����. ������ 1 ������ (����� 1 * 3 - �� ���� ���������� ��� �����) */
    for (i = 0; i < 4; i++) {
	if ((bitmap >> i) & 0x01) {
	    bytes += 3;
	}
    }

    /* ����� ����� ���������� ������ ������ ping-pong */
    for (i = 0; i < sizeof(adc_work_struct) / sizeof(ADS131_WORK_STRUCT); i++) {

	if (bytes == 0 || len == 0) {
	    break;
	}

	/* ���� ����� ������� � ����� ���� ��������������  */
	if (freq == adc_work_struct[i].sample_freq && bytes == adc_work_struct[i].num_bytes) {
	    adc_pars_struct.bitmap = bitmap;	/* ����� ������ �����  */
	    adc_pars_struct.data_size = adc_work_struct[i].num_bytes;
	    adc_pars_struct.samples_in_ping = adc_work_struct[i].samples_in_ping;	/* ����� �������� � ����� ping */
	    adc_pars_struct.sig_in_min = adc_work_struct[i].sig_in_min;	/* �������� signal ��� � ������ */
	    adc_pars_struct.sig_in_time = adc_work_struct[i].sig_in_time;	/* ��������� ������� ������� ��� �� 1 ��� */
	    adc_pars_struct.sig_in_time *= len;	/* ������� ����� �����? **** */
	    adc_pars_struct.decim = adc_work_struct[i].decim_coef;	/* ����������� */
	    adc_pars_struct.num_sig = 0;
	    adc_pars_struct.ping_pong_flag = 0;
	    adc_pars_struct.handler_sig_flag = false;
	    adc_pars_struct.pack_cnt = 0;	/* �������� �������� */
	    adc_pars_struct.sample_us = adc_work_struct[i].period_us;

	    log_write_log_file("INFO: freq:  %d Hz, bytes: %d\n", TIMER_US_DIVIDER / adc_pars_struct.sample_us, bytes);
	    log_write_log_file("INFO: decimation in: %d time(s)\n", adc_pars_struct.decim);
	    log_write_log_file("INFO: data size: %d bytes\n", adc_pars_struct.data_size);
	    log_write_log_file("INFO: %d bytes for ping or pong\n", adc_work_struct[i].ping_pong_size_in_byte);
	    log_write_log_file("INFO: bitmap: %02X\n", adc_pars_struct.bitmap);
	    log_write_log_file("INFO: in ping: %d samples\n", adc_pars_struct.samples_in_ping);
	    log_write_log_file("INFO: bitmap: %02X\n", adc_pars_struct.bitmap);
	    log_write_log_file("INFO: write on SD: %d time(s) in min\n", adc_pars_struct.sig_in_min);
	    res = 0;
	    break;
	}
    }
    /* ���� - ����. 5% */
    adc_pars_struct.sample_us += adc_pars_struct.sample_us / 20;
    return res;
}


/**
 * ���������������� ��� ��� �������
 * ������� ��� � ��������� - ������� ����������� ������
 * ��������� ������������ ����������, ������� ���� ��������
 */
bool ADS131_config(ADS131_Params * par)
{
    int volatile i;
    ADS131_Regs regs;
    u8 tmp, coef;
    bool res = false;

    do {
	if (par == NULL) {
	    log_write_log_file("ERROR: ADS131 parameters can't be NULL!\n");
	    break;
	}
	// ������� ��������
	mem_set(&regs, 0, sizeof(regs));


	/* ���� ������� - ��������� ���������� ��� */
/*vvvvv		adc_irq_unregister(); */
	adc_pars_struct.mode = par->mode;	/* ��������� ����� ������ */

	/* ������ ������� CONFIG1 - data rate. �� 125 �� 4000 ��� 24 ����� 
	 * ���� �������� ���������. NB: SIVY �� ������������ ������� ������ 4 ��� */
	tmp = par->sps;
	if (tmp > SPS4K) {
	    log_write_log_file("ERROR:  ADS131 Sample Frequency > 4 kHz not supported\n");
	    break;
	}

	/* �������� �������  - ��� ��� ������� ������ 1 ��� ��������� �����������/���������� �� 8 ��� ������� ������� ���������� */
	coef = 6;		/* �����������  ��� ������� 1k ��� ���������� �� 1000 ��  */
	switch (tmp) {
	case SPS4K:
	    regs.config1 = coef - 4;
	    break;

	case SPS2K:
	    regs.config1 = coef - 3;
	    break;

	case SPS1K:
	    regs.config1 = coef - 2;
	    break;

	    /* ������� 4 ��� - ��������� �� 8 */
	case SPS500:
	    regs.config1 = coef - 1;
	    break;

	    /* ������� 2��� - ��������� �� 8  */
	case SPS250:
	    regs.config1 = coef;
	    break;

	    /* ������� 1 ��� - ��������� �� 8  */
	case SPS125:
	    regs.config1 = coef;
	    break;

	    /* ��� �������� < 1K ����� �������� ���������� */
	default:
	    regs.config1 = coef;
	}

	/* ������ �������: Sample frequency + daisy chain off */
	regs.config1 |= 0xD0;

	adc_pars_struct.sps_code = par->sps;	/* ��� ������� ��������� � ��������� */

	/* ������ �������: �� ������ - ��� ������ ��������� ������� */
	regs.config2 = (par->test_freq & 0x3) | ((par->test_sign << 4) & 0x10);
	regs.config2 |= 0xE0;

	/* ������ �������: VREF_4V - use with a 5-V analog supply */
	regs.config3 = 0xC0;


	/* ����������� �������� ��� �������������!!! ������� */
	for (i = 0; i < MAX_ADS131_CHAN; i++) {
	    tmp = (par->pga << 4) | (par->mux);

/*	    log_write_log_file("CH%dSET = 0x%02X\r\n", i + 1, tmp); */

	    /* �� ����� ���� 000 011 � 111 */
	    if (par->pga == 0 || par->pga == 0x03 || par->pga >= 0x07) {
		log_write_log_file("ERROR: ADS131 this gain parameter not supported!\n");
		break;
	    }

	    /* �� ���������� MUX 010 110 � 111 */
	    if (par->mux == 2 || par->mux > 5) {
		log_write_log_file("ERROR: ADS131 this mux parameter not supported!\n");
		break;
	    }
	    regs.chxset[i] = tmp;

	    /* ��������� �������� ������ */
	    if (i >= NUM_ADS131_CHAN) {
		regs.chxset[i] = 0x90;
	    }
	}

	/* � ������� ������ �������� ��� ���������  */
	if (par->mode == WORK_MODE) {
	    if (calculate_ping_pong_buffer(par->sps, par->bitmap, par->file_len) != 0) {
		log_write_log_file("ERROR: Unable to calculate buffer PING PONG\n");
		break;
	    }

	    /* ������ ��������� ��������� */
	    log_fill_adc_header(par->sps, adc_pars_struct.bitmap, adc_pars_struct.data_size);
	    log_write_log_file("INFO: change ADS131 header OK\n");

	    /* ��������� ����� - ��� WiFi ��� COM ���� */
	} else if (par->mode == CMD_MODE) {

	    adc_pars_struct.bitmap = 0x0F;	/* � ��������� ������ - ��� ������ */

	    /* ���������� ��������� - ���������� ����� �� ��� =0! */
	    adc_pars_struct.decim = 1;
	    if (par->sps < SPS1K) {
		adc_pars_struct.decim = 8 / (1 << par->sps);
	    }

	    /* ���� ����� �� ���������� �������� ��� �� TEST_BUFFER_SIZE ����� ADS131_DATA_h */
	    log_write_log_file("Try to alloc %d bytes for Circular Buffer\r\n", TEST_BUFFER_SIZE * sizeof(ADS131_DATA_h));
	    PRINTF("Try to alloc %d bytes for Circular Buffer\r\n", TEST_BUFFER_SIZE * sizeof(ADS131_DATA_h));
	    if (!ADS131_is_run() && cb_init(&cb, TEST_BUFFER_SIZE)) {
		adc_set_run(false);
		log_write_log_file("FAIL\r\n");
		break;
	    } else {
		log_write_log_file("SUCCESS\r\n");
	    }

	} else {		/* ��st mode */
	    log_write_log_file("-------Checking ADS131 config----------\n");
	    par->sps = SPS1K;
	    regs.config1 = 0x96;
	    regs.config2 = 0xe2;
	    regs.config3 = 0xe1;
	    for (i = 0; i < NUM_ADS131_CHAN; i++) {
		regs.chxset[i] = 0x15;
	    }
	}
	memset(pAdc_error, 0, sizeof(ADS131_ERROR_STRUCT));

	adc_pars_struct.is_set = true;
	if (regs_config(&regs) == 0) {	/*  ������������� �������� ��� �� ���� ������� */
	    adc_pars_struct.is_init = true;	/* ���������������  */
	    adc_pars_struct.num_irq = 0;
	    res = true;
	} else {
	    log_write_log_file("Error config regs\n");
	    adc_pars_struct.is_init = false;	/* ���������������  */
	}
    } while (0);

    return res;
}


/**
 * ��������������� �������� ���� 4 ��� 8 ���, SPI0, ��������� ������� � ���� �������
 * ����������� ������ _131_START - �����, _131_DRDY - ����
 * ����� �������� Data Rate � ���������
 */
static int regs_config(ADS131_Regs * regs)
{
    u8 byte = 0;		// �������� 8 ������� ����� ����
    int i, n;
    int res = 0;

    adc_pin_config();		/* ������ DRDY + SPI0 */

    /* Before using the Continuous command or configured in read Data serial interface: reset the serial interface. */
    adc_reset();

    if (!adc_pars_struct.is_set)
	return -1;

    /* ���������. SDATAC command must be issued before any other 
     * commands can be sent to the device.
     */
    cmd_write(CMD_SDATAC);
    DELAY(50);

    /* � �������� �������� ������� ������ - ������ ��������� �� �� ����� ��� �������� */
    log_write_log_file("------------ADC tunning--------\n");

    /* ������ ������� ID  */
    byte = reg_read(ID);
    if ((byte & 0x03) == 0) {
	PRINTF("INFO: 4-channel device\n");
	n = 4;
    } else if ((byte & 0x03) == 1) {
	PRINTF("INFO: 6-channel device\n");
	n = 6;
    } else if ((byte & 0x03) == 2) {
	PRINTF("INFO: 8-channel device\n");
	n = 8;
    } else {
	PRINTF("INFO: unknown-channel device\n");
        n = 8;
    }
    PRINTF("INFO: ID = 0x%02x\n", byte);


    /* ����� �������� */
    reg_write(CONFIG1, regs->config1);
    reg_write(CONFIG2, regs->config2);
    reg_write(CONFIG3, regs->config3);

    /* ������ conf */
    byte = reg_read(CONFIG1);
    if (byte != regs->config1) {
	log_write_log_file("FAIL: CONFIG1 = 0x%02x\n", byte);
	res = -1;
    } else {
	log_write_log_file("SUCCESS: CONFIG1: 0x%02x\n", byte);
    }

    PRINTF("CONFIG1: 0x%02x\n", byte);

    /* ������ conf */
    byte = reg_read(CONFIG2);
    if (byte != regs->config2) {
	log_write_log_file("FAIL: CONFIG2 = 0x%02x\n", byte);
	res = -1;
    } else {
	log_write_log_file("SUCCESS: CONFIG2: 0x%02x\n", byte);
    }

    PRINTF("CONFIG2: 0x%02x\n", byte);

    /* ������ conf - ������ ����� ��� 0x01 ����� ��������� �� XOR */
    byte = reg_read(CONFIG3);
    if (byte != regs->config3) {
	log_write_log_file("WARN: CONFIG3: 0x%02X, read 0x%02x \n", regs->config3, byte);
	//vvvvv:  res = -1;
    } else {
	log_write_log_file("SUCCESS: CONFIG3: 0x%02x\n", byte);
    }
    PRINTF("CONFIG3: 0x%02x\n", byte);


    /* �������� ��� ���, ���������� �� ����� ���������� �������. �������� ��������� */
    for (i = 0; i < n; i++) {
	reg_write(CH1SET + i, regs->chxset[i]);	/* ����� � ������� */
        
	/* ������ ��������  - ������ ���� � ������� 4-� ��������� */
	byte = reg_read(CH1SET + i);
	if (byte != regs->chxset[i] && i < NUM_ADS131_CHAN) {
	    log_write_log_file("FAIL: CH%iSET = 0x%02x\n", i, byte);
	    res = -1;
	} else {
	    log_write_log_file("SUCCESS: CH%iSET = 0x%02x\n", i, byte);
	}

	PRINTF("CH%dSET: 0x%02x\n", i + 1, byte);
    }   
   
     /* ��������� FAULT  */
     byte = reg_read(FAULT);
     PRINTF("FAULT: 0x%02x\n", byte);
    
    
    /* ��������� ������� ��� � �������� */
    adcp.adc_param.board_type = 2;
    adcp.adc_param.power_mode = 1;
    adcp.adc_param.sample_rate = adc_pars_struct.sps_code;

    adcp.adc_board_1.board_active = 1;
    adcp.adc_board_1.ch_1_gain = (regs->chxset[0] >> 4) & 7;
    adcp.adc_board_1.ch_2_gain = (regs->chxset[1] >> 4) & 7;
    adcp.adc_board_1.ch_3_gain = (regs->chxset[2] >> 4) & 7;
    adcp.adc_board_1.ch_4_gain = (regs->chxset[3] >> 4) & 7;
    adcp.adc_board_1.ch_1_range = 0;
    adcp.adc_board_1.ch_2_range = 0;
    adcp.adc_board_1.ch_3_range = 0;
    adcp.adc_board_1.ch_4_range = 0;

    if (adc_pars_struct.bitmap & 1) {
	adcp.adc_board_1.ch_1_en = 1;
    }
    if (adc_pars_struct.bitmap & 2) {
	adcp.adc_board_1.ch_1_en = 1;
    }
    if (adc_pars_struct.bitmap & 4) {
	adcp.adc_board_1.ch_1_en = 1;
    }
    if (adc_pars_struct.bitmap & 8) {
	adcp.adc_board_1.ch_1_en = 1;
    }
    log_write_log_file("---------------------------\n");
    return res;
}

/**
 * ��������� ��� ��� � �������� �������������
 */
void ADS131_start(void)
{
    RUNTIME_STATE_t t;

    if (ADS131_is_run()) {	/* ��� ��������� */
	return;
    }

    /* �������� ���� */
    status_get_runtime_state(&t);

    cmd_write(CMD_RDATAC);	/* Set the data mode. ��������� ��� */
    adc_sync();			/* �������������� ��� */

    adc_set_run(true);		/* ��� ��������� */

    /* ��� ��� �������� ��������� 1 ���! */
    OsiReturnVal_e ret;

    /* ������ ������ �� IVG14 ���� � ������� ������. �� pga �� �������. */
    if (adc_pars_struct.mode == WORK_MODE) {


	adc_pars_struct.handler_write_flag = true;	/* � ������ ��� �������, ��� ����� ������ */

	/* ������ ������������� (event ��� ��� �� ��������) */
	ret = osi_SyncObjCreate(&gSdSyncObj);
	if (ret != OSI_OK) {
	    PRINTF("Create gSdSyncObj error!\r\n");
	    while (1);
	} else {
	    PRINTF("Create gSdSyncObj OK!\r\n");
	}

	/* ������� ������, ������� ����� ���������� ������ */
	ret = osi_TaskCreate(vSdTask, "SdTask", OSI_STACK_SIZE, NULL, 1, &gSdTaskHandle);
	if (ret != OSI_OK) {
	    PRINTF("Create SdTask error!\r\n");
	    while (1);
	} else {
	    PRINTF("Create SdTask OK!\r\n");
	}
    } else if (adc_pars_struct.mode == CMD_MODE) {

	/* ������ ������������� (event ��� ��� �� ��������) */
	ret = osi_SyncObjCreate(&gCmdSyncObj);
	if (ret != OSI_OK) {
	    PRINTF("Create gCmdSyncObj error!\r\n");
	    while (1);
	} else {
	    PRINTF("Create gCmdSyncObj OK!\r\n");
	}


	/* ������� ������, ������� ����� ��������� ������ ��� ���������� ������ */
	ret = osi_TaskCreate(vCmdTask, "CmdTask", OSI_STACK_SIZE, NULL, 1, &gCmdTaskHandle);
	if (ret != OSI_OK) {
	    PRINTF("Create CmdTask error!\r\n");
	    while (1);
	} else {
	    PRINTF("Create CmdTask OK!\r\n");
	}

    }

    /* ������������ ���������� ��� ��� �� 2-� ������� ��������� �� �� ��������� */
    adc_irq_register();

    t.acqis_running = 1;
    status_set_runtime_state(&t);	/* ���������� ���� */
}


/**
 * ���� ��� �� ������ CONTINIOUS � PowerDown, �������� �������
 * ���������� SPI
 */
void ADS131_stop(void)
{
    RUNTIME_STATE_t t;

    adc_irq_unregister();	/* ���� ������� - ��������� ���������� ��� */
    adc_pin_config();		/* ������ DRDY � OWFL + ������������� + SPI */
    adc_stop();			/* ������ ���� ���� */

    DELAY(50);

    /* �������� ���� */
    status_get_runtime_state(&t);
    t.acqis_running = 0;

    /* �������� ������� Stop data continous  */
    cmd_write(CMD_SDATAC);

    if (WORK_MODE == adc_pars_struct.mode) {
	log_write_log_file("------------------ADC stop------------------\n");
	log_write_log_file("CFG1: 0x%02x\n", reg_read(CONFIG1));	/* ��������� �������� ��� �������� */
	log_write_log_file("CFG2: 0x%02x\n", reg_read(CONFIG2));
	log_write_log_file("CFG3: 0x%02x\n", reg_read(CONFIG3));
	cmd_write(CMD_STANDBY);
    }

    adc_set_run(false);		/* �� �������  */

    /* ������� �����  */
    if (WORK_MODE == adc_pars_struct.mode) {
	adc_pars_struct.is_init = false;	/* ��������� �������������  */
    }
    spi_stop();			/* �������� SPI */

    status_set_runtime_state(&t);	/* ���������� ���� */
}

/**
 * ���������� � ��������� ������ - ����� ������ � Little Endian
 */
static void cmd_mode_handler(u32 * data, int num)
{

    /* �� ������� ������ ����� ������� */
    if (adc_pars_struct.pack_cnt == 0) {
	u64 ms = get_msec_ticks();
	pack->t0 = ms * 1000000;	/* ������� + ������������ ������� ������. ����� UNIX_TIME_t */
	pack->adc_freq = adc_pars_struct.sps_code;	/* ������� ������������� */
    }

    /* ������� ����� ������� ���� */
    pack->sig[adc_pars_struct.pack_cnt].x = data[0] >> 8;	// x 3..4..1?..0?..6..
    pack->sig[adc_pars_struct.pack_cnt].y = data[1] >> 8;
    pack->sig[adc_pars_struct.pack_cnt].z = data[2] >> 8;
    pack->sig[adc_pars_struct.pack_cnt].h = data[3] >> 8;	// h

    adc_pars_struct.pack_cnt++;

    if (adc_pars_struct.pack_cnt % 20 == 0) {
	pack->rsvd++;
    }


    /* ����� � �������� ����� � �������� ������ �� ������ */
    if (adc_pars_struct.pack_cnt >= NUM_ADS131_PACK) {
	OsiReturnVal_e ret;

	adc_pars_struct.pack_cnt = 0;

	ret = osi_SyncObjSignalFromISR(&gCmdSyncObj);	/* ���������� ������ */
	if (ret != OSI_OK) {
	    PRINTF("SyncObjSignalFromISR CmdSyncObj error!\r\n");
//          while (1);
	}
    }
}


/**
 * ���������� � ������� ������ 4 ������ ������
 */
static void work_mode_handler(u32 * data, int num)
{
    u8 *ptr;
    register u8 i, ch, shift = 0;
    u32 d;
    u64 ns, us;
    OsiReturnVal_e ret;

    ns = get_long_time();	/* ����� � ������������ */
    us = ns / 1000;		/* ������������ */

    pAdc_error->time_last = pAdc_error->time_now;
    pAdc_error->time_now = us;	/* ����� ������ */

    /* ������� ������� ���������� ���������� ���. ���� �������� �� 5% - ������������� ������ ��� */
    if (pAdc_error->time_last != 0 && pAdc_error->time_now - pAdc_error->time_last > adc_pars_struct.sample_us) {
	pAdc_error->sample_miss++;
    }

    /* ����� ����� ������� ������ */
    if (0 == adc_pars_struct.pack_cnt) {
	adc_pars_struct.long_time0 = ns;	/* ������� + ����������� ������� ������ */
    }

    /* �������� ���� ��� ���� �� 1 ������� */
    if (0 == adc_pars_struct.ping_pong_flag) {
	ptr = ADC_DATA_ping;	/* 0 �������� ���� */
    } else {
	ptr = ADC_DATA_pong;	/* 1 �������� ���� */
    }

    /* ����� ������ �������� */
    ch = adc_pars_struct.bitmap;

    /* ���� ����� ������� - ���������� � Big Endian � ������� ������ 3 ����� */
    for (i = 0; i < NUM_ADS131_CHAN; i++) {
	if ((1 << i) & ch) {
	    d = byteswap4(data[i]);
	    mem_copy(ptr + adc_pars_struct.pack_cnt * adc_pars_struct.data_size + shift, &d, BYTES_IN_DATA);
	    shift += BYTES_IN_DATA;
	}
    }

    adc_pars_struct.pack_cnt++;	/* ������� ������� ��������  */

    /*  1 ��� � ������� �� 2 ��� �� 4 ������� ����� 1000 ������� */
    if (adc_pars_struct.pack_cnt >= adc_pars_struct.samples_in_ping) {
	adc_pars_struct.pack_cnt = 0;	/* �������� ������� */
	adc_pars_struct.ping_pong_flag = !adc_pars_struct.ping_pong_flag;	/* ������ ����. ��� ������� �� � �������� */

	/* ���� ��� ����� ������: handler_write_flag, ������ ������ �� SD ������������� - ��������� �� flash  */
	if (!adc_pars_struct.handler_write_flag) {
	    pAdc_error->block_timeout++;	/* ������� ������ ������ ����� �� SD */
	}

	adc_pars_struct.long_time1 = adc_pars_struct.long_time0;	/* ���������. ����� ����� �������� */

	ret = osi_SyncObjSignalFromISR(&gSdSyncObj);	/* ���������� ������ */
	if (ret != OSI_OK) {
	    PRINTF("SyncObjSignalFromISR error!\r\n");
//          while (1);
	}
    }
}


/**
 * ���������� �� �������� �� "1" � "0" ��� ���
 * ����� ����� ������������ ��������� � ������, ������� ����� ������������� ������
 */
void ADS131_ISR_func(void)
{
    u32 data[NUM_ADS131_CHAN];	/* 4 ������  */

    spi_cs_on();

    spi_write_read_data(NUM_ADS131_CHAN, data);	/* �������� � ������� BIG ENDIAN!!! ������ ���� */

    do {
	/* ��� ������� 125 - ���� ������ 2 ������. ������� � ����� ������������������! */
	if ((adc_pars_struct.sps_code == SPS125) && (adc_pars_struct.num_irq % 2 == 1)) {
	    break;
	}
#if 10
	/* ���� ���������� */
	static int k = 0, j = 0;
	data[0] = get_sin_table(k);
	if (j++ % 10 == 0) {
	    k++;
	}
#endif

	/* ����������� ����� - ������ �� SD �����  */
	if (adc_pars_struct.mode == WORK_MODE) {
	    work_mode_handler(data, NUM_ADS131_CHAN);
	} else {
	    /* �������� �� ������� 125 - ���� ������ ���� ������ ��� ��������� ������ ��� PC */
	    if (adc_pars_struct.num_irq % (1 << adc_pars_struct.sps_code) == 0) {
		cmd_mode_handler(data, NUM_ADS131_CHAN);	/* ��� ������ �� PC */
	    }
	}
    } while (0);

    adc_pars_struct.num_irq++;	/* ������� ���������� */

    if (adc_pars_struct.num_irq % 125 == 0) {
	led_toggle(LED_GREEN);
    }


    spi_cs_off();		/* ������ CS  */

    /* ���������� � �����, ����� ����� ��������� ������ ������ ���������� */
    MAP_GPIOIntClear(ADS131_DRDY_BASE, ADS131_DRDY_GPIO);
}


/**
 * ���������� ������� ������
 */
void ADS131_get_error_count(ADS131_ERROR_STRUCT * err)
{
    if (err != NULL) {
	mem_copy(err, pAdc_error, sizeof(ADS131_ERROR_STRUCT));
    }
}

/**
 * ������ ��� � �������������� �����������
 * ���������� ��������� �� �������� �� ������� � ����, ��� ����� �������� ��� 1 ��� �������� � 0 
 */
static void adc_irq_register(void)
{
    /* Set GPIO interrupt type */
    MAP_GPIOIntTypeSet(ADS131_DRDY_BASE, ADS131_DRDY_GPIO, GPIO_FALLING_EDGE);
    osi_InterruptRegister(ADS131_DRDY_GROUP_INT, ADS131_ISR_func, INT_PRIORITY_LVL_1);
}


/**
 * ������� ����������
 */
static void adc_irq_unregister(void)
{
    MAP_GPIOIntDisable(ADS131_DRDY_BASE, ADS131_DRDY_INT);
    MAP_GPIOIntClear(ADS131_DRDY_BASE, ADS131_DRDY_GPIO);
    MAP_IntDisable(ADS131_DRDY_GROUP_INT);
}

/**
 * ����� ��� - ������ �������� ����������! 
 */
void ADS131_start_irq(void)
{
    /* ������ ����� */
    MAP_GPIOIntClear(ADS131_DRDY_BASE, ADS131_DRDY_GPIO);

    /* Enable Interrupt */
    MAP_GPIOIntEnable(ADS131_DRDY_BASE, ADS131_DRDY_INT);
}

/**
 * ������� ��� � PD - ����� ����������
 */
void ADS131_standby(void)
{
    adc_pin_config();
    cmd_write(CMD_SDATAC);
    DELAY(5);
    cmd_write(CMD_STANDBY);
}

/**
 * ������� offset cal
 * OFFSETCAL must be executed every time there is a change in PGA gain settings.
 */
bool ADS131_ofscal(void)
{
    bool res = false;

    /* �� ����� �������!  */
    if (!ADS131_is_run()) {
	adc_pin_config();
	cmd_write(CMD_OFFSETCAL);
	res = true;
    }

    return res;
}

/**
 * ��� �������?
 */
bool ADS131_is_run(void)
{
    return adc_pars_struct.is_run;
}

/**
 * ���������� - ��� ������� ��� ����������
 */
IDEF void adc_set_run(bool run)
{
    adc_pars_struct.is_run = run;
}


/**
 *  ��� ������� ���������� ������ �� ������
 */
bool ADS131_get_pack(ADS131_DATA_h * buf)
{
    if (!cb_is_empty(&cb) && buf != NULL) {
	RUNTIME_STATE_t t;

	status_get_runtime_state(&t);	/* �������� ���� */
	t.mem_ovr = 0;		/* ������� ������ - "����� �����" */
	status_set_runtime_state(&t);	/* ���������� ���� */
	cb_read(&cb, (ElemType *) buf);
	return true;
    } else {
	return false;		/*  "������ �� ������" */
    }
}

/**  
 *  �������� ����� ������
 */
bool ADS131_clear_adc_buf(void)
{
    RUNTIME_STATE_t t;

    status_get_runtime_state(&t);	/* �������� ���� */
    t.mem_ovr = 0;		/* ������� ������ - "����� �����" */
    status_set_runtime_state(&t);	/* ���������� ���� */

    cb_clear(&cb);
    return true;
}


/**
 * ������ ���� ������ �������
 */
bool ADS131_get_handler_flag(void)
{
    return adc_pars_struct.handler_sig_flag;
}

/**
 * �������� ���� ������ �������
 */
void ADS131_reset_handler_flag(void)
{
    adc_pars_struct.handler_sig_flag = false;
}



/**
 * ������ ������� � ��� 
 * ��������: ������� 
 * �������:  ���
 */
static void cmd_write(u8 cmd)
{
    int volatile i;

    spi_cs_on();

    spi_write_read(cmd);	/* �������� ������� */

    /* ����� � 24 ����� Fclk ������� ~5 ����������� - ����������� ������ ����!!! 
     * ��������� ������������ ����� �� datasheet!!! */
    for (i = 0; i < 25; i++);
    spi_cs_off();
}

/**
 * ������ � 1 �������  ���
 * ��������:  ����� ��������, ������
 * �������:  ���
 */
static void reg_write(u8 addr, u8 data)
{
    int volatile i, j;
    u8 volatile cmd[3];

    spi_cs_on();

    cmd[0] = 0x40 + addr;
    cmd[1] = 0;
    cmd[2] = data;


    for (i = 0; i < 3; i++) {
	spi_write_read(cmd[i]);

	/* ����� � ~24 ����� Fclk (5 ���) - �������! */
	for (j = 0; j < 25; j++);
    }
    spi_cs_off();
}

/**
 * ��������� 1 �������  ���: 
 * ��������:  ����� ��������
 * �������:   ������
 */
static u8 reg_read(u8 addr)
{
    int volatile i, j;
    u8 volatile cmd[2];
    u8 volatile data;

    spi_cs_on();

    cmd[0] = 0x20 + addr;
    cmd[1] = 0;

    for (i = 0; i < 2; i++) {
	spi_write_read(cmd[i]);

	/* ����� � ~24 ����� Fclk (5 ���) - �������! */
	for (j = 0; j < 25; j++);
    }
    data = spi_write_read(0);

    spi_cs_off();
    return data;
}

/**
 * ������ ������������ ����� DRDY � START � ������������� ������������� ���
 * ������ �� ���� ��� �����!
 */
static void adc_pin_config(void)
{
    spi_init();
    adc_mux_config();
}

/**
 * ���������� ���� DRDY � START
 */
static void adc_mux_config(void)
{
    /* Enable Peripheral Clocks  */
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA0, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA1, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA2, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA3, PRCM_RUN_MODE_CLK);

    /*  ���� reset �� ����� � "1" */
    MAP_PinTypeGPIO(ADS131_RESET_PIN, ADS131_RESET_MODE, false);
    MAP_GPIODirModeSet(ADS131_RESET_BASE, ADS131_RESET_BIT, GPIO_DIR_MODE_OUT);
    MAP_GPIOPinWrite(ADS131_RESET_BASE, ADS131_RESET_BIT, ADS131_RESET_BIT);	/* ������ � 1 */

    /* ���� ����� / sync_in (PIN_63 / GPIO_08) �� ����� � 0 */
    MAP_PinTypeGPIO(ADS131_START_PIN, ADS131_START_MODE, false);
    MAP_GPIODirModeSet(ADS131_START_BASE, ADS131_START_BIT, GPIO_DIR_MODE_OUT);
    MAP_GPIOPinWrite(ADS131_START_BASE, ADS131_START_BIT, ~ADS131_START_BIT);	/* ������ � 0 */

    /* ���� DRDY (PIN_61 / GPIO_06) �� ���� - ��������� ��� �� ���������� � ��� */
    MAP_PinTypeGPIO(ADS131_DRDY_PIN, ADS131_DRDY_MODE, false);
    MAP_GPIODirModeSet(ADS131_DRDY_BASE, ADS131_DRDY_BIT, GPIO_DIR_MODE_IN);
}

/**
 * ����� ���� ��� ������ �������� START ������� 0.5 ��� 
 */
static void adc_sync(void)
{
    /* �������� ������� ��� START - ���������� � ���� - ������ � 1 */
    MAP_GPIOPinWrite(ADS131_START_BASE, ADS131_START_BIT, ADS131_START_BIT);

    /* ����� ���������� IRQ - clear pending interrupts */
    MAP_IntPendClear(ADS131_DRDY_GROUP_INT);
}

/* ���� ���  */
static void adc_stop(void)
{
    /* ������ � 1 */
    MAP_GPIOPinWrite(ADS131_START_BASE, ADS131_START_BIT, ADS131_START_BIT);
}

/**
 * ��������� ������ ��� ��� ���������� 1-�� ������ ��� ���������� ������
 */
static void vCmdTask(void *s)
{
    OsiReturnVal_e ret;

    while (1) {
	/* Wait for a message to arrive. */
	ret = osi_SyncObjWait(&gCmdSyncObj, OSI_WAIT_FOREVER);

	if (ret == OSI_OK) {

	    if (cb_is_full(&cb)) {
		RUNTIME_STATE_t t;

		/* �������� ���� */
		status_get_runtime_state(&t);
		t.mem_ovr = 1;	/* ������ ������ - "����� �����". ������� ��� ������, �� �����! */
		status_set_runtime_state(&t);	/* ���������� ���� */
	    } else {
		cb_write(&cb, pack);	/* ����� � ����� */
	    }
	}
    }
}

/**
 * ������ - ��������� 1...4 ���� � ������� ��� ����� ������� ��� ���������� ������ PING_PONG
 */
static void vSdTask(void *s)
{
    u8 *ptr;
    u64 ns;
    int res;
    OsiReturnVal_e ret;

    /* ��������� ���� �������� */
    while (ADS131_is_run()) {

	/* Wait for a message to arrive. */
	ret = osi_SyncObjWait(&gSdSyncObj, OSI_WAIT_FOREVER);
	PRINTF("Enter Task\r\n");

	if (ret == OSI_OK) {

	    ns = adc_pars_struct.long_time1;	/* �������� � ������������ */
	    adc_pars_struct.handler_write_flag = false;	/* ������� ���� ������� - ������ � ���������� */

	    /*  ������ �������� ����� ��� ������� - �� ������� ���������� � ISR */
	    if (0 == adc_pars_struct.num_sig % adc_pars_struct.sig_in_time) {
		res = log_create_hour_data_file(ns);
	    }

	    /* 1 ��� � ������ ��������� ��������� � ������������ ��������  */
	    if (0 == adc_pars_struct.num_sig % adc_pars_struct.sig_in_min) {
		log_change_num_irq_to_header(adc_pars_struct.num_irq);
		res += log_write_adc_header_to_file(ns);	/* ������������ ����� � ��������� */
	    }


	    /* C��������� ����� �� SD ����� */
	    if (1 == adc_pars_struct.ping_pong_flag) {	/* ����� ������ ���� */
		ptr = ADC_DATA_ping;
	    } else {		/* ����� ������ ���� */
		ptr = ADC_DATA_pong;
	    }

	    res += log_write_adc_data_to_file(ptr, adc_pars_struct.samples_in_ping * adc_pars_struct.data_size);
	    adc_pars_struct.num_sig++;	/* ���������� ������ �� 4, 2 ��� 1 ������� */
	    adc_pars_struct.handler_sig_flag = true;	/* 1 ��� � ������� - ������� ��������� */
	    adc_pars_struct.handler_write_flag = true;	/* ������ ���� - ������� �� �����������. */
	} else {
	    PRINTF("Error get message!\r\n");
	}
	PRINTF("Leave Task %s\r\n", res == RES_NO_ERROR ? "OK" : "Fail");

	delay_ms(10);
    }
    PRINTF("End of SdTask\r\n");
}


/**
 * ����� ��� �����
 */
static void adc_reset(void)
{
    /* ������ � 0 */
    MAP_GPIOPinWrite(ADS131_RESET_BASE, ADS131_RESET_BIT, (unsigned char) ~ADS131_RESET_BIT);

    /* ��������  */
    DELAY(5);

    /* ������ � 1 */
    MAP_GPIOPinWrite(ADS131_RESET_BASE, ADS131_RESET_BIT, ADS131_RESET_BIT);
    DELAY(5);			/* ��������  */
}


/* ������, ������� ����� ������� ���������� ������� �� ��� */
static void vAdcTask(void *p)
{
    OsiReturnVal_e ret;
    int i = 0;

    /* ���������� ���� ��� ����� ��������� ����������� � ���� ���������� */
    do {

	if (adc_pars_struct.num_irq) {
	    PRINTF("-->(%d) Num ADC irq: %d.\r\n", i++, adc_pars_struct.num_irq);
	}
	osi_Sleep(1000);
    } while (!is_finish());


    ADS131_stop();
    log_write_log_file("Stop conversion\r\n");
    log_close_data_file();	/* ��������� ���� ������ */
    log_close_log_file();
    PRINTF("Stop conversion\r\n");


    /* ������� Sync ������ */
    ret = osi_SyncObjDelete(&gSdSyncObj);
    if (ret != OSI_OK) {
	PRINTF("Delete gSdSyncObj error!\r\n");
	while (1);
    } else {
	PRINTF("Delete gSdSyncObj OK!\r\n");
    }

    // ��� ��������� �������� - ������� ������ ������, � ����� ������ ����� ��������
    /* ������� ������ - ���� ���� */
    osi_TaskDelete(&gSdTaskHandle);
    PRINTF("Delete SdTask OK!\r\n");
}

/* ������� ������ ��� ����� ��� */
void ADS131_test(ADS131_Params * par)
{
    bool res;
    GPS_DATA_t date;
    OsiReturnVal_e ret;

    status_get_gps_data(&date);
    log_create_adc_header(&date);

    res = ADS131_config(par);
    PRINTF("ADC freq: %d Hz\r\n", (1 << par->sps) * 125);
    PRINTF("ADC mode: %s\r\n", par->mode == 0 ? "TEST_MODE" : par->mode == 1 ? "WORK_MODE" : "CMD_MODE");
    PRINTF("ADC pga: %s\r\n", par->pga == PGA1 ? "PGA1" : par->pga == PGA2 ? "PGA2" : par->pga == PGA4 ? "PGA4" : par->pga == PGA8 ? "PGA8" : "PGA12");
    PRINTF("ADC bitmap: 0x%02X\r\n", par->bitmap);
    PRINTF("ADC mux: %s\r\n", par->mux == 0 ? "Normal" :
	   par->mux == 1 ? "shorted" : par->mux == 2 ? "reserved" : par->mux == 3 ? "MVDD" : par->mux == 4 ? "temperature" : "test");
    PRINTF("ADC test signal: %s\r\n", par->test_sign == 0 ? "external" : "internal");
    PRINTF("ADC test freq: %d\r\n", par->test_freq);
    PRINTF("File len: 0x%02X\r\n", par->file_len);

    /* ������� �������� 3dfix GPS � ��������� �� �������� ����� */
    PRINTF("Init test...: result = %s\r\n", res ? "OK" : "FAIL");
    ADS131_start();
    PRINTF("ADS131 start...\n");

/*    ADS131_start_irq(); */


    /* ������� ������, ������� ����� ���������� ������ */
    ret = osi_TaskCreate(vAdcTask, "AdcTask", OSI_STACK_SIZE, NULL, 1, NULL);
    if (ret != OSI_OK) {
	while (1) {
	    PRINTF("Create AdcTask error!\r\n");
	    delay_ms(1000);
	}
    } else {
	PRINTF("Create AdcTask OK!\r\n");
    }
}

/* ��������� ��� */
void ADS131_get_adcp(ADCP_h * par)
{
    if (par != NULL) {
	mem_copy(par, &adcp, sizeof(adcp));
    }
}


/**
 * ��������� ��� ����������  
 */
void ADS131_set_adcp(ADCP_h * par)
{
    if (par != NULL) {
	mem_copy(&adcp, par, sizeof(adcp));
    }
}


/**
 * ������ ��� � ��������� ������ 
 */
bool ADS131_write_parameters(ADCP_h * v)
{
    bool res = false;
    u8 tmp;

    if (v != NULL && !ADS131_is_run()) {
	ADS131_Params par;
	mem_copy(&adcp, v, sizeof(ADCP_h));
	par.mode = CMD_MODE;	/* ����� ������ �������� */
	par.sps = (ADS131_FreqEn) adcp.adc_param.sample_rate;	/* �������  */
	tmp = (ADS131_PgaEn) adcp.adc_board_1.ch_1_gain;	/* PGA */
	par.pga = (tmp == 0) ? 1 : tmp;

	/* ���� �������� ������ */
	if (adcp.adc_param.test_signal) {
	    par.mux = MUX_TEST;	/* ������������� �� ����� */
	    par.test_sign = TEST_INT;	/* �������� ������  */
	    par.test_freq = TEST_FREQ_0;	/* ������� ��������� ������� */
	} else {
	    par.mux = MUX_NORM;	/* ������������� �� ����� */
	    par.test_sign = TEST_EXT;	/* ������� ������  */
	    par.test_freq = TEST_FREQ_NONE;	/* ������� ��������� ������� �� ������������ */
	}

	par.bitmap = 0x0f;	/* ���������� ������: 1 ����� �������, 0 - �������� */
	par.file_len = 1;	/* ����� ����� ������ � ����� */

	/* �������� ��������� */
	if (ADS131_config(&par) == true) {
	    ADS131_start();	/* ��������� ��� � PGA  */
	    res = true;
	}
    }
    return res;
}

/**
 * ������ ��� � ��������� ������ 
 */
bool ADS131_start_osciloscope(void)
{
    bool res = false;
    RUNTIME_STATE_t t;

    /* �������� ���� */
    status_get_runtime_state(&t);

    if (t.acqis_running) {
	ADS131_start_irq();	/* ��������� IRQ  */
	res = true;
    }
    return res;
}

/**
 * ���� ���� ��� 
 */
bool ADS131_stop_osciloscope(void)
{
    bool res = false;
    RUNTIME_STATE_t t;

    /* �������� ���� */
    status_get_runtime_state(&t);

    if (t.acqis_running) {
	ADS131_stop();
	res = true;
    }
    return res;
}


/**
 * �������� ������ - ���� false? ������ ��� 
 */
bool ADS131_get_data(ADS131_DATA_h * buf)
{
    bool res = false;
    if (buf != NULL) {
	res = ADS131_get_pack(buf);
    }
    return res;
}
