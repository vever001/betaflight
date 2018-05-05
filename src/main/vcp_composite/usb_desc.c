/**
 ******************************************************************************
 * @file    usb_desc.c
 * @author  MCD Application Team
 * @version V4.0.0
 * @date    21-January-2013
 * @brief   Descriptors for Virtual Com Port Demo
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
 *
 * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *        http://www.st.com/software_license_agreement_liberty_v2
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "usb_lib.h"
#include "usb_desc.h"

/* USB Standard Device Descriptor */
const uint8_t Composite_DeviceDescriptor[Composite_SIZ_DEVICE_DESC] = {
    0x12, /* bLength */
    USB_DEVICE_DESCRIPTOR_TYPE, /* bDescriptorType */
    0x00, 0x02, /* bcdUSB = 2.00 */
    0xEF, /* bDeviceClass: CDC */
    0x02, /* bDeviceSubClass */
    0x01, /* bDeviceProtocol */
    0x40, /* bMaxPacketSize0 */
    0x83, 0x04, /* idVendor  = 0x0483 */
    0x58, 0x32, /* idProduct = 0x5740 */
    0x00, 0x02, /* bcdDevice = 2.00 */
    1, /* Index of string descriptor describing manufacturer */
    2, /* Index of string descriptor describing product */
    3, /* Index of string descriptor describing the device's serial number */
    0x01 /* bNumConfigurations */
};

const uint8_t Composite_ConfigDescriptor[Composite_SIZ_CONFIG_DESC] = {
	0x09, /* bLength: Configuration Descriptor size */
	USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType: Configuration */
	Composite_SIZ_CONFIG_DESC,
	/* wTotalLength: Bytes returned */
	0x00,
	0x03,         /*bNumInterfaces: 2 interfaces (1 for CDC, 1 for HID)*/
	0x01,         /*bConfigurationValue: Configuration value*/
	0x00,         /*iConfiguration: Index of string descriptor describing
the configuration*/
	0xC0,         /*bmAttributes: bus powered and Support Remote Wake-up */
	0x32,         /*MaxPower 100 mA: this current is used for detecting Vbus*/

	/************** Descriptor of Joystick Mouse interface ****************/
	/* 09 */
	0x09,         /*bLength: Interface Descriptor size*/
	USB_INTERFACE_DESCRIPTOR_TYPE,/*bDescriptorType: Interface descriptor type*/
	HID_INTERFACE,         /*bInterfaceNumber: Number of Interface*/
	0x00,         /*bAlternateSetting: Alternate setting*/
	0x01,         /*bNumEndpoints*/
	0x03,         /*bInterfaceClass: HID*/
	0x00,         /*bInterfaceSubClass : 1=BOOT, 0=no boot*/
	0x00,         /*nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse*/
	0,            /*iInterface: Index of string descriptor*/
	/******************** Descriptor of Joystick Mouse HID ********************/
	/* 18 */
	0x09,         /*bLength: HID Descriptor size*/
	HID_DESCRIPTOR_TYPE, /*bDescriptorType: HID*/
	0x11,         /*bcdHID: HID Class Spec release number*/
	0x01,
	0x00,         /*bCountryCode: Hardware target country*/
	0x01,         /*bNumDescriptors: Number of HID class descriptors to follow*/
	0x22,         /*bDescriptorType*/
	CUSTOMHID_SIZ_REPORT_DESC, /*wItemLength: Total length of Report descriptor*/
	0x00,
	/******************** Descriptor of Mouse endpoint ********************/
	/* 27 */
	0x07,          /*bLength: Endpoint Descriptor size*/
	USB_ENDPOINT_DESCRIPTOR_TYPE, /*bDescriptorType:*/

	HID_IN_EP,     /*bEndpointAddress: Endpoint Address (IN)*/
	0x03,          /*bmAttributes: Interrupt endpoint*/
	HID_IN_PACKET, /*wMaxPacketSize: 8 Byte max */
	0x00,
	0x0A,          /*bInterval: Polling Interval (10 ms)*/
	/* 34 */

	/******** /IAD should be positioned just before the CDC interfaces ******
				IAD to associate the two CDC interfaces */

	0x08, /* bLength */
	0x0B, /* bDescriptorType */
	0x01, /* bFirstInterface */
	0x02, /* bInterfaceCount */
	0x02, /* bFunctionClass */
	0x02, /* bFunctionSubClass */
	0x01, /* bFunctionProtocol */
	0x00, /* iFunction (Index of string descriptor describing this function) */
	/*Interface Descriptor */
	0x09,   /* bLength: Interface Descriptor size */
	USB_INTERFACE_DESCRIPTOR_TYPE,  /* bDescriptorType: Interface */
	/* Interface descriptor type */
	CDC_COM_INTERFACE,   /* bInterfaceNumber: Number of Interface */
	0x00,   /* bAlternateSetting: Alternate setting */
	0x01,   /* bNumEndpoints: One endpoints used */
	0x02,   /* bInterfaceClass: Communication Interface Class */
	0x02,   /* bInterfaceSubClass: Abstract Control Model */
	0x01,   /* bInterfaceProtocol: Common AT commands */
	0x00,   /* iInterface: */

	/*Header Functional Descriptor*/
	0x05,   /* bLength: Endpoint Descriptor size */
	0x24,   /* bDescriptorType: CS_INTERFACE */
	0x00,   /* bDescriptorSubtype: Header Func Desc */
	0x10,   /* bcdCDC: spec release number */
	0x01,

	/*Call Management Functional Descriptor*/
	0x05,   /* bFunctionLength */
	0x24,   /* bDescriptorType: CS_INTERFACE */
	0x01,   /* bDescriptorSubtype: Call Management Func Desc */
	0x00,   /* bmCapabilities: D0+D1 */
	0x02,   /* bDataInterface: 2 */

	/*ACM Functional Descriptor*/
	0x04,   /* bFunctionLength */
	0x24,   /* bDescriptorType: CS_INTERFACE */
	0x02,   /* bDescriptorSubtype: Abstract Control Management desc */
	0x02,   /* bmCapabilities */

	/*Union Functional Descriptor*/
	0x05,   /* bFunctionLength */
	0x24,   /* bDescriptorType: CS_INTERFACE */
	0x06,   /* bDescriptorSubtype: Union func desc */
	0x01,   /* bMasterInterface: Communication class interface */
	0x02,   /* bSlaveInterface0: Data Class Interface */

	/*Endpoint 2 Descriptor*/
	0x07,                           /* bLength: Endpoint Descriptor size */
	USB_ENDPOINT_DESCRIPTOR_TYPE,   /* bDescriptorType: Endpoint */
	CDC_CMD_EP,                     /* bEndpointAddress */
	0x03,                           /* bmAttributes: Interrupt */
	VIRTUAL_COM_PORT_INT_SIZE, /* wMaxPacketSize: */
	0x00, 0xFF, /* bInterval: */

	/*---------------------------------------------------------------------------*/

	/*Data class interface descriptor*/
	0x09,   /* bLength: Endpoint Descriptor size */
	USB_INTERFACE_DESCRIPTOR_TYPE,  /* bDescriptorType: */
	0x02,   /* bInterfaceNumber: Number of Interface */
	0x00,   /* bAlternateSetting: Alternate setting */
	0x02,   /* bNumEndpoints: Two endpoints used */
	0x0A,   /* bInterfaceClass: CDC */
	0x00,   /* bInterfaceSubClass: */
	0x00,   /* bInterfaceProtocol: */
	0x00,   /* iInterface: */

	/*Endpoint OUT Descriptor*/
	0x07,   /* bLength: Endpoint Descriptor size */
	USB_ENDPOINT_DESCRIPTOR_TYPE,      /* bDescriptorType: Endpoint */
	CDC_OUT_EP,                        /* bEndpointAddress */
	0x02,                              /* bmAttributes: Bulk */
	VIRTUAL_COM_PORT_DATA_SIZE, /* wMaxPacketSize: */
	0x00, 0x00, /* bInterval: ignore for Bulk transfer */

	/*Endpoint IN Descriptor*/
	0x07,   /* bLength: Endpoint Descriptor size */
	USB_ENDPOINT_DESCRIPTOR_TYPE,     /* bDescriptorType: Endpoint */
	CDC_IN_EP,                        /* bEndpointAddress */
	0x02,                             /* bmAttributes: Bulk */
	VIRTUAL_COM_PORT_DATA_SIZE, /* wMaxPacketSize: */
	0x00, 0x00 /* bInterval */
};

