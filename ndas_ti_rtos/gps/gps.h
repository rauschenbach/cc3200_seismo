#ifndef _GPS_H
#define _GPS_H

#include "main.h"


/* Navigation Position Velocity Time Solution - сообщение прямо по протоколу */
#pragma pack(1)
typedef struct {
	u32 iTow;		// GPS time of week
	u16 year;		// Year (UTC)
	u8 month;
	u8 day;

	u8 hour;
	u8 min;
	u8 sec;
	u8 validDate :1;		//Valid UTC Date
	u8 validTime :1;		//Valid UTC Time of Day
	u8 fullyResolved :1;	//UTC Time of Day has been fully resolved (no seconds uncertainty)
	u8 rsvd1 :5;

	u32 tAcc;		//Time accuracy estimate
	s32 nano;		//Fraction of second

	u8 fixType;		// GNSSfix Type: 0 - no fix, 1 - dead reckoning, 2 - 2D-fix, 3 - 3D-fix, 4 - GNSS+dead reckoning, 5 - Time only fix
	u8 gnssFixOK:1;		// A valid fix
	u8 diffSoln:1;		// 1 if differential corrections were applied
	u8 psmState:3;		// 0 - PSM is inactive, 1 - Enabled, 2 - Acquisition, 3 - Tracking, 4 - Power optimized tracking, 5 - Inactive
	u8 rsvd2:3;
	u8 rsvd3;
	u8 numSV;		// Number of satellites in NAV Solution

	s32 lon;		// lontitude, deg
	s32 lat;		// Latitude, deg
	s32 height;		// Height above Ellipsoid, mm
	s32 hMSL;		// Height above mean sea level, mm
	u32 hAcc;		// Horizontal accuracy estimate, mm
	u32 vAcc;		// Vertical accuracy estimate, mm
	s32 velN;		// NED north velocity mm/s
	s32 velE;		// NED east velocity mm/s
	s32 velD;		// NED down velocity mm/s
	s32 gSpeed;		// Ground speed (2-D) mm/s
	s32 heading;		// Heading of motion 2-D, deg
	u32 sAcc;		// Speed accuracy estimate, mm/s
	u32 headingAcc;		// Heading accuracy estimate, deg
	u16 pDOP;		// Position DOP
	u16 rsvd4;
	u32 rsvd5;
} NAV_PVT_Payload;

/* Счетчики приема */
typedef struct {
    u32 hdr;
    u8  len;
    u8  ind;			/* Начало пакета */
    u8  cnt;			/* Счетчик принятого */
    u8  end;			/* Конец приема */
} GPS_COUNT_STRUCT_t;

int  gps_create_task(void);
int  gps_init(void);
u16  gps_check_avail_bytes(void);
int  gps_read_data_string(u8 *, u16);
int  gps_write_data_string(u8*, u16);
void gps_print_data(void);
long gps_get_gps_time(void);

#endif				/* gps.h */
