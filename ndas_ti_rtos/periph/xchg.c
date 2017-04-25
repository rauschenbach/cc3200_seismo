/***************************************************************************************
 * 				Сокеты и UDP и TCP одновременно
 ***************************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <simplelink.h>
#include <wlan.h>
#include <hw_types.h>
#include <hw_gprcm.h>


// Driverlib includes
#include <rom.h>
#include <rom_map.h>
#include <prcm.h>


#include "minIni.h"
#include "rtos_handlers.h"
#include "xchg.h"
#include "led.h"
#include "board.h"
#include "sdreader.h"
#include "dma.h"
#include "timtick.h"
#include "userfunc.h"
#include "status.h"
#include "main.h"



#define		UART_RX_TIMEOUT		100	/* 100 миллисекунд таймаут приема - высокий пока */
#define 	UART_BAUD_RATE		115200	/* Скорость порта для обмена с PC  */
#define         UART_TASK_SLEEP         1

/************************************************************************
 * 	Статические переменные
 * 	отображаются или указателем SRAM (USE_THE_LOADER)
 * 	или используются в секции данных  
 ************************************************************************/
static XCHG_DATA_STRUCT_h rx_data;
static XCHG_DATA_STRUCT_h tx_data;
static XCHG_COUNT_STRUCT_h xchg_counts;

static XCHG_DATA_STRUCT_h *pRxBuf = &rx_data;	/* Указатель на нашу структуру приема! */
static XCHG_DATA_STRUCT_h *pTxBuf = &tx_data;	/* Указатель на нашу структуру передачи! */
static XCHG_COUNT_STRUCT_h *pCount = &xchg_counts;	/* Счетчики обмена */

static volatile int iTimeout = 0;	/* Счет таймаутов приема  */

/* Номер станции  */
static u32 dev_addr = 0;


/* Параметры подключения по сети UDP и TCP */
static SlSockAddrIn_t sAddr;
static SlSockAddrIn_t sLocalAddr;
static int iSockUDP = -1;
static int iSockTCP = -1;



/* Тип соединения  - НЕ присоединено */
static CONNECTION_TYPE conn_type = CONNECT_NONE;

/* Синк объект - будет ожидать сообщение которое приходит из прерывания */
static OsiSyncObj_t gTxSyncObj;
static OsiReturnVal_e xchg_create_sync_obj(void);



static void vTxTask(void *);	/* Передающая задача   */
static void vUdpTask(void *);	/* Приемная задача по UDP */
static void vTcpTask(void *);	/* Приемная задача по TCP */
static void vWlanStationTask(void *);
static OsiTaskHandle gUdpTaskHandle;
static OsiTaskHandle gTcpTaskHandle;

#if 0
static OsiSyncObj_t gUdpSyncObj;
static OsiSyncObj_t gTcpSyncObj;
#endif

static bool net_debug_parse_func(void);

static long ConfigureSimpleLinkToDefaultState(void);
static void com_debug_read_ISR(u8);
static void com_rx_end(int);

static void com_tx_data(u8 *, int);
static long wlan_connect(void *);
static void wlan_tx_udp_data(u8 *, int);
static void wlan_tx_tcp_data(u8 *, int);
static void com_isr_handler(void);



/**
 * Инициализируем UART
 */
void com_init(void)
{
    u32 flags;
    flags = UART_INT_RX | UART_INT_DMATX | UART_INT_OE | UART_INT_PE | UART_INT_FE | UART_INT_BE;

    /*  Настроим FIFO на прием и передачу, иначе DMA не будет работать. Минимальный пакет - 20 байт */
    MAP_UARTFIFOLevelSet(UARTA0_BASE, UART_FIFO_TX1_8, UART_FIFO_RX1_8 /* UART_FIFO_RX2_8 */ );
    MAP_UARTFIFOEnable(UARTA0_BASE);

    /* Чистим буферы */
    mem_set(pTxBuf, 0, sizeof(XCHG_DATA_STRUCT_h));
    mem_set(pRxBuf, 0, sizeof(XCHG_DATA_STRUCT_h));
    mem_set(pCount, 0, sizeof(XCHG_COUNT_STRUCT_h));

    /* Убрать непринятые симводы если они там есть */
    while (UARTCharsAvail(UARTA0_BASE)) {
	(void) UARTCharGet(UARTA0_BASE);
    }

    /* Регистрируем прерывания */
    osi_InterruptRegister(INT_UARTA0, com_isr_handler, INT_PRIORITY_LVL_5);

    /* Объект синхронизации запустить */
    if (xchg_create_sync_obj() != OSI_OK) {
	while (1);
    }

    /* Чистим ошибки  */
    MAP_UARTRxErrorClear(UARTA0_BASE);

    /* Чистим флаги прерывания */
    MAP_UARTIntClear(UARTA0_BASE, flags);

    /* Ставим прерывания по-приему и ошибкам */
    MAP_UARTIntEnable(UARTA0_BASE, flags);
}

/**
 * создать передающую задачу и объект синхронизации
 */
