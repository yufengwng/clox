#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UINT8_COUNT (UINT8_MAX + 1)

#define UNUSED(param) __attribute__((unused))param

#define NAN_BOXING

// #define DEBUG_LOG_GC
// #define DEBUG_STRESS_GC
// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION
