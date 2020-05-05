/*
 * \brief  Connection to TRACE service
 * \author Norman Feske
 * \date   2013-08-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__TRACE_SESSION__CONNECTION_H_
#define _INCLUDE__TRACE_SESSION__CONNECTION_H_

#include <util/retry.h>
#include <trace_session/client.h>
#include <base/connection.h>

namespace Genode { namespace Trace { struct Connection; } }


struct Genode::Trace::Connection : Genode::Connection<Genode::Trace::Session>,
                                   Genode::Trace::Session_client
{
	/**
	 * Extend session quota on demand while calling an RPC function
	 *
	 * \noapi
	 */
	template <typename FUNC>
	auto _retry(FUNC func) -> decltype(func())
	{
		enum { UPGRADE_ATTEMPTS = ~0U };
		return Genode::retry<Out_of_ram>(
			[&] () {
				return Genode::retry<Out_of_caps>(
					[&] () { return func(); },
					[&] () { Trace::Connection::upgrade_caps(2); },
					UPGRADE_ATTEMPTS);
			},
			[&] () { Trace::Connection::upgrade_ram(8*1024); },
			UPGRADE_ATTEMPTS);
	}

	/**
	 * Constructor
	 *
	 * \param ram_quota        RAM donated for tracing purposes
	 * \param arg_buffer_size  session argument-buffer size
	 * \param parent_levels    number of parent levels to trace
	 */
	Connection(Env &env, size_t ram_quota, size_t arg_buffer_size, unsigned parent_levels)
	:
		Genode::Connection<Session>(env,
			session(env.parent(), "ram_quota=%lu, arg_buffer_size=%lu, parent_levels=%u",
			        ram_quota + 2048, arg_buffer_size, parent_levels)),
		Session_client(env.rm(), cap())
	{ }

	size_t subjects(Subject_id *dst, size_t dst_len) override
	{
		return _retry([&] () {
			return Session_client::subjects(dst, dst_len); });
	}
};

#endif /* _INCLUDE__TRACE_SESSION__CONNECTION_H_ */