const uint8_t CustomHID_ReportDescriptor[CUSTOMHID_SIZ_REPORT_DESC] =
{
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    // COLLECTION (Application)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x09, 0x32,                    //     USAGE (Z)
    0x09, 0x33,                    //     USAGE (Rx)
    0x09, 0x35,                    //     USAGE (Rz)
    0x09, 0x34,                    //     USAGE (Ry)
    0x09, 0x40,                    //     USAGE (Vx)
    0x09, 0x38,                    //     USAGE (Wheel)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x08,                    //     REPORT_COUNT (8)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0xc0,                          //   END_COLLECTION
    0xc0                           /*     END_COLLECTION	             */
}; /* CustomHID_ReportDescriptor */

/* USB String Descriptors */
const uint8_t Composite_StringLangID[Composite_SIZ_STRING_LANGID] = {
    Composite_SIZ_STRING_LANGID,
	USB_STRING_DESCRIPTOR_TYPE,
	0x09,
	0x04 /* LangID = 0x0409: U.S. English */
};

const uint8_t Composite_StringVendor[Composite_SIZ_STRING_VENDOR] = {
Composite_SIZ_STRING_VENDOR, /* Size of Vendor string */
USB_STRING_DESCRIPTOR_TYPE, /* bDescriptorType*/
/* Manufacturer: "STMicroelectronics" */
'S', 0, 'T', 0, 'M', 0, 'i', 0, 'c', 0, 'r', 0, 'o', 0, 'e', 0, 'l', 0, 'e', 0, 'c', 0, 't', 0, 'r', 0, 'o', 0, 'n', 0, 'i', 0, 'c', 0, 's', 0 };

const uint8_t Composite_StringProduct[Composite_SIZ_STRING_PRODUCT] = {
Composite_SIZ_STRING_PRODUCT, /* bLength */
USB_STRING_DESCRIPTOR_TYPE, /* bDescriptorType */
/* Product name: "STM32 Virtual COM Port" */
'S', 0, 'T', 0, 'M', 0, '3', 0, '2', 0, ' ', 0, 'V', 0, 'i', 0, 'r', 0, 't', 0, 'u', 0, 'a', 0, 'l', 0, ' ', 0, 'C', 0, 'O', 0, 'M', 0, ' ', 0, 'P', 0, 'o', 0, 'r', 0, 't', 0, ' ', 0, ' ', 0 };

uint8_t Composite_StringSerial[Composite_SIZ_STRING_SERIAL] = {
Composite_SIZ_STRING_SERIAL, /* bLength */
USB_STRING_DESCRIPTOR_TYPE, /* bDescriptorType */
'S', 0, 'T', 0, 'M', 0, '3', 0, '2', 0 };

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
