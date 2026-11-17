/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * AVR 7.3 port: android16-6.12 ASoC names used by the canoe audio-kernel.
 * - snd_soc_kcontrol_component(k) was snd_kcontrol_chip(k) (component-registered
 *   controls keep the component as private_data in 7.3)
 * - snd_soc_dapm_kcontrol_widget -> snd_soc_dapm_kcontrol_to_widget
 * - snd_soc_component_get_dapm -> snd_soc_component_to_dapm (dapm is now a pointer)
 * - SND_SOC_DAIFMT_CBS_CFS -> SND_SOC_DAIFMT_CBC_CFC (same value meaning)
 * - PIN_CONFIG_OUTPUT -> PIN_CONFIG_LEVEL (output + level, same semantics)
 */
#ifndef AVR_AUDIO_COMPAT_H
#define AVR_AUDIO_COMPAT_H
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/pinctrl/pinconf-generic.h>

#define snd_soc_kcontrol_component(kcontrol) \
	((struct snd_soc_component *)snd_kcontrol_chip(kcontrol))
#define snd_soc_dapm_kcontrol_widget	snd_soc_dapm_kcontrol_to_widget
#define snd_soc_component_get_dapm	snd_soc_component_to_dapm
#define SND_SOC_DAIFMT_CBS_CFS		SND_SOC_DAIFMT_CBC_CFC
#define PIN_CONFIG_OUTPUT		PIN_CONFIG_LEVEL

/* 6.12 inline: bias level of the component's DAPM context */
#define snd_soc_component_get_bias_level(component) \
	snd_soc_dapm_get_bias_level(snd_soc_component_to_dapm(component))

/* irq_domain_add_linear() was removed; same domain from the OF node's fwnode */
#include <linux/irqdomain.h>
#include <linux/of.h>
static inline struct irq_domain *avr_irq_domain_add_linear(struct device_node *np,
		unsigned int size, const struct irq_domain_ops *ops, void *host_data)
{
	/* 6.12: of_node_to_fwnode(NULL) == NULL as well */
	return irq_domain_create_linear(np ? of_fwnode_handle(np) : NULL, size, ops, host_data);
}
#define irq_domain_add_linear avr_irq_domain_add_linear

#endif
