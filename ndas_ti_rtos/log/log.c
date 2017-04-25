/* ��������� �������: ������� ����, ������ �� SD ����� */
#include <utils.h>
#include <ff.h>
#include <fs.h>
#include "sdcard.h"
#include "board.h"
#include "status.h"
#include "userfunc.h"
#include "ads131.h"
#include "timtick.h"
#include "log.h"
#include "main.h"
#include "minIni.h"
#include "xchg.h"


#define   	MAX_FILE_SIZE		256
#define   	MAX_START_NUMBER	100	/* ������������ ����� �������� */
#define	  	MAX_TIME_STRLEN		26	/* ����� ������ �� ��������  */
#define   	MAX_LOG_FILE_LEN	134217728	/* 128 ����� */
#define   	MAX_FILE_NAME_LEN	31	/* ����� ����� ������� ��� ���������� � '\0' */
#define		LOCK_FILE_NAME		"lock.fil"
#define		PARAM_FILE_NAME		"recparam.ini"
#define 	ERROR_LOG_NAME		"error.log"	/* ���� ������  */
#define 	LOADER_FILE_NAME	"ndas.bin"	/* ��� ����� ��������� */
#define 	PROGRAM_FILE_NAME	"/sys/mcuimg.bin"	/* ��� ����� ��������� � �������� ������� FLASH */
#define 	BOOT_LOG_NAME		"boot.log"	/* ���� ���� �������� */


/************************************************************************
 *      ��� ����������� ���������� �� ������ � ������ ������ 
 * 	������������ ��� ���������� SRAM (USE_THE_LOADER)
 * 	��� ������������ � ������ ������  
 ************************************************************************/
static FATFS fatfs;		/* File system object - ����� ������ �� global? ���! */
static DIR dir;			/* ���������� ��� ������� ��� ����� - ����� ������ �� global? */
static FIL log_file;		/* File object */
static FIL adc_file;		/* File object ��� ��� */
static FIL boot_file;		/* File object ��� ��� */
static ADC_HEADER adc_hdr;	/* ��������� ����� ������� ��� - 80 ���� */

static FATFS *pFatfs = &fatfs;
static DIR *pDir = &dir;
static FIL *pLogfile = &log_file;
static FIL *pAdcfile = &adc_file;
static FIL *pBootfile = &boot_file;
static ADC_HEADER *pAdc_hdr = &adc_hdr;



/**
 * ��� ������ ������� � ��� - �������� �����, ���� ������1 �� �������, �� �� RTC
 */
static void time_to_str(char *str)
{
    TIME_DATE t;
    u64 msec;
    char sym;
    int stat = CLOCK_RTC_TIME;

    msec = timer_get_msec_ticks();	/* �������� ����� �� ������� */
#if 0
    stat = get_clock_status();
#endif

    if (stat == CLOCK_RTC_TIME)
	sym = 'R';
    else if (stat == CLOCK_NO_GPS_TIME)
	sym = 'G';
    else if (stat == CLOCK_GPS_TIME)
	sym = 'P';
    else
	sym = 'N';		/* ��� ������� */

    /* ���������� ���� � �������: P: 09-07-2013 - 13:11:39.871  */
    if (sec_to_td(msec / TIMER_MS_DIVIDER, &t) != -1) {
	sprintf(str, "%c %02d-%02d-%04d %02d:%02d:%02d.%03d ", sym, t.day, t.mon, t.year, t.hour, t.min, t.sec, (u32) (msec % TIMER_MS_DIVIDER));
    } else {
	sprintf(str, "set time error ");
    }
}

/**
 * ����� �� �����, ��� ������� - ���� ��� ����� ������
 */
int log_term_printf(const char *fmt, ...)
{
    char buf[256];
    int ret = 0, i = 0;
    va_list list;
    RUNTIME_STATE_t t;

    /* �������� ���� */
    status_get_runtime_state(&t);

    /* ���� �������� ��������� ��� ����� ������ �� ������� */
    if (!t.acqis_running /*&& GNS_NORMAL_MODE != get_gns_work_mode() */ ) {

	/* �������� ������� ����� - MAX_TIME_STRLEN �������� � �������� - ������ ����� */
	time_to_str(buf);
	va_start(list, fmt);
	ret = vsnprintf(buf + MAX_TIME_STRLEN, sizeof(buf), fmt, list);
	va_end(list);

	if (ret < 0) {
	    return ret;
	}


	while (buf[i] != '\0') {
	    MAP_UARTCharPut(UARTA0_BASE, buf[i++]);
	}
    }
    return ret;
}

