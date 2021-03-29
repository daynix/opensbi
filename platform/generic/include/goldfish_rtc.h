#ifndef __GOLDFISH_H__
#define __GOLDFISH_H__

int  rtc_get_irq(void);
void rtc_restart(u32 delta);
void rtc_stop(void);

#endif

