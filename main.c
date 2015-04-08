#include <stdint.h>
#include <stdio.h>

#include "gpio.h"
#include "delay.h"
#include "uart.h"
#include "mspi.h"
#include "interrupt.h"

#define XBUFLEN 32
#define RBUFLEN 4

/* You might want to specify idata, pdata or idata for the buffers */
static unsigned char __pdata rbuf[RBUFLEN], xbuf[XBUFLEN];
static unsigned char rcnt, xcnt, rpos, xpos;
static __bit busy;

void ser_init(void)
{
   rcnt = xcnt = rpos = xpos = 0;  /* init buffers */
   busy = 0;
   
	// Setup UART pins
	gpio_pin_configure(GPIO_PIN_ID_FUNC_RXD,
			GPIO_PIN_CONFIG_OPTION_DIR_INPUT |
			GPIO_PIN_CONFIG_OPTION_PIN_MODE_INPUT_BUFFER_ON_NO_RESISTORS);

	gpio_pin_configure(GPIO_PIN_ID_FUNC_TXD,
			GPIO_PIN_CONFIG_OPTION_DIR_OUTPUT |
			GPIO_PIN_CONFIG_OPTION_OUTPUT_VAL_SET |
			GPIO_PIN_CONFIG_OPTION_PIN_MODE_OUTPUT_BUFFER_NORMAL_DRIVE_STRENGTH);

	uart_configure_8_n_1_38400();
	uart_rx_disable();
	interrupt_control_uart_enable();

}

interrupt_isr_uart()
{
   if (S0CON_SB_RI0) {
           S0CON_SB_RI0 = 0;
           /* don't overwrite chars already in buffer */
           if (rcnt < RBUFLEN)
                   rbuf [(unsigned char)(rpos+rcnt++) % RBUFLEN] = S0BUF;
   }
   if (S0CON_SB_TI0) {
           S0CON_SB_TI0 = 0;
           if (busy = xcnt) {   /* Assignment, _not_ comparison! */
                   xcnt--;
                   S0BUF = xbuf [xpos++];
                   if (xpos >= XBUFLEN)
                           xpos = 0;
           }
   }
}

void
putchar (char c)
{
   while (xcnt >= XBUFLEN) /* wait for room in buffer */
           ;
   interrupt_control_uart_disable();
   if (busy) {
           xbuf[(unsigned char)(xpos+xcnt++) % XBUFLEN] = c;
   } else {
           S0BUF = c;
           busy = 1;
   }
   interrupt_control_uart_enable();
}

char
getchar (void)
{
   unsigned char c;
   while (!rcnt)   /* wait for character */
           ;
   interrupt_control_uart_disable();
   rcnt--;
   c = rbuf [rpos++];
   if (rpos >= RBUFLEN)
           rpos = 0;
   interrupt_control_uart_enable();
   return (c);
}

void putstr(const char *s)
{
	for (; *s; s++)
		putchar(*s);
}

void putuint16(uint16_t val)
{
	char pbuf[8];
	uint8_t num_digits = 0;

	pbuf[0] = '0';
	for (; val > 0 && num_digits < sizeof(pbuf); val = val/10, num_digits++) {
		pbuf[num_digits] = '0' + (val % 10);
	}

	if (num_digits > 0)
		num_digits--;

	for (; num_digits > 0; num_digits--) {
		putchar(pbuf[num_digits]);
	}
	putchar(pbuf[0]);
}

void putfixed(uint16_t val)
{
	putuint16(val >> 2);
	putchar('.');
	putuint16( (val & 0x3) * 25 );
}

uint16_t max6675_sample(void)
{
	uint16_t data = 0;
	uint8_t cnt;

	//make sure the RX FIFO is clear before sending the byte
	while(mspi_is_rx_data_ready())
	{
		mspi_get();
	}

	// CS low to get data
	gpio_pin_val_sbit_clear(P1_SB_D3);
	delay_us(15);
	mspi_enable();
	mspi_send(0);
	mspi_send(0);

	for (cnt = 0; cnt < 2; cnt++) {
		while (!mspi_is_rx_data_ready());
		data = (data<<8) | mspi_get();
	}

	delay_us(1);
	gpio_pin_val_sbit_set(P1_SB_D3);

	return data;
}

uint16_t max6675_value(uint16_t data)
{
	return data >> 3;
}

bool max6675_input_error(uint16_t data)
{
	return (data & 2);
}

void main()
{
	ser_init();

	putstr("Starting up\r\n");

	delay_s(1);

	// Setup pin for Chip Select, default HIGH
	gpio_pin_configure(GPIO_PIN_ID_P1_3,
			GPIO_PIN_CONFIG_OPTION_DIR_OUTPUT |
			GPIO_PIN_CONFIG_OPTION_OUTPUT_VAL_SET |
			GPIO_PIN_CONFIG_OPTION_PIN_MODE_OUTPUT_BUFFER_NORMAL_DRIVE_STRENGTH);
	
	mspi_configure(MSPI_CONFIG_OPTION_MCLK_IS_CCLK_DIV_4 |
			MSPI_CONFIG_OPTION_TX_FIFO_READY_INT_DISABLE | MSPI_CONFIG_OPTION_TX_FIFO_EMPTY_INT_DISABLE |
			MSPI_CONFIG_OPTION_RX_FIFO_FULL_INT_DISABLE | MSPI_CONFIG_OPTION_RX_DATA_READY_INT_DISABLE);

	interrupt_control_global_enable();

	while(1)
	{
		uint16_t data = max6675_sample();
		putuint16(data);
		putchar(' ');
		putuint16(max6675_value(data));
		putchar(' ');
		putfixed(max6675_value(data));
		putchar(' ');
		putuint16(max6675_input_error(data));
		putstr("\r\n");

		delay_s(1);
	}
}
