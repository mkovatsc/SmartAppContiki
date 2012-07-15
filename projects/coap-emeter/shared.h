/* shared.h */
#include "contiki.h"
#include "settings.h"

#define min(x,y) ((x) < (y) ? (x) : (y))

typedef enum {A, V, W, Wh, kWh, C, UNKNOWN} unit;

// sml functions
char *unit_to_string(unit u);

// universal functions
double reabs(double x);
double zpow(double base, int32_t exponent);
void double_to_2ints(double val, int32_t *z_val, uint32_t *r_val);

// ctime for contiki
int16_t ctime(uint64_t seconds, char *str, int32_t length);

//default time
#ifndef DEFAULT_TIME
    #define TIME_SEC     0
    #define TIME_MIN     0
    #define TIME_HOUR    0
    #define TIME_DAY     1
    #define TIME_MONTH   1
    #define TIME_YEAR    1970
#endif