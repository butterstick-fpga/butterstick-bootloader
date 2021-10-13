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
#include <generated/luna_usb.h>

#include <irq.h>
#include <uart.h>

#include <sleep.h>
#include <flash.h>

#include "tusb.h"


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
const uint32_t alt_offsets[] = {
	0x400000,
	0x800000,
};

static int complete_timeout;

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

	msleep(250);

	timer_init();
	tusb_init();

	while (1)
	{
		tud_task(); // tinyusb device task
		led_blinking_task();

		if(complete_timeout){
			static uint32_t start_ms = 0;

			// timeout in ms
			if (board_millis() != start_ms)
			{
				start_ms = board_millis();

				complete_timeout--;
				if(complete_timeout == 0)
					break;
			}
				
		}
	}


	/* Reboot to our user bitstream */
	irq_setie(0);
	usb_device_controller_connect_write(0);
	msleep(200);	

	while(1){
		reset_out_write(1);
	}

	return 0;
}

const uint8_t values[] = {127, 152, 176, 198, 218, 233, 245, 253, 255, 253, 245, 233, 218, 198, 176, 152, 127, 102, 78, 56, 37, 21, 9, 2, 0, 2, 9, 21, 37, 56, 78, 102};
const uint16_t _gamma[] = {0, 0, 0, 1, 2, 2, 3, 4, 5, 6, 7, 9, 10, 11, 13, 14, 16, 17, 19, 20, 22, 24, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49, 52, 54, 56, 58, 61, 63, 65, 68, 70, 73, 75, 78, 80, 83, 86, 88, 91, 94, 96, 99, 102, 105, 108, 110, 113, 116, 119, 122, 125, 128, 131, 134, 137, 140, 143, 147, 150, 153, 156, 159, 163, 166, 169, 173, 176, 179, 183, 186, 189, 193, 196, 200, 203, 207, 210, 214, 218, 221, 225, 228, 232, 236, 240, 243, 247, 251, 255, 258, 262, 266, 270, 274, 278, 281, 285, 289, 293, 297, 301, 305, 309, 313, 317, 322, 326, 330, 334, 338, 342, 346, 351, 355, 359, 363, 368, 372, 376, 381, 385, 389, 394, 398, 402, 407, 411, 416, 420, 425, 429, 434, 438, 443, 447, 452, 456, 461, 466, 470, 475, 480, 484, 489, 494, 498, 503, 508, 513, 518, 522, 527, 532, 537, 542, 547, 551, 556, 561, 566, 571, 576, 581, 586, 591, 596, 601, 606, 611, 616, 621, 627, 632, 637, 642, 647, 652, 657, 663, 668, 673, 678, 684, 689, 694, 699, 705, 710, 715, 721, 726, 731, 737, 742, 748, 753, 759, 764, 769, 775, 780, 786, 791, 797, 803, 808, 814, 819, 825, 830, 836, 842, 847, 853, 859, 864, 870, 876, 882, 887, 893, 899, 905, 910, 916, 922, 928, 934, 939, 945, 951, 957, 963, 969, 975, 981, 987, 993, 999, 1005, 1010, 1016, 1023};
void led_blinking_task(void)
{
	static uint32_t start_ms = 0;
	static bool led_state = false;

	// Blink every interval ms
	if (board_millis() - start_ms < 10)
		return; // not enough time
	start_ms += 10;


	int count = board_millis()/ 20;

	volatile uint32_t *p = (uint32_t*)CSR_LEDS_OUT0_ADDR;

	for(int i = 0; i < 7; i++){
		p[i] = _gamma[values[(count + i*4) % 32]] << 20;
		p[i] |= _gamma[values[(count + i*4) % 32] / 2] << 10;		
	}
	
	//leds_out_write(led_state);
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
    return 1; /* Request we are polled in 1ms */
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

  	uint32_t flash_address = alt_offsets[alt] + block_num * CFG_TUD_DFU_XFER_BUFSIZE;

	if((flash_address & (FLASH_64K_BLOCK_ERASE_SIZE-1)) == 0){
		//printf("Erasing. flash_address=%08x\n", flash_address);

		/* First block in 64K erase block */
		spiflash_write_enable();
		spiflash_sector_erase(flash_address);

		/* While FLASH erase is in progress update LEDs */
		while(spiflash_read_status_register() & 1){ led_blinking_task(); };
	}
  
	//printf("tud_dfu_download_cb(), alt=%u, block=%u, flash_address=%08x\n", alt, block_num, flash_address);

	for(int i = 0; i < CFG_TUD_DFU_XFER_BUFSIZE / 256; i++){

		spiflash_write_enable();
		spiflash_page_program(flash_address, data, 256);
		flash_address += 256;
		data += 256;


		/* While FLASH erase is in progress update LEDs */
		while(spiflash_read_status_register() & 1){ led_blinking_task(); };
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

  //uint16_t const xfer_len = (uint16_t) strlen(upload_image[alt]);
  //memcpy(data, upload_image[alt], xfer_len);

  return 0;
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
	complete_timeout = 10;
}