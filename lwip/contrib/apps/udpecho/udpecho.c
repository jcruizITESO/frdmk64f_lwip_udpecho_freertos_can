/*
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Juan Carlos Ruiz Ochoa <jruiz.ochoa@iteso.mx>
 *
 */

#include "udpecho.h"

#include "lwip/opt.h"

#if LWIP_NETCONN

#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"

/* CAN */
#include "fsl_debug_console.h"
#include "fsl_flexcan.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include <stdlib.h>
#include <time.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define EXAMPLE_CAN            CAN0
#define EXAMPLE_CAN_CLK_SOURCE (kFLEXCAN_ClkSrc1)
#define EXAMPLE_CAN_CLK_FREQ   CLOCK_GetFreq(kCLOCK_BusClk)
/* Set USE_IMPROVED_TIMING_CONFIG macro to use api to calculates the improved CAN / CAN FD timing values. */
#define USE_IMPROVED_TIMING_CONFIG (1U)
#define EXAMPLE_FLEXCAN_IRQn       CAN0_ORed_Message_buffer_IRQn
#define EXAMPLE_FLEXCAN_IRQHandler CAN0_ORed_Message_buffer_IRQHandler
#define RX_MESSAGE_BUFFER_NUM      (9)
#define TX_MESSAGE_BUFFER_NUM      (8)
#define DLC                        (8)

/* Fix MISRA_C-2012 Rule 17.7. */
#define LOG_INFO (void)PRINTF
/*******************************************************************************
 * Variables
 ******************************************************************************/
volatile bool rxComplete = false;
#if (defined(USE_CANFD) && USE_CANFD)
flexcan_fd_frame_t txFrame, rxFrame;
#else
flexcan_frame_t txFrame, rxFrame;
#endif
QueueHandle_t xQueue1;
/*-----------------------------------------------------------------------------------*/
static void
udpecho_thread(void *arg)
{
  struct netconn *conn;
  /*struct netbuf *buf;
  char buffer[4096];*/
  err_t err;
  LWIP_UNUSED_ARG(arg);
  ip_addr_t addr;


/*#if LWIP_IPV6
  conn = netconn_new(NETCONN_UDP_IPV6);
  LWIP_ERROR("udpecho: invalid conn", (conn != NULL), return;);
  netconn_bind(conn, IP6_ADDR_ANY, 7);
#else /* LWIP_IPV6 */
  conn = netconn_new(NETCONN_UDP);
  LWIP_ERROR("udpecho: invalid conn", (conn != NULL), return;);
  netconn_bind(conn, IP_ADDR_ANY, 7);
  IP4_ADDR(&addr, 192, 168, 0, 100);

/*#endif /* LWIP_IPV6 */

  	  struct netbuf *buf;
	  /* create a new netbuf */
	  buf = netbuf_new();

  while(1)
  {
	  if( xQueue1 != 0 )
	  {
		  flexcan_frame_t CanFrame;
	        // Receive a message on the created queue.  Block for 10 ticks if a
	        // message is not immediately available.
	        if( xQueueReceive( xQueue1, &( CanFrame ), ( TickType_t ) 10 ) )
	       {
	      	  void *data;
	      	  u16_t len;


	      	  /* reference the text into the netbuf */
	      	  netbuf_ref(buf,(const void *) &CanFrame, sizeof(flexcan_frame_t));
	      	  len = sizeof(flexcan_frame_t);

	      	  err = netconn_sendto(conn, buf, &addr, 50000);
	      	  if (err != ERR_OK) {
	      		  PRINTF("udpecho: netconn_write: error \"%s\"\n", lwip_strerr(err));
	      		  break;

	      	  }
	       }


	    }
  }

  netconn_close(conn);
  netconn_delete(conn);

  vTaskDelete(NULL);

}

