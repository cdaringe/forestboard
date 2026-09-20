#pragma once
#include <cstdint>
#include <cstdio>

// These are physical QWERTY-position USB usages, before any host-layout
// mapping. Enter/Escape are reserved for accepting/cancelling binding edits.
// Only keys present on the matrix are accepted; Fn, Layout and the encoder stay
// local.
inline bool isBindablePhysicalKey(uint32_t usage) {
  return (usage >= 0x04 && usage <= 0x27) || (usage >= 0x2a && usage <= 0x31) ||
      (usage >= 0x33 && usage <= 0x47) || (usage >= 0x49 && usage <= 0x57) ||
      (usage >= 0x59 && usage <= 0x63);
}
inline uint32_t stepPhysicalKey(uint32_t usage, int8_t direction) {
  do {
    usage = direction > 0 ? (usage == 0x63 ? 0x04 : usage + 1)
                          : (usage == 0x04 ? 0x63 : usage - 1);
  } while (!isBindablePhysicalKey(usage));
  return usage;
}
inline void physicalKeyLabel(uint32_t usage, char* text, size_t size) {
  if (usage >= 4 && usage <= 29) {
    snprintf(
        text, size, "QWERTY %c position", static_cast<char>('A' + usage - 4));
  } else if (usage >= 0x1e && usage <= 0x27) {
    snprintf(text, size, "Number %c position",
        usage == 0x27 ? '0' : static_cast<char>('1' + usage - 0x1e));
  } else {
    const char* name = nullptr;
    switch (usage) {
    case 0x2a:
      name = "Backspace";
      break;
    case 0x2b:
      name = "Tab";
      break;
    case 0x2c:
      name = "Space";
      break;
    case 0x2d:
      name = "Minus";
      break;
    case 0x2e:
      name = "Equals";
      break;
    case 0x2f:
      name = "Left bracket";
      break;
    case 0x30:
      name = "Right bracket";
      break;
    case 0x31:
      name = "Backslash";
      break;
    case 0x33:
      name = "Semicolon";
      break;
    case 0x34:
      name = "Quote";
      break;
    case 0x35:
      name = "Grave";
      break;
    case 0x36:
      name = "Comma";
      break;
    case 0x37:
      name = "Period";
      break;
    case 0x38:
      name = "Slash";
      break;
    case 0x39:
      name = "Caps Lock";
      break;
    case 0x46:
      name = "Print Screen";
      break;
    case 0x47:
      name = "Scroll Lock";
      break;
    case 0x49:
      name = "Insert";
      break;
    case 0x4a:
      name = "Home";
      break;
    case 0x4b:
      name = "Page Up";
      break;
    case 0x4c:
      name = "Delete";
      break;
    case 0x4d:
      name = "End";
      break;
    case 0x4e:
      name = "Page Down";
      break;
    case 0x4f:
      name = "Right arrow";
      break;
    case 0x50:
      name = "Left arrow";
      break;
    case 0x51:
      name = "Down arrow";
      break;
    case 0x52:
      name = "Up arrow";
      break;
    case 0x53:
      name = "Num Lock";
      break;
    case 0x54:
      name = "Keypad /";
      break;
    case 0x55:
      name = "Keypad *";
      break;
    case 0x56:
      name = "Keypad -";
      break;
    case 0x57:
      name = "Keypad +";
      break;
    case 0x58:
      name = "Keypad Enter";
      break;
    case 0x63:
      name = "Keypad .";
      break;
    }
    if (name) {
      snprintf(text, size, "%s", name);
    } else if (usage >= 0x3a && usage <= 0x45) {
      snprintf(
          text, size, "F%lu", static_cast<unsigned long>(usage - 0x3a + 1));
    } else if (usage >= 0x59 && usage <= 0x62) {
      snprintf(text, size, "Keypad %c",
          usage == 0x62 ? '0' : static_cast<char>('1' + usage - 0x59));
    } else {
      snprintf(text, size, "Unbound");
    }
  }
}
