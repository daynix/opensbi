
#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_string.h>
#include <sbi/test_infra.h>

u32 __attribute__((weak))  external_source = 25;

static struct
{
	u64 last_timestamp;
	u64 expired;
	u32 counter;
	u32 rounds;
	u32 dip_data;
	bool enable;
	bool do_measurement;
	u64 perf_counter_val;
} test_data = {};

#define NOF_REGISTERS	15

typedef u64 (*csr_reader)(void);
typedef struct perf_reader_entry
{
	const char *name;
	csr_reader  reader;
	u32         csr_no;
} perf_reader_entry;

#define __readername(r) reader__##r
#define __reader(r) static u64 __readername(r)() { return csr_read64(CSR_##r); }

__reader(MINSTRET)
__reader(MHPMCOUNTER3)
__reader(MHPMCOUNTER4)
__reader(MHPMCOUNTER5)
__reader(MHPMCOUNTER6)
__reader(MHPMCOUNTER7)
__reader(MHPMCOUNTER8)
__reader(MHPMCOUNTER9)
__reader(MHPMCOUNTER10)
__reader(MHPMCOUNTER11)
__reader(MHPMCOUNTER12)
__reader(MHPMCOUNTER13)
__reader(MHPMCOUNTER14)
__reader(MHPMCOUNTER15)
__reader(MHPMCOUNTER16)

#define PERF_ENTRY(name, csr) { name,  __readername(csr), CSR_## csr }

static const perf_reader_entry perf_table[NOF_REGISTERS] =
{
    PERF_ENTRY("Instructions retrieved", MINSTRET),
    PERF_ENTRY("L1 Instr Cache Miss", MHPMCOUNTER3),
    PERF_ENTRY("L1 Data Cache Miss",  MHPMCOUNTER4),
    PERF_ENTRY("ITLB Miss",  MHPMCOUNTER5),
    PERF_ENTRY("DTLB Miss",  MHPMCOUNTER6),
    PERF_ENTRY("Loads",  MHPMCOUNTER7),
    PERF_ENTRY("Stores",  MHPMCOUNTER8),
    PERF_ENTRY("Taken exceptions",  MHPMCOUNTER9),
    PERF_ENTRY("Exception return",  MHPMCOUNTER10),
    PERF_ENTRY("Software change of PC",  MHPMCOUNTER11),
    PERF_ENTRY("Procedure call",  MHPMCOUNTER12),
    PERF_ENTRY("Procedure Return",  MHPMCOUNTER13),
    PERF_ENTRY("Branch mis-predicted",  MHPMCOUNTER14),
    PERF_ENTRY("Scoreboard full",  MHPMCOUNTER15),
    PERF_ENTRY("Instruction fetch queue empty",  MHPMCOUNTER16),
};

static void do_measurement()
{
	u32 dip = *(volatile u32 *)0x40000008;
	const perf_reader_entry *re;
	if (dip != test_data.dip_data) {
		u32 idx = dip & 0x7F;
		if (idx < NOF_REGISTERS) {
			re = perf_table + idx;
			if ((dip & 0x80) && (test_data.dip_data & 0x80) == 0) {
				// left DIP is switched ON
				dip |= 0x80000000;
				test_data.perf_counter_val = re->reader();
			} else if (test_data.dip_data & 0x80000000) {
				// next timer interrupt after left DIP on
				u64 val = re->reader();
				val -= test_data.perf_counter_val; 
				sbi_printf("%s = %lu\n", re->name, val);
			}
		}
		test_data.dip_data = dip;
	}
}

void test_infra_process_timer(void)
{
	u64 current, delta, expired_high;
    if (!test_data.enable)
        return;

    current = csr_read64(CSR_MCYCLE);
	delta = current - test_data.last_timestamp;
	test_data.counter++;
	test_data.expired += delta;
	expired_high = test_data.expired >> 32;
	if (expired_high) {
		test_data.expired &= 0xffffffff;
		test_data.rounds += expired_high;
		if (expired_high > 1) {
			sbi_printf("%s: Missed round %u@%u\n", __func__, test_data.counter, test_data.rounds);
		} else if (test_data.dip_data & 0x40) {
			sbi_printf("%s: %u@%u\n", __func__, test_data.counter, test_data.rounds);
        }
		test_data.counter = 0;
	}
	test_data.last_timestamp = current;
	if (test_data.do_measurement) {
		do_measurement();
	}
}

void test_infra_init(struct sbi_scratch *scratch, u32 hartid)
{
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);
    test_data.enable = !sbi_strcmp("ARIANE RISC-V", plat->name);
	sbi_printf("%s: %sabled for %s\n", __func__, test_data.enable ? "en" : "dis", plat->name);

	sbi_platform_irqchip_request(plat, IRQ_OP_PRIORITY, external_source, 1);
	sbi_platform_irqchip_request(plat, IRQ_OP_THRESHOLD, external_source, 0);
	sbi_platform_irqchip_request(plat, IRQ_OP_ENABLE, external_source, 1);
}

void test_infra_start(void)
{
	if (test_data.enable && !test_data.counter && !test_data.rounds) {

		test_data.do_measurement = 
			sbi_platform_get_features(sbi_platform_thishart_ptr()) & SBI_PLATFORM_HAS_GPIO;
		sbi_printf("%s, test data %p\n", __func__, &test_data);
	}
}

void test_infra_process_irq(void)
{
	const struct sbi_platform *plat = sbi_platform_thishart_ptr();
	u32 irq = sbi_platform_irqchip_request(plat, IRQ_OP_CLAIM, 0, 0);
	if (irq) {
		sbi_printf("%s, irq %u\n", __func__, irq);
		sbi_platform_irqchip_request(plat, IRQ_OP_COMPLETE, irq, 0);
	} else {
		sbi_printf("%s, no irq\n", __func__);
	}
}
