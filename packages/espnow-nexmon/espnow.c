// SPDX-License-Identifier: GPL-2.0
/*
 * espnow: ESP-NOW over a radiotap monitor interface (Nexmon brcmfmac mon0).
 *
 * Receive-only. Creates espnow0, which delivers each ESP-NOW frame heard on
 * the parent as an Ethernet-shaped frame (dst, src, ethertype 0x88B5,
 * payload). Sending is refused: injecting on the Nexmon 43455 firmware
 * crashes it (https://github.com/seemoo-lab/nexmon/issues/335).
 * See docs/espnow.md.
 */
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/unaligned.h>
#include <net/cfg80211.h>
#include "espnow_frame.h"

static char *parent = "mon0";
module_param(parent, charp, 0444);
MODULE_PARM_DESC(parent, "radiotap monitor interface to attach to");

struct espnow_priv {
	struct net_device __rcu *parent;
};

static struct net_device *espnow_dev;

static rx_handler_result_t espnow_rx(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb, *nskb;
	struct net_device *dev = rcu_dereference(skb->dev->rx_handler_data);
	unsigned int hlen, rtlen;
	struct ethhdr *eth;
	const u8 *f;
	int n;

	/* taps may hold references; linearize so the parser sees the whole frame */
	skb = skb_share_check(skb, GFP_ATOMIC);
	if (!skb)
		return RX_HANDLER_CONSUMED;
	if (skb_linearize(skb))
		goto out;
	/* data may be past the radiotap header here; start where AF_PACKET does */
	if (skb_mac_header_was_set(skb) && skb->data > skb_mac_header(skb))
		skb_push(skb, skb->data - skb_mac_header(skb));
	hlen = skb->len;
	if (hlen < 4)
		goto out;
	rtlen = get_unaligned_le16(skb->data + 2);
	if (rtlen > hlen)
		goto out;
	f = skb->data + rtlen;
	n = espnow_parse(f, hlen - rtlen, dev->dev_addr, NULL);
	if (n < 0)
		goto out;

	nskb = netdev_alloc_skb(dev, ETH_HLEN + n);
	if (!nskb) {
		DEV_STATS_INC(dev, rx_dropped);
		goto out;
	}
	eth = skb_put(nskb, ETH_HLEN);
	ether_addr_copy(eth->h_dest, f + 4);
	ether_addr_copy(eth->h_source, f + 10);
	eth->h_proto = htons(ESPNOW_ETHERTYPE);
	espnow_parse(f, hlen - rtlen, dev->dev_addr, skb_put(nskb, n));
	nskb->protocol = eth_type_trans(nskb, dev);
	DEV_STATS_INC(dev, rx_packets);
	DEV_STATS_ADD(dev, rx_bytes, n);
	netif_rx(nskb);
out:
	/* we own the monitor interface; nothing else needs its frames */
	consume_skb(skb);
	return RX_HANDLER_CONSUMED;
}

static netdev_tx_t espnow_xmit(struct sk_buff *skb, struct net_device *dev)
{
	/* receive-only, see top of file */
	DEV_STATS_INC(dev, tx_dropped);
	kfree_skb(skb);
	return NETDEV_TX_OK;
}

static const struct net_device_ops espnow_ops = {
	.ndo_start_xmit = espnow_xmit,
	.ndo_validate_addr = eth_validate_addr,
};

/* called under rtnl */
static void espnow_attach(struct net_device *pdev)
{
	struct espnow_priv *priv = netdev_priv(espnow_dev);

	if (pdev->type != ARPHRD_IEEE80211_RADIOTAP) {
		netdev_warn(espnow_dev, "%s is not a radiotap monitor interface\n", pdev->name);
		return;
	}
	if (netdev_rx_handler_register(pdev, espnow_rx, espnow_dev)) {
		netdev_warn(espnow_dev, "%s already has an rx handler\n", pdev->name);
		return;
	}
	dev_hold(pdev);
	rcu_assign_pointer(priv->parent, pdev);
	/* Nexmon's mon0 has an all-zero MAC; use the radio's own address */
	if (pdev->ieee80211_ptr && is_valid_ether_addr(pdev->ieee80211_ptr->wiphy->perm_addr))
		eth_hw_addr_set(espnow_dev, pdev->ieee80211_ptr->wiphy->perm_addr);
	else if (is_valid_ether_addr(pdev->dev_addr))
		eth_hw_addr_set(espnow_dev, pdev->dev_addr);
	netif_carrier_on(espnow_dev);
	netdev_info(espnow_dev, "attached to %s\n", pdev->name);
}

/* called under rtnl */
static void espnow_detach(void)
{
	struct espnow_priv *priv = netdev_priv(espnow_dev);
	struct net_device *pdev = rtnl_dereference(priv->parent);

	if (!pdev)
		return;
	netif_carrier_off(espnow_dev);
	netdev_rx_handler_unregister(pdev);
	RCU_INIT_POINTER(priv->parent, NULL);
	synchronize_net();
	dev_put(pdev);
	netdev_info(espnow_dev, "detached from %s\n", pdev->name);
}

static int espnow_notify(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct net_device *pdev = netdev_notifier_info_to_dev(ptr);
	struct espnow_priv *priv = netdev_priv(espnow_dev);
	struct net_device *cur = rtnl_dereference(priv->parent);

	/* NETDEV_UP, not REGISTER: Nexmon sets the radiotap type after registering */
	if (event == NETDEV_UP && !cur && !strcmp(pdev->name, parent))
		espnow_attach(pdev);
	else if (event == NETDEV_UNREGISTER && pdev == cur)
		espnow_detach();
	return NOTIFY_DONE;
}

static struct notifier_block espnow_nb = {
	.notifier_call = espnow_notify,
};

static void espnow_setup(struct net_device *dev)
{
	ether_setup(dev);
	dev->netdev_ops = &espnow_ops;
	dev->flags |= IFF_NOARP;
	dev->flags &= ~IFF_MULTICAST;
	eth_hw_addr_random(dev);
}

static int __init espnow_init(void)
{
	int err;

	espnow_dev = alloc_netdev(sizeof(struct espnow_priv), "espnow%d",
				  NET_NAME_ENUM, espnow_setup);
	if (!espnow_dev)
		return -ENOMEM;
	netif_carrier_off(espnow_dev);
	err = register_netdev(espnow_dev);
	if (err)
		goto free;
	/* replays NETDEV_UP for an already-running parent */
	err = register_netdevice_notifier(&espnow_nb);
	if (err)
		goto unreg;
	return 0;
unreg:
	unregister_netdev(espnow_dev);
free:
	free_netdev(espnow_dev);
	return err;
}

static void __exit espnow_exit(void)
{
	unregister_netdevice_notifier(&espnow_nb);
	rtnl_lock();
	espnow_detach();
	rtnl_unlock();
	unregister_netdev(espnow_dev);
	free_netdev(espnow_dev);
}

module_init(espnow_init);
module_exit(espnow_exit);
MODULE_DESCRIPTION("ESP-NOW receiver over a radiotap monitor interface");
MODULE_LICENSE("GPL");
