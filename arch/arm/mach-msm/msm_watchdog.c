/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/pm.h>
#include <linux/mfd/pmic8058.h>
#include <linux/jiffies.h>
#include <linux/suspend.h>
#include <linux/percpu.h>
#include <linux/interrupt.h>
#include <linux/nmi.h>
#include <mach/msm_iomap.h>
#include <asm/mach-types.h>
#include <mach/scm.h>
#include <mach/socinfo.h>
#include "msm_watchdog.h"
#include "timer.h"
#ifdef CONFIG_MSM_WATCHDOG_CTX_PRINT
#include <linux/memory_alloc.h>
#include <linux/platform_data/ram_console.h>
#include <asm/bootinfo.h>
#include <asm/stacktrace.h>
#include <mach/msm_memtypes.h>
#endif /* CONFIG_MSM_WATCHDOG_CTX_PRINT */

#define MODULE_NAME "msm_watchdog"

#define TCSR_WDT_CFG	0x30

#define WDT0_RST	0x38
#define WDT0_EN		0x40
#define WDT0_STS	0x44
#define WDT0_BARK_TIME	0x4C
#define WDT0_BITE_TIME	0x5C

#define WDT_HZ		32768

static void __iomem *msm_tmr0_base;

static unsigned long delay_time;
static unsigned long bark_time;
static unsigned long long last_pet;

/*
 * On the kernel command line specify
 * msm_watchdog.enable=1 to enable the watchdog
 * By default watchdog is turned on
 */
static int enable = 1;
module_param(enable, int, 0);

/*
 * If the watchdog is enabled at bootup (enable=1),
 * the runtime_disable sysfs node at
 * /sys/module/msm_watchdog/runtime_disable
 * can be used to deactivate the watchdog.
 * This is a one-time setting. The watchdog
 * cannot be re-enabled once it is disabled.
 */
static int runtime_disable;
static DEFINE_MUTEX(disable_lock);
static int wdog_enable_set(const char *val, struct kernel_param *kp);
module_param_call(runtime_disable, wdog_enable_set, param_get_int,
			&runtime_disable, 0644);

/*
 * On the kernel command line specify msm_watchdog.appsbark=1 to handle
 * watchdog barks in Linux. By default barks are processed by the secure side.
 */
static int appsbark;
module_param(appsbark, int, 0);

/*
 * Use /sys/module/msm_watchdog/parameters/print_all_stacks
 * to control whether stacks of all running
 * processes are printed when a wdog bark is received.
 */
static int print_all_stacks = 1;
module_param(print_all_stacks, int,  S_IRUGO | S_IWUSR);

/* Area for context dump in secure mode */
static unsigned long scm_regsave;	/* phys */

static struct msm_watchdog_pdata __percpu **percpu_pdata;

static void pet_watchdog_work(struct work_struct *work);
static void init_watchdog_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(dogwork_struct, pet_watchdog_work);
static DECLARE_WORK(init_dogwork_struct, init_watchdog_work);

static int msm_watchdog_suspend(struct device *dev)
{
	if (!enable)
		return 0;

	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	__raw_writel(0, msm_tmr0_base + WDT0_EN);
	mb();
	return 0;
}

static int msm_watchdog_resume(struct device *dev)
{
	if (!enable)
		return 0;

	__raw_writel(1, msm_tmr0_base + WDT0_EN);
	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	mb();
	return 0;
}

static int panic_wdog_handler(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	if (panic_timeout == 0) {
		__raw_writel(0, msm_tmr0_base + WDT0_EN);
		mb();
	} else {
		__raw_writel(WDT_HZ * (panic_timeout + 4),
				msm_tmr0_base + WDT0_BARK_TIME);
		__raw_writel(WDT_HZ * (panic_timeout + 4),
				msm_tmr0_base + WDT0_BITE_TIME);
		__raw_writel(1, msm_tmr0_base + WDT0_RST);
	}
	return NOTIFY_DONE;
}

static int touch_nmi_wdog_handler(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	if (!enable)
		return NOTIFY_DONE;
	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call	= panic_wdog_handler,
};

static struct notifier_block touch_nmi_blk = {
	.notifier_call  = touch_nmi_wdog_handler,
};