static OsiReturnVal_e xchg_create_sync_obj(void)
{
    OsiReturnVal_e ret;
    do {
	/* Объект синхронизации (event или что то подобное) */
	ret = osi_SyncObjCreate(&gTxSyncObj);
	if (ret != OSI_OK) {
	    break;		/* Queue was not created and must not be used. */
	}

	/* Создаем задачу, которая будет передавать данные */
	ret = osi_TaskCreate(vTxTask, "TxDataTask", OSI_STACK_SIZE, NULL, TX_DATA_TASK_PRIORITY, NULL);
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
    /* Смотрим ошибки и очищае5м */
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
    /* Очистить статус */
    MAP_UARTIntClear(UARTA0_BASE, status);
    if (status & (UART_INT_OE | UART_INT_PE | UART_INT_FE | UART_INT_BE)) {
	pCount->rx_cmd_err++;
    }

    /* Вызываем функцию которая будет разбирать даные */
    if (status & UART_INT_RX) {
	while (MAP_UARTCharsAvail(UARTA0_BASE)) {
	    c = MAP_UARTCharGetNonBlocking(UARTA0_BASE);
	    com_debug_read_ISR(c);
	}
    }

    /* Завершить DMA при передаче */
    if (status & UART_INT_DMATX) {
	MAP_UARTDMADisable(UARTA0_BASE, UART_DMA_TX);
	pTxBuf->end = 1;
    }
}



/**
 * Обслуживание командного режима - прием
 * на любые неправильные пакеты не отвечаем
 */
static void com_debug_read_ISR(u8 rx_byte)
{
    /* Пришел самый первый байт посылки и наш адрес. Принимаем заголовок */
    if (pRxBuf->ind == 0 && rx_byte == 0x4e) {	/* Первый байт NDAS 0x444e */
	pRxBuf->ind = 1;	/* Один байт приняли */
	pRxBuf->cnt = 1;	/* Счетчик пакетов - первый байт приняли */
	pRxBuf->hdr.ndas = rx_byte;
	/* Устанавливаем таймаут на ~10 мс, если пришли лишние байты 
	 * или не окончился прием - сбросить автомат приема */
	set_timeout(UART_RX_TIMEOUT);
    } else {			/* Пришли последующие байты */
	if (1 == pRxBuf->cnt) {
	    pRxBuf->hdr.ndas |= (u16) rx_byte << 8;
	    if (pRxBuf->hdr.ndas != NDAS) {
		com_rx_end(RX_DATA_ERR);	/* рвем передачу. Второй байт д.б 0x44 */
	    }
	} else if (2 == pRxBuf->cnt) {	/* мл. байт crc16 */
	    pRxBuf->hdr.crc16 = rx_byte;
	} else if (3 == pRxBuf->cnt) {	/* ст. байт crc16 */
	    pRxBuf->hdr.crc16 |= (u16) rx_byte << 8;
	} else if (4 == pRxBuf->cnt) {	/* мл. байт dev_id */
	    pRxBuf->hdr.dev_id = rx_byte;
	} else if (5 == pRxBuf->cnt) {	/* мл. ср. байт dev_id */
	    pRxBuf->hdr.dev_id |= (u32) rx_byte << 8;
	} else if (6 == pRxBuf->cnt) {	/* ст. ср. байт dev_id */
	    pRxBuf->hdr.dev_id |= (u32) rx_byte << 16;
	} else if (7 == pRxBuf->cnt) {	/* ст.байт dev_id */
	    pRxBuf->hdr.dev_id |= (u32) rx_byte << 24;
/* НЕ проверяем адрес - это адрес отправителя!!! */
/*
	    if (pRxBuf->hdr.dev_id != DEV_ADDR) {
		com_rx_end(RX_DATA_ERR);	
	    }
*/
	} else if (8 == pRxBuf->cnt) {	/* мл.байт pack_type */
	    pRxBuf->hdr.pack_type = (PACK_TYPE) rx_byte;
	} else if (9 == pRxBuf->cnt) {	/* ст. байт pack_type */
	    pRxBuf->hdr.pack_type |= (u16) rx_byte << 16;
	    if (pRxBuf->hdr.pack_type != PACK_REQUEST) {
		com_rx_end(RX_DATA_ERR);	/* может быть только REQUEST */
	    }
	} else if (10 == pRxBuf->cnt) {
	    pRxBuf->hdr.cmd = (USER_CMD) rx_byte;	/* мл байт команды */
	} else if (11 == pRxBuf->cnt) {
	    pRxBuf->hdr.cmd |= (u16) rx_byte << 8;	/* ст. байт команды  */
	} else if (12 == pRxBuf->cnt) {
	    pRxBuf->hdr.ver = rx_byte;	/* мл байт ver */
	} else if (13 == pRxBuf->cnt) {
	    pRxBuf->hdr.ver |= (u32) rx_byte << 8;	/* мл. ср. байт ver */
	} else if (14 == pRxBuf->cnt) {
	    pRxBuf->hdr.ver |= (u32) rx_byte << 16;	/* ст. ср. байт ver */
	} else if (15 == pRxBuf->cnt) {
	    pRxBuf->hdr.ver |= (u32) rx_byte << 24;	/* ст. байт ver */
	    /* Не проверяем. При приёме пакета на контролере значение поля version игнорировать, ошибки не выдавать. */
	} else if (16 == pRxBuf->cnt) {
	    pRxBuf->hdr.size = rx_byte;	/* мл. байт size */
	} else if (17 == pRxBuf->cnt) {
	    pRxBuf->hdr.size |= (u16) rx_byte << 8;	/* ст. байт size */
	    if (pRxBuf->hdr.size > XCHG_BUF_LEN) {
		com_rx_end(RX_DATA_ERR);	/* не наш размер */
	    }

	    /* Сбросим автомат, если длина не соответсвует команде */
	    if (pRxBuf->hdr.size) {
		if (pRxBuf->hdr.cmd != CMD_WRITE_PARAMETERS && pRxBuf->hdr.cmd != CMD_START_PREVIEW) {
		    com_rx_end(RX_DATA_ERR);	/* Сброс автомата */
		}
	    }
	} else if (18 == pRxBuf->cnt) {
	    pRxBuf->hdr.rsvd0 = rx_byte;	/* мл. байт rsvd */
	} else if (19 == pRxBuf->cnt) {
	    /* reserved также игнорировать и не выдавать по нему ошибок. */
	    pRxBuf->hdr.rsvd0 |= (u16) rx_byte << 8;	/* ст. байт rsvd */
	}
	/* Принимаем даные, здесь можно включить прием по DMA, так как длина посылки нам известна */
	else if (pRxBuf->cnt >= sizeof(HEADER_t) && pRxBuf->cnt < pRxBuf->hdr.size + sizeof(HEADER_t)) {
	    pRxBuf->data[pRxBuf->cnt - sizeof(HEADER_t)] = rx_byte;	/* Набиваем буфер */
	}

	pRxBuf->cnt++;
	/* Проверим CRC16 */
	if (pRxBuf->cnt >= pRxBuf->hdr.size + sizeof(HEADER_t)) {
#if 0
	    if (check_crc(pRxBuf->data, pRxBuf->hdr.size) != pRxBuf->hdr.crc16) {
		com_rx_end(RX_CRC_ERR);
	    } else
#endif
		clr_timeout();	/* Очищаем таймаут */
	    com_rx_end(RX_DATA_OK);
	}
    }
}

/**
 * Конец приема. Запускаем задачу на передачу
 * Этот код можно сделать "NAKED" и вызывать напрямую из ISR
 */
static void com_rx_end(int err)
{
    OsiReturnVal_e ret;
    pRxBuf->ind = 0;
    pRxBuf->cnt = 0;
    pRxBuf->end = 0;		/* Посылка приянята Error */
    if (err == RX_DATA_OK) {
	pRxBuf->end = 1;	/* Посылка принята OK */
	pTxBuf->end = 0;	/* Начало передачи */
	pCount->rx_pack++;	/* Правильно принятые пакеты */
	conn_type = CONNECT_COMM;	/* Пришла команда по COM порту */
	/* Старт передачи. Передаем сообщение из процедуры ISR внешней задаче. */
	ret = osi_SyncObjSignalFromISR(&gTxSyncObj);	/* Отправляем сигнал */
	if (ret != OSI_OK) {
	    while (1);		/* Queue was not created and must not be used. */
	}
    } else if (err == RX_CRC_ERR) {	/* Контрольная сумма неверна-начинаем прием заново */
	pCount->rx_crc_err++;	/* Ошибка CRC16 */
    } else if (err == RX_DATA_ERR) {	/* Если что-то пошло не так */
	pCount->rx_cmd_err++;	/* Ошибочная команда */
    }
}

/**
 * Передающая задача RTOS. Ожидает сообщения и передает буфер данных
 * Отправляем или до DMA или побайтно всю цепочку
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

	    /* Формируем ответ на входящую команду */
	    switch (pRxBuf->hdr.cmd) {

		/* 12 получить сейсмограмму в режиме preview */
	    case CMD_QUERY_PREVIEW:
		res = ADS131_get_pack((ADS131_DATA_h *) pTxBuf);
		if (res) {
		    pTxBuf->hdr.pack_type = PACK_ACK;	/* Подтвердим */
		    pTxBuf->hdr.ver++;	/* При отсылке сигнала в version вставлять последовательно увеличивающиеся значения */
		    len = sizeof(ADS131_DATA_h);	/* Столько передавать */
		} else {
		    pTxBuf->hdr.pack_type = PACK_EMPTY;	/* Нет данных */
		    len = sizeof(HEADER_t);
		}
		pTxBuf->hdr.cmd = CMD_QUERY_PREVIEW;	/* Команда идет обратно */
		break;
		/* Прием параметров верхней PC */
	    case CMD_RECEIVE_PARAMETERS:
		ADS131_get_adcp((ADCP_h *) pTxBuf);
		pTxBuf->hdr.pack_type = PACK_ACK;	/* Подтвердим */
		pTxBuf->hdr.cmd = CMD_RECEIVE_PARAMETERS;	/* Команда идет обратно */
		len = sizeof(ADCP_h);
		break;
		/* Cтарт приёма сигнала в режиме preview - время для Sync */
	    case CMD_START_PREVIEW:
		res = ADS131_start_osciloscope((START_PREVIEW_h *) pRxBuf);
		pTxBuf->hdr.pack_type = res ? PACK_ACK : PACK_NAK;	/* Подтвердим */
		pTxBuf->hdr.cmd = CMD_START_PREVIEW;	/* Команда идет обратно */
		len = sizeof(HEADER_t);
		break;
		/*Стоп измерений */
	    case CMD_STOP_ACQUIRING:
		res = ADS131_stop_osciloscope();
		pTxBuf->hdr.pack_type = res ? PACK_ACK : PACK_NAK;	/* Подтвердим */
		pTxBuf->hdr.cmd = CMD_STOP_ACQUIRING;	/* Команда идет обратно */
		pTxBuf->hdr.ver = 0;
		len = sizeof(HEADER_t);
		break;
		/* Записать параметры/Применить параметры 
		 * Скопируем принятые данные. учитываем заголовок */
	    case CMD_APPLY_PARAMETERS:
	    case CMD_WRITE_PARAMETERS:
		res = ADS131_write_parameters((ADCP_h *) pRxBuf);
		pTxBuf->hdr.pack_type = res ? PACK_ACK : PACK_NAK;	/* Подтвердим */
		pTxBuf->hdr.cmd = CMD_WRITE_PARAMETERS;	/* Команда идет обратно */
		len = sizeof(HEADER_t);
		break;
		/* Запрос устройства */
	    case CMD_QUERY_DEVICE:
		proto_get_dev_id((DEV_ID_h *) pTxBuf);
		pTxBuf->hdr.pack_type = PACK_ACK;	/* Подтвердим */
		pTxBuf->hdr.cmd = CMD_QUERY_DEVICE;	/* Команда идет обратно */
		len = sizeof(DEV_ID_h);
		break;
		/* Запрос статуса устройства */
	    case CMD_QUERY_STAT:
		status_get_full_status((STATUS_h *) pTxBuf);
		pTxBuf->hdr.pack_type = PACK_ACK;	/* Подтвердим */
		pTxBuf->hdr.cmd = CMD_QUERY_STAT;	/* Команда идет обратно */
		len = sizeof(STATUS_h);
		break;
		/* Запрос информации об устройстве */
	    case CMD_QUERY_INFO:
		proto_get_info((INFO_h *) pTxBuf);
		pTxBuf->hdr.pack_type = PACK_ACK;	/* Подтвердим */
		pTxBuf->hdr.cmd = CMD_QUERY_INFO;	/* Команда идет обратно */
		len = sizeof(INFO_h);
		break;
		/* карту к процессору, бессмысленная команда при этой схеме */
	    case CMD_SELECT_SD_TO_MCU:
		set_sd_to_mcu();
		pTxBuf->hdr.pack_type = PACK_ACK;	/* Подтвердим */
		pTxBuf->hdr.cmd = CMD_SELECT_SD_TO_MCU;	/* Команда идет обратно */
		len = sizeof(HEADER_t);
		break;
		/* карту к картридеру */
	    case CMD_SELECT_SD_TO_CR:
		set_sd_to_cr();
		pTxBuf->hdr.pack_type = PACK_ACK;	/* Подтвердим */
		pTxBuf->hdr.cmd = CMD_SELECT_SD_TO_CR;	/* Команда идет обратно */
		len = sizeof(HEADER_t);
		break;
		/* Сброс процессора */
	    case CMD_RESET_MCU:
		pTxBuf->hdr.pack_type = PACK_ACK;	/* Подтвердим */
		pTxBuf->hdr.cmd = CMD_RESET_MCU;	/* Команда идет обратно */
		len = sizeof(HEADER_t);
		break;
		/* Добавим команду - выдать счетчики обмена */
	    case CMD_GET_COUNT:
		mem_copy(pTxBuf, pCount, sizeof(XCHG_COUNT_STRUCT_h));
		pTxBuf->hdr.pack_type = PACK_ACK;	/* Подтвердим */
		pTxBuf->hdr.cmd = CMD_GET_COUNT;	/* Команда идет обратно */
		len = sizeof(XCHG_COUNT_STRUCT_h);
		break;
	    default:
		len = 0;	/* Не овечать на неизвестную команду */
		break;
	    }


	    if (len >= sizeof(HEADER_t)) {
		/* Сформируем заголовок на отправление */
		pTxBuf->hdr.ndas = NDAS;
		pTxBuf->hdr.dev_id = dev_addr;
		pTxBuf->hdr.rsvd0 = 0;	/* При посылке пакетов с контролера reserved всегда 0 */
		pTxBuf->hdr.size = len - sizeof(HEADER_t);	/* Считаем длину: длина без заголовка */
		/* Считаем CRC */
		if (len == sizeof(HEADER_t)) {
		    pTxBuf->hdr.crc16 = 0;
		} else {
		    /* CRC для проверки */
		    pTxBuf->hdr.crc16 = check_crc((u8 *) pTxBuf->data, len - sizeof(HEADER_t));	/* CRC для проверки */
		}

		/* В зависимости от того, откуда пришел запрос - выдаем ответ */
		if (conn_type == CONNECT_COMM) {
		    com_tx_data((u8 *) pTxBuf, len);
		} else if (conn_type == CONNECT_UDP) {
		    wlan_tx_udp_data((u8 *) pTxBuf, len);
		} else if (conn_type == CONNECT_TCP) {
		    wlan_tx_tcp_data((u8 *) pTxBuf, len);
		}

		/* После ответной передачи на команду сброс процессора! */
		if (pTxBuf->hdr.cmd == CMD_RESET_MCU) {
		    delay_ms(50);
		    board_reset();
		}
	    }
	}
	osi_Sleep(UART_TASK_SLEEP);
    }
}

