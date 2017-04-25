#ifndef _MAIN_H
#define _MAIN_H


#include "my_defs.h"
#include "ads131.h"
#include "xchg.h"
#include "timtick.h"
#include "log.h"
#include "userfunc.h"

#include "board.h"
#include "sdcard.h"

#include <osi.h>


OsiLockObj_t* get_lock_obj_prt(void);
void get_gns_start_params(GNS_PARAM_STRUCT *);
int  get_gns_work_mode(void);
void set_gns_start_params(GNS_PARAM_STRUCT *);
bool is_finish(void);


#endif /* main.h */
