//---------------------------------------------------------------------------
#ifndef protoH
#define protoH
#include "my_cmd.h"
#include "my_defs.h"
//---------------------------------------------------------------------------


u8     test_crc16(u8 *);
void   add_crc16(u8 *);

String TimeString (void);

int    st_to_sec(SYSTEMTIME*);
long   td_to_sec(TIME_DATE *);
void   sec_to_st(int, SYSTEMTIME *);
s64    get_msec_ticks(void);
long   get_sec_ticks(void);
void   str_to_cap(char *, int);
void   sec_to_str(long, char *);
int    sec_to_td(long, TIME_DATE *);
void   sec_to_simple_str(long, char *);
u16    check_crc16(u8 *, u16);
void   ver_to_str(DEV_STATUS_STRUCT *, char *str);

int parse_nmea_string(char *, void*);
void filt_ok_and_error(XCHG_ERR_COUNT *, XCHG_ERR_COUNT *);
void clr_xchg_err(void);
int bufs_to_st(char*, char*, SYSTEMTIME*);
void grad_to_str(s32 grad, char* str);

#endif