/**
 * Передача данных по DMA
 */
static void com_tx_data(u8 * data, int len)
{
    /* Initialize uDMA */
    UDMAInit();
    /* Настраиваем канал DMA для UARTA0 */
    UDMASetupTransfer(UDMA_CH9_UARTA0_TX, UDMA_MODE_BASIC,	/* Простой режим */
		      len,	/* Длина передачи с заголовком */
		      UDMA_SIZE_8,	/* По байту */
		      UDMA_ARB_1,	/* Гранулярность? */
		      data,	/* Буфер передачи */
		      UDMA_SRC_INC_8,	/* Адрес источника инкрементируется */
		      (void *) (UARTA0_BASE + UART_O_DR),	/* Адрес назначения */
		      UDMA_DST_INC_NONE);	/* Адрес назначения не инкрементируется */
    /* Запустить прередачу */
    MAP_UARTDMAEnable(UARTA0_BASE, UART_DMA_TX);
    pCount->tx_pack++;		/* Счетчики передач */
}


/**
 * Передача данных по сети через UDP сокет
 */
static void wlan_tx_udp_data(u8 * data, int len)
{
    int iAddrSize;
    int iStatus;
    iAddrSize = sizeof(SlSockAddrIn_t);
    iStatus = sl_SendTo(iSockUDP, data, len, 0, (SlSockAddr_t *) & sAddr, (SlSocklen_t) iAddrSize);
    if (iStatus != len) {
	PRINTF("ERROR: SendTo...\r\n");
    } else {
	pCount->tx_pack++;	/* Счетчики передач */
    }
    conn_type = CONNECT_COMM;	/* Сбрасываем указатель на порт */
}


