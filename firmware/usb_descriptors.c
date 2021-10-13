/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"
#include "class/dfu/dfu_device.h"
#include "flash.h"

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

  #if CFG_TUD_CDC
    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  #else
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
  #endif

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0x1209,
    .idProduct          = 0x5af0,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

// Number of Alternate Interface (each for 1 flash partition)
#define ALT_COUNT   2

enum
{
  ITF_NUM_DFU_MODE,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_DFU_DESC_LEN(ALT_COUNT))

#define FUNC_ATTRS (DFU_ATTR_WILL_DETACH | DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_MANIFESTATION_TOLERANT)

uint8_t const desc_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // Interface number, Alternate count, starting string index, attributes, detach timeout, transfer size
  TUD_DFU_DESCRIPTOR(ITF_NUM_DFU_MODE, ALT_COUNT, 4, FUNC_ATTRS, 1000, CFG_TUD_DFU_XFER_BUFSIZE),
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations
  return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const* string_desc_arr [] =
{
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "Good Stuff Department",       // 1: Manufacturer
  "butterstick-fpga (dfu)",      // 2: Product
  "",                            // 3: Serial, derived from FLASH UUID
  "main-gateware @0x100000",     // 4: DFU alt0 name
  "main-firmware @0x400000",     // 5: DFU alt1 name
};

// Microsoft Compatible ID Feature Descriptor
#define MSFT_VENDOR_CODE    '~'     // Arbitrary, but should be printable ASCII
#define MSFT_WCID_LEN       40

// Microsoft OS String Descriptor. See: https://github.com/pbatard/libwdi/wiki/WCID-Devices
static const uint16_t usb_string_microsoft = {0x0318, 'M','S','F','T','1','0','0', MSFT_VENDOR_CODE};
 
// Microsoft WCID
const uint8_t usb_microsoft_wcid[MSFT_WCID_LEN] = {
    MSFT_WCID_LEN, 0, 0, 0,         // Length
    0x00, 0x01,                     // Version
    0x04, 0x00,                     // Compatibility ID descriptor index
    0x01,                           // Number of sections
    0, 0, 0, 0, 0, 0, 0,            // Reserved (7 bytes)

    0,                              // Interface number
    0x01,                           // Reserved
    'W','I','N','U','S','B',0,0,    // Compatible ID
    0,0,0,0,0,0,0,0,                // Sub-compatible ID (unused)
    0,0,0,0,0,0,                    // Reserved
};


//--------------------------------------------------------------------+
// WCID use vendor class
//--------------------------------------------------------------------+
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  // nothing to with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) return true;

  switch (request->bmRequestType_bit.type)
  {
    case TUSB_REQ_TYPE_VENDOR:
      switch (request->bRequest)
      {
        case MSFT_VENDOR_CODE:
          if ( request->wIndex == 0x0004 )
          {
            // Return a Microsoft Compatible ID Feature Descriptor
            return tud_control_xfer(rhport, request, (void*) usb_microsoft_wcid, MSFT_WCID_LEN);
          }
          break;
        default: break;
      }
    break;


    default: break;
  }

  // stall unknown request
  return false;
}

static uint16_t _desc_str[32];

char hex(uint8_t d){
  if(d <= 0x9)
    return d + '0';
  return d - 10 + 'a';
}

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  size_t chr_count;

  if ( index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }
  else if(index == 3){
    uint8_t uuid[8];

    spiflash_read_uuid(uuid);
    chr_count = 19;

    uint16_t* s = &_desc_str[1];
    // Convert ASCII string into UTF-16
    for(uint8_t i=0; i<8; i++)
    {
      /* Add dashes every 2 bytes */
      if(i && !(i & 1)){
        *s++ = '-';
      }

      *s++ = hex(uuid[i] >> 4);
      *s++ = hex(uuid[i] & 0xF);
    }
  }
  else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors
    if(index == 0xEE){
      return usb_string_microsoft;
    }

    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

    const char* str = string_desc_arr[index];

    // Cap at max char
    chr_count = strlen(str);
    if ( chr_count > 31 ) {
      chr_count = 31;
    }

    // Convert ASCII string into UTF-16
    for(uint8_t i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t)((((uint16_t)TUSB_DESC_STRING) << 8 ) | (2u*chr_count + 2u));

  return _desc_str;
}