/* ����� ��� ������� */
int log_term_out(const char *fmt, ...)
{
    char buf[256];
    int ret = 0, i = 0;
    va_list list;
    RUNTIME_STATE_t t;

    /* �������� ���� */
    status_get_runtime_state(&t);

    /* ������� ���� �� �������� ��������� � �� � ������ Normal */
    if (!t.acqis_running /*&& GNS_NORMAL_MODE != get_gns_work_mode() */ ) {


	/* �������� ������� ����� - MAX_TIME_STRLEN �������� � �������� - ������ ����� */
	time_to_str(buf);
	va_start(list, fmt);
	ret = vsnprintf(buf, sizeof(buf), fmt, list);
	va_end(list);

	if (ret < 0) {
	    return ret;
	}

	while (buf[i] != '\0') {
	    MAP_UARTCharPut(UARTA0_BASE, buf[i++]);
	}
    }
    return ret;
}




/**
 * ������������� �������� �������
 * ��� ������������ ������ ������ � FLASH
 */
int log_mount_fs(void)
{
    int res;
    sd_card_mux_config();
    res = f_mount(pFatfs, "0", 1);	/* ��������� �� */
    return res;
}

/**
 * ����� ����������� ��� ���?
 */
bool log_check_mounted(void)
{
    return pFatfs->fs_type;
}


/**
 * �������� ������� ���-����� �� SD - ��� �����������-����� �� ����� WUSB 2 ������
 */
int log_check_lock_file(void)
{
    FIL lock_file;
    FRESULT res;		/* Result code */

    /* ��������� ������������ ���� */
    if (f_open(&lock_file, LOCK_FILE_NAME, FA_READ | FA_OPEN_EXISTING)) {
	return RES_NO_LOCK_FILE;	/* ���� ���� �� ����������! */
    }

    /* ���� ���� ���������� - ������ ��� */
    res = f_unlink(LOCK_FILE_NAME);
    if (res) {
	return RES_DEL_LOCK_ERR;	/* �� ������� */
    }
    return RES_NO_ERROR;	/* ���� ���� ���������� */
}


/**
 * �������� � ��� ����������
 */
int log_write_boot_file(char *fmt, ...)
{
    char str[256];
    FRESULT res;		/* Result code */
    unsigned bw;		/* ��������� ��� �������� ����  */
    int i, ret = 0;
    va_list p_vargs;		/* return value from vsnprintf  */

    /* �������� ������� ����� - MAX_TIME_STRLEN �������� � �������� - ������ ����� */
    time_to_str(str);		// �������� ����� ������
    va_start(p_vargs, fmt);
    i = vsnprintf(str + MAX_TIME_STRLEN, sizeof(str), fmt, p_vargs);
    va_end(p_vargs);
    if (i < 0)			/* formatting error?            */
	return RES_FORMAT_ERR;

    i = strlen(str);
    res = f_write(pBootfile, str, i, &bw);
    if (res) {
	return RES_WRITE_BOOTFILE_ERR;
    }

    /* ����������� �������! */
    res = f_sync(pBootfile);
    if (res) {
	return RES_WRITE_BOOTFILE_ERR;
    }

    return ret;
}

/**
 * ������� ��� ����������
 */
int log_close_boot_file(int r)
{
    char *str = "------------------------------\r\n";
    unsigned bw;

    if (r)
	log_write_boot_file("INFO: Umount fs and reboot...\r\n");
    else
	log_write_boot_file("INFO: Umount fs...\r\n");
    f_write(pBootfile, str, strlen(str), &bw);
    f_close(pBootfile);
    f_mount(0, 0, 0);
    return 0;
}


/**
 * ������� ��� ����������
 */
int log_open_boot_file(void)
{
    int ret, n;

    /* ���� ����������, ���� �������� ������� �� SD �����. ������� boot */
    ret = f_open(pBootfile, BOOT_LOG_NAME, FA_WRITE | FA_READ | FA_OPEN_ALWAYS);
    if (ret) {
	PRINTF("ERROR:Can't open file %s\n", BOOT_LOG_NAME);
	return RES_NO_LOCK_FILE;
    }

    /* ��������� ������ */
    n = f_size(pBootfile);
    if (n > MAX_LOG_FILE_LEN) {
	f_truncate(pBootfile);
	n = 0;
    }

    /* ���������� ��������� ����� */
    f_lseek(pBootfile, n);
    return 0;
}

/**
 * ���� �� SD ����� ���� ���� � ���������� - ������� ��� � ������� �� ���������� SPI FLASH 
 */
