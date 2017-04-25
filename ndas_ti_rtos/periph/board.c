// Driverlib includes
#include <hw_types.h>
#include <rom.h>
#include <rom_map.h>
#include <hw_gprcm.h>
#include <prcm.h>
#include "board.h"

#include <hw_memmap.h>
#include <hw_common_reg.h>
#include <hw_types.h>
#include <hw_ints.h>
#include <uart.h>
#include <interrupt.h>
#include <utils.h>
#include <prcm.h>


//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
#if defined(ccs)
extern void (*const g_pfnVectors[]) (void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif


//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
void board_init(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
    //
    // Set vector table base
    //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long) &g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long) &__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);
    
    PRCMCC3200MCUInit();
}

/**
 * —брос CPU
 */
void board_reset(void)
{
	    HWREG(GPRCM_BASE+ GPRCM_O_MCU_GLOBAL_SOFT_RESET) |= 0x1;
}

