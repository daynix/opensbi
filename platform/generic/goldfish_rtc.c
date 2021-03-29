#include <libfdt.h>
#include <sbi/sbi_scratch.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <goldfish_rtc.h>
#include <sbi/sbi_console.h>

static unsigned long rtc_base = 0;

#define RTC(offset) (volatile u32 *)(rtc_base + offset)
// these defines are from QEMU, the are different from ones
// defined in GoldFish RTC spec
#define RTC_TIME_LOW            0x00
#define RTC_TIME_HIGH           0x04
#define RTC_ALARM_LOW           0x08
#define RTC_ALARM_HIGH          0x0c
#define RTC_IRQ_ENABLED         0x10
#define RTC_CLEAR_ALARM         0x14
#define RTC_ALARM_STATUS        0x18
#define RTC_CLEAR_INTERRUPT     0x1c

void rtc_restart(u32 delta)
{
	u64 val;
	u32 vlow;
	*RTC(RTC_CLEAR_ALARM) = 0;
	*RTC(RTC_CLEAR_INTERRUPT) = 0;

	// read of LOW updates HIGH
	vlow = *RTC(RTC_TIME_LOW);
	val = *RTC(RTC_TIME_HIGH);
	val = val << 32;
	val |= vlow;
	val += delta;

	*RTC(RTC_ALARM_HIGH) = (u32)(val >> 32);
	// write of LOW sets next timer
	*RTC(RTC_ALARM_LOW) = (u32)val;
	*RTC(RTC_IRQ_ENABLED) = 1;
}

void rtc_stop(void)
{
	*RTC(RTC_CLEAR_ALARM) = 0;
	*RTC(RTC_CLEAR_INTERRUPT) = 0;
	*RTC(RTC_IRQ_ENABLED) = 0;
}

int rtc_get_irq(void)
{
	void *fdt = sbi_scratch_thishart_arg1_ptr();
	int offset, count;
	u32 *cells, interrupt = 0;
	unsigned long size;
	const struct fdt_match *found;
	const struct fdt_match match[3] =
	{
		{ .compatible = "google,goldfish-rtc" },
		{ }
	};

	offset = fdt_find_match(fdt, -1, match, &found);
	if (offset < 0)
		return 0;

	if (fdt_get_node_addr_size(fdt, offset, &rtc_base, &size) < 0) {
		return 0;
	}

	cells = (u32 *)fdt_getprop(fdt, offset, "interrupts", &count);
	if (cells)
		interrupt = fdt32_to_cpu(*cells);

	sbi_printf("%s, irq %u, base %lx\n", __func__, interrupt, rtc_base);
	return interrupt;
}
