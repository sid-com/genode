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
			        ram_quota + 10*1024, arg_buffer_size, parent_levels)),
		Session_client(env.rm(), cap())
	{ }

	size_t subjects(Subject_id *dst, size_t dst_len) override
	{
		return retry_with_upgrade(Ram_quota{8*1024}, Cap_quota{2}, [&] () {
			return Session_client::subjects(dst, dst_len); });
	}

	template <typename FN>
	size_t for_each_subject_info(FN const &fn)
	{
		return retry_with_upgrade(Ram_quota{8*1024}, Cap_quota{2}, [&] () {
			return Session_client::for_each_subject_info(fn); });
	}
};

#endif /* _INCLUDE__TRACE_SESSION__CONNECTION_H_ */