/**
 * Передача данных по сети через TCP сокет
 */
static void wlan_tx_tcp_data(u8 * data, int len)
{
    int iStatus;
    iStatus = sl_Send(iSockTCP, data, len, 0);
    if (iStatus != len) {
	PRINTF("ERROR: Send...\r\n");
    } else {
#if 0
	PRINTF("SendTo (%d.%d.%d.%d)... OK\r\n",
	       (u8) (sAddr.sin_addr.s_addr), (u8) (sAddr.sin_addr.s_addr >> 8), (u8) (sAddr.sin_addr.s_addr >> 16), (u8) (sAddr.sin_addr.s_addr >> 24));
#endif
	pCount->tx_pack++;	/* Счетчики передач */
    }
    conn_type = CONNECT_COMM;	/* Сбрасываем указатель на порт */
}


/**
 * Создание сокета UDP и прием через него
 */
static void vUdpTask(void *par)
{
    long ret = -1;
    int iAddrSize;
    int iStatus;
    fd_set read_s;		// Множество
    timeval time_out;		// Таймаут
    while (1) {

	/* Ждать семафора, что есть соединение с WiFi */
	while (!IS_CONNECTED(get_wlan_status())) {
	    osi_Sleep(500);
	}


	/* filling the UDP server socket address */
	sLocalAddr.sin_family = SL_AF_INET;
	sLocalAddr.sin_port = sl_Htons((u16) ((GNS_PARAM_STRUCT *) par)->gns_server_udp_port);
	sLocalAddr.sin_addr.s_addr = 0;
	iAddrSize = sizeof(SlSockAddrIn_t);
	/* Создаем UDP socket */
	iSockUDP = sl_Socket(SL_AF_INET, SL_SOCK_DGRAM, 0);
	if (iSockUDP < 0) {
	    PRINTF("ERROR: Create UDP Socket\r\n");
	    continue;
	}
	PRINTF("INFO: Create UDP Socket OK. Port: %d\r\n", sl_Htons(sLocalAddr.sin_port));
	/* binding the UDP socket to the UDP server address */
	iStatus = sl_Bind(iSockUDP, (SlSockAddr_t *) & sLocalAddr, iAddrSize);
	if (iStatus < 0) {
	    sl_Close(iSockUDP);
	    PRINTF("ERROR: Bind UDP Socket\r\n");
	    continue;
	}
	PRINTF("INFO: Bind UDP Socket OK\r\n");
	FD_ZERO(&read_s);	/* Обнуляем множество */
	FD_SET(iSockUDP, &read_s);	/* Заносим в него наш сокет  */
	time_out.tv_sec = 0;
	time_out.tv_usec = 500000;	/* Таймаут 0.5 секунды - ожидаем входящих запросов */
	while (IS_CONNECTED(get_wlan_status())) {

	    ret = select(iSockUDP + 1, &read_s, NULL, NULL, &time_out);
	    if ((ret != 0) && (FD_ISSET(iSockUDP, &read_s))) {
		iStatus = sl_RecvFrom(iSockUDP, pRxBuf, sizeof(XCHG_DATA_STRUCT_h), 0, (SlSockAddr_t *) & sAddr, (SlSocklen_t *) & iAddrSize);
		if (iStatus < 0) {
		    PRINTF("ERROR: Recv Socket\r\n");
		    break;
		} else if (iStatus > 0) {
		    conn_type = CONNECT_UDP;
		    net_debug_parse_func();
		} else {
		    osi_Sleep(1);
		}
	    }
	}
	/* closing the socket */
	sl_Close(iSockUDP);
	PRINTF("INFO: Close UDP Socket. Reconnect\r\n");
    }
}

