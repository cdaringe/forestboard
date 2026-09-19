#include "display/widgets/keystroke_count_format.h"

#include <cstdio>

void formatCompactKeystrokeCount(
    uint32_t count, char* output, size_t outputSize) {
  if (count >= 1000000000UL) {
    snprintf(output, outputSize, "%luB",
        static_cast<unsigned long>(count / 1000000000UL));
  } else if (count >= 1000000UL) {
    snprintf(output, outputSize, "%luM",
        static_cast<unsigned long>(count / 1000000UL));
  } else if (count >= 1000UL) {
    snprintf(
        output, outputSize, "%luK", static_cast<unsigned long>(count / 1000UL));
  } else {
    snprintf(output, outputSize, "%lu", static_cast<unsigned long>(count));
  }
}
