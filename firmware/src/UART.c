/********************************** (C) COPYRIGHT *******************************
* File Name          : UART.C
* Author             : WCH
* Version            : V1.01
* Date               : 2023/04/06
* Description        : uart serial port related initialization and processing
*******************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#include "UART.h"

/*******************************************************************************/
/* Variable Definition */
/* Global */

/* The following are serial port transmit and receive related variables and buffers */
volatile UART_CTL Uart;

__attribute__ ((aligned(4))) uint8_t  UART2_Tx_Buf[ DEF_UARTx_TX_BUF_LEN ];  /* Serial port 2 transmit data buffer */
__attribute__ ((aligned(4))) uint8_t  UART2_Rx_Buf[ DEF_UARTx_RX_BUF_LEN ];  /* Serial port 2 receive data buffer */
volatile uint32_t UARTx_Rx_DMACurCount;                       /* Serial port 1 receive dma current counter */
volatile uint32_t UARTx_Rx_DMALastCount;                      /* Serial port 1 receive dma last value counter  */

/*********************************************************************
 * @fn      RCC_Configuration
 *
 * @brief   Configures the different system clocks.
 */
uint8_t RCC_Configuration( void )
{
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA, ENABLE );
    RCC_APB1PeriphClockCmd( RCC_APB1Periph_USART2, ENABLE );
    RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM3, ENABLE );
    RCC_AHBPeriphClockCmd( RCC_AHBPeriph_DMA1, ENABLE );
    return 0;
}

/*********************************************************************
 * @fn      TIM3_Init
 *
 * @brief   100us Timer
 */
void TIM3_Init( void )
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure = {0};
    TIM_DeInit( TIM3 );
    TIM_TimeBaseStructure.TIM_Period = 100 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = SystemCoreClock / 1000000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit( TIM3, &TIM_TimeBaseStructure );
    TIM_ClearFlag( TIM3, TIM_FLAG_Update );
    TIM_ITConfig( TIM3, TIM_IT_Update, ENABLE );
    NVIC_EnableIRQ( TIM3_IRQn );
    TIM_Cmd( TIM3, ENABLE );
}

/*********************************************************************
 * @fn      UART_CfgInit
 *
 * @brief   Uart configuration initialization
 */
void UART_CfgInit( USART_TypeDef *USARTx, uint32_t baudrate, uint8_t stopbits, uint8_t parity, uint8_t half_duplex )
{
    USART_InitTypeDef USART_InitStructure = {0};
    GPIO_InitTypeDef  GPIO_InitStructure = {0};

    if(USARTx == USART2) {
        RCC_APB1PeriphClockCmd( RCC_APB1Periph_USART2, ENABLE );
        RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE );
        
        // Remap USART2 to PA2/PA3 (Default: 000)
        AFIO->PCFR1 &= ~AFIO_PCFR1_USART2_REMAP;

        // Configure PA2 as AF_PP (TX)
        GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP; 
        GPIO_Init( GPIOA, &GPIO_InitStructure );

        // Force PA3 to AIN to disconnect logical RX from PWM noise
        // (USART2 still receives from PA2 in 1-Wire HDSEL mode)
        GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_3;
        GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AIN;
        GPIO_Init( GPIOA, &GPIO_InitStructure );
        
        // Enable Half-Duplex mode
        USARTx->CTLR3 |= USART_CTLR3_HDSEL; 
    } 
    else if(USARTx == USART4) {
        RCC_APB1PeriphClockCmd( RCC_APB1Periph_USART4, ENABLE );
        RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE );
        
        // Remap USART4 TX to PA5 (Partial Remap 1: 001)
        AFIO->PCFR1 = (AFIO->PCFR1 & ~AFIO_PCFR1_USART4_REMAP) | AFIO_PCFR1_USART4_REMAP_0;

        GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
        GPIO_Init( GPIOA, &GPIO_InitStructure );
        
        // Enable Half-Duplex mode
        USARTx->CTLR3 |= USART_CTLR3_HDSEL;
    }

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = (parity == 0) ? USART_WordLength_8b : USART_WordLength_9b;
    
    if( stopbits == 1 ) USART_InitStructure.USART_StopBits = USART_StopBits_1_5;
    else if( stopbits == 2 ) USART_InitStructure.USART_StopBits = USART_StopBits_2;
    else USART_InitStructure.USART_StopBits = USART_StopBits_1;

    if( parity == 1 ) USART_InitStructure.USART_Parity = USART_Parity_Odd;
    else if( parity == 2 ) USART_InitStructure.USART_Parity = USART_Parity_Even;
    else USART_InitStructure.USART_Parity = USART_Parity_No;

    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init( USARTx, &USART_InitStructure );
    USART_Cmd( USARTx, ENABLE );
}

