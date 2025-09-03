#pragma once
#include <cstdint>

namespace ma {
// Base
static constexpr uint16_t FMT_NONE = 0;

// Double
inline bool isDoublePrecision(uint16_t s) { return s > 0 && s < 20; }

// Currency
static constexpr uint16_t FMT_CUR_LPS = 101;
static constexpr uint16_t FMT_CUR_USD = 102;
static constexpr uint16_t FMT_CUR_EUR = 103;
inline bool isCurrencyFmt(uint16_t s) { return s >= 100 && s < 200; }

// Bool
static constexpr uint16_t FMT_BOOL_TRUEFALSE = 201;
static constexpr uint16_t FMT_BOOL_YESNO     = 202;
static constexpr uint16_t FMT_BOOL_ONOFF     = 203;
inline bool isBoolFmt(uint16_t s) { return s >= 200 && s < 300; }

// Number
static constexpr uint16_t FMT_NUM_BYTE  = 301;
static constexpr uint16_t FMT_NUM_INT16 = 302;
static constexpr uint16_t FMT_NUM_INT32 = 303;
inline bool isNumberSubtype(uint16_t s) { return s >= 300 && s < 400; }

// Date/Time
static constexpr uint16_t FMT_DT_GENERAL   = 401;
static constexpr uint16_t FMT_DT_LONGDATE  = 402;
static constexpr uint16_t FMT_DT_SHORTDATE = 403;
static constexpr uint16_t FMT_DT_LONGTIME  = 404;
static constexpr uint16_t FMT_DT_SHORTTIME = 405;
inline bool isDateTimeFmt(uint16_t s) { return s >= 400 && s < 500; }
}
