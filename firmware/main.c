/* This file is part of OrangeCrab-test
 *
 * Copyright 2020 Gregory Davill <greg.davill@gmail.com> 
 * Copyright 2020 Michael Welling <mwelling@ieee.org>
 */

#include <stdlib.h>
#include <stdio.h>

#include <generated/csr.h>
#include <generated/mem.h>
#include <generated/git.h>

#include <irq.h>
#include <uart.h>

#include <sleep.h>
#include <flash.h>

#include "tusb.h"

/* Blink pattern
 * - 1000 ms : device should reboot
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_DFU_MODE = 100,
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

// Current system tick timer.
volatile uint32_t system_ticks = 0;
#if CFG_TUSB_OS == OPT_OS_NONE
uint32_t board_millis(void)
{
	return system_ticks;
}
#endif

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

static void timer_init(void)
{
	// Set up our timer to generate an interrupt every millisecond.
	timer0_reload_write(60 * 1000);
	timer0_en_write(1);
	timer0_ev_enable_write(1);

	// Enable our timer's interrupt.
	//irq_setie(1);
	irq_setmask((1 << TIMER0_INTERRUPT) | irq_getmask());
}

static void timer_isr(void)
{
	// Increment our total millisecond count.
	++system_ticks;
}


void isr(void)
{
	unsigned int irqs;
	irqs = irq_pending() & irq_getmask();

	bool pending_usb_event = irqs & (1 << USB_DEVICE_CONTROLLER_INTERRUPT | 1 << USB_IN_EP_INTERRUPT | 1 << USB_OUT_EP_INTERRUPT | 1 << USB_SETUP_INTERRUPT);
		// usb_device_controller_interrupt_pending() ||
		// usb_setup_interrupt_pending()             ||
		// usb_in_ep_interrupt_pending()             ||
		// usb_out_ep_interrupt_pending();

	// // Dispatch USB events.
	if (pending_usb_event) {
	 	tud_int_handler(0);
	// 	// ... and call the core TinyUSB interrupt handler.
	}

	// Dispatch timer events.
	if (irqs & (1 << TIMER0_INTERRUPT)) {
		timer0_ev_pending_write(timer0_ev_pending_read());
		timer_isr();
	}
	
	if(irqs & (1 << UART_INTERRUPT)){
		uart_isr();
	}


}


void print_buffer(uint8_t* ptr, uint8_t len){
	for(int i = 0; i < len; i++){
		printf("%02x ", ptr[i]);
	}
}

void test_fail(const char* str){
	printf("%s\n", str);

	while(1);
}

int main(int i, char **c)
{

	/* Setup IRQ, needed for UART */
	irq_setmask(0);
	irq_setie(1);
	uart_init();

	msleep(50);

	timer_init();

	printf("\n");

	printf("butterstick-fpga-dfu\n");
	printf("build date: "__DATE__" "__TIME__ "\n");
	printf("git hash: "__GIT_SHA1__"\n");

	printf("err: %u\n", ctrl_bus_errors_read());

	tusb_init();

	while (1)
	{
		tud_task(); // tinyusb device task
		led_blinking_task();
	}

	return 0;
}

void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  
  leds_out_write(led_state);
  //board_led_write(led_state);

  led_state = 1 - led_state; // toggle
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  //blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  //blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  //blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  //blink_interval_ms = BLINK_MOUNTED;
}

// Invoked on DFU_DETACH request to reboot to the bootloader
void tud_dfu_runtime_reboot_to_dfu_cb(void)
{
  //blink_interval_ms = BLINK_DFU_MODE;
}

//--------------------------------------------------------------------+
// Class callbacks
//--------------------------------------------------------------------+
bool tud_dfu_firmware_valid_check_cb(void)
{
  printf("    Firmware check\r\n");
  return true;
}

void tud_dfu_req_dnload_data_cb(uint16_t wBlockNum, uint8_t* data, uint16_t length)
{
  (void) data;
  printf("    Received BlockNum %u of length %u\r\n", wBlockNum, length);

#if DFU_VERBOSE
  for(uint16_t i=0; i<length; i++)
  {
    printf("    [%u][%u]: %x\r\n", wBlockNum, i, (uint8_t)data[i]);
  }
#endif

  tud_dfu_dnload_complete();
}

bool tud_dfu_device_data_done_check_cb(void)
{
  printf("    Host said no more data... Returning true\r\n");
  return true;
}

void tud_dfu_abort_cb(void)
{
  printf("    Host aborted transfer\r\n");
}

#define UPLOAD_SIZE (29)
const uint8_t upload_test[UPLOAD_SIZE] = "Hello world from TinyUSB DFU!";

uint16_t tud_dfu_req_upload_data_cb(uint16_t block_num, uint8_t* data, uint16_t length)
{
  (void) block_num;
  (void) length;

  memcpy(data, upload_test, UPLOAD_SIZE);

  return UPLOAD_SIZE;
}