void UART2_ParaInit( uint8_t mode )
{
    uint8_t i;
    Uart.Rx_LoadPtr = 0x00;
    Uart.Rx_DealPtr = 0x00;
    Uart.Rx_RemainLen = 0x00;
    Uart.Rx_TimeOut = 0x00;
    Uart.Rx_TimeOutMax = 30;
    Uart.Tx_LoadNum = 0x00;
    Uart.Tx_DealNum = 0x00;
    Uart.Tx_RemainNum = 0x00;
    for( i = 0; i < DEF_UARTx_TX_BUF_NUM_MAX; i++ ) Uart.Tx_PackLen[ i ] = 0x00;
    Uart.Tx_Flag = 0x00;
    Uart.Tx_CurPackLen = 0x00;
    Uart.Tx_CurPackPtr = 0x00;
    Uart.USB_Up_IngFlag = 0x00;
    Uart.USB_Up_TimeOut = 0x00;
    Uart.USB_Up_Pack0_Flag = 0x00;
    Uart.USB_Down_StopFlag = 0x00;
    UARTx_Rx_DMACurCount = 0x00;
    UARTx_Rx_DMALastCount = 0x00;
    if( mode )
    {
        Uart.Com_Cfg[ 0 ] = (uint8_t)( DEF_UARTx_BAUDRATE );
        Uart.Com_Cfg[ 1 ] = (uint8_t)( DEF_UARTx_BAUDRATE >> 8 );
        Uart.Com_Cfg[ 2 ] = (uint8_t)( DEF_UARTx_BAUDRATE >> 16 );
        Uart.Com_Cfg[ 3 ] = (uint8_t)( DEF_UARTx_BAUDRATE >> 24 );
        Uart.Com_Cfg[ 4 ] = DEF_UARTx_STOPBIT;
        Uart.Com_Cfg[ 5 ] = DEF_UARTx_PARITY;
        Uart.Com_Cfg[ 6 ] = DEF_UARTx_DATABIT;
        Uart.Com_Cfg[ 7 ] = DEF_UARTx_RX_TIMEOUT;
    }
}

void UART2_DMAInit( uint8_t type, uint8_t *pbuf, uint32_t len )
{
    DMA_InitTypeDef DMA_InitStructure = {0};
    if( type == 0x00 )
    {
        DMA_DeInit( DMA1_Channel7 );
        DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&USART2->DATAR);
        DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)pbuf;
        DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
        DMA_InitStructure.DMA_BufferSize = len;
        DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
        DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
        DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
        DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
        DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
        DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
        DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
        DMA_Init( DMA1_Channel7, &DMA_InitStructure );
        DMA_Cmd( DMA1_Channel7, ENABLE );
    }
    else
    {
        DMA_DeInit( DMA1_Channel6 );
        DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&USART2->DATAR);
        DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)pbuf;
        DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
        DMA_InitStructure.DMA_BufferSize = len;
        DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
        DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
        DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
        DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
        DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
        DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
        DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
        DMA_Init( DMA1_Channel6, &DMA_InitStructure );
        DMA_Cmd( DMA1_Channel6, ENABLE );
    }
}

void UART2_Init( uint8_t mode, uint32_t baudrate, uint8_t stopbits, uint8_t parity )
{
    // Disable DMA for RX to allow CPU polling in main loop
    USART_DMACmd( USART2, USART_DMAReq_Rx, DISABLE );
    DMA_Cmd( DMA1_Channel6, DISABLE );
    DMA_Cmd( DMA1_Channel7, DISABLE );
    
    // Initialize USART2 in 1-Wire mode
    UART_CfgInit( USART2, baudrate, stopbits, parity, 1 );
    
    // Para init only, no DMA RX enablement
    UART2_ParaInit( mode );
}

