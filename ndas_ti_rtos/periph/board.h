#ifndef _BOARD_H
#define _BOARD_H


#include <hw_types.h>
#include <hw_ints.h>
#include <interrupt.h>
#include <systick.h>
#include <rom_map.h>
#include <prcm.h>
#include <rom.h>

void board_init(void);
void board_reset(void);

#endif /* board.h */
