/******************************************************************************
 * Функции перевода дат, проверка контрольной суммы т.ж. здесь 
 * Все функции считают время от начала Эпохи (01-01-1970)
 * Все функции с маленькой буквы
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <utils.h>
#include <hw_types.h>
#include <rom.h>
#include <rom_map.h>
#include <prcm.h>

#include "main.h"
#include "userfunc.h"
#include "log.h"
#include "timtick.h"
#include "my_defs.h"
#include "my_ver.h"

/* Константы, пусть живут в ROM */
static const char *monthes[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };


/**
 * Переводит секунды (time_t) с начала Эпохи в формат struct tm
 */
int sec_to_tm(long ls, struct tm *time)
{
    struct tm *tm_ptr;

    if ((int) ls != -1 && time != NULL) {
	tm_ptr = gmtime((time_t *) & ls);

	/* Записываем дату, что получилось */
	mem_copy(time, tm_ptr, sizeof(struct tm));
	return 0;
    } else
	return -1;
}

/**
 * Время struct tm в секунды 
 */
long tm_to_sec(struct tm *tm_time)
{
    long r;

    /* Делаем секунды */
    r = mktime(tm_time);
    return r;			/* -1 или нормальное число  */
}


/**
 * Переводит секунды (time_t) с начала Эпохи в формат TIME_DATE
 */
int sec_to_td(long ls, TIME_DATE * t)
{
    struct tm *tm_ptr;

    if ((int) ls != -1) {
	tm_ptr = gmtime((time_t *) & ls);

	/* Записываем дату, что получилось */
	t->sec = tm_ptr->tm_sec;
	t->min = tm_ptr->tm_min;
	t->hour = tm_ptr->tm_hour;
	t->day = tm_ptr->tm_mday;
	t->mon = tm_ptr->tm_mon + 1;	/* В TIME_DATE месяцы с 1 по 12, в tm_time с 0 по 11 */
	t->year = tm_ptr->tm_year + 1900;	/* В tm годы считаются с 1900 - го, в TIME_DATA с 0-го */
	return 0;
    } else
	return -1;
}


/**
 * Переводит текущее время и год в секунды с начала Эпохи
 * int t_year,t_mon,t_day; - текущая дата
 * int t_hour,t_min,t_sec; - текущее время
 */
long td_to_sec(TIME_DATE * t)
{
    long r;
    struct tm tm_time;

    tm_time.tm_sec = t->sec;
    tm_time.tm_min = t->min;
    tm_time.tm_hour = t->hour;
    tm_time.tm_mday = t->day;
    tm_time.tm_mon = t->mon - 1;	/* В TIME_DATE месяцы с 1 по 12, в tm_time с 0 по 11 */
    tm_time.tm_year = t->year - 1900;	/* В tm годы считаются с 1900 - го */
    tm_time.tm_wday = 0;	/* Не используются */
    tm_time.tm_yday = 0;
    tm_time.tm_isdst = 0;

    /* Делаем секунды */
    r = mktime(&tm_time);
    return r;			/* -1 или нормальное число  */
}



/* Строку в верхний регистр   */
void str_to_cap(char *str, int len)
{
    int i = len;

    while (i--)
	str[i] = (str[i] > 0x60) ? str[i] - 0x20 : str[i];
}

/***********************************************************************************
 * Какой у процессора ENDIAN: 1 - Big-endian, 0 - Little-endian. В проверке наоборот 
 ***********************************************************************************/
int get_cpu_endian(void)
{
    int i = 1;
    if (*((u8 *) & i) == 0)	/* Big-endian */
	return 1;
    else
	return 0;		/* Little-endian */
}


/**
 * Перевести в дату 
 */