static void
CAN_thread(void *arg)
{
	flexcan_config_t flexcanConfig;
	flexcan_rx_mb_config_t mbConfig;
#if (defined(FSL_FEATURE_FLEXCAN_HAS_EXTENDED_FLAG_REGISTER)) && (FSL_FEATURE_FLEXCAN_HAS_EXTENDED_FLAG_REGISTER > 0)
	    uint64_t flag = 1U;
#else
	uint32_t flag = 1U;
#endif

	/* Initialize board hardware. */
	/*BOARD_InitBootPins();
	BOARD_InitBootClocks();
	BOARD_InitDebugConsole();*/

	LOG_INFO("\r\n==FlexCAN loopback functional example -- Start.==\r\n\r\n");

    /* Init FlexCAN module. */
    /*
     * flexcanConfig.clkSrc                 = kFLEXCAN_ClkSrc0;
     * flexcanConfig.baudRate               = 1000000U;
     * flexcanConfig.baudRateFD             = 2000000U;
     * flexcanConfig.maxMbNum               = 16;
     * flexcanConfig.enableLoopBack         = false;
     * flexcanConfig.enableSelfWakeup       = false;
     * flexcanConfig.enableIndividMask      = false;
     * flexcanConfig.disableSelfReception   = false;
     * flexcanConfig.enableListenOnlyMode   = false;
     * flexcanConfig.enableDoze             = false;
     */
    FLEXCAN_GetDefaultConfig(&flexcanConfig);

#if defined(EXAMPLE_CAN_CLK_SOURCE)
    flexcanConfig.clkSrc = EXAMPLE_CAN_CLK_SOURCE;
#endif

    flexcanConfig.enableLoopBack = true;
    flexcanConfig.enableIndividMask = true;

#if (defined(USE_IMPROVED_TIMING_CONFIG) && USE_IMPROVED_TIMING_CONFIG)
    flexcan_timing_config_t timing_config;
    memset(&timing_config, 0, sizeof(flexcan_timing_config_t));
#if (defined(USE_CANFD) && USE_CANFD)
    if (FLEXCAN_FDCalculateImprovedTimingValues(EXAMPLE_CAN, flexcanConfig.baudRate, flexcanConfig.baudRateFD,
                                                EXAMPLE_CAN_CLK_FREQ, &timing_config))
    {
        /* Update the improved timing configuration*/
        memcpy(&(flexcanConfig.timingConfig), &timing_config, sizeof(flexcan_timing_config_t));
    }
    else
    {
        LOG_INFO("No found Improved Timing Configuration. Just used default configuration\r\n\r\n");
    }
#else
    if (FLEXCAN_CalculateImprovedTimingValues(EXAMPLE_CAN, flexcanConfig.baudRate, EXAMPLE_CAN_CLK_FREQ,
                                              &timing_config))
    {
        /* Update the improved timing configuration*/
        memcpy(&(flexcanConfig.timingConfig), &timing_config, sizeof(flexcan_timing_config_t));
    }
    else
    {
        LOG_INFO("No found Improved Timing Configuration. Just used default configuration\r\n\r\n");
    }
#endif
#endif

#if (defined(USE_CANFD) && USE_CANFD)
    FLEXCAN_FDInit(EXAMPLE_CAN, &flexcanConfig, EXAMPLE_CAN_CLK_FREQ, BYTES_IN_MB, true);
#else
    FLEXCAN_Init(EXAMPLE_CAN, &flexcanConfig, EXAMPLE_CAN_CLK_FREQ);
#endif

    /* Setup Rx Message Buffer. */
    mbConfig.format = kFLEXCAN_FrameFormatStandard;
    mbConfig.type   = kFLEXCAN_FrameTypeData;
    mbConfig.id     = FLEXCAN_ID_STD(0x000);
#if (defined(USE_CANFD) && USE_CANFD)
    FLEXCAN_SetFDRxMbConfig(EXAMPLE_CAN, RX_MESSAGE_BUFFER_NUM, &mbConfig, true);
#else
    FLEXCAN_SetRxMbConfig(EXAMPLE_CAN, RX_MESSAGE_BUFFER_NUM, &mbConfig, true);
    FLEXCAN_SetRxIndividualMask(EXAMPLE_CAN, RX_MESSAGE_BUFFER_NUM, 0x7FF);
#endif

/* Setup Tx Message Buffer. */
#if (defined(USE_CANFD) && USE_CANFD)
    FLEXCAN_SetFDTxMbConfig(EXAMPLE_CAN, TX_MESSAGE_BUFFER_NUM, true);
#else
    FLEXCAN_SetTxMbConfig(EXAMPLE_CAN, TX_MESSAGE_BUFFER_NUM, true);
#endif

    /* Enable Rx Message Buffer interrupt. */
    FLEXCAN_EnableMbInterrupts(EXAMPLE_CAN, flag << RX_MESSAGE_BUFFER_NUM);
    (void)EnableIRQ(EXAMPLE_FLEXCAN_IRQn);

    //uint32_t id = 0x123;
    uint32_t id;

    /* Inicializa el generador de números aleatorios */
    srand(time(NULL));

    // Create a queue capable of containing 10 uint32_t values.
    xQueue1 = xQueueCreate( 10, sizeof( flexcan_frame_t ) );
    if( xQueue1 == 0 )
    {
    	LOG_INFO("Fail to create queue");
    }

    while (true) {

    /* Genera un número aleatorio entre 0 y 0xFFF (2047 en decimal) y lo asigna al ID */
    id = rand() % 0x7FF;

    /* Prepare Tx Frame for sending. */
    txFrame.format = (uint8_t)kFLEXCAN_FrameFormatStandard;
    txFrame.type   = (uint8_t)kFLEXCAN_FrameTypeData;
    txFrame.id     = FLEXCAN_ID_STD(id);
    txFrame.length = (uint8_t)DLC;
#if (defined(USE_CANFD) && USE_CANFD)
    txFrame.brs = 1U;
#endif
#if (defined(USE_CANFD) && USE_CANFD)
    uint8_t i = 0;
    for (i = 0; i < DWORD_IN_MB; i++)
    {
        txFrame.dataWord[i] = i;
    }
#else
    txFrame.dataWord0 = CAN_WORD0_DATA_BYTE_0(0x11) | CAN_WORD0_DATA_BYTE_1(0x22) | CAN_WORD0_DATA_BYTE_2(0x33) |
                        CAN_WORD0_DATA_BYTE_3(0x44);
    txFrame.dataWord1 = CAN_WORD1_DATA_BYTE_4(0x55) | CAN_WORD1_DATA_BYTE_5(0x66) | CAN_WORD1_DATA_BYTE_6(0x77) |
                        CAN_WORD1_DATA_BYTE_7(0x88);
#endif

    LOG_INFO("Send message with ID 0x%x from MB%d to MB%d\r\n", txFrame.id, TX_MESSAGE_BUFFER_NUM, RX_MESSAGE_BUFFER_NUM);
#if (defined(USE_CANFD) && USE_CANFD)
    for (i = 0; i < DWORD_IN_MB; i++)
    {
        LOG_INFO("tx word%d = 0x%x\r\n", i, txFrame.dataWord[i]);
    }
#else
    LOG_INFO("tx word0 = 0x%x\r\n", txFrame.dataWord0);
    LOG_INFO("tx word1 = 0x%x\r\n", txFrame.dataWord1);
#endif

/* Send data through Tx Message Buffer using polling function. */
#if (defined(USE_CANFD) && USE_CANFD)
    (void)FLEXCAN_TransferFDSendBlocking(EXAMPLE_CAN, TX_MESSAGE_BUFFER_NUM, &txFrame);
#else
    (void)FLEXCAN_TransferSendBlocking(EXAMPLE_CAN, TX_MESSAGE_BUFFER_NUM, &txFrame);
#endif

    /* Waiting for Message receive finish. */
    while (!rxComplete)
    {
    }

    LOG_INFO("\r\nReceived message from MB%d\r\n", RX_MESSAGE_BUFFER_NUM);
#if (defined(USE_CANFD) && USE_CANFD)
    for (i = 0; i < DWORD_IN_MB; i++)
    {
        LOG_INFO("rx word%d = 0x%x\r\n", i, rxFrame.dataWord[i]);
    }
#else
    LOG_INFO("rx word0 = 0x%x\r\n", rxFrame.dataWord0);
    LOG_INFO("rx word1 = 0x%x\r\n", rxFrame.dataWord1);
#endif


    if( xQueue1 != 0 )
    {
    	// Send an uint32_t.  Wait for 10 ticks for space to become
    	// available if necessary.
    	if( xQueueSend( xQueue1, ( void * ) &rxFrame, ( TickType_t ) 10 ) != pdPASS )
    	{
    		LOG_INFO("Fail to sent queue");
    	}
    }

    vTaskDelay(2000/portTICK_PERIOD_MS);

    }

    /* Stop FlexCAN Send & Receive. */
   // FLEXCAN_DisableMbInterrupts(EXAMPLE_CAN, flag << RX_MESSAGE_BUFFER_NUM);

    //LOG_INFO("\r\n==FlexCAN loopback functional example -- Finish.==\r\n");


  //vTaskDelete(NULL);

}

