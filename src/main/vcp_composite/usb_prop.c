/**
 ******************************************************************************
 * @file    usb_prop.c
 * @author  MCD Application Team
 * @version V4.0.0
 * @date    21-January-2013
 * @brief   All processing related to Virtual Com Port Demo
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
#include "usb_conf.h"
#include "usb_prop.h"
#include "usb_desc.h"
#include "usb_pwr.h"
#include "hw_config.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint32_t ProtocolValue;
uint8_t Request = 0;
uint8_t Report_Buf[2];

LINE_CODING linecoding = {
  115200,
  /* baud rate*/
  0x00,
  /* stop bits-1*/
  0x00,
  /* parity - none*/
  0x08 /* no. of bits 8*/
};

/* -------------------------------------------------------------------------- */
/*  Structures initializations */
/* -------------------------------------------------------------------------- */

DEVICE Device_Table = {
  EP_NUM,
  1
};

DEVICE_PROP Device_Property = {
  CustomHID_init,
  CustomHID_Reset,
  CustomHID_Status_In,
  CustomHID_Status_Out,
  CustomHID_Data_Setup,
  CustomHID_NoData_Setup,
  CustomHID_Get_Interface_Setting,
  CustomHID_GetDeviceDescriptor,
  CustomHID_GetConfigDescriptor,
  CustomHID_GetStringDescriptor,
  0,
  0x40 /*MAX PACKET SIZE*/
};

USER_STANDARD_REQUESTS User_Standard_Requests = {
  CustomHID_GetConfiguration,
  CustomHID_SetConfiguration,
  CustomHID_GetInterface,
  CustomHID_SetInterface,
  CustomHID_GetStatus,
  CustomHID_ClearFeature,
  CustomHID_SetEndPointFeature,
  CustomHID_SetDeviceFeature,
  CustomHID_SetDeviceAddress
};

ONE_DESCRIPTOR Device_Descriptor = {
  (uint8_t * ) Composite_DeviceDescriptor,
  Composite_SIZ_DEVICE_DESC
};

ONE_DESCRIPTOR Config_Descriptor = {
  (uint8_t * ) Composite_ConfigDescriptor,
  Composite_SIZ_CONFIG_DESC
};

ONE_DESCRIPTOR CustomHID_Report_Descriptor = {
  (uint8_t * ) CustomHID_ReportDescriptor,
  CUSTOMHID_SIZ_REPORT_DESC
};

ONE_DESCRIPTOR CustomHID_Hid_Descriptor = {
  (uint8_t * ) Composite_ConfigDescriptor + CUSTOMHID_OFF_HID_DESC,
  CUSTOMHID_SIZ_HID_DESC
};

ONE_DESCRIPTOR String_Descriptor[4] = {
  {
    (uint8_t * ) Composite_StringLangID, Composite_SIZ_STRING_LANGID
  },
  {
    (uint8_t * ) Composite_StringVendor,
    Composite_SIZ_STRING_VENDOR
  },
  {
    (uint8_t * ) Composite_StringProduct,
    Composite_SIZ_STRING_PRODUCT
  },
  {
    (uint8_t * ) Composite_StringSerial,
    Composite_SIZ_STRING_SERIAL
  }
};

/* Extern variables ----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Extern function prototypes ------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
uint8_t * CustomHID_SetReport_Feature(uint16_t Length);
/*******************************************************************************
 * Function Name  : CustomHID_init.
 * Description    : Custom HID init routine.
 * Input          : None.
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
void CustomHID_init(void) {
  /* Update the serial number string descriptor with the data from the unique 
  ID*/
  Get_SerialNum();

  pInformation->Current_Configuration = 0;
  /* Connect the device */
  PowerOn();

  /* Perform basic device initialization operations */
  USB_SIL_Init();

  bDeviceState = UNCONNECTED;
}

