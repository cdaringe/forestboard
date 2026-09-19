#include "../../src/usb/hid_device.cpp"
#include "Arduino.h"
#include <cassert>
#include <cstdio>
static std::vector<uint8_t> controlReply;
static uint8_t* ledRx;
static uint8_t* inFlight;
extern "C" {
USBD_ClassTypeDef USBD_COMPOSITE_HID = {};
USBD_HandleTypeDef hUSBD_Device_HID = {};
USBD_StatusTypeDef __real_USBD_CtlSendData(
    USBD_HandleTypeDef*, uint8_t* data, uint32_t len) {
  controlReply.assign(data, data + len);
  return USBD_OK;
}
USBD_StatusTypeDef USBD_CtlPrepareRx(
    USBD_HandleTypeDef*, uint8_t* buffer, uint32_t) {
  ledRx = buffer;
  return USBD_OK;
}
uint8_t USBD_HID_MOUSE_SendReport(
    USBD_HandleTypeDef* device, uint8_t* data, uint16_t n) {
  assert(n == 1);
  inFlight = data;
  static_cast<USBD_HID_HandleTypeDef*>(device->pClassDataCmsit[0])->Mousestate =
      HID_BUSY;
  return USBD_OK;
}
}
static void testInterfaceDescriptors() {
  uint8_t configuration[] = {9, 2, 59, 0, 2, 1, 0, 0x80, 50, 9, 4, 0, 0, 1, 3,
      1, 2, 0, 9, 0x21, 0x11, 1, 0, 1, 0x22, 74, 0, 7, 5, 0x81, 3, 4, 0, 10, 9,
      4, 1, 0, 1, 3, 1, 1, 0, 9, 0x21, 0x11, 1, 0, 1, 0x22, 45, 0, 7, 5, 0x82,
      3, 8, 0, 10};
  __wrap_USBD_CtlSendData(
      &hUSBD_Device_HID, configuration, sizeof configuration);
  assert(
      controlReply[15] == 0 && controlReply[16] == 0); // Consumer is non-boot.
  assert(controlReply[25] == sizeof consumerDescriptor);
  assert(controlReply[50] == 63); // Keyboard LEDs are retained.
  assert(
      controlReply[40] == 1 && controlReply[41] == 1); // Keyboard stays boot.
}

static void testConsumerDescriptorRequests() {
  for (uint16_t length : {7, 64}) {
    USBD_SetupReqTypedef request = {0x81, 6, 0x2200, 0, length};
    assert(USBD_COMPOSITE_HID.Setup(&hUSBD_Device_HID, &request) == USBD_OK);
    assert(
        controlReply.size() == min<size_t>(length, sizeof consumerDescriptor));
    assert(controlReply[0] == 5 && controlReply[1] == 0x0C);
  }
}

static void testConsumerDescriptorBits() {
  // Decode HID short items to check three Consumer buttons and exactly 8 bits.
  unsigned bits = 0, reportSize = 0, reportCount = 0;
  std::vector<unsigned> usages;
  for (unsigned i = 0; i < sizeof consumerDescriptor;) {
    uint8_t prefix = consumerDescriptor[i++];
    unsigned n = prefix & 3;
    n = n == 3 ? 4 : n;
    unsigned value = 0;
    for (unsigned j = 0; j < n; ++j) {
      value |= consumerDescriptor[i++] << (j * 8);
    }
    if (prefix == 0x75) {
      reportSize = value;
    }
    if (prefix == 0x95) {
      reportCount = value;
    }
    if (prefix == 0x09) {
      usages.push_back(value);
    }
    if (prefix == 0x81) {
      bits += reportSize * reportCount;
    }
  }
  assert(bits == 8 && usages == std::vector<unsigned>({1, 0xE2, 0xE9, 0xEA}));
}

static void testHidDescriptorLength() {
  uint8_t hidDescriptor[] = {9, 0x21, 0x11, 1, 0, 1, 0x22, 74, 0};
  __wrap_USBD_CtlSendData(
      &hUSBD_Device_HID, hidDescriptor, sizeof hidDescriptor);
  assert(controlReply[7] == sizeof consumerDescriptor);
}

static void testKeyboardLeds() {
  USBD_SetupReqTypedef leds = {0x21, 9, 0x0200, 1, 1};
  assert(USBD_COMPOSITE_HID.Setup(&hUSBD_Device_HID, &leds) == USBD_OK);
  *ledRx = 1;
  USBD_COMPOSITE_HID.EP0_RxReady(&hUSBD_Device_HID);
  assert(isUsbHostNumLockActive());
}

static void testConsumerReportStorage() {
  USBD_HID_HandleTypeDef state = {};
  hUSBD_Device_HID.pClassDataCmsit[0] = &state;
  assert(!sendUsbConsumerReport(2));
  hUSBD_Device_HID.dev_state = USBD_STATE_CONFIGURED;
  assert(sendUsbConsumerReport(2) && *inFlight == 2);
  assert(!sendUsbConsumerReport(0) &&
      *inFlight == 2); // Busy must not overwrite storage.
  state.Mousestate = HID_IDLE;
  assert(sendUsbConsumerReport(0) && *inFlight == 0);
  USBD_SetupReqTypedef getReport = {0xA1, 1, 0x0100, 0, 1};
  assert(USBD_COMPOSITE_HID.Setup(&hUSBD_Device_HID, &getReport) == USBD_OK);
  assert(controlReply == std::vector<uint8_t>{0});
}

int main() {
  installUsbHidSupport();
  testInterfaceDescriptors();
  testConsumerDescriptorRequests();
  testConsumerDescriptorBits();
  testHidDescriptorLength();
  testKeyboardLeds();
  testConsumerReportStorage();
  puts("USB descriptor/report tests passed");
}