/**
 * Приемная задача по TCP 
 */
static void vTcpTask(void *par)
{
    int iAddrSize;
    int iStatus;
    int tcpSock;		/* Временный доакцептный сокет */
    long lNonBlocking = 0;
    while (1) {

	/* Ждем сети */
	while (!IS_CONNECTED(get_wlan_status())) {
	    osi_Sleep(500);
	}


	/* filling the TCP server socket address */
	sLocalAddr.sin_family = SL_AF_INET;
	sLocalAddr.sin_port = sl_Htons((u16) ((GNS_PARAM_STRUCT *) par)->gns_server_tcp_port);
	sLocalAddr.sin_addr.s_addr = INADDR_ANY;
	iAddrSize = sizeof(SlSockAddrIn_t);
	/* creating a TCP socket */
	tcpSock = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, 0);
	if (tcpSock < 0) {
	    PRINTF("ERROR: Create TCP Socket\r\n");
	    continue;
	}
	PRINTF("INFO: Create TCP Socket OK. Port: %d\r\n", sl_Htons(sLocalAddr.sin_port));
	/* binding the TCP socket to the UDP server address */
	iStatus = sl_Bind(tcpSock, (SlSockAddr_t *) & sLocalAddr, iAddrSize);
	if (iStatus < 0) {
	    sl_Close(tcpSock);
	    PRINTF("ERROR: Bind TCP Socket\r\n");
	    continue;
	}
	PRINTF("INFO: Bind TCP Socket OK\r\n");
	/* Только одно соединение  */
	iStatus = sl_Listen(tcpSock, 1);
	if (iStatus < 0) {
	    sl_Close(tcpSock);
	    PRINTF("ERROR: Listen TCP Socket\r\n");
	    continue;
	}
	PRINTF("INFO: Listen TCP Socket OK\r\n");
	/* setting socket option to make the socket as non blocking */
	iStatus = sl_SetSockOpt(tcpSock, SL_SOL_SOCKET, SL_SO_NONBLOCKING, &lNonBlocking, sizeof(lNonBlocking));
	if (iStatus < 0) {
	    sl_Close(tcpSock);
	    PRINTF("ERROR: SOCKET_OPT_ERROR\n");
	    continue;
	}
	iSockTCP = SL_EAGAIN;
	int connected = -1;
	while (IS_CONNECTED(get_wlan_status())) {

	    /* waiting for a new incoming TCP connection */
	    PRINTF("INFO: Waiting for connection...\r");
	    while (iSockTCP < 0) {
		// accepts a connection form a TCP client, if there is any otherwise returns SL_EAGAIN
		iSockTCP = sl_Accept(tcpSock, (struct SlSockAddr_t *) &sAddr, (SlSocklen_t *) & iAddrSize);
		if (iSockTCP == SL_EAGAIN) {
		    MAP_UtilsDelay(10000);
		} else if (iSockTCP < 0) {
		    // error
		    sl_Close(iSockTCP);
		    sl_Close(tcpSock);
		    PRINTF("ACCEPT_ERROR\n");
		    break;
		} else {
		    PRINTF("Accept OK\n");
		    connected = 0;
		}
	    }

	    if (connected < 0) {
		break;
	    }


	    /* Пока есть TCP соединение - крутимся в этом цикле  */
	    while (1) {
		iStatus = sl_Recv(iSockTCP, pRxBuf, sizeof(XCHG_DATA_STRUCT_h), 0);
		if (iStatus < 0) {
		    PRINTF("ERROR: Recv Socket\r\n");
		    break;
		} else if (iStatus > 0) {
		    connected = 1;
		    conn_type = CONNECT_TCP;
		    net_debug_parse_func();
		} else {
		    if (connected == 1) {
			PRINTF("WARN: Connection lost\n");
			break;
		    }
		}
	    }

	    /* closing the socket after receiving 1000 packets */
	    sl_Close(iSockTCP);
	    PRINTF("INFO: Close Socket\n");
	    iSockTCP = -1;
	    if (iStatus < 0) {
		PRINTF("Try to reconnect\n");
		break;
	    }
	}
    }
}




