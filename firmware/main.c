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


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
const char* upload_image[2]=
{
  "Hello world from TinyUSB DFU! - Partition 0",
  "Hello world from TinyUSB DFU! - Partition 1"
};


/* Blink pattern
 * - 1000 ms : device should reboot
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum
{
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

	// Dispatch USB events.
	if (irqs & (1 << USB_DEVICE_CONTROLLER_INTERRUPT | 1 << USB_IN_EP_INTERRUPT | 1 << USB_OUT_EP_INTERRUPT | 1 << USB_SETUP_INTERRUPT))
	{
		tud_int_handler(0);
	}

	// Dispatch timer events.
	if (irqs & (1 << TIMER0_INTERRUPT))
	{
		timer0_ev_pending_write(timer0_ev_pending_read());
		timer_isr();
	}

	// Dispatch UART events.
	if (irqs & (1 << UART_INTERRUPT))
	{
		uart_isr();
	}
}

int main(int i, char **c)
{

	/* Setup IRQ, needed for UART */
	irq_setmask(0);
	irq_setie(1);
	uart_init();

	msleep(25);

	timer_init();

	printf("\n");

	printf("butterstick-fpga-dfu\n");
	printf("build date: "__DATE__
		   " "__TIME__
		   "\n");
	printf("git hash: "__GIT_SHA1__
		   "\n");

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
	if (board_millis() - start_ms < blink_interval_ms)
		return; // not enough time
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
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// DFU callbacks
// Note: alt is used as the partition number, in order to support multiple partitions like FLASH, EEPROM, etc.
//--------------------------------------------------------------------+

// Invoked right before tud_dfu_download_cb() (state=DFU_DNBUSY) or tud_dfu_manifest_cb() (state=DFU_MANIFEST)
// Application return timeout in milliseconds (bwPollTimeout) for the next download/manifest operation.
// During this period, USB host won't try to communicate with us.
uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state)
{
  if ( state == DFU_DNBUSY )
  {
    // For this example
    // - Atl0 Flash is fast : 1   ms
    // - Alt1 EEPROM is slow: 100 ms
    return (alt == 0) ? 1 : 100;
  }
  else if (state == DFU_MANIFEST)
  {
    // since we don't buffer entire image and do any flashing in manifest stage
    return 0;
  }

  return 0;
}

// Invoked when received DFU_DNLOAD (wLength>0) following by DFU_GETSTATUS (state=DFU_DNBUSY) requests
// This callback could be returned before flashing op is complete (async).
// Once finished flashing, application must call tud_dfu_finish_flashing()
void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, uint8_t const* data, uint16_t length)
{
  (void) alt;
  (void) block_num;

  //printf("\r\nReceived Alt %u BlockNum %u of length %u\r\n", alt, wBlockNum, length);

  for(uint16_t i=0; i<length; i++)
  {
    //printf("%c", data[i]);
  }

  // flashing op for download complete without error
  tud_dfu_finish_flashing(DFU_STATUS_OK);
}

// Invoked when download process is complete, received DFU_DNLOAD (wLength=0) following by DFU_GETSTATUS (state=Manifest)
// Application can do checksum, or actual flashing if buffered entire image previously.
// Once finished flashing, application must call tud_dfu_finish_flashing()
void tud_dfu_manifest_cb(uint8_t alt)
{
  (void) alt;
  printf("Download completed, enter manifestation\r\n");

  // flashing op for manifest is complete without error
  // Application can perform checksum, should it fail, use appropriate status such as errVERIFY.
  tud_dfu_finish_flashing(DFU_STATUS_OK);
}

// Invoked when received DFU_UPLOAD request
// Application must populate data with up to length bytes and
// Return the number of written bytes
uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block_num, uint8_t* data, uint16_t length)
{
  (void) block_num;
  (void) length;

  uint16_t const xfer_len = (uint16_t) strlen(upload_image[alt]);
  memcpy(data, upload_image[alt], xfer_len);

  return xfer_len;
}

// Invoked when the Host has terminated a download or upload transfer
void tud_dfu_abort_cb(uint8_t alt)
{
  (void) alt;
  printf("Host aborted transfer\r\n");
}

// Invoked when a DFU_DETACH request is received
void tud_dfu_detach_cb(void)
{
  printf("Host detach, we should probably reboot\r\n");
}