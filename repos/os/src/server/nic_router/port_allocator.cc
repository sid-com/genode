/*
 * \brief  Allocator for UDP/TCP ports
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* local includes */
#include <port_allocator.h>

using namespace Net;
using namespace Genode;


bool Net::dynamic_port(Port const port) {
	return port.value >= Port_allocator::FIRST; }


/********************
 ** Port_allocator **
 ********************/

Port Net::Port_allocator::alloc()
{
	try {
		Port const port(_alloc.alloc() + FIRST);
		return port;
	}
	catch (Num_allocator::Out_of_indices) {
		throw Out_of_indices(); }
}


void Net::Port_allocator::alloc(Port const port)
{
	try { _alloc.alloc_addr(port.value - FIRST); }
	catch (Num_allocator::Range_conflict) {
		throw Allocation_conflict(); }
}


/**************************
 ** Port_allocator_guard **
 **************************/

Port Port_allocator_guard::alloc()
{
	if (_used == _max) {
		throw Out_of_indices(); }

	try {
		Port const port = _port_alloc.alloc();
		_used++;
		return port;
	} catch (Port_allocator::Out_of_indices) {
		throw Out_of_indices(); }
}


void Port_allocator_guard::alloc(Port const port)
{
	if (_used == _max) {
		throw Out_of_indices(); }

	_port_alloc.alloc(port);
	_used++;
}


void Port_allocator_guard::free(Port const port)
{
	_port_alloc.free(port);
	_used = _used ? _used - 1 : 0;
}


Port_allocator_guard::Port_allocator_guard(Port_allocator &port_alloc,
                                           unsigned const  max)
:
	_port_alloc(port_alloc),
	_max(min(max, static_cast<uint16_t>(Port_allocator::COUNT)))
{
	if (max > (Port_allocator::COUNT))
		warning("number of configured nat ports too high. Setting to ",
		         static_cast<uint16_t>(Port_allocator::COUNT));
}

