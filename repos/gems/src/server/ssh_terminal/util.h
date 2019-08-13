/*
 * \brief  Component providing a Terminal session via SSH
 * \author Josef Soentgen
 * \author Pirmin Duss
 * \date   2019-05-29
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SSH_TERMINAL_UTIL_H_
#define _SSH_TERMINAL_UTIL_H_

/* Genode includes */
#include <util/string.h>
#include <libc/component.h>

/* libc includes */
#include <unistd.h>
#include <time.h>


namespace Util
{
	using Filename = Genode::String<256>;
	/*
	 * get the current time from the libc backend.
	 */
	char const *get_time();
}

#endif /* _SSH_TERMINAL_UTIL_H_ */
