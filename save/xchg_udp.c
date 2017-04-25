#include <stdlib.h>
#include <stdio.h>
#include <simplelink.h>
#include <wlan.h>
#include <hw_types.h>
#include <hw_gprcm.h>

// Driverlib includes
#include <rom.h>
#include <rom_map.h>
#include <prcm.h>



#include "rtos_handlers.h"
#include "xchg.h"
#include "board.h"
#include "dma.h"
#include "timtick.h"
#include "userfunc.h"
#include "status.h"
#include "main.h"




#define 	UART_BAUD_RATE		115200	/* �������� ����� ��� ������ � PC  */
#define         UART_TASK_SLEEP         1

/************************************************************************
 * 	����������� ����������
 * 	������������ ��� ���������� SRAM (USE_THE_LOADER)
 * 	��� ������������ � ������ ������  
 ************************************************************************/
#if defined USE_THE_LOADER_PLACE
static XCHG_DATA_STRUCT_t *pRxBuf = (XCHG_DATA_STRUCT_t *) XCHG_RX_MEM_ADDR;	/* ��������� �� ���� ��������� ������! */
static XCHG_DATA_STRUCT_t *pTxBuf = (XCHG_DATA_STRUCT_t *) XCHG_TX_MEM_ADDR;	/* ��������� �� ���� ��������� ��������! */
static XCHG_COUNT_STRUCT_t *pCount = (XCHG_COUNT_STRUCT_t *) XCHG_COUNT_MEM_ADDR;	/* �������� ������ */
#else
static XCHG_DATA_STRUCT_t rx_data;
static XCHG_DATA_STRUCT_t tx_data;
static XCHG_COUNT_STRUCT_t xchg_counts;

static XCHG_DATA_STRUCT_t *pRxBuf = &rx_data;	/* ��������� �� ���� ��������� ������! */
static XCHG_DATA_STRUCT_t *pTxBuf = &tx_data;	/* ��������� �� ���� ��������� ��������! */
static XCHG_COUNT_STRUCT_t *pCount = &xchg_counts;	/* �������� ������ */
#endif


/* ��������� ����������� �� ���� */
static SlSockAddrIn_t sAddr;
static SlSockAddrIn_t sLocalAddr;
static int iSockID;

/* ��� ���������� */
static CONNECTION_TYPE conn_type;


/* ���� ������ - ����� ������� ��������� ������� �������� �� ���������� */
static OsiSyncObj_t gTxSyncObj;
static OsiReturnVal_e xchg_create_sync_obj(void);
static void vTxTask(void *);
static void vWlanStationTask(void *);
static bool net_debug_parse_func(void);

static long ConfigureSimpleLinkToDefaultState(void);
static void com_debug_read_ISR(u8);
static void com_rx_end(int);

static void com_tx_data(u8 *, int);
static long wlan_connect(void *);
static void wlan_tx_data(u8 *, int);
static void com_isr_handler(void);
static void buf_clear(void);


/** 
 * �������� ������ ��� ������� ������ � ��������
 */
static void buf_clear(void)
{
    mem_set(pTxBuf, 0, sizeof(XCHG_DATA_STRUCT_t));
    mem_set(pRxBuf, 0, sizeof(XCHG_DATA_STRUCT_t));
    mem_set(pCount, 0, sizeof(XCHG_COUNT_STRUCT_t));
}

/**
 * �������������� UART
 */
void com_init(void)
{
    u32 flags;

    flags = UART_INT_RX | UART_INT_DMATX | UART_INT_OE | UART_INT_PE | UART_INT_FE | UART_INT_BE;

    /* ������������� ���� UART */
    com_mux_config();

    /* ��������� UART - 115200 - 8N2 */
    MAP_UARTConfigSetExpClk(UARTA0_BASE, MAP_PRCMPeripheralClockGet(PRCM_UARTA0), UART_BAUD_RATE,
			    UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_TWO | UART_CONFIG_PAR_NONE);

    /*  �������� FIFO �� ����� � ��������, ����� DMA �� ����� ��������. ����������� ����� - 20 ���� */
    MAP_UARTFIFOLevelSet(UARTA0_BASE, UART_FIFO_TX1_8, UART_FIFO_RX1_8 /* UART_FIFO_RX2_8 */ );
    MAP_UARTFIFOEnable(UARTA0_BASE);

    /* ������ ������ */
    buf_clear();

    osi_InterruptRegister(INT_UARTA0, com_isr_handler, INT_PRIORITY_LVL_5);	/* ������������ ���������� */

    if (xchg_create_sync_obj() != OSI_OK) {
	while (1);
    }
    conn_type = CONNECT_COMM;

    /* ������ ����� ���������� */
    MAP_UARTIntClear(UARTA0_BASE, flags);

    /* ������ ���������� ��-������ � ������� */
    MAP_UARTIntEnable(UARTA0_BASE, flags);
}