int log_check_loader_file(void)
{
    FIL file;
    int ret, hdl, offset = 0;
    unsigned bw;		/* ��������� ��� �������� ����  */
    unsigned long MaxSize = 128 * 1024;
    unsigned long token;
    long lRetVal;
    u32 mode;
    u8 buf[MAX_FILE_SIZE];	/* ����� ��� ������ ����� - 256 */

    /* ��� BOOT ����  */
    log_open_boot_file();

    /* ��������� ������������ ���� */
    if (f_open(&file, LOADER_FILE_NAME, FA_READ | FA_OPEN_EXISTING)) {
	log_write_boot_file("WARN: File %s does not exist on SD card\r\n", LOADER_FILE_NAME);
	log_close_boot_file(0);
	return RES_NO_LOCK_FILE;	// ���� ����� ��� - �� � ��� ������!
    }
    PRINTF("INFO: File %s on SD card open OK\r\n", LOADER_FILE_NAME);
    log_write_boot_file("INFO: File %s on SD card open OK\r\n", LOADER_FILE_NAME);

    do {
	/* ���� ����������, ��������� ���� �� FLASH � �������� ��� �������. ��� ������� ������������� / ��������/ ������� / ����� �������������? */
	mode = FS_MODE_OPEN_CREATE(MaxSize, _FS_FILE_OPEN_FLAG_COMMIT | _FS_FILE_PUBLIC_WRITE);
	lRetVal = sl_FsOpen((unsigned char *) PROGRAM_FILE_NAME, mode, &token, (_i32 *) & hdl);
	if (lRetVal < 0) {
	    // File may already be created
	    lRetVal = sl_FsClose(hdl, 0, 0, 0);
	    PRINTF("ERROR:File %s already be created\r\n");
	    break;
	} else {
	    // close the user file
	    lRetVal = sl_FsClose(hdl, 0, 0, 0);
	    //   PRINTF("INFO: Can't open file for commit. Try open for writing\r\n");
	    if (SL_RET_CODE_OK != lRetVal) {
		break;
	    }
	}

	//  open a user file for writing
	lRetVal = sl_FsOpen((unsigned char *) PROGRAM_FILE_NAME, FS_MODE_OPEN_WRITE, &token, (_i32 *) & hdl);
	if (lRetVal < 0) {
	    lRetVal = sl_FsClose(hdl, 0, 0, 0);
	    PRINTF("ERROR:Can't create File %s on SPI Flash\r\n", PROGRAM_FILE_NAME);
	    break;
	}

	PRINTF("INFO: File %s on SPI Flash was created OK\r\n", PROGRAM_FILE_NAME);

	// ������ � SD ����� � ����� �� FLASH   
	do {
	    ret = f_read(&file, buf, MAX_FILE_SIZE, &bw);
	    if (ret == FR_OK) {
		// ����� �� FLASH ������� ���������
		sl_FsWrite(hdl, offset, buf, bw);
		offset += bw;	// ������ ��������
	    } else {
		PRINTF("WARN: Nothing to read from SD\r\n");
		break;
	    }
	} while (bw > 0);
	PRINTF("INFO: %d bytes were written onto SPI Flash\r\n", offset);
	log_write_boot_file("INFO: %d bytes were written onto SPI Flash\r\n", offset);

	// ������� ���� �� FLASH
	sl_FsClose(hdl, 0, 0, 0);

	// ������ �� SD
	if (f_unlink(LOADER_FILE_NAME)) {
	    PRINTF("ERROR:Can't delete file %s on SD\r\n", LOADER_FILE_NAME);
	    return RES_CLOSE_LOADER_ERR;	/* �� ������� */
	} else {
	    PRINTF("INFO: Delete file %s on SD OK\r\n", LOADER_FILE_NAME);
	    log_write_boot_file("INFO: Delete file %s on SD OK\r\n", LOADER_FILE_NAME);
	    log_close_boot_file(1);
	    return RES_NO_ERROR;	/* ���� ���� ���������� */
	}
    } while (0);

    /* ������� ���� - �� ������ ���� �������! */
    if (f_close(&file)) {
	return RES_CLOSE_LOADER_ERR;
    }
    log_close_boot_file(0);
    return RES_OPEN_LOADER_ERR;
}


/**
 * �������� ������� ����� ����������� �� SD
 */
int log_check_reg_file(void)
{
    FIL file;

    /* ��������� ������������ ���� */
    if (f_open(&file, PARAM_FILE_NAME, FA_READ | FA_OPEN_EXISTING)) {
	return RES_OPEN_PARAM_ERR;
    }

    /* ������� ���� */
    if (f_close(&file)) {
	return RES_CLOSE_PARAM_ERR;
    }
    return RES_NO_ERROR;	/* ���� ���� ���������� */
}


/**
 * ������ ������ � ��� ����, ���������� ������� ��������. � �������� ������!
 * ����� �� 10 �����?
 */
