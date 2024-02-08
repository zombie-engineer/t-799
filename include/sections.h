#pragma once

#define BSS_NOMMU __attribute__((section(".bss_nommu")))
#define EXCEPTION __attribute__((section(".exception_vector")))