/**
 * ������� ���������� ������ � ������ �������������
 */
static OsiReturnVal_e xchg_create_sync_obj(void)
{
    OsiReturnVal_e ret;

    do {
	/* ������ ������������� (event ��� ��� �� ��������) */
	ret = osi_SyncObjCreate(&gTxSyncObj);
	if (ret != OSI_OK) {
	    break;		/* Queue was not created and must not be used. */
	}

	/* ������� ������, ������� ����� ���������� ������ */
	ret = osi_TaskCreate(vTxTask, "TxTask", OSI_STACK_SIZE, NULL, 5, NULL);
	if (ret != OSI_OK) {
	    break;
	}
    } while (0);
    return ret;
}

/**
 * Interrupt handler for UART interupt 
 */
static void com_isr_handler(void)
{
    volatile unsigned long status;
    volatile unsigned long err;
    volatile char c;

    /* ������� ������ � ������� */
    err = MAP_UARTRxErrorGet(UARTA0_BASE);
    if (err) {
	MAP_UARTRxErrorClear(UARTA0_BASE);
	pRxBuf->ind = 0;
	pRxBuf->cnt = 0;
	pRxBuf->end = 0;
	pCount->rx_stat_err++;
    }


    /* Read the interrupt status of the UART. */
    status = MAP_UARTIntStatus(UARTA0_BASE, 1);

    /* �������� ������ */
    MAP_UARTIntClear(UARTA0_BASE, status);

    if (status & (UART_INT_OE | UART_INT_PE | UART_INT_FE | UART_INT_BE)) {
	pCount->rx_cmd_err++;
    }

    /* �������� ������� ������� ����� ��������� ����� */
    if (status & UART_INT_RX) {
	while (MAP_UARTCharsAvail(UARTA0_BASE)) {
	    c = MAP_UARTCharGetNonBlocking(UARTA0_BASE);
	    com_debug_read_ISR(c);
	}
    }

    /* ��������� DMA ��� �������� */
    if (status & UART_INT_DMATX) {
	MAP_UARTDMADisable(UARTA0_BASE, UART_DMA_TX);
	pTxBuf->end = 1;
    }
}


/*
 * ��������� com ����
 */
void com_mux_unconfig(void)
{
    /* ������ ������������ */
    MAP_PRCMPeripheralClkDisable(PRCM_UARTA0, PRCM_RUN_MODE_CLK);
}


/**
 * ������������ ���������� ������ - �����
 * �� ����� ������������ ������ �� ��������
 */
