#pragma once
#include <stdint.h>
typedef enum { USBD_OK, USBD_BUSY, USBD_FAIL } USBD_StatusTypeDef;
enum {
  USB_REQ_TYPE_MASK = 0x60,
  USB_REQ_TYPE_CLASS = 0x20,
  USB_REQ_TYPE_STANDARD = 0,
  USB_REQ_GET_DESCRIPTOR = 6,
  USB_DESC_TYPE_INTERFACE = 4,
  USBD_STATE_CONFIGURED = 3
};
typedef struct {
  uint8_t dev_state, classId;
  void* pClassDataCmsit[1];
} USBD_HandleTypeDef;
typedef struct {
  uint8_t bmRequest, bRequest;
  uint16_t wValue, wIndex, wLength;
} USBD_SetupReqTypedef;
typedef struct {
  uint8_t (*Setup)(USBD_HandleTypeDef*, USBD_SetupReqTypedef*);
  uint8_t (*EP0_RxReady)(USBD_HandleTypeDef*);
} USBD_ClassTypeDef;
USBD_StatusTypeDef USBD_CtlPrepareRx(USBD_HandleTypeDef*, uint8_t*, uint32_t);

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef*, uint8_t);