int log_write_log_file(char *fmt, ...)
{
    char str[256];
    FRESULT res;		/* Result code */
    unsigned bw;		/* ��������� ��� �������� ����  */
    int i, ret = 0;
    va_list p_vargs;		/* return value from vsnprintf  */

    /* �� ����������� (��� �� - �������� �� PC), ��� � ��������� �������� */
    if (pLogfile->fs == NULL) {
	return RES_MOUNT_ERR;
    }

    /* �������� ������� ����� - MAX_TIME_STRLEN �������� � �������� - ������ ����� */
    time_to_str(str);		// �������� ����� ������
    va_start(p_vargs, fmt);
    i = vsnprintf(str + MAX_TIME_STRLEN, sizeof(str), fmt, p_vargs);

    va_end(p_vargs);
    if (i < 0)			/* formatting error?            */
	return RES_FORMAT_ERR;


    // ������� �������� ������ �� UNIX (�� � ������!)
    for (i = MAX_TIME_STRLEN + 4; i < sizeof(str) - 3; i++) {
	if (str[i] == 0x0d || str[i] == 0x0a) {
	    str[i] = 0x0d;	// ������� ������
	    str[i + 1] = 0x0a;	// Windows
	    str[i + 2] = 0;
	    break;
	}
    }

    /* ��������� ������ �� irq, ����� �� ����� � log */
//vvv:

    res = f_write(pLogfile, str, strlen(str), &bw);
    if (res) {
	return RES_WRITE_LOG_ERR;
    }

    /* ����������� �������! */
    res = f_sync(pLogfile);
    if (res) {
	return RES_SYNC_LOG_ERR;
    }

    /* �������� ������ �� irq */
////vvvvv:
    return ret;
}


/**
 * ������� ���-����
 */
int log_close_log_file(void)
{
    FRESULT res;		/* Result code */

    res = f_close(pLogfile);
    if (res) {
	return RES_CLOSE_LOG_ERR;
    }
    return RES_NO_ERROR;
}


/**
 * ������� ��������� - �� ����� �������������� ��� ����� ������ 
 * ���������� �� ����� main
 * ��������� ����� ������������!!!
 */
void log_create_adc_header(GPS_DATA_t * par)
{
    if (par != NULL) {
#if defined		ENABLE_NEW_SIVY
	strncpy(pAdc_hdr->DataHeader, "SeismicDat1\0", 12);	/* ��������� ������ SeismicDat1\0 - ����� Sivy */
	pAdc_hdr->HeaderSize = sizeof(ADC_HEADER);	/* ������ ���������     */
	pAdc_hdr->GPSTime = par->time;	/* ����� �������������: ����������� */

///vvvv:        pAdc_hdr->Drift = par->tAcc;    /* vvvvvvv: ����� �� ������ ����� GPS: ������������ ��� Accuraty */

	pAdc_hdr->Drift = timer_get_drift();
	pAdc_hdr->lat = par->lat;	/* ������: 55417872 = 5541.7872N */
	pAdc_hdr->lon = par->lon;	/* �������:37213760 = 3721.3760E */
#else
	char str[MAX_TIME_STRLEN];	// 26 ����
	TIME_DATE data;
	long t0;
	u8 ind;

	t0 = par->time / TIMER_NS_DIVIDER;
	sec_to_td(t0, &data);	/* ����� ������������� ���� */
	strncpy(pAdc_hdr->DataHeader, "SeismicData\0", 12);	/* ���� ID  */
	pAdc_hdr->HeaderSize = sizeof(ADC_HEADER);	/* ������ ���������     */

	///vvv: pAdc_hdr->Drift = par->drift * 32768;     /* ����� ��������� - ������� � ������������� �� ������� */

	mem_copy(&pAdc_hdr->GPSTime, &data, sizeof(TIME_DATE));	/* ����� ��������� ������ �� GPS */
	pAdc_hdr->NumberSV = 3;	/* ����� ���������: ����� ����� 3 */
	pAdc_hdr->params.coord.comp = true;	/* ������ �������  */

	/* ���������� ����� - ��� �� ��������� */
	if (par->lat == par->lon == 0) {
	    ind = 90;
	} else {
	    ind = 12;		// +554177+03721340000009
	}

	/* ���������� �������� ����������� */
	snprintf(str, sizeof(str), "%c%06d%c%07d%06d%02d", (par->lat >= 0) ? '+' : '-', abs(par->lat / 100), (par->lon >= 0) ? '+' : '-', abs(par->lon / 100),
		 0, ind);
	mem_copy(pAdc_hdr->params.coord.pos, str, sizeof(pAdc_hdr->params.coord.pos));

#endif
	log_write_log_file("INFO: Create ADS1282 header, lat: %d lon: %d\n", par->lat, par->lon);
    }
}



/**
 * ������� ���������. ���������� �� ����� ADS1282
 * ����� ������ ����������, ����� ����� � ������
 */
