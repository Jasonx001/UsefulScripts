#include <stdint.h>

#define ECU_SW_VERSION_ADDR 0x1000

/* Some other strings that should be ignored */
const char *build_info = "V2.999 build info string";

const uint8_t ECU_SW_VERSION[6] __attribute__               \
                                          (                                    \
                                           (space(prog),                       \
                                            section(".ECU_SW_VERSION"),          \
                                            address(ECU_SW_VERSION_ADDR))                              \
                                          ) =
{
    'V', 1, '.', 1, 0, 0
};
