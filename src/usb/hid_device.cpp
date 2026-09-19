#include <Arduino.h>

#include "usb/hid_device.h"

#if defined(ERGOBOARD_KEYBOARD_MODE)

#include <cstring>

extern "C" {
#include <usbd_ioreq.h>
// STM32duino exports USBDevice/inc; its class-state header lives beside it.
#include <../src/hid/usbd_hid_composite.h>

extern USBD_ClassTypeDef USBD_COMPOSITE_HID;
extern USBD_HandleTypeDef hUSBD_Device_HID;

USBD_StatusTypeDef __real_USBD_CtlSendData(
    USBD_HandleTypeDef* pdev, uint8_t* pbuf, uint32_t len);
}

namespace {

constexpr uint8_t kHidDescriptorType = 0x21;
constexpr uint8_t kHidReportDescriptorType = 0x22;
constexpr uint8_t kConfigurationDescriptorType = 0x02;
constexpr uint8_t kSetReportRequest = 0x09;
constexpr uint8_t kOutputReportType = 0x02;
constexpr uint8_t kKeyboardInterface = 0x01;
constexpr uint8_t kOriginalKeyboardDescriptorSize = 45;
constexpr uint8_t kLedOutputDescriptorSize = 18;
constexpr uint8_t kExtendedKeyboardDescriptorSize =
    kOriginalKeyboardDescriptorSize + kLedOutputDescriptorSize;
constexpr uint8_t kNumLockLedMask = 0x01;

// STM32duino already allocates a second HID interface/endpoint for a mouse.
// This keyboard has no pointing device: expose that endpoint as Consumer
// Control instead, without adding report IDs to the boot keyboard report.
constexpr uint8_t kConsumerInterface = 0;
uint8_t consumerDescriptor[] = {
    0x05,
    0x0C, // Usage Page (Consumer)
    0x09,
    0x01, // Usage (Consumer Control)
    0xA1,
    0x01, // Collection (Application)
    0x15,
    0x00,
    0x25,
    0x01, // Logical Minimum/Maximum (0/1)
    0x09,
    0xE2, // Mute
    0x09,
    0xE9, // Volume Increment
    0x09,
    0xEA, // Volume Decrement
    0x75,
    0x01,
    0x95,
    0x03,
    0x81,
    0x02, // Three one-bit buttons
    0x75,
    0x05,
    0x95,
    0x01,
    0x81,
    0x03, // Five constant padding bits
    0xC0,
};
uint8_t consumerReport = 0; // Keep USB IN storage alive until completion.

uint8_t keyboardLedRxBuffer = 0;
volatile uint8_t keyboardLedReport = 0;
bool isWaitingForKeyboardLedReport = false;
bool isHooksInstalled = false;

uint8_t (*originalSetup)(USBD_HandleTypeDef*, USBD_SetupReqTypedef*) = nullptr;
uint8_t (*originalEp0RxReady)(USBD_HandleTypeDef*) = nullptr;

bool isRequestType(const USBD_SetupReqTypedef& request, uint8_t type) {
  return (request.bmRequest & USB_REQ_TYPE_MASK) == type;
}

bool isRequestForInterface(
    const USBD_SetupReqTypedef& request, uint8_t interface) {
  return (request.wIndex & 0xffU) == interface;
}

bool isConsumerInputRequest(const USBD_SetupReqTypedef& request) {
  const bool isGetInput =
      request.bRequest == 0x01 && (request.wValue >> 8) == 0x01;
  return isRequestType(request, USB_REQ_TYPE_CLASS) && isGetInput &&
      isRequestForInterface(request, kConsumerInterface);
}

bool isConsumerDescriptorRequest(const USBD_SetupReqTypedef& request) {
  const bool isGetReportDescriptor =
      request.bRequest == USB_REQ_GET_DESCRIPTOR &&
      (request.wValue >> 8) == kHidReportDescriptorType;
  return isRequestType(request, USB_REQ_TYPE_STANDARD) &&
      isGetReportDescriptor &&
      isRequestForInterface(request, kConsumerInterface);
}

bool isKeyboardLedRequest(const USBD_SetupReqTypedef& request) {
  const bool isSetOutput = request.bRequest == kSetReportRequest &&
      (request.wValue >> 8) == kOutputReportType;
  return isRequestType(request, USB_REQ_TYPE_CLASS) && isSetOutput &&
      isRequestForInterface(request, kKeyboardInterface) &&
      request.wLength == 1;
}

uint8_t sendConsumerDescriptor(
    USBD_HandleTypeDef* device, uint16_t requestedLength) {
  const uint16_t length =
      min(requestedLength, uint16_t{sizeof(consumerDescriptor)});
  return __real_USBD_CtlSendData(device, consumerDescriptor, length);
}

uint8_t receiveKeyboardLeds(USBD_HandleTypeDef* device) {
  isWaitingForKeyboardLedReport = true;
  return USBD_CtlPrepareRx(
      device, &keyboardLedRxBuffer, sizeof(keyboardLedRxBuffer));
}

uint8_t handleKeyboardSetup(
    USBD_HandleTypeDef* device, USBD_SetupReqTypedef* request) {
  if (isConsumerInputRequest(*request)) {
    const uint16_t length = min(request->wLength, uint16_t{1});
    return __real_USBD_CtlSendData(device, &consumerReport, length);
  }
  if (isConsumerDescriptorRequest(*request)) {
    return sendConsumerDescriptor(device, request->wLength);
  }
  if (isKeyboardLedRequest(*request)) {
    return receiveKeyboardLeds(device);
  }
  return originalSetup != nullptr ? originalSetup(device, request) : USBD_FAIL;
}

uint8_t handleKeyboardEp0RxReady(USBD_HandleTypeDef* pdev) {
  if (isWaitingForKeyboardLedReport) {
    isWaitingForKeyboardLedReport = false;
    keyboardLedReport = keyboardLedRxBuffer;
    return USBD_OK;
  }
  return originalEp0RxReady != nullptr ? originalEp0RxReady(pdev) : USBD_OK;
}

bool isHidDescriptor(const uint8_t* data, uint32_t length) {
  return length == 9 && data[0] == 9 && data[1] == kHidDescriptorType;
}

bool isConfigurationDescriptor(const uint8_t* data, uint32_t length) {
  const bool isValidLength = length >= 9 && length <= 128;
  return isValidLength && data[0] == 9 &&
      data[1] == kConfigurationDescriptorType;
}

bool isKeyboardReportDescriptor(const uint8_t* data, uint32_t length) {
  static constexpr uint8_t prefix[] = {0x05, 0x01, 0x09, 0x06, 0xA1, 0x01};
  return length == kOriginalKeyboardDescriptorSize &&
      memcmp(data, prefix, sizeof(prefix)) == 0;
}

void patchHidLength(uint8_t* descriptor) {
  const bool isReportDescriptor = descriptor[6] == kHidReportDescriptorType;
  const bool isShortLength = descriptor[8] == 0;
  if (!isReportDescriptor || !isShortLength) {
    return;
  }
  switch (descriptor[7]) {
  case HID_MOUSE_REPORT_DESC_SIZE:
    descriptor[7] = sizeof(consumerDescriptor);
    break;
  case kOriginalKeyboardDescriptorSize:
    descriptor[7] = kExtendedKeyboardDescriptorSize;
    break;
  default:
    break;
  }
}

bool isConsumerInterfaceDescriptor(const uint8_t* data, uint8_t length) {
  return length >= 9 && data[1] == USB_DESC_TYPE_INTERFACE &&
      data[2] == kConsumerInterface;
}

void patchConfiguration(uint8_t* data, uint32_t length) {
  for (uint32_t offset = 0; offset + 1 < length;) {
    uint8_t* descriptor = data + offset;
    const uint8_t size = descriptor[0];
    const bool isCompleteDescriptor = size >= 2 && offset + size <= length;
    if (!isCompleteDescriptor) {
      return;
    }
    if (isConsumerInterfaceDescriptor(descriptor, size)) {
      descriptor[6] = 0; // No boot subclass.
      descriptor[7] = 0; // No mouse protocol.
    }
    if (isHidDescriptor(descriptor, size)) {
      patchHidLength(descriptor);
    }
    offset += size;
  }
}

uint8_t* extendKeyboardDescriptor(const uint8_t* original) {
  static uint8_t extended[kExtendedKeyboardDescriptorSize];
  // Five standard keyboard LEDs, then three constant padding bits.
  static constexpr uint8_t leds[kLedOutputDescriptorSize] = {
      0x95,
      0x05,
      0x75,
      0x01,
      0x05,
      0x08,
      0x19,
      0x01,
      0x29,
      0x05,
      0x91,
      0x02,
      0x95,
      0x01,
      0x75,
      0x03,
      0x91,
      0x03,
  };
  constexpr uint8_t prefixSize = kOriginalKeyboardDescriptorSize - 1;
  memcpy(extended, original, prefixSize);
  memcpy(extended + prefixSize, leds, sizeof(leds));
  extended[sizeof(extended) - 1] = 0xC0;
  return extended;
}

uint8_t* patchedConfiguration(const uint8_t* original, uint32_t length) {
  static uint8_t patched[128];
  memcpy(patched, original, length);
  patchConfiguration(patched, length);
  return patched;
}

uint8_t* patchedHidDescriptor(const uint8_t* original) {
  static uint8_t patched[9];
  memcpy(patched, original, sizeof(patched));
  patchHidLength(patched);
  return patched;
}

bool isConsumerEndpointReady() {
  if (hUSBD_Device_HID.dev_state != USBD_STATE_CONFIGURED) {
    return false;
  }
  const auto* state = static_cast<USBD_HID_HandleTypeDef*>(
      hUSBD_Device_HID.pClassDataCmsit[hUSBD_Device_HID.classId]);
  return state != nullptr && state->Mousestate == HID_IDLE;
}

} // namespace