/**
 * Start simplelink, connect to the ap and run the ping test
 */
static void vWlanStationTask(void *par)
{
    long /*OsiReturnVal_e */ ret;
    while (1) {
	osi_Sleep(500);
#if 0
	/* Объект синхронизации (event или что то подобное) */
	if (gUdpSyncObj == NULL) {
	    ret = osi_SyncObjCreate(&gUdpSyncObj);
	    if (ret != OSI_OK) {
		PRINTF("FAILED: Create UDP SyncObj\n\r");
		continue;
	    }
	} else {
	    PRINTF("INFO: UDP SyncObj already exists\n\r");
	}


	/* Объект синхронизации (event или что то подобное) */
	if (gTcpSyncObj == NULL) {
	    ret = osi_SyncObjCreate(&gTcpSyncObj);
	    if (ret != OSI_OK) {
		PRINTF("FAILED: Create TCP SyncObj\n\r");
		continue;
	    }
	} else {
	    PRINTF("INFO: TCP SyncObj already exists\n\r");
	}
#endif

	ret = ConfigureSimpleLinkToDefaultState();
	if (ret < 0) {
	    if (DEVICE_NOT_IN_STATION_MODE == ret) {
		PRINTF("FAILED: Configure the device in its default state\n");
	    }
	    continue;
	}

	PRINTF("INFO: Device is configured in default state \n\r");
	/* Assumption is that the device is configured in station mode already and it is in its default state */
	ret = sl_Start(0, 0, 0);
	if (ret < 0 || ROLE_STA != ret) {
	    PRINTF("FAILED: Start the device\n");
	    continue;
	}
	PRINTF("INFO: Device started as STATION\n");
	/* Connecting to WLAN AP */
	ret = wlan_connect(par);
	if (ret < 0) {
	    PRINTF("FAILED: Establish connection w/ an AP\n");
	    continue;
	}
	PRINTF("INFO: Connection established w/ AP and IP is aquired\n");
/////------>
	/* Создаем задачу, которая будет принимать данные */
	if (gUdpTaskHandle == NULL) {
	    ret = osi_TaskCreate(vUdpTask, "UdpRxTask", OSI_STACK_SIZE, par, UDP_RX_TASK_PRIORITY, &gUdpTaskHandle);
	    if (ret != OSI_OK) {
		continue;
	    } else {
		PRINTF("INFO: UDP Task create OK\n\r");
	    }
	}
#if 0
	/* Старт передачи. Передаем сообщение из процедуры ISR внешней задаче. */
	ret = osi_SyncObjSignal(&gUdpSyncObj);	/* Отправляем сигнал */
	if (ret != OSI_OK) {
	    conn_type = CONNECT_COMM;	/* Сбрасываем указатель на порт */
	    while (1);		/* Queue was not created and must not be used. */
	}
#endif

	/* Создаем задачу, которая будет принимать данные */
	if (gTcpTaskHandle == NULL) {
	    ret = osi_TaskCreate(vTcpTask, "TcpRxTask", OSI_STACK_SIZE, par, TCP_RX_TASK_PRIORITY, &gTcpTaskHandle);
	    if (ret != OSI_OK) {
		continue;
	    } else {
		PRINTF("INFO: TCP Rx Task create OK\n\r");
	    }
	}

	/* Пока не рухнет  */
	while (IS_CONNECTED(get_wlan_status())) {
#if 0
	    /* Проверяем подключен ли USB кабель. При такой последовательности  будет редко опрашивать TWI  */
	    if (get_sd_state() == SD_CARD_TO_CR && get_sd_cable() == 0) {
		/*      set_sd_to_mcu(); */
		board_reset();	/* Сброс платы */
	    }
#endif
	    osi_Sleep(100);
	}

	/* power off the network processor */
	ret = sl_Stop(SL_STOP_TIMEOUT);
	PRINTF("INFO: Stop network processor\r\n");
	PRINTF("INFO: End....Try to reconnect\r\n");
    }
    log_close_log_file();
}



