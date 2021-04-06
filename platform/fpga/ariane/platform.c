/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2019 FORTH-ICS/CARV
 *				Panagiotis Peristerakis <perister@ics.forth.gr>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_platform.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi_utils/irqchip/plic.h>
#include <sbi_utils/serial/uart8250.h>
#include <sbi_utils/sys/clint.h>

#define ARIANE_UART_ADDR			0x10000000
#define ARIANE_UART_FREQ			50000000
#define ARIANE_UART_BAUDRATE			115200
#define ARIANE_UART_REG_SHIFT			2
#define ARIANE_UART_REG_WIDTH			4
#define ARIANE_PLIC_ADDR			0xc000000
#define ARIANE_PLIC_NUM_SOURCES			30
#define ARIANE_HART_COUNT			1
#define ARIANE_CLINT_ADDR			0x2000000

static struct plic_data plic = {
	.addr = ARIANE_PLIC_ADDR,
	.num_src = ARIANE_PLIC_NUM_SOURCES,
};

static struct clint_data clint = {
	.addr = ARIANE_CLINT_ADDR,
	.first_hartid = 0,
	.hart_count = ARIANE_HART_COUNT,
	.has_64bit_mmio = TRUE,
};

/*
 * Ariane platform early initialization.
 */
static int ariane_early_init(bool cold_boot)
{
	/* For now nothing to do. */
	return 0;
}

/*
 * Ariane platform final initialization.
 */
static int ariane_final_init(bool cold_boot)
{
	void *fdt;

	if (!cold_boot)
		return 0;

	fdt = sbi_scratch_thishart_arg1_ptr();
	fdt_fixups(fdt);

	return 0;
}

/*
 * Initialize the ariane console.
 */
static int ariane_console_init(void)
{
	return uart8250_init(ARIANE_UART_ADDR,
			     ARIANE_UART_FREQ,
			     ARIANE_UART_BAUDRATE,
			     ARIANE_UART_REG_SHIFT,
			     ARIANE_UART_REG_WIDTH);
}

static int plic_ariane_warm_irqchip_init(int m_cntx_id, int s_cntx_id)
{
	size_t i, ie_words = ARIANE_PLIC_NUM_SOURCES / 32 + 1;

	/* By default, enable all IRQs for M-mode of target HART */
	if (m_cntx_id > -1) {
		for (i = 0; i < ie_words; i++)
			plic_set_ie(&plic, m_cntx_id, i, 1);
	}
	/* Enable all IRQs for S-mode of target HART */
	if (s_cntx_id > -1) {
		for (i = 0; i < ie_words; i++)
			plic_set_ie(&plic, s_cntx_id, i, 1);
	}
	/* By default, enable M-mode threshold */
	if (m_cntx_id > -1)
		plic_set_thresh(&plic, m_cntx_id, 1);
	/* By default, disable S-mode threshold */
	if (s_cntx_id > -1)
		plic_set_thresh(&plic, s_cntx_id, 0);

	return 0;
}

/*
 * Initialize the ariane interrupt controller for current HART.
 */
static int ariane_irqchip_init(bool cold_boot)
{
	u32 hartid = current_hartid();
	int ret;

	if (cold_boot) {
		ret = plic_cold_irqchip_init(&plic);
		if (ret)
			return ret;
	}
	return plic_ariane_warm_irqchip_init(2 * hartid, 2 * hartid + 1);
}

#define USE_APB_TIMER	3
//#define USE_SHIRI_BLOCK
//#define DO_TEST         1
#if defined(USE_APB_TIMER)
/* APB TIMER #N CMP interrupt */
#define M_TIMER_INTERRUPT	(5 + USE_APB_TIMER * 2)
#define APB_TIMER_STRIDE	0x10
#define RTC(offset) (volatile u32 *)(0x18000000 + (APB_TIMER_STRIDE * USE_APB_TIMER) + (offset))
#define RTC_TIME            0x00
#define RTC_CTL             0x04
#define RTC_CMP             0x08
static void rtc_restart(u32 value)
{
	*RTC(RTC_CMP) = *RTC(RTC_TIME) + value;
	*RTC(RTC_CTL) = 1;
}

static void rtc_stop()
{
	sbi_printf("Using APB timer %u, stride 0x%x\n", USE_APB_TIMER, APB_TIMER_STRIDE);
}
#elif defined(USE_SHIRI_BLOCK)
#define M_TIMER_INTERRUPT	16
#define RTC(offset) (volatile u32 *)(0x50000000 + offset)
#define RTC_VER             0x00
#define RTC_CMP             0x04
#define RTC_CTL             0x08
#define RTC_TIME            0x0C
#define RTC_ENABLE			0x01
#define RTC_SELECT			0x02
#define RTC_EDGE			0x04
#define RTC_LEVEL			0x00
#define RTC_START			0x08
#define RTC_INTR			RTC_LEVEL
//#define RTC_INTR			RTC_EDGE
#define RTC_STOPPED			(RTC_INTR)
#define RTC_STARTED			(RTC_INTR | RTC_START | RTC_ENABLE)
static void rtc_restart(u32 value)
{
	*RTC(RTC_CTL) = RTC_STOPPED;
	*RTC(RTC_CMP) = *RTC(RTC_TIME) + value;
	*RTC(RTC_CTL) = RTC_STARTED;
}