static int wdog_enable_set(const char *val, struct kernel_param *kp)
{
	int ret = 0;
	int old_val = runtime_disable;

	mutex_lock(&disable_lock);

	if (!enable) {
		printk(KERN_INFO "MSM Watchdog is not active.\n");
		ret = -EINVAL;
		goto done;
	}

	ret = param_set_int(val, kp);

	if (ret)
		goto done;

	switch (runtime_disable) {

	case 1:
		if (!old_val) {
			__raw_writel(0, msm_tmr0_base + WDT0_EN);
			mb();
			disable_percpu_irq(WDT0_ACCSCSSNBARK_INT);
			free_percpu_irq(WDT0_ACCSCSSNBARK_INT, percpu_pdata);
			free_percpu(percpu_pdata);
			enable = 0;
			atomic_notifier_chain_unregister(&panic_notifier_list,
			       &panic_blk);
			atomic_notifier_chain_unregister(
					&touch_watchdog_notifier_head,
					&touch_nmi_blk);
			cancel_delayed_work(&dogwork_struct);
			/* may be suspended after the first write above */
			__raw_writel(0, msm_tmr0_base + WDT0_EN);
			printk(KERN_INFO "MSM Watchdog deactivated.\n");
		}
	break;

	default:
		runtime_disable = old_val;
		ret = -EINVAL;
	break;

	}

done:
	mutex_unlock(&disable_lock);
	return ret;
}

unsigned min_slack_ticks = UINT_MAX;
unsigned long long min_slack_ns = ULLONG_MAX;

void pet_watchdog(void)
{
	int slack;
	unsigned long long time_ns;
	unsigned long long slack_ns;
	unsigned long long bark_time_ns = bark_time * 1000000ULL;

	slack = __raw_readl(msm_tmr0_base + WDT0_STS) >> 3;
	slack = ((bark_time*WDT_HZ)/1000) - slack;
	if (slack < min_slack_ticks)
		min_slack_ticks = slack;
	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	time_ns = sched_clock();
	slack_ns = (last_pet + bark_time_ns) - time_ns;
	if (slack_ns < min_slack_ns)
		min_slack_ns = slack_ns;
	last_pet = time_ns;
}

static void pet_watchdog_work(struct work_struct *work)
{
	pet_watchdog();

	if (enable)
		schedule_delayed_work_on(0, &dogwork_struct, delay_time);
}

static int msm_watchdog_remove(struct platform_device *pdev)
{
	if (enable) {
		__raw_writel(0, msm_tmr0_base + WDT0_EN);
		mb();
		disable_percpu_irq(WDT0_ACCSCSSNBARK_INT);
		free_percpu_irq(WDT0_ACCSCSSNBARK_INT, percpu_pdata);
		free_percpu(percpu_pdata);
		enable = 0;
		/* In case we got suspended mid-exit */
		__raw_writel(0, msm_tmr0_base + WDT0_EN);
	}
	printk(KERN_INFO "MSM Watchdog Exit - Deactivated\n");
	return 0;
}

static irqreturn_t wdog_bark_handler(int irq, void *dev_id)
{
	unsigned long nanosec_rem;
	unsigned long long t = sched_clock();
	struct task_struct *tsk;

	nanosec_rem = do_div(t, 1000000000);
	printk(KERN_INFO "Watchdog bark! Now = %lu.%06lu\n", (unsigned long) t,
		nanosec_rem / 1000);

	nanosec_rem = do_div(last_pet, 1000000000);
	printk(KERN_INFO "Watchdog last pet at %lu.%06lu\n", (unsigned long)
		last_pet, nanosec_rem / 1000);

	if (print_all_stacks) {

		/* Suspend wdog until all stacks are printed */
		msm_watchdog_suspend(NULL);

		printk(KERN_INFO "Stack trace dump:\n");

		for_each_process(tsk) {
			printk(KERN_INFO "\nPID: %d, Name: %s\n",
				tsk->pid, tsk->comm);
			show_stack(tsk, NULL);
		}

		msm_watchdog_resume(NULL);
	}

	pr_err("Apps watchdog bark received!");
	BUG();
	return IRQ_HANDLED;
}

#define SCM_SET_REGSAVE_CMD 0x2

#ifdef CONFIG_MSM_WATCHDOG_CTX_PRINT