/*******************************************************************************
 * Function Name  : CustomHID_Reset.
 * Description    : Custom HID reset routine.
 * Input          : None.
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
void CustomHID_Reset(void) {
  /* Set Composite_DEVICE as not configured */
  pInformation->Current_Configuration = 0;
  pInformation->Current_Interface = 0; /*the default Interface*/

  /* Current Feature initialization */
  pInformation->Current_Feature = Composite_ConfigDescriptor[7];

  SetBTABLE(BTABLE_ADDRESS);

  /* Initialize Endpoint 0 */
  SetEPType(ENDP0, EP_CONTROL);
  SetEPTxStatus(ENDP0, EP_TX_STALL);
  SetEPRxAddr(ENDP0, ENDP0_RXADDR);
  SetEPTxAddr(ENDP0, ENDP0_TXADDR);
  Clear_Status_Out(ENDP0);
  SetEPRxCount(ENDP0, Device_Property.MaxPacketSize);
  SetEPRxValid(ENDP0);

  /* Initialize Endpoint 1 */
  SetEPType(ENDP1, EP_BULK);
  SetEPTxAddr(ENDP1, ENDP1_TXADDR);
  SetEPTxStatus(ENDP1, EP_TX_NAK);
  SetEPRxAddr(ENDP1, ENDP1_RXADDR);
  SetEPRxCount(ENDP1, VIRTUAL_COM_PORT_DATA_SIZE);
  SetEPRxStatus(ENDP1, EP_RX_VALID);

  /* Initialize Endpoint 2 */
  SetEPType(ENDP2, EP_INTERRUPT);
  SetEPTxAddr(ENDP2, ENDP2_TXADDR);
  SetEPRxStatus(ENDP2, EP_RX_DIS);
  SetEPTxStatus(ENDP2, EP_TX_NAK);

  /* Initialize Endpoint 3 */
  SetEPType(ENDP3, EP_BULK);
  SetEPTxAddr(ENDP3, ENDP3_TXADDR);
  SetEPRxStatus(ENDP3, EP_RX_DIS);
  SetEPTxStatus(ENDP3, EP_TX_NAK);

  /* Set this device to response on default address */
  SetDeviceAddress(0);

  bDeviceState = ATTACHED;
}

/*******************************************************************************
 * Function Name  : CustomHID_SetConfiguration.
 * Description    : Update the device state to configured.
 * Input          : None.
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
void CustomHID_SetConfiguration(void) {
  DEVICE_INFO * pInfo = & Device_Info;

  if (pInfo->Current_Configuration != 0) {
    /* Device configured */
    bDeviceState = CONFIGURED;
  }
}

/*******************************************************************************
 * Function Name  : CustomHID_SetConfiguration.
 * Description    : Update the device state to addressed.
 * Input          : None.
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
void CustomHID_SetDeviceAddress(void) {
  bDeviceState = ADDRESSED;
}

/*******************************************************************************
 * Function Name  : CustomHID_Status_In.
 * Description    : Joystick status IN routine.
 * Input          : None.
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
void CustomHID_Status_In(void) {
  if (Request == SET_LINE_CODING) {
    Request = 0;
  }
}

/*******************************************************************************
 * Function Name  : CustomHID_Status_Out
 * Description    : Joystick status OUT routine.
 * Input          : None.
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
void CustomHID_Status_Out(void) {}

/*******************************************************************************
 * Function Name  : CustomHID_Data_Setup
 * Description    : Handle the data class specific requests.
 * Input          : Request Nb.
 * Output         : None.
 * Return         : USB_UNSUPPORT or USB_SUCCESS.
 *******************************************************************************/