static void com_debug_read_ISR(u8 rx_byte)
{
    /* ������ ����� ������ ���� ������� � ��� �����. ��������� ��������� */
    if (pRxBuf->ind == 0 && rx_byte == 0x4e) {	/* ������ ���� NDAS 0x444e */
	pRxBuf->ind = 1;	/* ���� ���� ������� */
	pRxBuf->cnt = 1;	/* ������� ������� - ������ ���� ������� */
	pRxBuf->hdr.ndas = rx_byte;
    } else {			/* ������ ����������� ����� */
	if (1 == pRxBuf->cnt) {
	    pRxBuf->hdr.ndas |= (u16) rx_byte << 8;
	    if (pRxBuf->hdr.ndas != NDAS) {
		com_rx_end(RX_DATA_ERR);	/* ���� ��������. ������ ���� �.� 0x44 */
	    }
	} else if (2 == pRxBuf->cnt) {	/* ��. ���� crc16 */
	    pRxBuf->hdr.crc16 = rx_byte;
	} else if (3 == pRxBuf->cnt) {	/* ��. ���� crc16 */
	    pRxBuf->hdr.crc16 |= (u16) rx_byte << 8;
	} else if (4 == pRxBuf->cnt) {	/* ��. ���� dev_id */
	    pRxBuf->hdr.dev_id = rx_byte;
	} else if (5 == pRxBuf->cnt) {	/* ��. ��. ���� dev_id */
	    pRxBuf->hdr.dev_id |= (u32) rx_byte << 8;
	} else if (6 == pRxBuf->cnt) {	/* ��. ��. ���� dev_id */
	    pRxBuf->hdr.dev_id |= (u32) rx_byte << 16;
	} else if (7 == pRxBuf->cnt) {	/* ��.���� dev_id */
	    pRxBuf->hdr.dev_id |= (u32) rx_byte << 24;
	    if (pRxBuf->hdr.dev_id != DEV_ADDR) {
		com_rx_end(RX_DATA_ERR);	/* ��������� ����� � ���� ��������. �� ��� ����� */
	    }
	} else if (8 == pRxBuf->cnt) {	/* ��.���� pack_type */
	    pRxBuf->hdr.pack_type = (PACK_TYPE) rx_byte;
	} else if (9 == pRxBuf->cnt) {	/* ��. ���� pack_type */
	    pRxBuf->hdr.pack_type |= (u16) rx_byte << 16;
	    if (pRxBuf->hdr.pack_type != PACK_REQUEST) {
		com_rx_end(RX_DATA_ERR);	/* ����� ���� ������ REQUEST */
	    }
	} else if (10 == pRxBuf->cnt) {
	    pRxBuf->hdr.cmd = (USER_CMD) rx_byte;	/* �� ���� ������� */
	} else if (11 == pRxBuf->cnt) {
	    pRxBuf->hdr.cmd |= (u16) rx_byte << 8;	/* ��. ���� �������  */
	} else if (12 == pRxBuf->cnt) {
	    pRxBuf->hdr.ver = rx_byte;	/* �� ���� ver */
	} else if (13 == pRxBuf->cnt) {
	    pRxBuf->hdr.ver |= (u32) rx_byte << 8;	/* ��. ��. ���� ver */
	} else if (14 == pRxBuf->cnt) {
	    pRxBuf->hdr.ver |= (u32) rx_byte << 16;	/* ��. ��. ���� ver */
	} else if (15 == pRxBuf->cnt) {
	    pRxBuf->hdr.ver |= (u32) rx_byte << 24;	/* ��. ���� ver */
	    if (pRxBuf->hdr.ver != DEV_VERSION) {
		com_rx_end(RX_DATA_ERR);	/* �� ���� ������ */
	    }
	} else if (16 == pRxBuf->cnt) {
	    pRxBuf->hdr.size = rx_byte;	/* ��. ���� size */
	} else if (17 == pRxBuf->cnt) {
	    pRxBuf->hdr.size |= (u16) rx_byte << 8;	/* ��. ���� size */
	    if (pRxBuf->hdr.size > XCHG_BUF_LEN) {
		com_rx_end(RX_DATA_ERR);	/* �� ���� ������ */
	    }
	} else if (18 == pRxBuf->cnt) {
	    pRxBuf->hdr.rsvd0 = rx_byte;	/* ��. ���� rsvd */
	} else if (19 == pRxBuf->cnt) {
	    pRxBuf->hdr.rsvd0 |= (u16) rx_byte << 8;	/* ��. ���� rsvd */
	}
	/* ��������� �����, ����� ����� �������� ����� �� DMA, ��� ��� ����� ������� ��� �������� */
	else if (pRxBuf->cnt >= sizeof(HEADER_t)
		 && pRxBuf->cnt < pRxBuf->hdr.size + sizeof(HEADER_t)) {
	    pRxBuf->data[pRxBuf->cnt - sizeof(HEADER_t)] = rx_byte;	/* �������� ����� */
	}

	pRxBuf->cnt++;

	/* �������� CRC16 */
	if (pRxBuf->cnt >= pRxBuf->hdr.size + sizeof(HEADER_t)) {
#if 0
	    if (check_crc(pRxBuf->data, pRxBuf->hdr.size) != pRxBuf->hdr.crc16) {
		com_rx_end(RX_CRC_ERR);
	    } else
#endif
		com_rx_end(RX_DATA_OK);
	}
    }
}


