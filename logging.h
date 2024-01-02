#pragma once

namespace nextapp::logging {

} // ns

#define LOGFAULT_USE_TID_AS_NAME 1

#include "logfault/logfault.h"

#define LOG_ERROR   LFLOG_ERROR
#define LOG_WARN    LFLOG_WARN
#define LOG_INFO    LFLOG_INFO
#define LOG_DEBUG   LFLOG_DEBUG
#define LOG_TRACE   LFLOG_TRACE

#define LOG_ERROR_N   LFLOG_ERROR  << __PRETTY_FUNCTION__ << " - "
#define LOG_WARN_N    LFLOG_WARN   << __PRETTY_FUNCTION__ << " - "
#define LOG_INFO_N    LFLOG_INFO   << __PRETTY_FUNCTION__ << " - "
#define LOG_DEBUG_N   LFLOG_DEBUG  << __PRETTY_FUNCTION__ << " - "
#define LOG_TRACE_N   LFLOG_TRACE  << __PRETTY_FUNCTION__ << " - "