RESULT CustomHID_Data_Setup(uint8_t RequestNo) {
  uint8_t * ( * CopyRoutine)(uint16_t);

  //if (pInformation->USBwIndex != 0)
  //  return USB_UNSUPPORT;

  CopyRoutine = NULL;

  if ((RequestNo == GET_DESCRIPTOR) && (Type_Recipient == (STANDARD_REQUEST | INTERFACE_RECIPIENT))) {

    if (pInformation->USBwValue1 == REPORT_DESCRIPTOR) {
      CopyRoutine = CustomHID_GetReportDescriptor;
    } else if (pInformation->USBwValue1 == HID_DESCRIPTOR_TYPE) {
      CopyRoutine = CustomHID_GetHIDDescriptor;
    }

  } /* End of GET_DESCRIPTOR */

  /*** GET_PROTOCOL, GET_REPORT, SET_REPORT ***/
  else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))) {
    switch (RequestNo) {
    case GET_PROTOCOL:
      CopyRoutine = CustomHID_GetProtocolValue;
      break;
    case SET_REPORT:
      CopyRoutine = CustomHID_SetReport_Feature;
      Request = SET_REPORT;
      break;
    case GET_LINE_CODING:
      CopyRoutine = CustomHID_GetLineCoding;
      break;
    case SET_LINE_CODING:
      CopyRoutine = CustomHID_SetLineCoding;
      Request = SET_LINE_CODING;
      break;
    default:
      break;
    }
  }

  if (CopyRoutine == NULL) {
    return USB_UNSUPPORT;
  }

  pInformation->Ctrl_Info.CopyData = CopyRoutine;
  pInformation->Ctrl_Info.Usb_wOffset = 0;
  ( * CopyRoutine)(0);
  return USB_SUCCESS;
}

/*******************************************************************************
 * Function Name  : CustomHID_SetReport_Feature
 * Description    : Set Feature request handling
 * Input          : Length.
 * Output         : None.
 * Return         : Buffer
 *******************************************************************************/
uint8_t * CustomHID_SetReport_Feature(uint16_t Length) {
    if (Length == 0) {
      pInformation->Ctrl_Info.Usb_wLength = 2;
      return NULL;
    } else {
      return Report_Buf;
    }
  }
  /*******************************************************************************
   * Function Name  : CustomHID_NoData_Setup.
   * Description    : handle the no data class specific requests.
   * Input          : Request Nb.
   * Output         : None.
   * Return         : USB_UNSUPPORT or USB_SUCCESS.
   *******************************************************************************/
RESULT CustomHID_NoData_Setup(uint8_t RequestNo) {

  if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT)) {
    if (RequestNo == SET_COMM_FEATURE) {
      return USB_SUCCESS;
    } else if (RequestNo == SET_CONTROL_LINE_STATE) {
      return USB_SUCCESS;
    } else if (RequestNo == SET_PROTOCOL) {
      return CustomHID_SetProtocol();
    }
  }

  return USB_UNSUPPORT;
}

/*******************************************************************************
 * Function Name  : CustomHID_GetDeviceDescriptor.
 * Description    : Gets the device descriptor.
 * Input          : Length
 * Output         : None.
 * Return         : The address of the device descriptor.
 *******************************************************************************/
uint8_t * CustomHID_GetDeviceDescriptor(uint16_t Length) {
  return Standard_GetDescriptorData(Length, & Device_Descriptor);
}

/*******************************************************************************
 * Function Name  : CustomHID_GetConfigDescriptor.
 * Description    : Gets the configuration descriptor.
 * Input          : Length
 * Output         : None.
 * Return         : The address of the configuration descriptor.
 *******************************************************************************/
uint8_t * CustomHID_GetConfigDescriptor(uint16_t Length) {
  return Standard_GetDescriptorData(Length, & Config_Descriptor);
}

/*******************************************************************************
 * Function Name  : CustomHID_GetStringDescriptor
 * Description    : Gets the string descriptors according to the needed index
 * Input          : Length
 * Output         : None.
 * Return         : The address of the string descriptors.
 *******************************************************************************/
uint8_t * CustomHID_GetStringDescriptor(uint16_t Length) {
  uint8_t wValue0 = pInformation->USBwValue0;
  if (wValue0 >= 4) {
    return NULL;
  } else {
    return Standard_GetDescriptorData(Length, & String_Descriptor[wValue0]);
  }
}

