#include <Arduino.h>
#include "serial-appender.hpp"

namespace esp32m
{

  bool SerialAppender::append(const char *message)
  {
    if (message)
      Serial.println(message);
    return true;
  }

} // namespace esp32m
