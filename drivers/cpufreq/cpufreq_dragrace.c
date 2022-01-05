// SPDX-License-Identifier: GPL-2.0-only
/*
 *  linux/drivers/cpufreq/cpufreq_dragrace.c
 *
*/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/msm_drm_notify.h>
#include <linux/cpu.h>
#include <linux/percpu-defs.h>
#include <linux/slab.h>
#include <linux/tick.h>
#include <linux/sched/cpufreq.h>
#include <linux/mutex.h>

static DEFINE_PER_CPU(unsigned int, cpu_is_managed);
static DEFINE_MUTEX(dragrace_mutex);

static bool screen_on = true;

extern void update_screen_status(bool on)
{
	screen_on = on;
}

static int msm_drm_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	printk("Display notifier callback\n");
	struct msm_drm_notifier *evdata = data;

	int *blank;
	if (event != MSM_DRM_EVENT_BLANK && event != MSM_DRM_EARLY_EVENT_BLANK)
		printk("Non compatible event\n");
		goto out;

	if (!evdata || !evdata->data || evdata->id != MSM_DRM_PRIMARY_DISPLAY)
		printk("Not primary displayx\n");
		goto out;

	blank = evdata->data;
	switch (*blank) {
	case MSM_DRM_BLANK_POWERDOWN:
		update_screen_status(false);
		break;
	case MSM_DRM_BLANK_UNBLANK:
		update_screen_status(true);
		break;
	}

out:
	return NOTIFY_OK;
}

static struct notifier_block fb_notifier_block = {
	.notifier_call = msm_drm_notifier_callback,
};

static int cpufreq_set(struct cpufreq_policy *policy, unsigned int freq)
{
	int ret = -EINVAL;
	unsigned int *setspeed = policy->governor_data;
	*setspeed = freq;

	pr_debug("cpufreq_set for cpu %u, freq %u kHz\n", policy->cpu, freq);

	mutex_lock(&dragrace_mutex);
	if (!per_cpu(cpu_is_managed, policy->cpu))
		goto err;

	ret = __cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_L);
 err:
	printk("error setting cpu freq\n");
	mutex_unlock(&dragrace_mutex);
	return ret;
}

static ssize_t show_speed(struct cpufreq_policy *policy, char *buf)
{
	return sprintf(buf, "%u\n", policy->cur);
}

static int cpufreq_dragrace_policy_init(struct cpufreq_policy *policy)
{
	printk("DRAGRACE: init dragrace scheduler\n");
	msm_drm_register_client(&fb_notifier_block);
	unsigned int *setspeed;

	setspeed = kzalloc(sizeof(*setspeed), GFP_KERNEL);
	if (!setspeed)
		return -ENOMEM;

	policy->governor_data = setspeed;
	return 0;
}

static void cpufreq_dragrace_policy_exit(struct cpufreq_policy *policy)
{
	mutex_lock(&dragrace_mutex);
	kfree(policy->governor_data);
	policy->governor_data = NULL;
	mutex_unlock(&dragrace_mutex);
}

static int cpufreq_dragrace_policy_start(struct cpufreq_policy *policy)
{
	unsigned int *setspeed = policy->governor_data;

	BUG_ON(!policy->cur);
	pr_debug("started managing cpu %u\n", policy->cpu);

	mutex_lock(&dragrace_mutex);
	per_cpu(cpu_is_managed, policy->cpu) = 1;
	*setspeed = policy->cur;
	mutex_unlock(&dragrace_mutex);
	return 0;
}

static void cpufreq_dragrace_policy_stop(struct cpufreq_policy *policy)
{
	unsigned int *setspeed = policy->governor_data;

	pr_debug("managing cpu %u stopped\n", policy->cpu);

	mutex_lock(&dragrace_mutex);
	per_cpu(cpu_is_managed, policy->cpu) = 0;
	*setspeed = 0;
	mutex_unlock(&dragrace_mutex);
}

static void cpufreq_dragrace_policy_limits(struct cpufreq_policy *policy)
{
	unsigned int *setspeed = policy->governor_data;

	mutex_lock(&dragrace_mutex);

	pr_debug("limit event for cpu %u: %u - %u kHz, currently %u kHz, last set to %u kHz\n",
		 policy->cpu, policy->min, policy->max, policy->cur, *setspeed);

	// if (policy->max < *setspeed)
	if (screen_on) {
		printk("Setting freq high\n");
		__cpufreq_driver_target(policy, policy->max, CPUFREQ_RELATION_H);		
	}
	// else if (policy->min > *setspeed)
	// {
	// __cpufreq_driver_target(policy, policy->min, CPUFREQ_RELATION_L);
	// }
	else {
		printk("Setting freq low\n");
		__cpufreq_driver_target(policy, policy->max, CPUFREQ_RELATION_L);
	}


	mutex_unlock(&dragrace_mutex);
}

static struct cpufreq_governor cpufreq_gov_dragrace = {
	.name		= "dragrace",
	.owner		= THIS_MODULE,
	.init		= cpufreq_dragrace_policy_init,
	.exit		= cpufreq_dragrace_policy_exit,
	.start		= cpufreq_dragrace_policy_start,
	.stop		= cpufreq_dragrace_policy_stop,
	.limits		= cpufreq_dragrace_policy_limits,
	.store_setspeed	= cpufreq_set,
	.show_setspeed	= show_speed,
};

static int __init cpufreq_gov_dragrace_init(void)
{
	msm_drm_register_client(&fb_notifier_block);
	return cpufreq_register_governor(&cpufreq_gov_dragrace);
}

static void __exit cpufreq_gov_dragrace_exit(void)
{
	msm_drm_unregister_client(&fb_notifier_block);
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