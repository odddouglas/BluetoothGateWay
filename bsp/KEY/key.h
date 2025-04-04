#ifndef LED_H
#define LED_H

#include <stdint.h>

void key_init(void);
uint8_t key_read(void);
uint8_t scan_keyval(void);
#endif // LED_H
