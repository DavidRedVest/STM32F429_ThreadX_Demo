#ifndef APP_H_
#define APP_H_

#include "main.h"
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

void app_init(void);
void app_task(void);
void led_set(uint8_t sta);

#endif