/*******************************************************************************
 * Function Name  : CustomHID_GetReportDescriptor.
 * Description    : Gets the HID report descriptor.
 * Input          : Length
 * Output         : None.
 * Return         : The address of the configuration descriptor.
 *******************************************************************************/
uint8_t * CustomHID_GetReportDescriptor(uint16_t Length) {
  return Standard_GetDescriptorData(Length, & CustomHID_Report_Descriptor);
}

/*******************************************************************************
 * Function Name  : CustomHID_GetHIDDescriptor.
 * Description    : Gets the HID descriptor.
 * Input          : Length
 * Output         : None.
 * Return         : The address of the configuration descriptor.
 *******************************************************************************/
uint8_t * CustomHID_GetHIDDescriptor(uint16_t Length) {
  return Standard_GetDescriptorData(Length, & CustomHID_Hid_Descriptor);
}

/*******************************************************************************
 * Function Name  : CustomHID_Get_Interface_Setting.
 * Description    : tests the interface and the alternate setting according to the
 *                  supported one.
 * Input          : - Interface : interface number.
 *                  - AlternateSetting : Alternate Setting number.
 * Output         : None.
 * Return         : USB_SUCCESS or USB_UNSUPPORT.
 *******************************************************************************/
RESULT CustomHID_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting) {
  if (AlternateSetting > 0) {
    return USB_UNSUPPORT;
  } else if (Interface > 2) {
    return USB_UNSUPPORT;
  }
  return USB_SUCCESS;
}

/*******************************************************************************
 * Function Name  : CustomHID_SetProtocol
 * Description    : Joystick Set Protocol request routine.
 * Input          : None.
 * Output         : None.
 * Return         : USB SUCCESS.
 *******************************************************************************/
RESULT CustomHID_SetProtocol(void) {
  uint8_t wValue0 = pInformation->USBwValue0;
  ProtocolValue = wValue0;
  return USB_SUCCESS;
}

/*******************************************************************************
 * Function Name  : CustomHID_GetProtocolValue
 * Description    : get the protocol value
 * Input          : Length.
 * Output         : None.
 * Return         : address of the protocol value.
 *******************************************************************************/
uint8_t * CustomHID_GetProtocolValue(uint16_t Length) {
  if (Length == 0) {
    pInformation->Ctrl_Info.Usb_wLength = 1;
    return NULL;
  } else {
    return (uint8_t * )( & ProtocolValue);
  }
}

/*******************************************************************************
 * Function Name  : CustomHID_GetLineCoding.
 * Description    : send the linecoding structure to the PC host.
 * Input          : Length.
 * Output         : None.
 * Return         : Linecoding structure base address.
 *******************************************************************************/
uint8_t * CustomHID_GetLineCoding(uint16_t Length) {
  if (Length == 0) {
    pInformation->Ctrl_Info.Usb_wLength = sizeof(linecoding);
    return NULL;
  }
  return (uint8_t * ) & linecoding;
}

/*******************************************************************************
 * Function Name  : CustomHID_SetLineCoding.
 * Description    : Set the linecoding structure fields.
 * Input          : Length.
 * Output         : None.
 * Return         : Linecoding structure base address.
 *******************************************************************************/
uint8_t * CustomHID_SetLineCoding(uint16_t Length) {
  if (Length == 0) {
    pInformation->Ctrl_Info.Usb_wLength = sizeof(linecoding);
    return NULL;
  }
  return (uint8_t*) &linecoding;
}

/*******************************************************************************
 * Function Name  : CustomHID_GetBaudRate.
 * Description    : Get the current baudrate
 * Input          : None.
 * Output         : None.
 * Return         : baudrate in bps
 *******************************************************************************/
uint32_t CustomHID_GetBaudRate(void) {
  return linecoding.bitrate;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/