/*
 * \brief  NIC handler
 * \author Stefan Kalkowski
 * \date   2013-05-24
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <net/dhcp.h>

#include <component.h>

using namespace Net;
using namespace Genode;


bool Net::Nic::handle_arp(Ethernet_frame *eth, Genode::size_t size) {
	Arp_packet &arp = eth->data<Arp_packet>(size - sizeof(Ethernet_frame));

	/* ignore broken packets */
	if (!arp.ethernet_ipv4())
		return true;

	/* look whether the IP address is one of our client's */
	Ipv4_address_node *node = vlan().ip_tree.first();
	if (node)
		node = node->find_by_address(arp.dst_ip());
	if (node) {
		if (arp.opcode() == Arp_packet::REQUEST) {
			/*
			 * The ARP packet gets re-written, we interchange source
			 * and destination MAC and IP addresses, and set the opcode
			 * to reply, and then push the packet back to the NIC driver.
			 */
			Ipv4_address old_src_ip = arp.src_ip();
			arp.opcode(Arp_packet::REPLY);
			arp.dst_mac(arp.src_mac());
			arp.src_mac(mac());
			arp.src_ip(arp.dst_ip());
			arp.dst_ip(old_src_ip);
			eth->dst(arp.dst_mac());

			/* set our MAC as sender */
			eth->src(mac());
			send(eth, size);
		} else {
			/* overwrite destination MAC */
			arp.dst_mac(node->component().mac_address().addr);
			eth->dst(node->component().mac_address().addr);
			node->component().send(eth, size);
		}
		return false;
	}
	return true;
}


bool Net::Nic::handle_ip(Ethernet_frame *eth, Genode::size_t size) {

	size_t const ip_max_size = size - sizeof(Ethernet_frame);
	Ipv4_packet &ip = eth->data<Ipv4_packet>(ip_max_size);
	size_t const ip_size = ip.size(ip_max_size);

	/* is it an UDP packet ? */
	if (ip.protocol() == Ipv4_packet::Protocol::UDP)
	{
		size_t const udp_size = ip_size - sizeof(Ipv4_packet);
		Udp_packet &udp = ip.data<Udp_packet>(udp_size);

		/* is it a DHCP packet ? */
		if (Dhcp_packet::is_dhcp(&udp)) {
			size_t const dhcp_size = udp_size - sizeof(Udp_packet);
			Dhcp_packet &dhcp = udp.data<Dhcp_packet>(dhcp_size);

			/* check for DHCP ACKs containing new client ips */
			if (dhcp.op() == Dhcp_packet::REPLY) {

				try {
					Dhcp_packet::Message_type const msg_type =
						dhcp.option<Dhcp_packet::Message_type_option>().value();

					/*
					 * Extract the IP address and set it in the client's
					 * session-component
					 */
					if (msg_type == Dhcp_packet::Message_type::ACK) {
						Mac_address_node *node =
							vlan().mac_tree.first();
						if (node)
							node = node->find_by_address(dhcp.client_mac());
						if (node)
							node->component().set_ipv4_address(dhcp.yiaddr());
					}
				}
				catch (Dhcp_packet::Option_not_found) { }
			}
		}
	}

	/* is it an unicast message to one of our clients ? */
	if (eth->dst() == mac()) {
		Ipv4_address_node *node = vlan().ip_tree.first();
		if (node) {
			node = node->find_by_address(ip.dst());
			if (node) {
				/* overwrite destination MAC */
				eth->dst(node->component().mac_address().addr);

				/* deliver the packet to the client */
				node->component().send(eth, size);
				return false;
			}
		}
	}
	return true;
}


Net::Nic::Nic(Genode::Env &env, Genode::Heap &heap, Net::Vlan &vlan)
: Packet_handler(env.ep(), vlan),
  _tx_block_alloc(&heap),
  _nic(env, &_tx_block_alloc, BUF_SIZE, BUF_SIZE),
  _mac(_nic.mac_address().addr)
{
	_nic.rx_channel()->sigh_ready_to_ack(_sink_ack);
	_nic.rx_channel()->sigh_packet_avail(_sink_submit);
	_nic.tx_channel()->sigh_ack_avail(_source_ack);
	_nic.tx_channel()->sigh_ready_to_submit(_source_submit);
	_nic.link_state_sigh(_client_link_state);
}