/**
 * ����� ������. ��������� ������ �� ��������
 * ���� ��� ����� ������� "NAKED" � �������� �������� �� ISR
 */
static void com_rx_end(int err)
{
    OsiReturnVal_e ret;

    pRxBuf->ind = 0;
    pRxBuf->cnt = 0;
    pRxBuf->end = 0;		/* ������� �������� Error */

    if (err == RX_DATA_OK) {
	pRxBuf->end = 1;	/* ������� ������� OK */
	pTxBuf->end = 0;	/* ������ �������� */
	pCount->rx_pack++;	/* ��������� �������� ������ */

	/* ����� ��������. �������� ��������� �� ��������� ISR ������� ������. */
	ret = osi_SyncObjSignalFromISR(&gTxSyncObj);	/* ���������� ������ */
	if (ret != OSI_OK) {
	    while (1);		/* Queue was not created and must not be used. */
	}
    } else if (err == RX_CRC_ERR) {	/* ����������� ����� �������-�������� ����� ������ */
	pCount->rx_crc_err++;	/* ������ CRC16 */
    } else if (err == RX_DATA_ERR) {	/* ���� ���-�� ����� �� ��� */
	pCount->rx_cmd_err++;	/* ��������� ������� */
    }
}

/**
 * ���������� ������ RTOS. ������� ��������� � �������� ����� ������
 * ���������� ��� �� DMA ��� �������� ��� �������
 */
