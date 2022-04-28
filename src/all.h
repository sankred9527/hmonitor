#ifndef _ALL_H_
#define _ALL_H_

#include <rte_log.h>

#define NB_SOCKETS 8

#define HM_LOG(level, ...) RTE_LOG(level, USER1, "["#level"] "__VA_ARGS__)
#define HM_INFO(...)  HM_LOG(INFO, __VA_ARGS__)

#endif
