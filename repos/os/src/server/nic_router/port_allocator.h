/*
 * \brief  Allocator for UDP/TCP ports
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PORT_ALLOCATOR_H_
#define _PORT_ALLOCATOR_H_

/* Genode includes */
#include <net/port.h>

/* local includes */
#include <util/bit_allocator.h>

namespace Net {

	template<Genode::uint16_t MAX>
	class Monotonic_number_allocator;
	class Port_allocator;
	class Port_allocator_guard;

	bool dynamic_port(Port const port);
}


template<Genode::uint16_t MAX>
class Net::Monotonic_number_allocator
{
	private:

		using Bit_alloc = Genode::Bit_allocator<MAX>;

		Bit_alloc        _alloc { };
		Genode::uint16_t _next;

	public:

		struct Range_conflict : Genode::Exception { };
		struct Out_of_indices : Genode::Exception { };

		Monotonic_number_allocator(Genode::uint16_t init) : _next(init % MAX) { }

		Genode::uint16_t alloc();

		void alloc_addr(Genode::uint16_t number);

		void free(Genode::uint16_t number) { _alloc.free(number); }
};


class Net::Port_allocator
{
	public:

		enum { FIRST = 49152, COUNT = 16384 };

	private:

		using Num_allocator = Monotonic_number_allocator<COUNT>;

		Num_allocator _alloc { 0 };

	public:

		struct Allocation_conflict : Genode::Exception { };
		struct Out_of_indices      : Genode::Exception { };

		Port alloc();

		void alloc(Port const port);

		void free(Port const port) { _alloc.free(port.value - FIRST); }
};


class Net::Port_allocator_guard
{
	private:

		Port_allocator &_port_alloc;
		unsigned const  _max;
		unsigned        _used = 0;

	public:

		class Out_of_indices : Genode::Exception {};

		Port alloc();

		void alloc(Port const port);

		void free(Port const port);

		Port_allocator_guard(Port_allocator & port_alloc, unsigned const max);

		unsigned max() const { return _max; }
};

#endif /* _PORT_ALLOCATOR_H_ */