#define TZBSP_SC_STATUS_NS_BIT		0x01
#define TZBSP_SC_STATUS_WDT		0x02
#define TZBSP_SC_STATUS_SGI		0x04
#define TZBSP_SC_STATUS_WARM_BOOT	0x08

#define TZBSP_DUMP_CTX_MAGIC		0x44434151
#define TZBSP_DUMP_CTX_VERSION		1

#define MSM_WDT_CTX_SIG			0x77647473
#define MSM_WDT_CTX_REV			0x00010000

#define NR_CPUS_MAX			4
#define NR_CPUS_2			2
#define NR_CPUS_4			4

struct tzbsp_dump_cpu_ctx_s {
	u32 mon_lr;
	u32 mon_spsr;
	u32 usr_r0;
	u32 usr_r1;
	u32 usr_r2;
	u32 usr_r3;
	u32 usr_r4;
	u32 usr_r5;
	u32 usr_r6;
	u32 usr_r7;
	u32 usr_r8;
	u32 usr_r9;
	u32 usr_r10;
	u32 usr_r11;
	u32 usr_r12;
	u32 usr_r13;
	u32 usr_r14;
	u32 irq_spsr;
	u32 irq_r13;
	u32 irq_r14;
	u32 svc_spsr;
	u32 svc_r13;
	u32 svc_r14;
	u32 abt_spsr;
	u32 abt_r13;
	u32 abt_r14;
	u32 und_spsr;
	u32 und_r13;
	u32 und_r14;
	u32 fiq_spsr;
	u32 fiq_r8;
	u32 fiq_r9;
	u32 fiq_r10;
	u32 fiq_r11;
	u32 fiq_r12;
	u32 fiq_r13;
	u32 fiq_r14;
};

/* Structure of the entire non-secure context dump buffer. Because TZ is single
 * entry only a single secure context is saved. */
struct tzbsp_dump_buf_s0 {
	u32 magic;
	u32 version;
	u32 cpu_count;
};

struct tzbsp_dump_buf_s2 {
	u32 magic;
	u32 version;
	u32 cpu_count;
	u32 sc_status[NR_CPUS_2];
	struct tzbsp_dump_cpu_ctx_s sc_ns[NR_CPUS_2];
	struct tzbsp_dump_cpu_ctx_s sec;
	u32 wdt0_sts[NR_CPUS_2];
};

struct tzbsp_dump_buf_s4 {
	u32 magic;
	u32 version;
	u32 cpu_count;
	u32 sc_status[NR_CPUS_4];
	struct tzbsp_dump_cpu_ctx_s sc_ns[NR_CPUS_4];
	struct tzbsp_dump_cpu_ctx_s sec;
	u32 wdt0_sts[NR_CPUS_4];
};

union tzbsp_dump_buf_s {
	struct tzbsp_dump_buf_s0 s0;
	struct tzbsp_dump_buf_s2 s2;
	struct tzbsp_dump_buf_s4 s4;
};

struct msm_wdt_tz_percpu {
	u32 ret;
	u32 stack_va;
};

struct msm_wdt_tz_info {
	u32 rev;
	u32 size;
	struct msm_wdt_tz_percpu percpu[NR_CPUS_MAX];
};

struct msm_wdt_lnx_percpu {
	u32 curr_ptr_pa;
};

struct msm_wdt_lnx_info {
	u32 rev;
	u32 size;
	u32 cpu_count;
	u32 tsk_stack_offset;
	u32 tsk_size;
	u32 ti_tsk_offset;
	u32 membank1_va_start;
	u32 membank1_vapa_delta;
	u32 membank0_vapa_delta;
	struct msm_wdt_lnx_percpu percpu[NR_CPUS_MAX];
};

struct msm_wdt_dump_buf {
	struct msm_wdt_tz_info tz;
	struct msm_wdt_lnx_info lnx;
	u32 sig;
};

struct msm_wdt_dump_s {
	u8 spare[PAGE_SIZE - sizeof(struct msm_wdt_dump_buf)];
	struct msm_wdt_dump_buf buf;
};

union msm_wdt_page1 {
	u8 padding[PAGE_SIZE];
	union tzbsp_dump_buf_s tzbsp;
	struct msm_wdt_dump_s wdt;
};

