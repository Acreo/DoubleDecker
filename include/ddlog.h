#ifdef __cplusplus
extern "C" {
#endif

#ifndef _DDLOG_H_
#define _DDLOG_H_

#define DD_LOG_NONE 0
#define DD_LOG_ERROR 1
#define DD_LOG_WARNING 2
#define DD_LOG_NOTICE 3
#define DD_LOG_INFO 4
#define DD_LOG_DEBUG 5
// logging
extern int loglevel;

//  Log error condition - highest priority
#define dd_error(...)                                                          \
  if (loglevel >= DD_LOG_ERROR)                                                \
    zsys_error(__VA_ARGS__);

//  Log warning condition - high priority
#define dd_warning(...)                                                        \
  if (loglevel >= DD_LOG_WARNING)                                              \
    zsys_warning(__VA_ARGS__);

//  Log normal, but significant, condition - normal priority
#define dd_notice(...)                                                         \
  if (loglevel >= DD_LOG_NOTICE)                                               \
    zsys_notice(__VA_ARGS__);

//  Log informational message - low priority
#define dd_info(...)                                                           \
  if (loglevel >= DD_LOG_INFO)                                                 \
    zsys_info(__VA_ARGS__);

//  Log debug-level message - lowest priority
#define dd_debug(...)                                                          \
  if (loglevel >= DD_LOG_DEBUG)                                                \
    zsys_error(__VA_ARGS__);

#endif

#ifdef __cplusplus
}
#endif