static void vTxTask(void *pvParameters)
{
    OsiReturnVal_e ret;
    int len;
    bool res;

    while (1) {

	/* Wait for a message to arrive. */
	ret = osi_SyncObjWait(&gTxSyncObj, OSI_WAIT_FOREVER);
	if (ret == OSI_OK) {

	    /* ��������� ����� �� �������� ������� */
	    switch (pRxBuf->hdr.cmd) {

		/* ������ ������ ��������� - �������� � ���������� � ������ � ��� ���� */
	    case CMD_QUERY_SIGNAL:
		res = ADS131_get_data((ADS131_DATA_h *) pTxBuf);
		if (res) {
		    pTxBuf->hdr.pack_type = PACK_ACK;	/* ���������� */
		    len = sizeof(ADS131_DATA_h);	/* ������� ���������� */
		} else {
		    pTxBuf->hdr.pack_type = PACK_NO_DATA;	/* ��� ������ */
		    len = sizeof(HEADER_t);
		}
		pTxBuf->hdr.cmd = CMD_QUERY_SIGNAL;	/* ������� ���� ������� */
		break;

		/* ����� ���������� ������� PC */
	    case CMD_RECEIVE_PARAMETERS:
		ADS131_get_adcp((ADCP_h *) pTxBuf);
		pTxBuf->hdr.pack_type = PACK_ACK;	/* ���������� */
		pTxBuf->hdr.cmd = CMD_RECEIVE_PARAMETERS;	/* ������� ���� ������� */
		len = sizeof(ADCP_h);
		break;

		/* ����� ��������� - ������� ��� ���������� */
	    case CMD_START_ACQUIRING:
		res = ADS131_start_osciloscope();
		pTxBuf->hdr.pack_type = res ? PACK_ACK : PACK_NAK;	/* ���������� */
		pTxBuf->hdr.cmd = CMD_START_ACQUIRING;	/* ������� ���� ������� */
		len = sizeof(HEADER_t);
		break;

		/*���� ��������� */
	    case CMD_STOP_ACQUIRING:
		res = ADS131_stop_osciloscope();
		pTxBuf->hdr.pack_type = res ? PACK_ACK : PACK_NAK;	/* ���������� */
		pTxBuf->hdr.cmd = CMD_STOP_ACQUIRING;	/* ������� ���� ������� */
		len = sizeof(HEADER_t);
		break;

		/* �������� ���������/��������� ��������� 
		 * ��������� �������� ������. ��������� ��������� */
	    case CMD_APPLY_PARAMETERS:
	    case CMD_WRITE_PARAMETERS:
		res = ADS131_write_parameters((ADCP_h *) pRxBuf);
		pTxBuf->hdr.pack_type = res ? PACK_ACK : PACK_NAK;	/* ���������� */
		pTxBuf->hdr.cmd = CMD_WRITE_PARAMETERS;	/* ������� ���� ������� */
		len = sizeof(HEADER_t);
		break;

		/* ������ ���������� */
	    case CMD_QUERY_DEVICE:
		proto_get_dev_id((DEV_ID_h *) pTxBuf);
		pTxBuf->hdr.pack_type = PACK_ACK;	/* ���������� */
		pTxBuf->hdr.cmd = CMD_QUERY_DEVICE;	/* ������� ���� ������� */
		len = sizeof(DEV_ID_h);
		pTxBuf->hdr.crc16 = check_crc((u8 *) pTxBuf->data, len - sizeof(HEADER_t));	/* CRC ��� �������� */
		break;

		/* ������ ������� ���������� */
	    case CMD_QUERY_STAT:
		status_get_full_status((STATUS_h *) pTxBuf);
		pTxBuf->hdr.pack_type = PACK_ACK;	/* ���������� */
		pTxBuf->hdr.cmd = CMD_QUERY_STAT;	/* ������� ���� ������� */
		len = sizeof(STATUS_h);
		mem_copy(&((STATUS_h *) pTxBuf)->xchg_counts, pCount, sizeof(XCHG_COUNT_STRUCT_t));
		break;

		/* ������ ���������� �� ���������� */
	    case CMD_QUERY_INFO:
		proto_get_info((INFO_h *) pTxBuf);
		pTxBuf->hdr.pack_type = PACK_ACK;	/* ���������� */
		pTxBuf->hdr.cmd = CMD_QUERY_INFO;	/* ������� ���� ������� */
		len = sizeof(INFO_h);
		break;

		/* ������������� �����  RTC */
	    case CMD_SYNC_CLOCK:
		sync_sys_clock((TIMER_CLOCK_h *) pRxBuf);
		pTxBuf->hdr.pack_type = PACK_ACK;	/* ���������� */
		pTxBuf->hdr.cmd = CMD_SYNC_CLOCK;	/* ������� ���� ������� */
		len = sizeof(HEADER_t);
		break;

		/* ����� ���������� */
	    case CMD_RESET_MCU:
		pTxBuf->hdr.pack_type = PACK_ACK;	/* ���������� */
		pTxBuf->hdr.cmd = CMD_RESET_MCU;	/* ������� ���� ������� */
		len = sizeof(HEADER_t);
		break;

	 /* ����� � ����������, ������������� ������� ��� ���� ����� */	
	  case  CMD_SELECT_SD_TO_MCU:
		select_sd_to_mcu();
		pTxBuf->hdr.pack_type = PACK_ACK;	/* ���������� */
		pTxBuf->hdr.cmd = CMD_SELECT_SD_TO_CR;	/* ������� ���� ������� */
		len = sizeof(HEADER_t);
		break;

	    /* ����� � ���������� */
	    case CMD_SELECT_SD_TO_CR:          
		select_sd_to_cr();
		pTxBuf->hdr.pack_type = PACK_ACK;	/* ���������� */
		pTxBuf->hdr.cmd = CMD_SELECT_SD_TO_CR;	/* ������� ���� ������� */
		len = sizeof(HEADER_t);
		break;



	    default:
		len = 0;	/* �� ������� �� ����������� ������� */
		break;
	    }

	    if (len >= sizeof(HEADER_t)) {
		/* ���������� ��������� �� ����������� */
		pTxBuf->hdr.ndas = NDAS;
		pTxBuf->hdr.dev_id = DEV_ADDR;
		pTxBuf->hdr.ver = DEV_VERSION;
		pTxBuf->hdr.rsvd0 = 0xffff;
		pTxBuf->hdr.size = len - sizeof(HEADER_t);	/* ������� �����: ����� ��� ��������� */

		/* ������� CRC */
		if (len == sizeof(HEADER_t)) {
		    pTxBuf->hdr.crc16 = 0;
		} else {
		    /* CRC ��� �������� */
//                  pTxBuf->hdr.crc16 = check_crc(pTxBuf->data, len - sizeof(HEADER_t));
		    pTxBuf->hdr.crc16 = 0xabcd;
		}

		if (conn_type == CONNECT_COMM) {
		    com_tx_data((u8 *) pTxBuf, len);
		} else {
		    wlan_tx_data((u8 *) pTxBuf, len);
		}

		/* ����� �������� �������� - �������! */
		if (pTxBuf->hdr.cmd == CMD_RESET_MCU) {
		    delay_ms(50);
		    //  PRCMSOCReset();
		    board_reset();
		}
	    }
	}
	osi_Sleep(UART_TASK_SLEEP);
    }
}

