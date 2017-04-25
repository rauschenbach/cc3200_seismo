#ifndef _MYSPI_H
#define _MYSPI_H

/* Driverlib includes */
#include <hw_types.h>
#include <hw_memmap.h>
#include <hw_common_reg.h>
#include <hw_ints.h>
#include <hw_mcspi.h>
#include <rom_map.h>
#include <gpio.h>
#include <pin.h>
#include <spi.h>
#include <rom.h>
#include <udma.h>

#include <utils.h>
#include <prcm.h>
#include <interrupt.h>
#include "my_defs.h"

/* На отладочной плате  */
#if 0
#define   SPI_CLK_PIN	        	PIN_45
#define   SPI_CLK_MODE			PIN_MODE_7


#define   SPI_MISO_PIN			PIN_06
#define   SPI_MISO_MODE			PIN_MODE_7

#define   SPI_MOSI_PIN			PIN_07
#define   SPI_MOSI_MODE			PIN_MODE_7

/* Чипселект дергаем сами */
#define SPI_CS_PIN			PIN_08 /* GPIO_17 */ 
#define SPI_CS_BIT			0x02   
#define SPI_CS_BASE			GPIOA2_BASE	 
#define SPI_CS_MODE			PIN_MODE_0
#else

/* Клок */
#define   SPI_CLK_PIN	        	PIN_05    /* GPIO14  */
#define   SPI_CLK_MODE			PIN_MODE_7

/* MISO  */
#define   SPI_MISO_PIN			PIN_53     /* GPIO30  */
#define   SPI_MISO_MODE			PIN_MODE_7

/*  MOSI */
#define   SPI_MOSI_PIN			PIN_07     /* GPIO16  */
#define   SPI_MOSI_MODE			PIN_MODE_7

/* Чипселект дергаем сами */
#define SPI_CS_PIN			PIN_08 /* GPIO_17 */ 
#define SPI_CS_BIT			0x02   
#define SPI_CS_BASE			GPIOA2_BASE	 
#define SPI_CS_MODE			PIN_MODE_0


#endif


void spi_init(void);
void spi_stop(void);
void spi_cs_on(void);
void spi_cs_off(void);
u8   spi_write_read(u8);
void spi_write_read_data(int, u32 *);

#endif /* myspi.h */