void UART4_Init( uint32_t baudrate )
{
    // Initialize USART4 in 1-Wire mode
    UART_CfgInit( USART4, baudrate, 0, 0, 1 );
}

void UART_System_Init( UART_Mode_t mode, uint32_t baudrate )
{
    // Both UARTs are initialized for Ring Bus (1-Wire)
    // Debug mode can be handled by choosing which port to connect to.
    UART2_Init(1, baudrate, 0, 0); 
    UART4_Init(baudrate);
}

void UART2_USB_Init( void )
{
    uint32_t baudrate;
    uint8_t  stopbits, parity;
    baudrate = ( uint32_t )( Uart.Com_Cfg[ 3 ] << 24 ) + ( uint32_t )( Uart.Com_Cfg[ 2 ] << 16 );
    baudrate += ( uint32_t )( Uart.Com_Cfg[ 1 ] << 8 ) + ( uint32_t )( Uart.Com_Cfg[ 0 ] );
    stopbits = Uart.Com_Cfg[ 4 ];
    parity = Uart.Com_Cfg[ 5 ];
    UART2_Init( 0, baudrate, stopbits, parity ); 
    USBFSD->UEP2_DMA = (uint32_t)(uint8_t *)&UART2_Tx_Buf[ 0 ];
    USBFSD->UEP2_CTRL_H &= ~USBFS_UEP_R_RES_MASK;
    USBFSD->UEP2_CTRL_H |= USBFS_UEP_R_RES_ACK;
}

void UART2_DataTx_Deal( void )
{
    uint16_t  count;
    if( Uart.Tx_Flag )
    {
        if( USART2->STATR & USART_FLAG_TC )
        {
            USART2->STATR = (uint16_t)( ~USART_FLAG_TC );
            USART2->CTLR3 &= ( ~USART_DMAReq_Tx );
            Uart.Tx_Flag = 0x00;
            NVIC_DisableIRQ( USBFS_IRQn );
            count = Uart.Tx_CurPackLen - DEF_UART2_TX_DMA_CH->CNTR;
            Uart.Tx_CurPackLen -= count;
            Uart.Tx_CurPackPtr += count;
            if( Uart.Tx_CurPackLen == 0x00 )
            {
                Uart.Tx_PackLen[ Uart.Tx_DealNum ] = 0x0000;
                Uart.Tx_DealNum++;
                if( Uart.Tx_DealNum >= DEF_UARTx_TX_BUF_NUM_MAX ) Uart.Tx_DealNum = 0x00;
                Uart.Tx_RemainNum--;
            }
            if( ( Uart.USB_Down_StopFlag == 0x01 ) && ( Uart.Tx_RemainNum < 2 ) )
            {
                USBFSD->UEP2_CTRL_H &= ~USBFS_UEP_R_RES_MASK;
                USBFSD->UEP2_CTRL_H |= USBFS_UEP_R_RES_ACK;
                Uart.USB_Down_StopFlag = 0x00;
            }
            NVIC_EnableIRQ( USBFS_IRQn );
        }
    }
    else
    {
        if( Uart.Tx_RemainNum )
        {
            if( Uart.Tx_CurPackLen == 0x00 )
            {
                Uart.Tx_CurPackLen = Uart.Tx_PackLen[ Uart.Tx_DealNum ];
                Uart.Tx_CurPackPtr = ( Uart.Tx_DealNum * DEF_USB_FS_PACK_LEN );
            }
            USART_ClearFlag( USART2, USART_FLAG_TC );
            DMA_Cmd( DEF_UART2_TX_DMA_CH, DISABLE );
            DEF_UART2_TX_DMA_CH->MADDR = (uint32_t)&UART2_Tx_Buf[ Uart.Tx_CurPackPtr ];
            DEF_UART2_TX_DMA_CH->CNTR = Uart.Tx_CurPackLen;
            DMA_Cmd( DEF_UART2_TX_DMA_CH, ENABLE );
            USART2->CTLR3 |= USART_DMAReq_Tx;
            Uart.Tx_Flag = 0x01;
        }
    }
}

