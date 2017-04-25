//---------------------------------------------------------------------------
// Различные утилиты, коорые исполльзуются различными модулями
#include <vcl.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <time.h>
#pragma hdrstop
#pragma hdrstop
#include "utils.h"
#include "my_defs.h"
#include "main.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)



extern TfrmMain *frmMain;


// Завершить процесс
bool bTerminate = false;

static char TimeStringWithZero[32];
/*
  Name  : CRC-16
  Poly  : 0x8005    x^16 + x^15 + x^2 + 1
  Init  : 0xFFFF
  Revert: true
  XorOut: 0x0000
  Check : 0x4B37 ("123456789")
  MaxLen: 4095 байт (32767 бит) - обнаружение
  одинарных, двойных, тройных и всех нечетных ошибок
*/
#pragma pack(push, 2)
static const u16 Crc16Table[256] = {
    0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
    0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
    0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
    0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
    0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
    0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
    0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
    0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
    0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
    0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
    0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
    0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
    0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
    0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
    0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
    0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
    0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
    0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
    0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
    0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
    0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
    0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
    0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
    0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
    0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
    0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
    0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
    0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
    0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
    0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
    0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
    0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202
};
#pragma pack(pop)
//---------------------------------------------------------------------------
// Проверка контрольной суммы
u8 test_crc16(u8 * p)
{
 HEADER_t *mes;
 u8 res = 1;

    mes =  (HEADER_t *)p;

    if(mes->ndas == NDAS && /*mes->crc16 == 0xabcd &&*/ mes->rsvd0 == 0)
        res = 0;

  return res;

#if 0
    u8 k, len, ind;
    u16 crc = 0;		// returns 0 = OK; !0 = wrong

    len = (mes[0]) & 0xff;

    for (k = 0; k < len; k++) {
	ind = (u8) ((crc >> 8) & 0xff);
	crc = (u16) (((crc & 0xff) << 8) | (mes[k] & 0xff));
	crc ^= Crc16Table[ind];
    }
    return crc & 0xffffu;
#endif
}

/* Результат двухбайтовая КС, хранящаяся по адресу CK.  */
u16 _test_crc16(const u8 * data, u16 size)
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



//---------------------------------------------------------------------------
/* Добавляет контрольную суммы к сообщению cmd - правильное!!!*/
void add_crc16(u8 * buf)
{
    int i, len, ind;
    unsigned crc = 0;

    // length - 3 байт + 5
    len = buf[2] + 5;
    buf[len - 2] = buf[len - 1] = 0;

    for (i = 0; i < len; ++i) {
	ind = (crc >> 8) & 255;
	crc = ((crc << 8) + buf[i]) ^ Crc16Table[ind];
    }

    buf[len - 1] = (u8) (crc & 0xff);
    buf[len - 2] = (u8) ((crc >> 8) & 0xff);
}

//---------------------------------------------------------------------------
void sec_to_str(long ls, char *str)
{
    TIME_DATE t;

    /* Записываем дату в формате: [08-11-2012 08:57:22] */
    if (sec_to_td(ls, &t) != -1) {
	sprintf(str, "%02d.%02d.%04d %02d:%02d:%02d", t.day, t.mon, t.year, t.hour, t.min, t.sec);
    } else {
	sprintf(str, "[set time error] ");
    }
}

//---------------------------------------------------------------------------
void ver_to_str(DEV_STATUS_STRUCT * addr, char *str)
{
    TIME_DATE t;

    //ver: 12.06.13 11.24

    if (sec_to_td(addr->comp_time, &t) != -1) {
	sprintf(str, "%d.%03d.%02d%02d%02d%02d%02d", addr->ver, addr->rev, t.day, t.mon, t.year - 2000, t.hour, t.min);
    } else {
	sprintf(str, "0000");
    }
}


//---------------------------------------------------------------------------
void sec_to_simple_str(long ls, char *str)
{
    int h, m, s;

    s = ls % 60;
    m = (ls / 60) % 60;
    h = ls / 3600;

    /* Записываем дату в формате: [08:57:22] */
    sprintf(str, "%#4d:%02d:%02d (%d sec)", h, m, s, ls);
}

