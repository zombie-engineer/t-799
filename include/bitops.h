#pragma once

#define BIT(__n) (1<<__n)
#define BIT_ENABLED(__value, __n) (__value & BIT(__n))
