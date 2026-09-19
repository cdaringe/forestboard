#pragma once
#include "usbd_ioreq.h"
#define HID_MOUSE_REPORT_DESC_SIZE 74U
typedef enum { HID_IDLE, HID_BUSY } HID_StateTypeDef;
typedef struct {
  uint32_t Protocol, IdleState, AltSetting;
  HID_StateTypeDef Mousestate, Keyboardstate;
} USBD_HID_HandleTypeDef;
uint8_t USBD_HID_MOUSE_SendReport(USBD_HandleTypeDef*, uint8_t*, uint16_t);
