#pragma once
#include <bitops.h>

#define IO_FLAG_READ BIT(0)
#define IO_FLAG_WRITE BIT(1)
#define IO_FLAG_RDWR (IO_FLAG_READ | IO_FLAG_WRITE)