bool parse_date_time(char *str_date, char *str_time, TIME_DATE * time)
{
    char buf[5];
    int i, len, x;

    len = strlen(str_date);
    str_to_cap(str_date, len);	// в верхний регистр

    for (i = 0; i < len; i++)
	if (isalpha(str_date[i]))	// найдем первую букву
	    break;

    if (i >= len)
	return false;


    memset(buf, 0, 5);
    strncpy(buf, str_date + i, 3);	// 3 символа скопировали
    for (i = 0; i < 12; i++) {
	if (strncmp(buf, monthes[i], 3) == 0) {
	    time->mon = i + 1;	// месяц нашли
	    break;
	}
    }
    if (time->mon > 12)
	return false;


    /* ищем первую цыфру */
    for (i = 0; i < len; i++)
	if (isdigit(str_date[i])) {
	    x = i;
	    break;
	}

    memset(buf, 0, 5);
    strncpy(buf, str_date + i, 2);	// 2 символа
    time->day = atoi(buf);

    if (time->day > 31)
	return false;

    // ищем год, пробел после цыфр
    for (i = x; i < len; i++) {
	if (str_date[i] == 0x20) {
	    break;
	}
    }
    strncpy(buf, str_date + i + 1, 4);	// 4 символа
    time->year = atoi(buf);

    if (time->year < 2013)
	return false;


    /* 3... Разбираем строку времени */
    memset(buf, 0, 5);

    /* часы */
    strncpy(buf, str_time, 2);
    time->hour = atoi(buf);

    if (time->hour > 24)
	return false;


    /* минуты */
    strncpy(buf, str_time + 3, 2);
    time->min = atoi(buf);
    if (time->min > 60)
	return false;


    /* секунды */
    strncpy(buf, str_time + 6, 2);
    time->sec = atoi(buf);
    if (time->sec > 60)
	return false;

    return true;
}



/**
 * Вывести параметры АЦП
 */
void print_ads131_parms(GNS_PARAM_STRUCT * par)
{

    log_write_log_file(">>>>>>>>>>ADS1282 parameters<<<<<<<<<\n");
    log_write_log_file("INFO: Sampling freq:  %####d\n", par->gns_adc_freq);
    log_write_log_file("INFO: Mode:           %s\n", par->gns_adc_consum == 1 ? "High res mode" : "Low power mode");
    log_write_log_file("INFO: PGA0:            %####d\n", par->gns_adc_pga[0]);
    log_write_log_file("INFO: PGA1:            %####d\n", par->gns_adc_pga[1]);
    log_write_log_file("INFO: PGA2:            %####d\n", par->gns_adc_pga[2]);
    log_write_log_file("INFO: PGA3:            %####d\n", par->gns_adc_pga[3]);


    /* Глюк компилятора по-видимому или мой */
    if (par->gns_adc_bitmap == 0) {
	log_write_log_file("ERROR: bitmap param's invalid\n");
	log_write_log_file("INFO: All channels will be turned on\n");
	par->gns_adc_bitmap = 0x0f;
    }

    log_write_log_file("INFO: Use channels:   4: %s, 3: %s, 2: %s, 1: %s\n",
		       (par->gns_adc_bitmap & 0x08) ? "On" : "Off",
		       (par->gns_adc_bitmap & 0x04) ? "On" : "Off",
		       (par->gns_adc_bitmap & 0x02) ? "On" : "Off", (par->gns_adc_bitmap & 0x01) ? "On" : "Off");
    log_write_log_file("INFO: File length %d hour(s)\n", 4);
}

/**
 * Вывести все заданные времена
 */
