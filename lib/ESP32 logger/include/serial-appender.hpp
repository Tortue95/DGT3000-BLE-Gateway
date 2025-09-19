#pragma once

#include "logging.hpp"

namespace esp32m
{

  /**
   * @brief Appender for USB Serial output.
   * 
   * This appender inherits from FormattingAppender to receive pre-formatted strings
   * and writes them to the default Serial port.
   */
  class SerialAppender : public FormattingAppender
  {
  public:
    /**
     * @brief Constructs a new Serial Appender object.
     * 
     * The constructor uses the default message formatter provided by the logging framework.
     */
    SerialAppender() : FormattingAppender(Logging::formatter()){};

  protected:
    /**
     * @brief Appends a formatted log message to the Serial port.
     * 
     * This is the core method that writes the final log string to Serial.
     * 
     * @param message The formatted log message to be written.
     * @return true if the message was successfully written, false otherwise.
     */
    bool append(const char *message) override;
  };

} // namespace esp32m
