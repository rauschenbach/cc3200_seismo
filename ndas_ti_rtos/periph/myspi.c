#include "myspi.h"
#include "dma.h"

//#define SPI_CLOCK_FREQ           8000000UL /* 80 / 10 = 8 МГц - достаточно */

#define SPI_CLOCK_FREQ         4000000UL /* 4 МГц - достаточно */

static void spi_mux_config(void);

void spi_init(void)
{
	u32 cfg;
        
        spi_mux_config();

	/* недокументированная возможность - включить гистерезис 10% 
	 * без нее работать не будет */
	HWREG(0x4402E144) = HWREG(0x4402e144) | (3 << 2);	
	
	/* CS аппаратный (отдельной ногой), активен вниз, 4 пинный режим, 8 бит длина  */
//старый вариант с CS:	cfg = SPI_HW_CTRL_CS | SPI_4PIN_MODE | SPI_TURBO_ON | SPI_CS_ACTIVELOW | SPI_WL_8;

	cfg = SPI_HW_CTRL_CS | SPI_4PIN_MODE/* | SPI_TURBO_ON */| SPI_CS_ACTIVELOW | SPI_WL_8;

	/* Reset SPI */
	MAP_SPIReset(GSPI_BASE);

	/* Configure SPI interface.  Polarity = 0 Phase = 1  Sub-Mode = 1 */
	MAP_SPIConfigSetExpClk(GSPI_BASE, MAP_PRCMPeripheralClockGet(PRCM_GSPI), 
				SPI_CLOCK_FREQ, SPI_MODE_MASTER, SPI_SUB_MODE_1, cfg);


	/* включить FIFO */
        MAP_SPIFIFOEnable(GSPI_BASE, SPI_RX_FIFO); 

	/* Enable SPI for communication */
	MAP_SPIEnable(GSPI_BASE);
}

/**
 * Остановить SPI
 */
void spi_stop(void)
{
	MAP_SPIDisable(GSPI_BASE);  
}

/**
 * Запись с одновременным чтением
 */
u8 spi_write_read(u8 data)
{
	u8 byte;

	/* Wait for space in FIFO */
	while (!(HWREG(GSPI_BASE + MCSPI_O_CH0STAT) & MCSPI_CH0STAT_TXS)) {}

	/* Шлем данные или DUMMY */
	HWREG(GSPI_BASE + MCSPI_O_TX0) = data;

	/* Wait for Rx data */
	while (!(HWREG(GSPI_BASE + MCSPI_O_CH0STAT) & MCSPI_CH0STAT_RXS)) {}

	/* Читаем ответ */
	byte = HWREG(GSPI_BASE + MCSPI_O_RX0);

	return byte;
}

/* CS вниз */
void spi_cs_on(void)
{
//старое для отл. платы        MAP_GPIOPinWrite(SPI_CS_BASE, SPI_CS_BIT, ~SPI_CS_BIT);  /* Ставим в 0 */
}

/* CS вверх */
void spi_cs_off(void)
{
//старое        MAP_GPIOPinWrite(SPI_CS_BASE, SPI_CS_BIT, SPI_CS_BIT);  /* Ставим в 1 */
}


/**
 * Тактируем входы SPI для ADS131 - нам надо 4 ноги для интерфейса SPI
 */
static void spi_mux_config(void)
{
	/* Enable Peripheral Clocks */
	MAP_PRCMPeripheralClkEnable(PRCM_GSPI, PRCM_RUN_MODE_CLK);
	
	/* PRCM_GPIOA2 - для ноги CS. Если поменяются PIN - то сменить! */
	MAP_PRCMPeripheralClkEnable(PRCM_GPIOA2, PRCM_RUN_MODE_CLK);

	/* Configure PIN_05 for SPI0 GSPI_CLK */
	MAP_PinTypeSPI(SPI_CLK_PIN, SPI_CLK_MODE);

	/* Configure PIN_06 for SPI0 GSPI_MISO */
	MAP_PinTypeSPI(SPI_MISO_PIN, SPI_MISO_MODE);

	/* Configure PIN_07 for SPI0 GSPI_MOSI */
	MAP_PinTypeSPI(SPI_MOSI_PIN, SPI_MOSI_MODE);

	/* Configure PIN_08 for SPI0 GSPI_CS на выход и ставим в "1" */	
	MAP_PinTypeGPIO(SPI_CS_PIN, SPI_CS_MODE, false);
	MAP_GPIODirModeSet(SPI_CS_BASE, SPI_CS_BIT, GPIO_DIR_MODE_OUT);      
        MAP_GPIOPinWrite(SPI_CS_BASE, SPI_CS_BIT, SPI_CS_BIT);  /* Ставим в 1 */
}


/**
 * Получить 32 бита из АЦП данные сразу со всех каналов в Little ENDIAN + 3 первых байта статуса
 * количество бит для чтения - по этой формуле: 24 + num * 24 
 * Переделать на DMA
 */
#if 1
void spi_write_read_data(int num, u32 * data)
{
    register u8 hi, mi, lo;
    u8 st[3];
    int i;

    // посмотреть картинку 38!!! - может изменить задержку CS и пр?
    // Читаем сначала статус 3 байта
    st[0] = spi_write_read(0);
    st[1] = spi_write_read(0);
    st[2] = spi_write_read(0);

    /* Получаем в формате Little ENDIAN!!! */
    for (i = 0; i < num; i++) {
	hi = spi_write_read(0);	// старший
	mi = spi_write_read(0);	// средний
	lo = spi_write_read(0);	// младший
	data[i] = ((u32) hi << 24) | ((u32) mi << 16) | ((u32) lo << 8);
    }
}
#else

/**
 * Передача данных по DMA
 */
void spi_write_read_data(int num, u32 * data)
{
    int len;
    len = 3 + 3 * num;     // длина в байтах 27 байт
    u8 buf[28];

    /* Initialize uDMA */
    UDMAInit();


    /* Настраиваем канал DMA для SPI Rx */
    UDMASetupTransfer(UDMA_CH6_GSPI_RX, /* канал DMA */
		      UDMA_MODE_BASIC,	/* Простой режим */
		      len,		/* Длина передачи с заголовком */
		      UDMA_SIZE_8,	/* По байту */
		      UDMA_ARB_1,	/* Гранулярность? */
		      (void *) (GSPI_BASE + 0x13c),	/* Адрес назначения - GPSI RX*/
		      UDMA_SRC_INC_NONE,	/* Адрес источника не инкрементируется */
		      buf,		/* Буфер приема */
		      UDMA_DST_INC_8);	/* Адрес назначения инкрементируется на 8 бит */

   // Ждать окончания передачи 
   for(volatile int i , 0; i < 1000;i++);
   memcpy(data, buf + 3, num * 3);


    /* Запустить прередачу */
    MAP_SPIDmaEnable(GSPI_BASE, SPI_RX_DMA);
}
#endif