/**
 * �������� ������ �� DMA
 */
static void com_tx_data(u8 * data, int len)
{
    /* Initialize uDMA */
    UDMAInit();

    /* ����������� ����� DMA ��� UARTA0 */
    UDMASetupTransfer(UDMA_CH9_UARTA0_TX, UDMA_MODE_BASIC,	/* ������� ����� */
		      len,	/* ����� �������� � ���������� */
		      UDMA_SIZE_8,	/* �� ����� */
		      UDMA_ARB_1,	/* �������������? */
		      data,	/* ����� �������� */
		      UDMA_SRC_INC_8,	/* ����� ��������� ���������������� */
		      (void *) (UARTA0_BASE + UART_O_DR),	/* ����� ���������� */
		      UDMA_DST_INC_NONE);	/* ����� ���������� �� ���������������� */

    /* ��������� ��������� */
    MAP_UARTDMAEnable(UARTA0_BASE, UART_DMA_TX);

    pCount->tx_pack++;		/* �������� ������� */
}


/**
 * �������� ������ �� ����
 */
static void wlan_tx_data(u8 * data, int len)
{
    int iAddrSize;
    int iStatus;

    iAddrSize = sizeof(SlSockAddrIn_t);

    iStatus = sl_SendTo(iSockID, data, len, 0, (SlSockAddr_t *) & sAddr, (SlSocklen_t) iAddrSize);
    if (iStatus != len) {
	PRINTF("ERROR: SendTo...\r\n");
    } else {
	PRINTF("SendTo (%d.%d.%d.%d)... OK\r\n",
	       (u8) (sAddr.sin_addr.s_addr), (u8) (sAddr.sin_addr.s_addr >> 8), (u8) (sAddr.sin_addr.s_addr >> 16), (u8) (sAddr.sin_addr.s_addr >> 24));
	pCount->tx_pack++;	/* �������� ������� */
    }
}

/**
 * Start simplelink, connect to the ap and run the ping test
 */
static void vWlanStationTask(void *par)
{
    long ret = -1, i = 50;
    int iAddrSize;
    int iStatus;

    // Following function configure the device to default state by cleaning
    // the persistent settings stored in NVMEM (viz. connection profiles &
    // policies, power policy etc)
    //
    // Applications may choose to skip this step if the developer is sure
    // that the device is in its default state at start of applicaton
    //
    // Note that all profiles and persistent settings that were done on the
    // device will be lost
    //
    ret = ConfigureSimpleLinkToDefaultState();
    if (ret < 0) {
	if (DEVICE_NOT_IN_STATION_MODE == ret) {
	    PRINTF("FAILED: Configure the device in its default state\n\r");
	}

	LOOP_FOREVER();
    }

    PRINTF("INFO: Device is configured in default state \n\r");

    /* Assumption is that the device is configured in station mode already and it is in its default state */
    ret = sl_Start(0, 0, 0);
    if (ret < 0 || ROLE_STA != ret) {
	PRINTF("FAILED: Start the device \n\r");
	LOOP_FOREVER();
    }
    PRINTF("INFO: Device started as STATION \n\r");

    /* Connecting to WLAN AP */
    ret = wlan_connect(par);
    if (ret < 0) {
	PRINTF("FAILED: Establish connection w/ an AP \n\r");
	LOOP_FOREVER();
    }
    PRINTF("INFO: Connection established w/ AP and IP is aquired\n\r");

    /* filling the UDP server socket address */
    sLocalAddr.sin_family = SL_AF_INET;
    sLocalAddr.sin_port = sl_Htons((u16) ((GNS_PARAM_STRUCT *) par)->gns_server_port);
    sLocalAddr.sin_addr.s_addr = 0;
    iAddrSize = sizeof(SlSockAddrIn_t);

    // creating a UDP socket
    iSockID = sl_Socket(SL_AF_INET, SL_SOCK_DGRAM, 0);
    if (iSockID < 0) {
	PRINTF("ERROR: Create Socket\r\n");
    }
    PRINTF("INFO: Create Socket OK. Port: %d\r\n", sl_Htons(sLocalAddr.sin_port));

    // binding the UDP socket to the UDP server address
    iStatus = sl_Bind(iSockID, (SlSockAddr_t *) & sLocalAddr, iAddrSize);
    if (iStatus < 0) {
	sl_Close(iSockID);
	PRINTF("ERROR: Bind Socket\r\n");
    }
    PRINTF("INFO: Bind Socket OK\r\n");

    /* ������� ���������� ������ */
    if (xchg_create_sync_obj() != OSI_OK) {
	while (1);
    }

    while (i) {
	//    memset(&pRxBuf->hdr, 0, sizeof(HEADER_t));
	iStatus = sl_RecvFrom(iSockID, pRxBuf, sizeof(XCHG_DATA_STRUCT_t), 0, (SlSockAddr_t *) & sAddr, (SlSocklen_t *) & iAddrSize);
	if (iStatus < 0) {
	    PRINTF("ERROR: Recv Socket\r\n");
	    break;
	} else {
	    //print_data_hex(pRxBuf, iStatus % sizeof(XCHG_DATA_STRUCT_t));
	    net_debug_parse_func();
	}
    }

    /* closing the socket after receiving 1000 packets */
    sl_Close(iSockID);
    PRINTF("INFO: Close Socket\r\n");

    PRINTF("INFO: Stop network\r\n");
    log_close_log_file();
    PRINTF("INFO: End....\r\n");

    /* power off the network processor */
    ret = sl_Stop(SL_STOP_TIMEOUT);
    LOOP_FOREVER();
}

