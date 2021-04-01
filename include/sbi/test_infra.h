#ifndef TEST_INFRA_H
#define TEST_INFRA_H

void test_infra_init(struct sbi_scratch *scratch, u32 hartid);
void test_infra_process_irq(void);

#endif
