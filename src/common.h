#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION
#define DEBUG_STRESS_GC
#define DEBUG_LOG_GC

#define NAN_BOXING

#define UINT8_COUNT (UINT8_MAX + 1)

#undef DEBUG_PRINT_CODE
#undef DEBUG_TRACE_EXECUTION
#undef DEBUG_STRESS_GC
#undef DEBUG_LOG_GC