struct msm_wdt_tsk {
	struct task_struct ts;
	u8 padding1[PAGE_SIZE / 4 - sizeof(struct task_struct)];
};

union msm_wdt_page2 {
	u8 padding[PAGE_SIZE];
	struct msm_wdt_tsk tsk[NR_CPUS_MAX];
};

struct msm_wdt_ctx {
	union msm_wdt_page1 p1;
	union msm_wdt_page2 p2;
};

#define MSM_WDT_CTX_SIZE	(sizeof(struct msm_wdt_ctx)	\
				+ (THREAD_SIZE * num_possible_cpus()))

void __init reserve_memory_for_watchdog(void)
{
	reserve_info->memtype_reserve_table[MEMTYPE_EBI1].size
		+= MSM_WDT_CTX_SIZE;
}

static unsigned long msm_watchdog_alloc_buf(void)
{
	BUILD_BUG_ON(sizeof(struct task_struct) > (PAGE_SIZE / 4));
	BUILD_BUG_ON((sizeof(union tzbsp_dump_buf_s)
		+ sizeof(struct msm_wdt_dump_buf)) > PAGE_SIZE);
	BUILD_BUG_ON(CONFIG_NR_CPUS > NR_CPUS_MAX);
	return allocate_contiguous_memory_nomap(MSM_WDT_CTX_SIZE,
			MEMTYPE_EBI1, PAGE_SIZE);
}

#define MSMWDTD(fmt, args...) do {		\
	ram_console_ext_oldbuf_print(fmt, ##args);	\
} while (0)

static void __init msm_wdt_show_status(u32 sc_status, const char *label)
{
	MSMWDTD("%s: was in %ssecure world.\n", label,
		(sc_status & TZBSP_SC_STATUS_NS_BIT) ? "non-" : "");
	if (sc_status & TZBSP_SC_STATUS_WDT)
		MSMWDTD("%s: experienced a watchdog timeout.\n", label);
	if (sc_status & TZBSP_SC_STATUS_SGI)
		MSMWDTD("%s: some other core experienced "
			"a watchdog timeout.\n", label);
	if (sc_status & TZBSP_SC_STATUS_WARM_BOOT)
		MSMWDTD("%s: WDT bark occured during TZ warm boot.\n", label);
}

static const char stat_nam[] = TASK_STATE_TO_CHAR_STR;
static void __init msm_wdt_show_task(struct task_struct *p,
			struct thread_info *ti)
{
	unsigned state;