void print_set_times(GNS_PARAM_STRUCT * time)
{
    char str[32];
    long t;

    if (time != NULL) {

	log_write_log_file(">>>>>>>>>>>>>>>>  Times  <<<<<<<<<<<<<<<<<\n");
	t = timer_get_sec_ticks();
	sec_to_str(t, str);
	log_write_log_file("INFO: now time is:\t\t%s\n", str);

	/* не показывать 1970-й год */
	if (time->gns_start_time > 1) {
	    sec_to_str(time->gns_start_time, str);
	    log_write_log_file("INFO: registration starts at:\t%s\n", str);
	}

	/* Если НЕ стоит "постоянная регистрация" - не показывать 1970-й год */
	if (time->gns_finish_time > 1) {
	    sec_to_str(time->gns_finish_time, str);
	    log_write_log_file("INFO: registration finish at:\t%s\n", str);
	}
	/* не показывать 1970-й год */
	if (time->gns_burn_time > 1) {
	    sec_to_str(time->gns_burn_time, str);
	    log_write_log_file("INFO: burn wire at:\t\t%s\n", str);
	}
	/* не показывать 1970-й год */
	if (time->gns_popup_time > 1) {
	    sec_to_str(time->gns_popup_time, str);
	    log_write_log_file("INFO: popup time  at:\t\t%s\n", str);
	}
	sec_to_str(time->gns_gps_time, str);
	log_write_log_file("INFO: turn on GPS at:\t\t%s\n", str);
    } else {
	log_write_log_file("ERROR: print times");
    }
}


/**
 * Записываем дату в формате: 08-11-15 - 08:57:22 
 */
int sec_to_str(long ls, char *str)
{
    struct tm t0;
    int res = 0;

    if (sec_to_tm((long)ls, &t0) != -1) {
	sprintf(str, "%02d-%02d-%04d - %02d:%02d:%02d",	t0.tm_mday, t0.tm_mon + 1, t0.tm_year + 1900, t0.tm_hour, t0.tm_min, t0.tm_sec);
    } else {
	sprintf(str, "[set time error]");
	res = -1;
    }
    return res;
}

/*************************************************************
 * Вывод данных в HEX на экран
 *************************************************************/
void print_data_hex(void * data, int len)
{
    int i;
    for (i = 0; i < len; i++) {
	if (i % 8 == 0 && i != 0)
	    log_term_out(" ");
	if (i % 16 == 0 && i != 0 && i != 8)
	    log_term_out("\n");
	log_term_out("%02X ", *(((u8*)data) + i));
    }
    log_term_out("\n");
}


/*************************************************************
 * Вывод версии - длина строки не проверяется!!!
 *************************************************************/
void get_str_ver(char* str)
{
 TIME_DATE t;
 long ls;
 
 /* Получили время компиляции */
 ls = get_comp_time();  
 sec_to_td(ls, &t);
 sprintf(str, "%d.%02d from %02d.%02d.%02d %02d:%02d:%02d",
   prog_ver, prog_rev,  t.day, t.mon, t.year - 2000, t.hour, t.min, t.sec);
}

/**
 * Получить время компиляции
 */
long get_comp_time(void)
{
    TIME_DATE d;
    long t0 = 0;
    char date[32], time[32];

    sprintf(date, "%s", __DATE__);
    sprintf(time, "%s", __TIME__);

    if (parse_date_time(date, time, &d) == true) {
	t0 = td_to_sec(&d);
    }
    return t0;
}

/* Результат двухбайтовая КС, хранящаяся по адресу CK.  */
u16 check_crc(const u8 * data, u16 size)
{
    u8 a = 0, b = 0;
    u16 i, crc;

    for(i = 0; i < size; i++) {
        a = a + *data;
        b = b + a;
        data++;
    }
    crc = a + ((u16)b << 8);

    return crc;
}

/**
 * Fast inverse square-root  
 * See: http://en.wikipedia.org/wiki/Fast_inverse_square_root
 */
f32 inv_sqrt(f32 x)
{
	unsigned int i = 0x5F1F1412 - (*(unsigned int *) &x >> 1);
	f32 tmp = *(f32 *) & i;
	f32 y = tmp * (1.69000231 - 0.714158168 * x * tmp * tmp);
	return y;
}

/* Получить значение для подачи на ШИМ */
u16 get_pwm_ctrl_value(u32 sum, u16 old)
{
    long err;
    int new;

    /* Сколькор насчитал таймер за 16 секунд  */
    err = sum - 16384000;

    new = -err / 4 + old;
//    new = (-err) * 1024 / coef + old;

    /* против переполнения */
    if (new > 4000) {
	new = 4000;
    }    
    if (new < 10) {
	new = 10;
    } 

    return  new;
}