/**
 * Обслуживание приема в командном режиме по сети
 * на любые неправильные пакеты не отвечаем
 */
static bool net_debug_parse_func(void)
{
    bool res = 0;
    HEADER_t *hdr = &pRxBuf->hdr;
    OsiReturnVal_e ret;
    /* Проверяем заголовок */
    do {
	if (hdr->ndas != NDAS /*|| hdr->dev_id != dev_addr */  || hdr->pack_type != PACK_REQUEST) {
	    pCount->rx_cmd_err++;	/* Счетчики ошибок */
	    break;
	}

	/* Сбросим автомат, если длина не соответсвует команде */
	if (hdr->size) {
	    if (hdr->cmd != CMD_WRITE_PARAMETERS && hdr->cmd != CMD_START_PREVIEW) {
		pCount->rx_cmd_err++;	/* Счетчики ошибок */
		break;
	    }
	}
	/* Проверим CRC16  */
	if (check_crc(pRxBuf->data, hdr->size) != hdr->crc16) {
	    pCount->rx_crc_err++;	/* Счетчики ошибок */
	    break;
	}

	/* Старт передачи. Передаем сообщение из процедуры ISR внешней задаче. */
	ret = osi_SyncObjSignal(&gTxSyncObj);	/* Отправляем сигнал */
	if (ret != OSI_OK) {
	    conn_type = CONNECT_COMM;	/* Сбрасываем указатель на порт */
	    while (1);		/* Queue was not created and must not be used. */
	}
	pCount->rx_pack++;	/* Счетчики приемов */
    } while (0);
    return res;
}