	state = p->state ? __ffs(p->state) + 1 : 0;
	MSMWDTD("%-15.15s %c", p->comm,
		state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
	if (state == TASK_RUNNING)
		MSMWDTD(" running  ");
	else
		MSMWDTD(" %08lx ", ti->cpu_context.pc);
	MSMWDTD("pid %6d tgid %6d 0x%08lx\n", task_pid_nr(p), task_tgid_nr(p),
			(unsigned long)ti->flags);
}

static void __init msm_wdt_show_regs(struct tzbsp_dump_cpu_ctx_s *regs,
				const char *label)
{
	MSMWDTD("%s\n", label);
	MSMWDTD("\tr12: %08lx  r11: %08lx  r10: %08lx  "
			"r9 : %08lx  r8 : %08lx\n",
			regs->usr_r12, regs->usr_r11, regs->usr_r10,
			regs->usr_r9, regs->usr_r8);
	MSMWDTD("\tr7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
			regs->usr_r7, regs->usr_r6, regs->usr_r5,
			regs->usr_r4);
	MSMWDTD("\tr3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
			regs->usr_r3, regs->usr_r2, regs->usr_r1,
			regs->usr_r0);
	MSMWDTD("MON:\tlr : %08lx  spsr: %08lx\n",
			regs->mon_lr, regs->mon_spsr);
	MSMWDTD("SVC:\tlr : %08lx  sp : %08lx  spsr : %08lx\n",
			regs->svc_r14, regs->svc_r13, regs->svc_spsr);
	MSMWDTD("IRQ:\tlr : %08lx  sp : %08lx  spsr : %08lx\n",
			regs->irq_r14, regs->irq_r13, regs->irq_spsr);
	MSMWDTD("ABT:\tlr : %08lx  sp : %08lx  spsr : %08lx\n",
			regs->abt_r14, regs->abt_r13, regs->abt_spsr);
	MSMWDTD("UND:\tlr : %08lx  sp : %08lx  spsr : %08lx\n",
			regs->und_r14, regs->und_r13, regs->und_spsr);
	MSMWDTD("FIQ:\tlr : %08lx  sp : %08lx  spsr : %08lx\n",
			regs->fiq_r14, regs->fiq_r13, regs->fiq_spsr);
	MSMWDTD("\tr12: %08lx  r11: %08lx  r10: %08lx  "
			"r9 : %08lx  r8 : %08lx\n",
			regs->fiq_r12, regs->fiq_r11, regs->fiq_r10,
			regs->fiq_r9, regs->fiq_r8);
	MSMWDTD("USR:\tlr : %08lx  sp : %08lx\n",
			regs->usr_r14, regs->usr_r13);
}

static void __init msm_wdt_show_raw_mem(unsigned long addr, int nbytes,
			unsigned long old_addr, const char *label)
{
	int	i, j;
	int	nlines;
	u32	*p;

	MSMWDTD("%s: %#lx: ", label, old_addr);
	if (!virt_addr_valid(old_addr)) {
		MSMWDTD("is not valid kernel address.\n");
		return;
	}
	MSMWDTD("\n");

	/*
	 * round address down to a 32 bit boundary
	 * and always dump a multiple of 32 bytes
	 */
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;


	for (i = 0; i < nlines; i++) {
		/*
		 * just display low 16 bits of address to keep
		 * each line of the dump < 80 characters
		 */
		MSMWDTD("%04lx ", (unsigned long)old_addr & 0xffff);
		for (j = 0; j < 8; j++) {
			MSMWDTD(" %08x", *p);
			++p;
			old_addr += sizeof(*p);
		}
		MSMWDTD("\n");
	}
}

static void __init msm_wdt_unwind_backtrace(struct tzbsp_dump_cpu_ctx_s *regs,
			unsigned long addr, unsigned long stack)
{
	struct stackframe frame;
	int offset;

	if (!virt_addr_valid(addr)) {
		MSMWDTD("%08x is not valid kernel address.\n", addr);
		return;
	}
	if ((regs->svc_r13 & ~(THREAD_SIZE - 1)) == addr) {
		frame.fp = (regs->usr_r11 & (THREAD_SIZE - 1)) + stack;
		frame.sp = (regs->svc_r13 & (THREAD_SIZE - 1)) + stack;
		frame.lr = regs->svc_r14;
		frame.pc = regs->mon_lr;
	} else {
		struct thread_info *ti = (struct thread_info *)stack;
		frame.fp = ti->cpu_context.fp - addr + stack;
		frame.sp = ti->cpu_context.sp - addr + stack;
		frame.lr = 0;
		frame.pc = ti->cpu_context.pc;
	}
	offset = (frame.sp - stack - 128) & ~(128 - 1);
	msm_wdt_show_raw_mem(stack, 96, addr, "thread_info");
	msm_wdt_show_raw_mem(stack + offset, THREAD_SIZE - offset,
			addr + offset, "stack");
	while (1) {
		int urc;
		unsigned long where = frame.pc;

		urc = unwind_frame(&frame);
		if (urc < 0)
			break;
		MSMWDTD("[<%08lx>] (%pS) from [<%08lx>] (%pS)\n", where,
			(void *)where, frame.pc, (void *)frame.pc);
	}
}

static void __init msm_wdt_ctx_print(struct msm_wdt_ctx *ctx)
{
	int i;
	unsigned long stack_tmp = 0, mon_lr = 0;
	u32 *sc_status, *wdt0_sts;
	struct tzbsp_dump_cpu_ctx_s *sc_ns;
	char label[64];
	u8 (*stack)[THREAD_SIZE];

	if ((ctx->p1.tzbsp.s0.magic != TZBSP_DUMP_CTX_MAGIC)
		|| (ctx->p1.tzbsp.s0.version != TZBSP_DUMP_CTX_VERSION)
		|| (ctx->p1.tzbsp.s0.cpu_count != num_possible_cpus())) {
		MSMWDTD("msm_wdt_ctx: tzbsp dump buffer mismatch.\n");
		MSMWDTD("Expected: magic 0x%08x, version %d, cpu_count %d\n",
			TZBSP_DUMP_CTX_MAGIC, TZBSP_DUMP_CTX_VERSION,
			num_possible_cpus());
		MSMWDTD("Found:    magic 0x%08x, version %d, cpu_count %d\n",
			ctx->p1.tzbsp.s0.magic, ctx->p1.tzbsp.s0.version,
			ctx->p1.tzbsp.s0.cpu_count);
		return;
	}
	switch (num_possible_cpus()) {
	case NR_CPUS_2:
		sc_status = ctx->p1.tzbsp.s2.sc_status;
		sc_ns = ctx->p1.tzbsp.s2.sc_ns;
		wdt0_sts = ctx->p1.tzbsp.s2.wdt0_sts;
		break;
	case NR_CPUS_4:
		sc_status = ctx->p1.tzbsp.s4.sc_status;
		sc_ns = ctx->p1.tzbsp.s4.sc_ns;
		wdt0_sts = ctx->p1.tzbsp.s4.wdt0_sts;
		break;
	default:
		MSMWDTD("msm_wdt_ctx: unsupported cpu_count %d\n",
			num_possible_cpus());
		return;
	}
	for (i = 0; i < num_possible_cpus(); i++)
		mon_lr |= sc_ns[i].mon_lr;
	if (!mon_lr) {
		if (bi_powerup_reason() == PU_REASON_WDOG_AP_RESET)
			MSMWDTD("*** LIKELY BAD DUMP ***\n");
		else
			return;
	}
	MSMWDTD("sc_status: ");
	for (i = 0; i < num_possible_cpus(); i++)
		MSMWDTD("0x%x ", sc_status[i]);
	MSMWDTD("\n");
	for (i = 0; i < num_possible_cpus(); i++) {
		snprintf(label, sizeof(label) - 1, "CPU%d", i);
		msm_wdt_show_status(sc_status[i], label);
	}
	MSMWDTD("wdt0_sts: ");
	for (i = 0; i < num_possible_cpus(); i++)
		MSMWDTD("0x%08x ", wdt0_sts[i]);
	MSMWDTD("\n");
	for (i = 0; i < num_possible_cpus(); i++) {
		snprintf(label, sizeof(label) - 1, "CPU%d nsec", i);
		msm_wdt_show_regs(&sc_ns[i], label);
	}
	msm_wdt_show_regs(&sc_ns[num_possible_cpus()], "sec");
	if ((ctx->p1.wdt.buf.sig != MSM_WDT_CTX_SIG)
		|| (ctx->p1.wdt.buf.tz.rev != MSM_WDT_CTX_REV)
		|| (ctx->p1.wdt.buf.tz.size != MSM_WDT_CTX_SIZE)) {
		MSMWDTD("msm_wdt_ctx: linux dump buffer mismatch.\n");
		MSMWDTD("Expected: sig 0x%08x, ver 0x%08x size 0x%08x\n",
			MSM_WDT_CTX_SIG, MSM_WDT_CTX_REV, MSM_WDT_CTX_SIZE);
		MSMWDTD("Found:    sig 0x%08x, ver 0x%08x size 0x%08x\n",
			ctx->p1.wdt.buf.sig, ctx->p1.wdt.buf.tz.rev,
			ctx->p1.wdt.buf.tz.size);
		return;
	}
	/* now dump customized stuff */
	MSMWDTD("\n");
	for (i = 0; i < num_possible_cpus(); i++) {
		MSMWDTD("CPU%d: ", i);
		MSMWDTD("ret 0x%x ", ctx->p1.wdt.buf.tz.percpu[i].ret);
		MSMWDTD("stack %08x ", ctx->p1.wdt.buf.tz.percpu[i].stack_va);
		MSMWDTD("\n");
	}
	MSMWDTD("\n");
	stack = (u8 (*)[THREAD_SIZE])(ctx + 1);
	if (!IS_ALIGNED((unsigned long)stack, THREAD_SIZE)) {
		stack_tmp = __get_free_pages(GFP_KERNEL, THREAD_SIZE_ORDER);
		if (!stack_tmp)
			pr_err("Allocating temporary storage for stack failed.\n");
	}
	for (i = 0; i < num_possible_cpus(); i++) {
		unsigned long addr, data;

		MSMWDTD("CPU%d: ", i);
		addr = ctx->p1.wdt.buf.tz.percpu[i].stack_va;
		if (stack_tmp) {
			memcpy((void *)stack_tmp, stack[i], THREAD_SIZE);
			data = stack_tmp;
		} else {
			data = (unsigned long)stack[i];
		}
		msm_wdt_show_task(&ctx->p2.tsk[i].ts,
				(struct thread_info *)data);
		msm_wdt_unwind_backtrace(&sc_ns[i], addr, data);
		MSMWDTD("\n");
	}
	if (stack_tmp)
		free_pages(stack_tmp, THREAD_SIZE_ORDER);
}

#ifdef CONFIG_DONT_MAP_HOLE_AFTER_MEMBANK0
static void __init msm_watchdog_ctx_lnx(struct msm_wdt_lnx_info *lnx)
{
	int i;
	lnx->tsk_stack_offset = offsetof(struct task_struct, stack);
	lnx->tsk_size = sizeof(struct task_struct);
	lnx->ti_tsk_offset = offsetof(struct thread_info, task);
	for_each_possible_cpu(i)
		lnx->percpu[i].curr_ptr_pa = __pa(cpu_curr_ptr_addr(i));
	lnx->membank1_va_start = MEMBANK1_PAGE_OFFSET;
	lnx->membank1_vapa_delta = (MEMBANK1_PAGE_OFFSET)
					- (MEMBANK1_PHYS_OFFSET);
	lnx->membank0_vapa_delta = (MEMBANK0_PAGE_OFFSET)
					- (MEMBANK0_PHYS_OFFSET);
}
#else
static void __init msm_watchdog_ctx_lnx(struct msm_wdt_lnx_info *lnx)
{
	int i;
	lnx->tsk_stack_offset = offsetof(struct task_struct, stack);
	lnx->tsk_size = sizeof(struct task_struct);
	lnx->ti_tsk_offset = offsetof(struct thread_info, task);
	for_each_possible_cpu(i)
		lnx->percpu[i].curr_ptr_pa = __pa(cpu_curr_ptr_addr(i));
	lnx->membank1_va_start = ULONG_MAX;
	lnx->membank1_vapa_delta = (PAGE_OFFSET) - (PHYS_OFFSET);
	lnx->membank0_vapa_delta = (PAGE_OFFSET) - (PHYS_OFFSET);
}
#endif

static void __init msm_wdt_ctx_reset(struct msm_wdt_ctx *ctx)
{
	memset(ctx, 0, MSM_WDT_CTX_SIZE);
	ctx->p1.wdt.buf.sig = MSM_WDT_CTX_SIG;
	ctx->p1.wdt.buf.lnx.rev = MSM_WDT_CTX_REV;
	ctx->p1.wdt.buf.lnx.size = MSM_WDT_CTX_SIZE;
	ctx->p1.wdt.buf.lnx.cpu_count = num_possible_cpus();
	msm_watchdog_ctx_lnx(&ctx->p1.wdt.buf.lnx);
}

#else /* CONFIG_MSM_WATCHDOG_CTX_PRINT undefined below */

#define MSM_WDT_CTX_SIZE	PAGE_SIZE

static unsigned long msm_watchdog_alloc_buf(void)
{
	unsigned long ctx_va;

	ctx_va = __get_free_page(GFP_KERNEL);
	if (ctx_va)
		return __pa(ctx_va);
	else
		return 0;
}
#endif /* CONFIG_MSM_WATCHDOG_CTX_PRINT */

static void configure_bark_dump(void)
{
	int ret;
	struct {
		unsigned addr;
		int len;
	} cmd_buf;

	if (!appsbark) {
		if (!scm_regsave)
			scm_regsave = msm_watchdog_alloc_buf();

		if (scm_regsave) {
			cmd_buf.addr = scm_regsave;
			cmd_buf.len  = MSM_WDT_CTX_SIZE;

			ret = scm_call(SCM_SVC_UTIL, SCM_SET_REGSAVE_CMD,
				       &cmd_buf, sizeof(cmd_buf), NULL, 0);
			if (ret)
				pr_err("Setting register save address failed.\n"
				       "Registers won't be dumped on a dog "
				       "bite\n");
		} else {
			pr_err("Allocating register save space failed\n"
			       "Registers won't be dumped on a dog bite\n");
			/*
			 * No need to bail if allocation fails. Simply don't
			 * send the command, and the secure side will reset
			 * without saving registers.
			 */
		}
	}
}

static void init_watchdog_work(struct work_struct *work)
{
	u64 timeout = (bark_time * WDT_HZ)/1000;

	configure_bark_dump();

	__raw_writel(timeout, msm_tmr0_base + WDT0_BARK_TIME);
	__raw_writel(timeout + 3*WDT_HZ, msm_tmr0_base + WDT0_BITE_TIME);

	schedule_delayed_work_on(0, &dogwork_struct, delay_time);

	atomic_notifier_chain_register(&panic_notifier_list,
				       &panic_blk);

	__raw_writel(1, msm_tmr0_base + WDT0_EN);
	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	last_pet = sched_clock();
	atomic_notifier_chain_register(&touch_watchdog_notifier_head,
			&touch_nmi_blk);

	printk(KERN_INFO "MSM Watchdog Initialized\n");

	return;
}

static int msm_watchdog_probe(struct platform_device *pdev)
{
	struct msm_watchdog_pdata *pdata = pdev->dev.platform_data;
	int ret;

	if (!enable || !pdata || !pdata->pet_time || !pdata->bark_time) {
		printk(KERN_INFO "MSM Watchdog Not Initialized\n");
		return -ENODEV;
	}

	if (!pdata->has_secure)
		appsbark = 1;

	bark_time = pdata->bark_time;

	msm_tmr0_base = msm_timer_get_timer0_base();

	percpu_pdata = alloc_percpu(struct msm_watchdog_pdata *);
	if (!percpu_pdata) {
		pr_err("%s: memory allocation failed for percpu data\n",
				__func__);
		return -ENOMEM;
	}

	*__this_cpu_ptr(percpu_pdata) = pdata;
	/* Must request irq before sending scm command */
	ret = request_percpu_irq(WDT0_ACCSCSSNBARK_INT, wdog_bark_handler,
			  "apps_wdog_bark", percpu_pdata);
	if (ret) {
		free_percpu(percpu_pdata);
		return ret;
	}

	enable_percpu_irq(WDT0_ACCSCSSNBARK_INT, 0);

	/*
	 * This is only temporary till SBLs turn on the XPUs
	 * This initialization will be done in SBLs on a later releases
	 */
	if (cpu_is_msm9615())
		__raw_writel(0xF, MSM_TCSR_BASE + TCSR_WDT_CFG);

	delay_time = msecs_to_jiffies(pdata->pet_time);
	schedule_work_on(0, &init_dogwork_struct);
	return 0;
}

static const struct dev_pm_ops msm_watchdog_dev_pm_ops = {
	.suspend_noirq = msm_watchdog_suspend,
	.resume_noirq = msm_watchdog_resume,
};

static struct platform_driver msm_watchdog_driver = {
	.probe = msm_watchdog_probe,
	.remove = msm_watchdog_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.pm = &msm_watchdog_dev_pm_ops,
	},
};

static int init_watchdog(void)
{
	return platform_driver_register(&msm_watchdog_driver);
}

#ifdef CONFIG_MSM_WATCHDOG_CTX_PRINT
static int __init init_watchdog_buf_early(void)
{
	struct msm_wdt_ctx *ctx;
	scm_regsave = msm_watchdog_alloc_buf();
	if (scm_regsave) {
		ctx = ioremap(scm_regsave, MSM_WDT_CTX_SIZE);
		if (!ctx) {
			pr_err("msm_watchdog: cannot remap buffer: "
				"%08lx\n", scm_regsave);
			return -EFAULT;
		}
		msm_wdt_ctx_print(ctx);
		msm_wdt_ctx_reset(ctx);
		iounmap(ctx);
	}
	return 0;
}

core_initcall(init_watchdog_buf_early);
#endif /* CONFIG_MSM_WATCHDOG_CTX_PRINT */

late_initcall(init_watchdog);
MODULE_DESCRIPTION("MSM Watchdog Driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
