#include <hw_types.h>
#include <rom.h>
#include <rom_map.h>
#include <prcm.h>

#include "proto.h"
#include "ads131.h"

#ifdef _WIN32
#include <stdlib.h>
#include <stdio.h>

#define PRINTF	printf
#define vmain main
#define mem_copy memcpy

#else
#include "main.h"
#include "log.h"
#endif


static DEV_ID_h dev_id;
static INFO_h info;



/* ID устройства вернуть */
void proto_get_dev_id(DEV_ID_h * par)
{
    if (par != NULL) {
	mem_copy(par, &dev_id, sizeof(dev_id));
    }
}


/* IP адрес устройства записать */
void proto_save_ip_addr(u32 ip)
{
/*   dev_id.device_id = ip; */
}


/* Структура INFO */
void proto_get_info(INFO_h * par)
{
    if (par != NULL) {
	mem_copy(par, &info, sizeof(info));
    }
}