void log_fill_adc_header(char sps, u8 bitmap, u8 size)
{
    u32 block;

    pAdc_hdr->ConfigWord = sps + 1;	/* 8 ����������� (������� 125 �� + 3 ���� */
    pAdc_hdr->ChannelBitMap = bitmap;	/* ����� 1.2.3.4 ������ */
    pAdc_hdr->SampleBytes = size;	/* ������ 1 ������ �� ���� ���������� ��� ����� */
    block = 7500 * (1 << sps);	/* ������� ������� ����� �� 1 ������ (SPSxxx * 60) */

#if defined		ENABLE_NEW_SIVY
    pAdc_hdr->BlockSamples = block;	/* � ����� Sivy ������ �����  - 4 ���� */
#else

    pAdc_hdr->BlockSamples = block & 0xffff;
    pAdc_hdr->params.coord.rsvd0 = (block >> 16) & 0xff;
    pAdc_hdr->Board = 777;	/* ����� ����� ������ ���� */
    pAdc_hdr->Rev = 2;		/* ����� ������� �.�. 2 */
#endif

    log_write_log_file("INFO: Fill ADS1282 header\n");
}


/**
 * ��� ��������� ����� ��������. ���������� ���������� � ����������.
 * ����������� ��� � ������
 */
void log_change_adc_header(SENSOR_DATE_t * sens)
{
    if (sens != NULL) {
#if defined		ENABLE_NEW_SIVY
	pAdc_hdr->power_reg = sens->voltage;	/* ���������� �������, U mv */
	pAdc_hdr->curr_reg = 0;
	pAdc_hdr->power_mod = 0;
	pAdc_hdr->curr_mod = 0;

	pAdc_hdr->temper_reg = sens->temper;	/* ����������� ������������, ������� ���� ������� */
	pAdc_hdr->humid_reg = sens->humid;	/* ��������� */
	pAdc_hdr->press_reg = sens->press;	/* �������� ������ ����� */
	pAdc_hdr->pitch = sens->pitch;
	pAdc_hdr->roll = sens->roll;
	pAdc_hdr->head = sens->head;
#else
	int temp;
	/* ������������� �� �������� DataFormatProposal */
	pAdc_hdr->Bat = (int) sens->voltage * 1024 / 50000;	/* ����� ������� ��� ������� 12 ����� */

	/* ���� ���� ������� */
	if (sens->temper == 0 && sens->press == 0) {
	    temp = 200;		/* 20 �������� */
	} else {
	    temp = sens->temper;
	}
	pAdc_hdr->Temp = ((temp + 600) * 1024 / 5000);	/* ����� ������� ��� ����������� * 10 */

#endif
    }
/*    PRINTF("INFO: Change ADS1282 header OK\n"); */
}


/**
 * ��������� ��������� ������ ���, ����� - � ��� ����� ���������� ������ �����
 * ����� �� ������� ���� ��� ������ �������� ���������� � ���
 *
 * ������������� ��� �������!!!!! 
 */
int log_create_hour_data_file(u64 ns)
{
    char name[32];		/* ��� ����� */
    struct tm date;
    int res;			/* Result code */
    u32 sec = ns / TIMER_NS_DIVIDER;

    do {
	/* ���� ��� ���� ������� ���� - ������� ��� � �������� �����  */
	if (pAdcfile->fs) {
	    if ((res = f_close(pAdcfile)) != 0) {
		res = RES_CLOSE_DATA_ERR;
		break;
	    }
	}

	/* �������� ����� � ����� ������� - ��� ����� ��� �������� */
	if (sec_to_tm(sec, &date) != -1) {
	    GNS_PARAM_STRUCT par;
	    get_gns_start_params(&par);	/* �������� ���������  */


	    /* �������� ����� �� �������: ���-�����-����-����.������ */
	    snprintf(name, MAX_FILE_NAME_LEN, "%s/%02d%02d%02d%02d.%02d",
		     &par.gns_dir_name[0], ((date.tm_year - 100) > 0) ? (date.tm_year - 100) : 0, date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min);

	    /* ������� ����� */
	    res = f_open(pAdcfile, name, FA_WRITE | FA_CREATE_ALWAYS);
	    if (res) {
		res = RES_OPEN_DATA_ERR;
		break;
	    }
	    res = RES_NO_ERROR;	/* ��� OK */
	} else {
	    res = RES_FORMAT_TIME_ERR;
	}
    } while (0);
    return res;			/* ��� OK */
}


/**
 * ����������� � ���������� ������ ������ ��������� �� SD ����� �  ������ F_SYNC!
 * � ��������� ������ ������ ����� � ����� �������� ����� GPS 
 * ������������� ��� �������!!!!! 
 */