void UART2_DataRx_Deal( void )
{
    uint16_t temp16;
    uint32_t remain_len;
    uint16_t packlen;
    NVIC_DisableIRQ( USBFS_IRQn );
    UARTx_Rx_DMACurCount = DEF_UART2_RX_DMA_CH->CNTR;
    if( UARTx_Rx_DMALastCount != UARTx_Rx_DMACurCount )
    {
        if( UARTx_Rx_DMALastCount > UARTx_Rx_DMACurCount ) temp16 = UARTx_Rx_DMALastCount - UARTx_Rx_DMACurCount;
        else {
            temp16 = DEF_UARTx_RX_BUF_LEN - UARTx_Rx_DMACurCount;
            temp16 += UARTx_Rx_DMALastCount;
        }
        UARTx_Rx_DMALastCount = UARTx_Rx_DMACurCount;
        if( ( Uart.Rx_RemainLen + temp16 ) > DEF_UARTx_RX_BUF_LEN ) printf("U0_O:%08lx\n",(uint32_t)Uart.Rx_RemainLen);
        else Uart.Rx_RemainLen += temp16;
        Uart.Rx_TimeOut = 0x00;
    }
    NVIC_EnableIRQ( USBFS_IRQn );
    if( Uart.Rx_RemainLen )
    {
        if( Uart.USB_Up_IngFlag == 0 )
        {
            remain_len = Uart.Rx_RemainLen;
            packlen = 0x00;
            if( remain_len >= DEF_USBD_FS_PACK_SIZE ) packlen = DEF_USBD_FS_PACK_SIZE;
            else if( Uart.Rx_TimeOut >= Uart.Rx_TimeOutMax ) packlen = remain_len;
            if( packlen > ( DEF_UARTx_RX_BUF_LEN - Uart.Rx_DealPtr ) ) packlen = ( DEF_UARTx_RX_BUF_LEN - Uart.Rx_DealPtr );
            if( packlen )
            {
                NVIC_DisableIRQ( USBFS_IRQn );
                Uart.USB_Up_IngFlag = 0x01;
                Uart.USB_Up_TimeOut = 0x00;
                USBFS_Endp_DataUp( DEF_UEP3, &UART2_Rx_Buf[ Uart.Rx_DealPtr ], packlen, DEF_UEP_CPY_LOAD );
                Uart.Rx_RemainLen -= packlen;
                Uart.Rx_DealPtr += packlen;
                if( Uart.Rx_DealPtr >= DEF_UARTx_RX_BUF_LEN ) Uart.Rx_DealPtr = 0x00;
                if( packlen == DEF_USBD_FS_PACK_SIZE ) Uart.USB_Up_Pack0_Flag = 0x01;
                NVIC_EnableIRQ( USBFS_IRQn );
            }
        }
        else if( Uart.USB_Up_TimeOut >= DEF_UARTx_USB_UP_TIMEOUT )
        {
            Uart.USB_Up_IngFlag = 0x00;
            USBFS_Endp_Busy[ DEF_UEP3 ] = 0;
        }
    }
    if( Uart.USB_Up_Pack0_Flag && Uart.USB_Up_IngFlag == 0 )
    {
        if( Uart.USB_Up_TimeOut >= ( DEF_UARTx_RX_TIMEOUT * 20 ) )
        {
            NVIC_DisableIRQ( USBFS_IRQn );
            Uart.USB_Up_IngFlag = 0x01;
            Uart.USB_Up_TimeOut = 0x00;
            USBFS_Endp_DataUp( DEF_UEP3, &UART2_Rx_Buf[ Uart.Rx_DealPtr ], 0, DEF_UEP_CPY_LOAD );
            Uart.USB_Up_IngFlag = 0;
            Uart.USB_Up_Pack0_Flag = 0x00;
            NVIC_EnableIRQ( USBFS_IRQn );
        }
    }
}
