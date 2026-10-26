// libev configuration for wrxvt. Included two ways, and both must see the
// same EV_* macros or libev's ABI won't match between ev.c and the rest:
//   - ev.c is compiled with -DEV_CONFIG_H='"ev_config.h"', so libev picks
//     this up itself (ev.c: #ifndef EV_STANDALONE / #include EV_CONFIG_H);
//   - ev_cpp.h includes it for the C++ event-loop wrappers (ev++.h).
// That is why ev.c no longer needs to be #include'd into a .C file.
#ifndef EV_CONFIG_WRXVT_H
#define EV_CONFIG_WRXVT_H

#define ECB_NO_THREADS 1

#define EV_COMPAT3 0
#define EV_USE_SELECT 1
#define EV_USE_EPOLL 1
#define EV_USE_KQUEUE 1
#define EV_USE_PORT 1
// #define EV_USE_LINUXAIO 1
#define EV_USE_IOURING 1
// #define EV_USE_TIMERFD 0
#define EV_PREPARE_ENABLE 1
#define EV_IDLE_ENABLE 1
#define EV_SIGNAL_ENABLE 1
#define EV_CHILD_ENABLE 1
#define EV_USE_STDEXCEPT 0

#include <config.h>

#if ENABLE_FRILLS
#define EV_FEATURES 1 | 2
#else
#define EV_FEATURES 0
#define NDEBUG
#endif

#endif
