#ifndef _USERFUNC_H
#define _USERFUNC_H

#include <hw_types.h>
#include <rom.h>
#include <rom_map.h>
#include <prcm.h>
#include <utils.h>
#include "my_defs.h"

/* Вместо fabs() - не забудь поставить скобки  */
#define		ABS(x)		(((x) > 0)? (x) : (-x))

/**
 * Переставим байты. Не нашел такую ф-ю.
 */

#ifndef _WIN32
IDEF uint32_t byteswap4(u32 val)
{
 u32 res;

/* res = ((val << 24) & 0xff000000) |  ((val << 16) & 0x00ff0000) |  ((val >> 24) & 0x000000ff) |  ((val >> 16) & 0x0000ff00); */


  __asm ("	rev %0, %1\n"
       : "=r"(res)
       : "r" (val));

 return(res);
}


IDEF u16 byteswap2(u16 val)
{
 return ((val >> 8) & 0xFF) | ((val << 8) & 0xFF00);
}
#endif


#define delay_us(x)	MAP_UtilsDelay(20 * x)
#define delay_ms(x)	MAP_UtilsDelay(20000 * x)

/* До 100 секунд только! */
#define delay_sec(x)	MAP_UtilsDelay(x * 1000 * 20000)



int  sec_to_tm(long, struct tm *);
int  sec_to_td(long, TIME_DATE *);
int  sec_to_str(long, char *);

long tm_to_sec(struct tm *);
long td_to_sec(TIME_DATE *);
void str_to_cap(char *str, int len);
int  get_cpu_endian(void);
void print_result(char* str);
bool parse_date_time(char *, char *, TIME_DATE *);
long get_comp_time(void);
long get_comp_time(void);
void print_set_times(GNS_PARAM_STRUCT *);
void print_ads131_parms(GNS_PARAM_STRUCT *);
void print_data_hex(void *, int);
void get_str_ver(char* str);
u16    check_crc(const u8 *, u16);
f32 inv_sqrt(f32);
u16 get_pwm_ctrl_value(u32, u16);

#endif				/* userfunc.h */

