#include "myspi.h"
#include "dma.h"

//#define SPI_CLOCK_FREQ           8000000UL /* 80 / 10 = 8 ��� - ���������� */

#define SPI_CLOCK_FREQ         4000000UL /* 4 ��� - ���������� */

static void spi_mux_config(void);

void spi_init(void)
{
	u32 cfg;
        
        spi_mux_config();

	/* ������������������� ����������� - �������� ���������� 10% 
	 * ��� ��� �������� �� ����� */
	HWREG(0x4402E144) = HWREG(0x4402e144) | (3 << 2);	
	
	/* CS ���������� (��������� �����), ������� ����, 4 ������ �����, 8 ��� �����  */
//������ ������� � CS:	cfg = SPI_HW_CTRL_CS | SPI_4PIN_MODE | SPI_TURBO_ON | SPI_CS_ACTIVELOW | SPI_WL_8;

	cfg = SPI_HW_CTRL_CS | SPI_4PIN_MODE/* | SPI_TURBO_ON */| SPI_CS_ACTIVELOW | SPI_WL_8;

	/* Reset SPI */
	MAP_SPIReset(GSPI_BASE);

	/* Configure SPI interface.  Polarity = 0 Phase = 1  Sub-Mode = 1 */
	MAP_SPIConfigSetExpClk(GSPI_BASE, MAP_PRCMPeripheralClockGet(PRCM_GSPI), 
				SPI_CLOCK_FREQ, SPI_MODE_MASTER, SPI_SUB_MODE_1, cfg);


	/* �������� FIFO */
        MAP_SPIFIFOEnable(GSPI_BASE, SPI_RX_FIFO); 

	/* Enable SPI for communication */
	MAP_SPIEnable(GSPI_BASE);
}

/**
 * ���������� SPI
 */
void spi_stop(void)
{
	MAP_SPIDisable(GSPI_BASE);  
}

/**
 * ������ � ������������� �������
 */
u8 spi_write_read(u8 data)
{
	u8 byte;

	/* Wait for space in FIFO */
	while (!(HWREG(GSPI_BASE + MCSPI_O_CH0STAT) & MCSPI_CH0STAT_TXS)) {}

	/* ���� ������ ��� DUMMY */
	HWREG(GSPI_BASE + MCSPI_O_TX0) = data;

	/* Wait for Rx data */
	while (!(HWREG(GSPI_BASE + MCSPI_O_CH0STAT) & MCSPI_CH0STAT_RXS)) {}

	/* ������ ����� */
	byte = HWREG(GSPI_BASE + MCSPI_O_RX0);

	return byte;
}

/* CS ���� */
void spi_cs_on(void)
{
//������ ��� ���. �����        MAP_GPIOPinWrite(SPI_CS_BASE, SPI_CS_BIT, ~SPI_CS_BIT);  /* ������ � 0 */
}

/* CS ����� */
void spi_cs_off(void)
{
//������        MAP_GPIOPinWrite(SPI_CS_BASE, SPI_CS_BIT, SPI_CS_BIT);  /* ������ � 1 */
}


/**
 * ��������� ����� SPI ��� ADS131 - ��� ���� 4 ���� ��� ���������� SPI
 */
static void spi_mux_config(void)
{
	/* Enable Peripheral Clocks */
	MAP_PRCMPeripheralClkEnable(PRCM_GSPI, PRCM_RUN_MODE_CLK);
	
	/* PRCM_GPIOA2 - ��� ���� CS. ���� ���������� PIN - �� �������! */
	MAP_PRCMPeripheralClkEnable(PRCM_GPIOA2, PRCM_RUN_MODE_CLK);

	/* Configure PIN_05 for SPI0 GSPI_CLK */
	MAP_PinTypeSPI(SPI_CLK_PIN, SPI_CLK_MODE);

	/* Configure PIN_06 for SPI0 GSPI_MISO */
	MAP_PinTypeSPI(SPI_MISO_PIN, SPI_MISO_MODE);

	/* Configure PIN_07 for SPI0 GSPI_MOSI */
	MAP_PinTypeSPI(SPI_MOSI_PIN, SPI_MOSI_MODE);

	/* Configure PIN_08 for SPI0 GSPI_CS �� ����� � ������ � "1" */	
	MAP_PinTypeGPIO(SPI_CS_PIN, SPI_CS_MODE, false);
	MAP_GPIODirModeSet(SPI_CS_BASE, SPI_CS_BIT, GPIO_DIR_MODE_OUT);      
        MAP_GPIOPinWrite(SPI_CS_BASE, SPI_CS_BIT, SPI_CS_BIT);  /* ������ � 1 */
}


/**
 * �������� 32 ���� �� ��� ������ ����� �� ���� ������� � Little ENDIAN + 3 ������ ����� �������
 * ���������� ��� ��� ������ - �� ���� �������: 24 + num * 24 
 * ���������� �� DMA
 */
#if 1
void spi_write_read_data(int num, u32 * data)
{
    register u8 hi, mi, lo;
    u8 st[3];
    int i;

    // ���������� �������� 38!!! - ����� �������� �������� CS � ��?
    // ������ ������� ������ 3 �����
    st[0] = spi_write_read(0);
    st[1] = spi_write_read(0);
    st[2] = spi_write_read(0);

    /* �������� � ������� Little ENDIAN!!! */
    for (i = 0; i < num; i++) {
	hi = spi_write_read(0);	// �������
	mi = spi_write_read(0);	// �������
	lo = spi_write_read(0);	// �������
	data[i] = ((u32) hi << 24) | ((u32) mi << 16) | ((u32) lo << 8);
    }
}
#else

/**
 * �������� ������ �� DMA
 */
void spi_write_read_data(int num, u32 * data)
{
    int len;
    len = 3 + 3 * num;     // ����� � ������ 27 ����
    u8 buf[28];

    /* Initialize uDMA */
    UDMAInit();


    /* ����������� ����� DMA ��� SPI Rx */
    UDMASetupTransfer(UDMA_CH6_GSPI_RX, /* ����� DMA */
		      UDMA_MODE_BASIC,	/* ������� ����� */
		      len,		/* ����� �������� � ���������� */
		      UDMA_SIZE_8,	/* �� ����� */
		      UDMA_ARB_1,	/* �������������? */
		      (void *) (GSPI_BASE + 0x13c),	/* ����� ���������� - GPSI RX*/
		      UDMA_SRC_INC_NONE,	/* ����� ��������� �� ���������������� */
		      buf,		/* ����� ������ */
		      UDMA_DST_INC_8);	/* ����� ���������� ���������������� �� 8 ��� */

   // ����� ��������� �������� 
   for(volatile int i , 0; i < 1000;i++);
   memcpy(data, buf + 3, num * 3);


    /* ��������� ��������� */
    MAP_SPIDmaEnable(GSPI_BASE, SPI_RX_DMA);
}
#endif