/**
 * ������������ ���������� ������ �� ����
 * �� ����� ������������ ������ �� ��������
 */
static bool net_debug_parse_func(void)
{
    bool res = 0;
    HEADER_t *hdr = &pRxBuf->hdr;
    OsiReturnVal_e ret;

    /* ��������� ��������� */
    do {
	if (hdr->ndas != NDAS || hdr->dev_id != DEV_ADDR || hdr->pack_type != PACK_REQUEST || hdr->ver != DEV_VERSION) {
	    pCount->rx_cmd_err++;	/* �������� ������ */
	    break;
	}

	/* ����� ��������. �������� ��������� �� ��������� ISR ������� ������. */
	ret = osi_SyncObjSignal(&gTxSyncObj);	/* ���������� ������ */
	if (ret != OSI_OK) {
	    while (1);		/* Queue was not created and must not be used. */
	}
	pCount->rx_pack++;	/* �������� ������� */
    } while (0);
    return res;
}

/**
 * �������������� � ����� �������, ��������� � ����� recparam.ini
 */
void wlan_start(GNS_PARAM_STRUCT * par)
{
    long ret = -1;

    /* ������ ������ */
    buf_clear();

    /* Start the SimpleLink Host */
    ret = VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);
    if (ret < 0) {
	LOOP_FOREVER();
    }

  
    
    /* Start the WlanStationMode task */
    ret = osi_TaskCreate(vWlanStationTask, (const signed char *) "Wlan Station Task", OSI_STACK_SIZE, par, 1, NULL);
    if (ret < 0) {
	LOOP_FOREVER();
    }
}

/**
 * Connecting to a WLAN Accesspoint
 */
static long wlan_connect(void *v)
{
    SlSecParams_t secParams = { 0 };
    long lRetVal = 0;
    GNS_PARAM_STRUCT *par;

    if (v) {
	par = (GNS_PARAM_STRUCT *) v;

	PRINTF("INFO: SSID Name:   %s\n", par->gns_ssid_name);
	PRINTF("INFO: SSID Passwd: %s\n", par->gns_ssid_pass);
	PRINTF("INFO: Sec  Type:   %d\n", par->gns_ssid_sec_type);

	secParams.Key = (_i8 *) par->gns_ssid_pass;
	secParams.KeyLen = strlen(par->gns_ssid_pass);
	secParams.Type = par->gns_ssid_sec_type;

	lRetVal = sl_WlanConnect((_i8 *) par->gns_ssid_name, strlen(par->gns_ssid_name), 0, &secParams, 0);
	ASSERT_ON_ERROR(lRetVal);

	/* Wait for WLAN Event */
	while ((!IS_CONNECTED(get_wlan_status())) || (!IS_IP_ACQUIRED(get_wlan_status()))) {
#if 10
	    /* Toggle LEDs to Indicate Connection Progress */
	    led_toggle(1);
	    delay_ms(50);
#endif
	}

	conn_type = CONNECT_WLAN;
	PRINTF("INFO: Connected OK\n");
    }
    
   //mDNS_Query_OneShot(); 
    
    
    return SUCCESS;
}