static void rtc_stop()
{
	sbi_printf("Using Shiri's timer, v. 0x%x\n", *RTC(RTC_VER));
	*RTC(RTC_CTL) = RTC_STOPPED;
}
#elif DO_TEST
#define M_TIMER_INTERRUPT	0
#define RTC(offset) (volatile u32 *)((unsigned long)0x18000000 + offset)
static void rtc_restart(u32 value) {}

static void test_reg(u32 offset)
{
	u32 val, addr = offset;
	sbi_printf("Testing address 0x%x\n", addr);
	val = *RTC(addr);
	sbi_printf("Read [0x%x] = %x\n", addr, val);
	addr = offset + 4;
	val = 1;
	sbi_printf("Writing [0x%x] = %x\n", addr, val);
	*RTC(addr) = val;
	val = *RTC(addr);
	sbi_printf("Read [0x%x] = %x\n", addr, val);
	addr = offset;
	val = *RTC(addr);
	sbi_printf("Read [0x%x] = %x\n", addr, val);
	val = *RTC(addr);
	sbi_printf("Read [0x%x] = %x\n", addr, val);
	val = *RTC(addr);
	sbi_printf("Read [0x%x] = %x\n", addr, val);
	addr = offset + 8;
	val = 0;
	sbi_printf("Writing [0x%x] = %x\n", addr, val);
	*RTC(addr) = val;
	val = *RTC(addr);
	sbi_printf("Read [0x%x] = %x\n", addr, val);
	addr = offset;
	val = *RTC(addr);
	sbi_printf("Read [0x%x] = %x\n", addr, val);
	addr = offset + 4;
	val = 0;
	sbi_printf("Writing [0x%x] = %x\n", addr, val);
	*RTC(addr) = val;
}

static void rtc_stop()
{
	test_reg(0x00);
	test_reg(0x0C);
	test_reg(0x10);
	test_reg(0x18);
	test_reg(0x20);
	test_reg(0x24);
	test_reg(0x28);
	test_reg(0x2C);
	test_reg(0x30);
}

#else
#endif
/*
 * Configure/query the ariane interrupt controller.
 */
int ariane_irqchip_request(irq_operation op, u32 irq_num, u32 value)
{
	u32 hartid = current_hartid();
	switch (op) {
		case IRQ_OP_SOURCE:
		{
			rtc_stop();
			return M_TIMER_INTERRUPT;
		}
		case IRQ_OP_TIMER:
			rtc_restart(value);
			return 0;
		default:
			break;
	}
	return plic_request(&plic, 2 * hartid, op, irq_num, value);
}

/*
 * Initialize IPI for current HART.
 */
static int ariane_ipi_init(bool cold_boot)
{
	int ret;

	if (cold_boot) {
		ret = clint_cold_ipi_init(&clint);
		if (ret)
			return ret;
	}

	return clint_warm_ipi_init();
}

/*
 * Initialize ariane timer for current HART.
 */
static int ariane_timer_init(bool cold_boot)
{
	int ret;

	if (cold_boot) {
		ret = clint_cold_timer_init(&clint, NULL);
		if (ret)
			return ret;
	}

	return clint_warm_timer_init();
}

/*
 * Platform descriptor.
 */
const struct sbi_platform_operations platform_ops = {
	.early_init = ariane_early_init,
	.final_init = ariane_final_init,
	.console_init = ariane_console_init,
	.console_putc = uart8250_putc,
	.console_getc = uart8250_getc,
	.irqchip_init = ariane_irqchip_init,
	.irqchip_request = ariane_irqchip_request,
	.ipi_init = ariane_ipi_init,
	.ipi_send = clint_ipi_send,
	.ipi_clear = clint_ipi_clear,
	.timer_init = ariane_timer_init,
	.timer_value = clint_timer_value,
	.timer_event_start = clint_timer_event_start,
	.timer_event_stop = clint_timer_event_stop,
};

const struct sbi_platform platform = {
	.opensbi_version = OPENSBI_VERSION,
	.platform_version = SBI_PLATFORM_VERSION(0x0, 0x01),
	.name = "ARIANE RISC-V",
	.features = SBI_PLATFORM_DEFAULT_FEATURES | SBI_PLATFORM_HAS_GPIO,
	.hart_count = ARIANE_HART_COUNT,
	.hart_stack_size = SBI_PLATFORM_DEFAULT_HART_STACK_SIZE,
	.platform_ops_addr = (unsigned long)&platform_ops
};
