#pragma once

#include <BellLogger.h>

#define CSPOT_LOG(type, ...)                                  \
  {                                                           \
    std::scoped_lock lock(bell::bellRegisteredLoggersMutex);  \
    for (auto& logger : bell::bellRegisteredLoggers) {        \
      logger->type(__FILE__, __LINE__, "cspot", __VA_ARGS__); \
    }                                                         \
  }