int log_write_adc_header_to_file(u64 ns)
{
    unsigned bw;		/* ��������� ��� �������� ����  */
    FRESULT res;		/* Result code */

#if defined		ENABLE_NEW_SIVY
    pAdc_hdr->SampleTime = ns;	/* ����� ������, ����������� */
    pAdc_hdr->GPSTime = status_get_gps_time();	/* ����� GPS � ������������ */
#else
    TIME_DATE date;

    u64 msec = ns / TIMER_US_DIVIDER;
    mem_copy(&pAdc_hdr->SedisTime, &msec, 8);	/* ����� ������������ ������ ��������� �� ����� Sedis */

    sec_to_td(msec / TIMER_MS_DIVIDER, &date);	/* �������� ������� � ����� � ����� ������� */
    mem_copy(&pAdc_hdr->SampleTime, &date, sizeof(TIME_DATE));	/* �������� ����� ������ ������ ������ */

    sec_to_td(status_get_gps_time() / TIMER_NS_DIVIDER, &date);	/* �������� ������� � ����� � ����� ������� */
    mem_copy(&pAdc_hdr->GPSTime, &date, sizeof(TIME_DATE));	/* �������� ����� GPS */

#endif

    /* ������ � ���� ��������� */
    res = f_write(pAdcfile, pAdc_hdr, sizeof(ADC_HEADER), &bw);
    if (res) {
	return RES_WRITE_HEADER_ERR;
    }

    /* ����������� �������! �� �������� ���� ���� �������� SD ����� */
    res = f_sync(pAdcfile);
    if (res) {
	return RES_SYNC_HEADER_ERR;
    }

    return RES_NO_ERROR;
}

/**
 * ��� �������� - ������ ����� ����������
 */
void log_change_num_irq_to_header(u64 num)
{
#if defined		ENABLE_NEW_SIVY
    pAdc_hdr->rsvd0 = (u32) num;	/* ���� ����� �� ����� rsvd0 */
#else
    pAdc_hdr->params.dword[6] = (u32) num;	/* ���� ����� �� ��� ����� */
#endif
}


/**
 * ������ � ���� ������ ���: ������ � ������ � ������
 * Sync ������ ��� � ������!
 * ������������� ��� �������!!!!! 
 */
int log_write_adc_data_to_file(void *data, int len)
{
    unsigned bw;		/* ��������� ��� �������� ����  */
    FRESULT res;		/* Result code */

    res = f_write(pAdcfile, (char *) data, len, &bw);
    if (res) {
	return RES_WRITE_DATA_ERR;
    }
    return RES_NO_ERROR;	/* �������� OK */
}


/**
 * ������� ���� ���-����� ���� ������� ������ �� ���� 
 */
int log_close_data_file(void)
{
    FRESULT res;		/* Result code */

    if (pAdcfile->fs == NULL)	// ��� ����� ���
	return RES_CLOSE_DATA_ERR;

    /* ����������� ������� */
    res = f_sync(pAdcfile);
    if (res) {
	return RES_CLOSE_DATA_ERR;
    }

    res = f_close(pAdcfile);
    if (res) {
	return RES_CLOSE_DATA_ERR;
    }
    return FR_OK;		/* ��� ���������! */
}


/**
 * ������� ini-���� �����������-���� ����������� ������,
 * ��������: ��������� �� ����� - ��������� �������  
 * �������:  ����� (0) ��� ��� (-1)
 * ���������� ����������� ��������� ������
 */