/**
 * Подсоединиться к точке доступа, указанной в файле recparam.ini
 */
void xchg_start(GNS_PARAM_STRUCT * par)
{
    long ret = -1;


    /* Адрес, который пришел с SD карты */
    dev_addr = par->gns_addr;

    /* Создать передающую задачу для порта и wlan + объект - семафор */
    com_init();

    /* Не запускать сетевые задачи если имя сети None. Только COM порт */
    if (par && strnicmp(par->gns_ssid_name, "None", 4)) {

	/* Start the SimpleLink Host */
	ret = VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);
	if (ret < 0) {
	    LOOP_FOREVER();
	}

	/* Start the WlanStationMode task */
	ret = osi_TaskCreate(vWlanStationTask, (const signed char *) "WlanStationTask", OSI_STACK_SIZE, par, WLAN_STATION_TASK_PRIORITY, NULL);
	if (ret < 0) {
	    LOOP_FOREVER();
	}
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Connecting to a WLAN Accesspoint
 */
static long wlan_connect(void *v)
{
    char server_name[64];
    SlSecParams_t secParams = {
	0
    };
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
	    led_toggle(1);	/* Toggle LEDs to Indicate Connection Progress */
	    osi_Sleep(50);
	}

	PRINTF("INFO: Connected OK\n");
	//Registering for the mDNS service.
	sprintf(server_name, "GNS%04d._uart._tcp.local", par->gns_addr);
	lRetVal = sl_NetAppMDNSUnRegisterService((signed char const *) server_name, (unsigned char) strlen(server_name));
	lRetVal = sl_NetAppMDNSRegisterService((signed char const *) server_name, (unsigned char) strlen(server_name),
					       "Service registered for 3200", (unsigned char) strlen("Service registered for 3200"), 200, 2000, 1);
	if (lRetVal == 0) {
	    PRINTF("INFO: MDNS Registration successful\n\r");
	} else {
	    PRINTF("ERROR: MDNS Registered failed\n\r");
	}

    }

    return SUCCESS;
}

/**
 * This function puts the device in its default state. It:
 * Following function configure the device to default state by cleaning
 * the persistent settings stored in NVMEM (viz. connection profiles &
 * policies, power policy etc)
 * Applications may choose to skip this step if the developer is sure
 * that the device is in its default state at start of applicaton
 * Note that all profiles and persistent settings that were done on the
 * device will be lost
 */
static long ConfigureSimpleLinkToDefaultState(void)
{
    SlVersionFull ver = {
	0
    };
    _WlanRxFilterOperationCommandBuff_t RxFilterIdMask = {
	0
    };
    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;
    long ret = -1;
    long lMode = -1;
    // Подключаем в режиме станции
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
/*    ret = sl_WlanPolicySet(SL_POLICY_PM, SL_ALWAYS_ON_POLICY, NULL, 0); */
    ret = sl_WlanPolicySet(SL_POLICY_PM, SL_LOW_POWER_POLICY, NULL, 0);
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
 * Затактировать ноги Com - порта 
 */
void com_mux_config(void)
{
    /* Затактировать ноги UART */
    MAP_PRCMPeripheralReset(PRCM_UARTA0);

    /* Enable Peripheral Clocks */
    MAP_PRCMPeripheralClkEnable(PRCM_UARTA0, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA0, PRCM_RUN_MODE_CLK);

    /* Configure PIN_55 for UART0 UART0_TX */
    MAP_PinTypeUART(PIN_55, PIN_MODE_3);

    /* Configure PIN_57 for UART0 UART0_RX */
    MAP_PinTypeUART(PIN_57, PIN_MODE_3);

    /* Настроить UART - 115200 - 8N2 */
    MAP_UARTConfigSetExpClk(UARTA0_BASE, MAP_PRCMPeripheralClockGet(PRCM_UARTA0), UART_BAUD_RATE,
			    UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_TWO | UART_CONFIG_PAR_NONE);

}

/*
 * Выключить com порт
 */
void com_mux_unconfig(void)
{
    /* Убрать тактирование */
    MAP_PRCMPeripheralClkDisable(PRCM_UARTA0, PRCM_RUN_MODE_CLK);
}


/*************** Таймауты *****/
/**
 * Вызывается из операционной системы
 */

#ifdef USE_FREERTOS
void vApplicationTickHook(void)
#else
void Clock_tick(void)
#endif
{
    /* Decrement the TimingDelay variable */
    if (iTimeout != 0x00) {
	--iTimeout;
	if (iTimeout == 0) {
	    pRxBuf->ind = 0;
	    pCount->rx_timeout++;	/* Таймаут приема */
	}
    }
}


/* Установить таймаут в мс */
void set_timeout(int ms)
{
    iTimeout = ms;
}

/* Проверить, прошел таймаут или не */
bool is_timeout(void)
{
    volatile bool res = false;
    if (iTimeout == 0) {
	res = true;
    }
    return res;
}

void clr_timeout(void)
{
    iTimeout = 0;
}