void EXAMPLE_FLEXCAN_IRQHandler(void)
{
#if (defined(FSL_FEATURE_FLEXCAN_HAS_EXTENDED_FLAG_REGISTER)) && (FSL_FEATURE_FLEXCAN_HAS_EXTENDED_FLAG_REGISTER > 0)
    uint64_t flag = 1U;
#else
    uint32_t flag = 1U;
#endif
    /* If new data arrived. */
    if (0U != FLEXCAN_GetMbStatusFlags(EXAMPLE_CAN, flag << RX_MESSAGE_BUFFER_NUM))
    {
        FLEXCAN_ClearMbStatusFlags(EXAMPLE_CAN, flag << RX_MESSAGE_BUFFER_NUM);
#if (defined(USE_CANFD) && USE_CANFD)
        (void)FLEXCAN_ReadFDRxMb(EXAMPLE_CAN, RX_MESSAGE_BUFFER_NUM, &rxFrame);
#else
        (void)FLEXCAN_ReadRxMb(EXAMPLE_CAN, RX_MESSAGE_BUFFER_NUM, &rxFrame);
#endif
        rxComplete = true;
    }
    SDK_ISR_EXIT_BARRIER;
}
/*-----------------------------------------------------------------------------------*/
void
udpecho_init(void)
{
 sys_thread_new("udpecho_thread", udpecho_thread, NULL, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
  sys_thread_new("CAN_thread", CAN_thread, NULL, 1024, DEFAULT_THREAD_PRIO+1);
}



#endif /* LWIP_NETCONN */
