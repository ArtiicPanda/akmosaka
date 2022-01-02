// SPDX-License-Identifier: GPL-2.0-only
/*
 *  linux/drivers/cpufreq/cpufreq_dragrace.c
 *
*/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/module.h>

static void cpufreq_gov_dragrace_limits(struct cpufreq_policy *policy)
{
	pr_debug("setting to %u kHz\n", policy->max);
	__cpufreq_driver_target(policy, policy->max, CPUFREQ_RELATION_H);
}

static struct cpufreq_governor cpufreq_gov_dragrace = {
	.name		= "dragrace",
	.owner		= THIS_MODULE,
	.limits		= cpufreq_gov_dragrace_limits,
};

static int __init cpufreq_gov_dragrace_init(void)
{
	return cpufreq_register_governor(&cpufreq_gov_dragrace);
}

static void __exit cpufreq_gov_dragrace_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_dragrace);
}

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_DRAGRACE
struct cpufreq_governor *cpufreq_default_governor(void)
{
	return &cpufreq_gov_dragrace;
}
#endif

MODULE_AUTHOR("Artiic Panda <artiicpandacodes@gmail.com>");
MODULE_DESCRIPTION("CPUfreq policy governor 'dragrace'");
MODULE_LICENSE("GPL");

core_initcall(cpufreq_gov_dragrace_init);
module_exit(cpufreq_gov_dragrace_exit);