//---------------------------------------------------------------------------
long td_to_sec(TIME_DATE * t)
{
    long r;
    char* tz;
    struct tm tm_time;
    char buf[128];

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
    EnterCriticalSection(&frmMain->time_cs);
    tz = getenv("TZ");
    putenv("TZ=UTC0");
    tzset();

    r = mktime(&tm_time);

    if(tz)  {
       sprintf(buf,"TZ=%s", tz);
       putenv(buf);
    } else {
       putenv("TZ=");
     }
    tzset();
    LeaveCriticalSection(&frmMain->time_cs);
    return r;			/* -1 или нормальное число  */
}
//---------------------------------------------------------------------------
// Время windows в секунды UNIX
int st_to_sec(SYSTEMTIME * t)
{
    long r;
    TIME_DATE td;
    struct tm tm_time;
    char* tz = NULL;
    char buf[128];

    td.sec = t->wSecond;
    td.min = t->wMinute;
    td.hour = t->wHour;
    td.day = t->wDay;
    td.mon = t->wMonth;	/* В SYSTEMTIME месяцы с 1 по 12 */
    td.year = t->wYear;

    r = td_to_sec(&td);    /* Делаем секунды */
    return r;			/* -1 или нормальное число  */
}
//---------------------------------------------------------------------------
int sec_to_td(long ls, TIME_DATE * t)
{
    struct tm *tm_ptr;

    if ((int) ls != -1) {
	tm_ptr = gmtime(&ls);

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

//---------------------------------------------------------------------------
String TimeString(void)
{
    SYSTEMTIME st;

    GetSystemTime(&st);

    // 09-07-2013 - 13:11:39.871
    sprintf(TimeStringWithZero, "%02d.%02d.%04d - %02d:%02d:%02d.%03d ",
            st.wDay, st.wMonth, st.wYear, st.wHour,
            st.wMinute, st.wSecond, st.wMilliseconds);
    return String(TimeStringWithZero);
}

//---------------------------------------------------------------------------
// Секунды UNIX во время Windows
void sec_to_st(int ls, SYSTEMTIME * t)
{
    struct tm *tm_ptr;

    if (ls != -1) {
	tm_ptr = gmtime((long *) &ls);

	/* Записываем дату, что получилось */
	t->wSecond = tm_ptr->tm_sec;
	t->wMinute = tm_ptr->tm_min;
	t->wHour = tm_ptr->tm_hour;
	t->wDay = tm_ptr->tm_mday;
	t->wMonth = tm_ptr->tm_mon + 1;	/* В Systemtime месяцы с 1 по 12, в tm_time с 0 по 11 */
	t->wYear = tm_ptr->tm_year + 1900;	/* В SYSTEM годы считаются с 0 - го */
    }
}

//---------------------------------------------------------------------------
/* Строку в верхний регистр   */
void str_to_cap(char *str, int len)
{
    int i = len;

    while (i--)
	str[i] = (str[i] > 0x60) ? str[i] - 0x20 : str[i];
}


//---------------------------------------------------------------------------
/*  Получить номер устройства из буфера */
s16 am3_get_dev_num(char *rx_buf)
{
    char str[32];
    short res = -1;
    int i = 0;

    if (rx_buf[6] != ',')
	return -1;

    // ищем 1-ю запятую: $AGANM,0021,00*6327 - не до конца буфера!!!
    // копируем 4 символа после запятой
    strncpy(str, (const char *) rx_buf + 7, 4);
    str[4] = 0;
    res = atoi(str);
    return res;
}

//---------------------------------------------------------------------------
/*  Получить часы реального времени */
int am3_get_modem_time(char *rx_buf, SYSTEMTIME * st)
{
    char str[32];
    char buf[8];
    int res = -1;
    int i;

    if (st != NULL && rx_buf[6] == ',') {

	// ищем 1-ю запятую: $AGACL,21.05.13,11:07,00*7cdd
	strncpy(str, rx_buf + 7, 14);	// Разбираем буфер прям здесь от запятой

	// день [1,31]
	memcpy(buf, str, 2);
	buf[2] = 0;
	st->wDay = atoi(buf);
	if (st->wDay == 0 || st->wDay > 31)
	    return res;

	// месец
	memcpy(buf, str + 3, 2);
	buf[2] = 0;
	st->wMonth = atoi(buf);
	if (st->wMonth == 0 || st->wMonth > 12)
	    return res;


	// Год
	memcpy(buf, str + 6, 2);
	buf[2] = 0;
	st->wYear = atoi(buf) + 2000;
	if (st->wYear == 0 || st->wYear > 2038)
	    return res;


	// Чисы
	memcpy(buf, str + 9, 2);
	buf[2] = 0;
	st->wHour = atoi(buf);
	if (st->wHour > 24)
	    return res;


	// минуты
	memcpy(buf, str + 12, 2);
	buf[2] = 0;
	st->wMinute = atoi(buf);
	if (st->wMinute > 60)
	    return res;


	// Секунд нету
	st->wSecond = 0;

	// Для проверки пробуем преобразовать
	res = st_to_sec(st);
    }
    return res;
}


/* Получить аварийное время всплытия */
int am3_get_alarm_time(char *rx_buf, SYSTEMTIME * st)
{
    char str[32];
    char buf[8];
    int res = -1, i;

    if (st != NULL && rx_buf[6] == ',') {
	strncpy(str, rx_buf + 7, 14);	// от запятой

	// день [1,31]
	memcpy(buf, str, 2);
	buf[2] = 0;
	st->wDay = atoi(buf);

	// месец
	memcpy(buf, str + 3, 2);
	buf[2] = 0;
	st->wMonth = atoi(buf);

	// Год
	memcpy(buf, str + 6, 2);
	buf[2] = 0;
	st->wYear = atoi(buf) + 2000;

	// Чисы
	memcpy(buf, str + 9, 2);
	buf[2] = 0;
	st->wHour = atoi(buf);

	// минуты
	memcpy(buf, str + 12, 2);
	buf[2] = 0;
	st->wMinute = atoi(buf);

	// Секунд нету
	st->wSecond = 0;

	// Для проверки пробуем преобразовать
	res = st_to_sec(st);
    }
    return res;
}


/* Получить светлое время суток */
int am3_get_cal_time(char *rx_buf, u16 * h0, u16 * m0, u16 * h1, u16 * m1)
{
    char str[64];
    char buf[8];

    if (rx_buf[6] != ',')
	return -1;

    /* Разбираем буфер прям здесь! */
    strncpy(str, (char *) rx_buf + 7, 14);	// от запятой

    // часы
    memcpy(buf, str, 2);
    buf[2] = 0;
    *h0 = atoi(buf);

    // минуты
    memcpy(buf, str + 3, 2);
    buf[2] = 0;
    *m0 = atoi(buf);

    // Чясы      
    memcpy(buf, str + 6, 2);
    buf[2] = 0;
    *h1 = atoi(buf);

    // минуты
    memcpy(buf, str + 9, 2);
    buf[2] = 0;
    *m1 = atoi(buf);

    return 0;
}

/* время всплытия  */
s16 am3_get_time_for_release(char *rx_buf)
{
    char str[32];
    short res = -1;
    int i = 0;

    if (rx_buf[6] != ',')
	return -1;

    // копируем 3 символа после запятой
    strncpy(str, (const char *) rx_buf + 7, 3);
    str[3] = 0;
    res = atoi(str);
    return res;
}



int am3_get_burn_len(char *rx_buf)
{
    char str[32];
    short res = -1;
    int i = 0;

    if (rx_buf[6] != ',')
	return -1;

    // ищем 1-ю запятую: $AGABT,0021,00*xxxx
    // копируем 4 символа после запятой
    strncpy(str, (const char *) rx_buf + 7, 4);
    str[4] = 0;
    res = atoi(str);
    return res;
}

// разобрать строку NMEA
int parse_nmea_string(char *str, void *p)
{
    NMEA_params *info;
    char buf[32];
    int i, count, pos0, pos1;

    if (str == NULL || p == NULL)
	return -1;

    if(strstr(str, "$GPRMC") == NULL && strstr(str, "$GNRMC") == NULL)
       return -1;

    info = (NMEA_params *) p;

    // $GPRMC,105105.000,A,5541.7967,N,03721.3407,E,0.39,0.00,261212,10.0,E,A*37
    // $GPRMC,111020.000,A,5541.7895,N,03721.3721,E,0.06,0.00,011013,10.0,E,A*35
    // ищем 1-ю запятую
    // $GPRMC,105105.000,A,5541.7967,N,03721.3407,E,0.39,0.00,261212,10.0,E,A*37
    // A*37 — индикатор режима: A — автономный, D — дифференциальный, E — аппроксимация, N — недостоверные данные
    for (i = 0; i < NMEA_PACK_SIZE - 20; i++) {
	if (str[i] == 0x2c) {
           pos0 = i + 1;
           break;
	}
    }

    memcpy(buf, str + pos0, 2);
    buf[2] = 0;
    info->utc.wHour = atoi(buf);

    memcpy(buf, str + pos0 + 2, 2);
    buf[2] = 0;
    info->utc.wMinute = atoi(buf);

    memcpy(buf, str + pos0 + 4, 2);
    buf[2] = 0;
    info->utc.wSecond = atoi(buf);


    // ищем 9-ю запятую
    // $GPRMC,105105.000,A,5541.7967,N,03721.3407,E,0.39,0.00,261212,10.0,E,A*37
    // $GPRMC,111709.000,A,5541.7887,N,03721.3808,E,0.02,0.00,011013,10.0,E,A*3A
    // A*37 — индикатор режима: A — автономный, D — дифференциальный, E — аппроксимация, N — недостоверные данные
    count = 0;
    for (i = 0; i < NMEA_PACK_SIZE; i++) {
	if (str[i] == 0x2c) {
	    count++;
	}
	if (count == 9) {
	    pos0 = i + 1;
	    break;
	}
    }
    memcpy(buf, str + pos0, 2);
    buf[2] = 0;              // день
    info->utc.wDay= atoi(buf);


    memcpy(buf, str + pos0 + 2, 2);
    buf[2] = 0;
    info->utc.wMonth = atoi(buf);


    memcpy(buf, str + pos0 + 4, 2);
    buf[2] = 0;
    info->utc.wYear = atoi(buf) + 2000;

     // Позиция

    // ищем 3-ю запятую
    // $GPRMC,105105.000,A,5541.7967,N,03721.3407,E,0.39,0.00,261212,10.0,E,A*37
    count = 0;
    for (i = 0; i < NMEA_PACK_SIZE; i++) {
	if (str[i] == 0x2c) {
	    count++;
	}
	if (count == 3) {
	    pos0 = i + 1;
	    break;
	}
    }

    // и 4-ю запятую!
    // $GPRMC,105105.000,A,5541.7967,N,03721.3407,E,0.39,0.00,261212,10.0,E,A*37
    count = 0;
    for (i = 0; i < NMEA_PACK_SIZE; i++) {
	if (str[i] == 0x2c) {
	    count++;
	}
	if (count == 4) {
	    pos1 = i;
	    break;
	}
    }
   // Определим сколько цыфр занимает широта!
    memcpy(buf, str + pos0, pos1 - pos0);
    buf[pos1 - pos0] = 0;
    if(str[pos1 + 1] == 'N')
       info->lat = atof(buf);
    else
       info->lat = -atof(buf);

///////
    // ищем 5-ю запятую
    // $GPRMC,105105.000,A,5541.7967,N,03721.3407,E,0.39,0.00,261212,10.0,E,A*37
    count = 0;
    for (i = 0; i < NMEA_PACK_SIZE; i++) {
	if (str[i] == 0x2c) {
	    count++;
	}
	if (count == 5) {
	    pos0 = i + 1;
	    break;
	}
    }

    // и 6-ю запятую!
    // $GPRMC,105105.000,A,5541.7967,N,03721.3407,E,0.39,0.00,261212,10.0,E,A*37
    count = 0;
    for (i = 0; i < NMEA_PACK_SIZE; i++) {
	if (str[i] == 0x2c) {
	    count++;
	}
	if (count == 6) {
	    pos1 = i;
	    break;
	}
    }
   // Определим сколько цыфр занимает широта!
    memcpy(buf, str + pos0, pos1 - pos0);
    buf[pos1 - pos0] = 0;
    if(str[pos1 + 1] == 'E')
       info->lon = atof(buf);
    else
       info->lon = -atof(buf);

    return 0;
}
//-----------------------------------------------------------------------------------
int bufs_to_st(char* b1, char* b2, SYSTEMTIME* st)
{
   char buf[32];

  //день
   memcpy(buf, b1, 2);
   buf[2] = 0;
   st->wDay = atoi(buf);

  //месец
   memcpy(buf, b1 + 3, 2);
   buf[2] = 0;
   st->wMonth = atoi(buf);

  //Год
   memcpy(buf, b1 + 6, 2);
   buf[2] = 0;
   st->wYear = atoi(buf) + 2000;


  //чесы
   memcpy(buf, b2, 2);
   buf[2] = 0;
   st->wHour = atoi(buf);

  //месец
   memcpy(buf, b2 + 3, 2);
   buf[2] = 0;
   st->wMinute = atoi(buf);

  //Год
   memcpy(buf, b2 + 6, 2);
   buf[2] = 0;
   st->wSecond = atoi(buf);

   return st_to_sec(st);
}
//-----------------------------------------------------------------------------------
#define ERROR_DROP_COUNT        1

// буфер данных
static XCHG_ERR_COUNT err_buf[ERROR_DROP_COUNT];

void filt_ok_and_error(XCHG_ERR_COUNT *in, XCHG_ERR_COUNT *out)
{
    static u32 count = 0;
    int i, t1 = 0, r1 = 0;

    /* Забиваем ошыбки в отсчеты */
    err_buf[count % ERROR_DROP_COUNT].tx_ok  = in->tx_ok;	/* Собираем счетчики */
    err_buf[count % ERROR_DROP_COUNT].rx_err = in->rx_err;	/* Собираем ошибки */
    count++;

    // считаем фильтыр
    for (i = 0; i < ERROR_DROP_COUNT; i++) {
	t1 += err_buf[i].tx_ok;
	r1 += err_buf[i].rx_err;
    }

    out->tx_ok  = t1 / ERROR_DROP_COUNT;
    out->rx_err = r1 / ERROR_DROP_COUNT;
}

//-----------------------------------------------------------------------------------
void clr_xchg_err(void)
{
    for (int i = 0; i < ERROR_DROP_COUNT; i++)
	err_buf[i].rx_err = 0;
}
//-----------------------------------------------------------------------------------
// Получить миллисекунды
s64 get_msec_ticks(void)
{
   SYSTEMTIME st;
   s64 msec;

   GetSystemTime(&st);

  // Получим время с миллисекундами
   msec = st_to_sec(&st) * (s64)1000 + st.wMilliseconds;

   return msec;
}
//-----------------------------------------------------------------------------------
// Получить секунды
long get_sec_ticks(void)
{
   SYSTEMTIME st;
   long sec;

   GetSystemTime(&st);

  // Получим время с миллисекундами
   sec = st_to_sec(&st);
   return sec;
}
//-----------------------------------------------------------------------------------
// Градусы в строку
void grad_to_str(s32 grad, char* str)
{
 char sign;
 int div = 10000000;
 u32 data;
 s16 g;
 f32 min;
 data = abs(grad);
 sign = grad < 0? '-' : ' ';

 g = data / div;
 min = (data - g * div) * 60.0 / div;

 if(grad < 0) {
 	sprintf(str, "%c%#3d° %02.04f'", sign, g, min);
 } else {
	 sprintf(str, "%#3d° %02.04f'", g, min);
 }
}
//-----------------------------------------------------------------------------------