void installUsbHidSupport() {
  if (isHooksInstalled) {
    return;
  }
  isHooksInstalled = true;
  originalSetup = USBD_COMPOSITE_HID.Setup;
  originalEp0RxReady = USBD_COMPOSITE_HID.EP0_RxReady;
  USBD_COMPOSITE_HID.Setup = handleKeyboardSetup;
  USBD_COMPOSITE_HID.EP0_RxReady = handleKeyboardEp0RxReady;
}

bool isUsbHostNumLockActive() {
  return (keyboardLedReport & kNumLockLedMask) != 0;
}

bool sendUsbConsumerReport(uint8_t buttons) {
  if (!isConsumerEndpointReady()) {
    return false;
  }
  consumerReport = buttons;
  return USBD_HID_MOUSE_SendReport(&hUSBD_Device_HID, &consumerReport, 1) ==
      USBD_OK;
}

extern "C" USBD_StatusTypeDef __wrap_USBD_CtlSendData(
    USBD_HandleTypeDef* device, uint8_t* data, uint32_t length) {
  if (isKeyboardReportDescriptor(data, length)) {
    data = extendKeyboardDescriptor(data);
    length = kExtendedKeyboardDescriptorSize;
  } else if (isConfigurationDescriptor(data, length)) {
    data = patchedConfiguration(data, length);
  } else if (isHidDescriptor(data, length)) {
    data = patchedHidDescriptor(data);
  }
  return __real_USBD_CtlSendData(device, data, length);
}

#else

void installUsbHidSupport() {}
bool isUsbHostNumLockActive() {
  return false;
}
bool sendUsbConsumerReport(uint8_t) {
  return false;
}

#endif // ERGOBOARD_KEYBOARD_MODE