#define MDNS_SERVICE_1     "PC1._ipp._tcp.local"
#define MDNS_TEXT_1           "vendor=TI"
#define MDNS_PORT_1           23
#define TTL                          2000
#define UNIQUE_SERVICE     1 
void mDNS_Query_OneShot(void)
{
   sl_NetAppStart(SL_NET_APP_MDNS_ID);
   sl_NetAppMDNSUnRegisterService(0, 0);
   sl_NetAppMDNSRegisterService(MDNS_SERVICE_1, strlen(MDNS_SERVICE_1), 
                                MDNS_TEXT_1, strlen(MDNS_TEXT_1),
                                MDNS_PORT_1, TTL, UNIQUE_SERVICE);}


/**
 * This function puts the device in its default state. It:
 */
static long ConfigureSimpleLinkToDefaultState(void)
{
    SlVersionFull ver = { 0 };
    _WlanRxFilterOperationCommandBuff_t RxFilterIdMask = { 0 };

    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;

    long ret = -1;
    long lMode = -1;

    // ���������� � ������ �������
    lMode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(lMode);

    /* Get the device's version-information */
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(ver);
    ret = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt, &ucConfigLen, (unsigned char *) (&ver));
    ASSERT_ON_ERROR(ret);

    PRINTF("Host Driver Version: %s\n\r", SL_DRIVER_VERSION);
    PRINTF("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
	   ver.NwpVersion[0], ver.NwpVersion[1], ver.NwpVersion[2], ver.NwpVersion[3],
	   ver.ChipFwAndPhyVersion.FwVersion[0], ver.ChipFwAndPhyVersion.FwVersion[1],
	   ver.ChipFwAndPhyVersion.FwVersion[2], ver.ChipFwAndPhyVersion.FwVersion[3],
	   ver.ChipFwAndPhyVersion.PhyVersion[0], ver.ChipFwAndPhyVersion.PhyVersion[1],
	   ver.ChipFwAndPhyVersion.PhyVersion[2], ver.ChipFwAndPhyVersion.PhyVersion[3]);

    // Set connection policy to Auto + SmartConfig 
    //      (Device's default connection policy)
    ret = sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(ret);

    // Remove all profiles
    ret = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(ret);

    //
    // Device in station-mode. Disconnect previous connection if any
    // The function returns 0 if 'Disconnected done', negative number if already
    // disconnected Wait for 'disconnection' event if 0 is returned, Ignore 
    // other return-codes
    //
    ret = sl_WlanDisconnect();
    if (0 == ret) {
	// Wait
	while (IS_CONNECTED(get_wlan_status())) {
	}
    }
    // Enable DHCP client
    ret = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE, 1, 1, &ucVal);
    ASSERT_ON_ERROR(ret);

    // Disable scan
    ucConfigOpt = SL_SCAN_POLICY(0);
    ret = sl_WlanPolicySet(SL_POLICY_SCAN, ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(ret);

    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    ret = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *) &ucPower);
    ASSERT_ON_ERROR(ret);

    // Set PM policy to normal
    ret = sl_WlanPolicySet(SL_POLICY_PM, SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(ret);

    // Unregister mDNS services
    ret = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(ret);

    // Remove  all 64 filters (8*8)
    memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    ret = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *) & RxFilterIdMask, sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(ret);

    ret = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(ret);
    return ret;			/* Success */
}

/**
 * ������������� ���� Com - ����� 
 */
void com_mux_config(void)
{
    MAP_PRCMPeripheralReset(PRCM_UARTA0);

    /* Enable Peripheral Clocks */
    MAP_PRCMPeripheralClkEnable(PRCM_UARTA0, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA0, PRCM_RUN_MODE_CLK);

    /* Configure PIN_55 for UART0 UART0_TX */
    MAP_PinTypeUART(PIN_55, PIN_MODE_3);

    /* Configure PIN_57 for UART0 UART0_RX */
    MAP_PinTypeUART(PIN_57, PIN_MODE_3);
}
