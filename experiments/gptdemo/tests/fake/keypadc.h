#ifndef FAKE_KEYPADC_H
#define FAKE_KEYPADC_H
#include <stdint.h>
extern volatile uint8_t kb_Data[8];
void kb_Scan(void);
#endif
