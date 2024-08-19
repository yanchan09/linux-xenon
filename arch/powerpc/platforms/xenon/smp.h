/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef XENON_SMP_H
#define XENON_SMP_H

extern int boot_cpuid;

#ifdef CONFIG_SMP
extern void smp_init_xenon(void);
#endif

#endif
