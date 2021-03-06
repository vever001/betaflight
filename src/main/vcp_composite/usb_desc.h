/**
 ******************************************************************************
 * @file    usb_desc.h
 * @author  MCD Application Team
 * @version V4.0.0
 * @date    21-January-2013
 * @brief   Descriptor Header for Virtual COM Port Device
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_DESC_H
#define __USB_DESC_H

/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported define -----------------------------------------------------------*/
#define USB_DEVICE_DESCRIPTOR_TYPE              0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE       0x02
#define USB_STRING_DESCRIPTOR_TYPE              0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE           0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE            0x05

#define HID_DESCRIPTOR_TYPE                     0x21
#define CUSTOMHID_SIZ_HID_DESC                  0x09
#define CUSTOMHID_OFF_HID_DESC                  0x12

#define VIRTUAL_COM_PORT_DATA_SIZE              64
#define VIRTUAL_COM_PORT_INT_SIZE               8

#define Composite_SIZ_DEVICE_DESC               18
#define Composite_SIZ_CONFIG_DESC               100
#define CUSTOMHID_SIZ_REPORT_DESC               38
#define Composite_SIZ_STRING_LANGID             4
#define Composite_SIZ_STRING_VENDOR             38
#define Composite_SIZ_STRING_PRODUCT            50
#define Composite_SIZ_STRING_SERIAL             26

#define USB_ENDPOINT_DESCRIPTOR_TYPE            0x05

#define HID_INTERFACE                           0x0
#define HID_IN_EP                               0x83
#define HID_IN_PACKET                           8

#define CDC_COM_INTERFACE                       0x1
#define CDC_CMD_EP                              0x82
#define CDC_IN_EP                               0x81  /* EP1 for data IN */
#define CDC_OUT_EP                              0x01  /* EP1 for data OUT */
#define CDC_CMD_PACKET_SZE                      8     /* Control Endpoint Packet size */

/* Exported functions ------------------------------------------------------- */
extern const uint8_t Composite_DeviceDescriptor[Composite_SIZ_DEVICE_DESC];
extern const uint8_t Composite_ConfigDescriptor[Composite_SIZ_CONFIG_DESC];
extern const uint8_t CustomHID_ReportDescriptor[CUSTOMHID_SIZ_REPORT_DESC];
extern const uint8_t Composite_StringLangID[Composite_SIZ_STRING_LANGID];
extern const uint8_t Composite_StringVendor[Composite_SIZ_STRING_VENDOR];
extern const uint8_t Composite_StringProduct[Composite_SIZ_STRING_PRODUCT];
extern uint8_t Composite_StringSerial[Composite_SIZ_STRING_SERIAL];

#endif /* __USB_DESC_H */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