int log_read_reg_file(GNS_PARAM_STRUCT * reg)
{
    int n, ret = RES_NO_ERROR;	/* ret - ��������� ���������� */
    char buf[8];		/* �������� �������� ���������� */
    char temp;
    char str[32];
    struct tm dir_time;
    FRESULT res;		/* Result code */


#if 10
    do {
	/* ��������� ����� ����� ����� */
	n = ini_gets("Station", "GNS Number", "1", str, sizeof(str), PARAM_FILE_NAME);
	reg->gns_addr = atoi(str);	///ini_getl("Station", "GNS number", 1, PARAM_FILE_NAME);


	/* ������� ������ ����������� */
	reg->gns_start_time = ini_gettime("Times", "Start recording", 0, PARAM_FILE_NAME);

	/* ����� ������ ���������� (����������) ����� �� 10 ����� �� �����  */
	reg->gns_wakeup_time = (int) (reg->gns_start_time) - 120;

	/* ����� ��������� ����������� */
	reg->gns_finish_time = ini_gettime("Times", "End recording", 0, PARAM_FILE_NAME);

	/* 3. ��������� � ������� ������� ������ �������� */
	reg->gns_burn_time = ini_gettime("Times", "Burn wire start", 0, PARAM_FILE_NAME);

	/* 4 ����� �� ����� 125, 250, 500, 1000, 2000, 4000 */
	n = ini_gets("ADC", "Sampling rate", "125", str, sizeof(str), PARAM_FILE_NAME);
	reg->gns_adc_freq = atoi(str);

	/* ������ � ������� ������� */
	if ((reg->gns_adc_freq != 125) && (reg->gns_adc_freq != 250) && (reg->gns_adc_freq != 500)
	    && (reg->gns_adc_freq != 1000) && (reg->gns_adc_freq != 2000) && (reg->gns_adc_freq != 4000)) {
	    ret = RES_FREQ_PARAM_ERR;
	    break;
	}

	/* ������������� */
	n = ini_gets("ADC", "Mux", "Normal", str, sizeof(str), PARAM_FILE_NAME);
	if (strnicmp(str, "short", 5) == 0) {
	    reg->gns_mux = 1;
	} else if (strnicmp(str, "mvdd", 4) == 0) {
	    reg->gns_mux = 3;
	} else if (strnicmp(str, "temp", 4) == 0) {
	    reg->gns_mux = 4;
	} else if (strnicmp(str, "test", 4) == 0) {
	    reg->gns_mux = 5;
	} else {
	    reg->gns_mux = 0;
	}


	/* ����-������ */
	n = ini_gets("ADC", "Test signal", "external", str, sizeof(str), PARAM_FILE_NAME);
	if (strnicmp(str, "internal", 8) == 0) {
	    reg->gns_test_sign = 1;
	} else {
	    reg->gns_test_sign = 0;
	}


	/* ������� ����-������� */
	n = ini_gets("ADC", "Test freq", "none", str, sizeof(str), PARAM_FILE_NAME);
	if (strnicmp(str, "freq_0", 6) == 0) {
	    reg->gns_test_freq = 0;
	} else if (strnicmp(str, "freq_1", 6) == 0) {
	    reg->gns_test_freq = 1;
	} else if (strnicmp(str, "AT_DC", 5) == 0) {
	    reg->gns_test_freq = 3;
	} else {
	    reg->gns_test_freq = 2;
	}

	/* ��� � ��� �������� � �����������? */
	n = ini_gets("ADC", "Power consumption", "Hi", str, sizeof(str), PARAM_FILE_NAME);
	if (strnicmp(str, "Hi", 2) == 0) {
	    reg->gns_adc_consum = 1;
	} else if (strnicmp(str, "Lo", 2) == 0) {
	    reg->gns_adc_consum = 0;
	} else {
	    ret = RES_CONSUMP_PARAM_ERR;
	    break;
	}

	/* 2 ����� �� ����� PGA0 */
	n = ini_gets("ADC", "PGA0", "1", str, sizeof(str), PARAM_FILE_NAME);
	temp = atoi(str);
	if ((temp != 1) && (temp != 2) && (temp != 4)
	    && (temp != 8) && (temp != 12)) {
	    ret = RES_PGA_PARAM_ERR;
	    break;
	}
	reg->gns_adc_pga[0] = temp;

	/* 2 ����� �� ����� PGA1 */
	n = ini_gets("ADC", "PGA1", "1", str, sizeof(str), PARAM_FILE_NAME);
	temp = atoi(str);
	if ((temp != 1) && (temp != 2) && (temp != 4)
	    && (temp != 8) && (temp != 12)) {
	    ret = RES_PGA_PARAM_ERR;
	    break;
	}
	reg->gns_adc_pga[1] = temp;

	/* 2 ����� �� ����� PGA2 */
	n = ini_gets("ADC", "PGA2", "1", str, sizeof(str), PARAM_FILE_NAME);
	temp = atoi(str);
	if ((temp != 1) && (temp != 2) && (temp != 4)
	    && (temp != 8) && (temp != 12)) {
	    ret = RES_PGA_PARAM_ERR;
	    break;
	}
	reg->gns_adc_pga[2] = temp;

	/* 2 ����� �� ����� PGA3 */
	n = ini_gets("ADC", "PGA3", "1", str, sizeof(str), PARAM_FILE_NAME);
	temp = atoi(str);
	if ((temp != 1) && (temp != 2) && (temp != 4)
	    && (temp != 8) && (temp != 12)) {
	    ret = RES_PGA_PARAM_ERR;
	    break;
	}
	reg->gns_adc_pga[3] = temp;
	// ��������� �������������� = 1
	reg->gns_adc_pga[4] = reg->gns_adc_pga[5] = reg->gns_adc_pga[6] = reg->gns_adc_pga[7] = 1;

	/* ������������ ������ ���. ������ ������ */
	n = ini_gets("ADC", "Channels ON", "1111", str, sizeof(str), PARAM_FILE_NAME);
	/* 4 ����� �� ����� */
	reg->gns_adc_bitmap = 0;
	strncpy(buf, str, 5);

	/* ������� �� ���������  */
	if (buf[0] != '0') {
	    reg->gns_adc_bitmap |= 8;
	}
	if (buf[1] != '0') {
	    reg->gns_adc_bitmap |= 4;
	}
	if (buf[2] != '0') {
	    reg->gns_adc_bitmap |= 2;
	}
	if (buf[3] != '0') {
	    reg->gns_adc_bitmap |= 1;
	}


	/* ����� ������ ������� */
	n = ini_gets("WorkMode", "Mode", "COM", str, sizeof(str), PARAM_FILE_NAME);
	if (n >= 3) {

	    if (strnicmp(str, "Normal", 3) == 0) {
		reg->gns_work_mode = GNS_NORMAL_MODE;
	    } else {
		reg->gns_work_mode = GNS_CMD_MODE;	/* ��������� �����  */
	    }
	}

	/* � ����� ������ ���������� ��� ������� */
	//if (reg->gns_work_mode == GNS_CMD_MODE) {

	/* ��� � ��� �������� � �������� AP? */
	n = ini_gets("Network", "SSID name", "Test", str, sizeof(str), PARAM_FILE_NAME);
	if (n > 1 && n < 32) {
	    strncpy(reg->gns_ssid_name, str, strlen(str));
	}

	/* ������ AP? */
	n = ini_gets("Network", "Security key", "Test", str, sizeof(str), PARAM_FILE_NAME);
	if (n >= 4) {
	    strncpy(reg->gns_ssid_pass, str, strlen(str));
	}

	/* ��� ���������� */
	reg->gns_ssid_sec_type = ini_getl("Network", "Security type", 0, PARAM_FILE_NAME);

	/* ����� ����� UDP */
	reg->gns_server_udp_port = ini_getl("Network", "UDP port", 1000, PARAM_FILE_NAME);

	/* ����� ����� TCP */
	reg->gns_server_tcp_port = ini_getl("Network", "TCP port", 1000, PARAM_FILE_NAME);

	//}


	/* ������� ������ ����������, ���� ��� ����������� - ������ ���������� - 8 ���� */
	sprintf(str, "GNS%04d", reg->gns_addr);
	if (f_opendir(pDir, str)) {	/* Open the directory */

	    /* �� ������� ������� ����� � ��������� �������! */
	    if (f_mkdir(str)) {
		ret = RES_MKDIR_PARAM_ERR;
		break;
	    }
	}


	/* ������� ����� � ��������� ������� ������� */
	int reset = 0;
	sec_to_tm(reg->gns_start_time, &dir_time);

	    if (reset == PRCM_WDT_RESET) {	/* ������� ������ - WDT */

		/* ������� ������ ����������, ���� ��� ����������� - ������ ���������� */
		for (n = MAX_START_NUMBER - 1; n >= 0; n--) {
		    sprintf(reg->gns_dir_name, "%s/%02d%02d%02d%02d",
			    str,
			    ((dir_time.tm_year - 100) > 0) ? (dir_time.tm_year - 100) % MAX_START_NUMBER : 0, 
			     (dir_time.tm_mon + 1) % MAX_START_NUMBER,
			      dir_time.tm_mday % MAX_START_NUMBER, n % MAX_START_NUMBER);

		    res = f_opendir(pDir, reg->gns_dir_name);	/* Open the directory - ����� ��������� */
		    if (res == FR_OK) {
			break;
		    }
		}
	    } else {

		/* ������� ������ ����������, ���� ��� ����������� - ������ ���������� */
		for (n = MAX_START_NUMBER - 1; n >= 0; n--) {
		    sprintf(reg->gns_dir_name, "%s/%02d%02d%02d%02d",
			    str,
			    ((dir_time.tm_year - 100) >
			     0) ? (dir_time.tm_year - 100) % MAX_START_NUMBER : 0, (dir_time.tm_mon + 1) % MAX_START_NUMBER,
			    dir_time.tm_mday % MAX_START_NUMBER, n % MAX_START_NUMBER);

		    res = f_opendir(pDir, reg->gns_dir_name);	/* Open the directory */
		    if (res == FR_OK && n < (MAX_START_NUMBER - 1)) {
			n++;
			sprintf(reg->gns_dir_name, "%s/%02d%02d%02d%02d",
				str,
				((dir_time.tm_year - 100) >
				 0) ? (dir_time.tm_year - 100) % 100 : 0, (dir_time.tm_mon + 1) % MAX_START_NUMBER, dir_time.tm_mday % MAX_START_NUMBER,
				n % MAX_START_NUMBER);
			break;
		    }
		}

		if (n > (MAX_START_NUMBER - 1)) {
		    ret = RES_MAX_RUN_ERR;
		    break;
		}

		/* ������� ����� � ���������: ����� � ����� ������� */
		if ((res = f_mkdir(reg->gns_dir_name))) {
		    ret = RES_MKDIR_PARAM_ERR;
		    break;
		}
	    }


	    /* ������� ���� ���� � ���� ���������� ������, ������� ���� ���!  */
	    sprintf(str, "%s/GNS110.log", reg->gns_dir_name);
	    if ((res = f_open(pLogfile, str, FA_WRITE | FA_READ | FA_OPEN_ALWAYS))) {
		ret = RES_CREATE_LOG_ERR;
		break;
	    }

	    /* ��������� ������ */
	    n = f_size(pLogfile);
	    if (n >= MAX_LOG_FILE_LEN)
		f_truncate(pLogfile);

	    /* ���������� ��������� ����� */
	    f_lseek(pLogfile, n);

	}
	while (0);

#endif
	return ret;		/* ��� �������� - ����� ��� �������! */
    }
