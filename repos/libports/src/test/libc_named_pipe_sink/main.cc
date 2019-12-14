/*
 * \brief  libc_named_pipe test
 * \author Sid Hussmann
 * \date   2019-12-12
 */

/*
 * Copyright (C) 2018-2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* Libc includes */
#include <errno.h>
#include <stdio.h>
#include <string.h>


int main()
{
	enum { BUF_SIZE = 16*1024 };
	static char buf[BUF_SIZE];

	size_t total = 0;
	size_t total_read = 0;

	while (true) {

		auto const num_read = fread(buf + total_read, 1, sizeof(buf) - total_read, stdin);
		total_read += num_read;
		if (num_read == 0 && feof(stdin)) {
			Genode::log("EOF after reading ", total_read, " bytes");
			break;
		}

		if (num_read < 1 || num_read > sizeof(buf)) {
			int res = errno;
			Genode::error((char const *)strerror(res));
			return res;
		}
	}

	auto remain = total_read;
	size_t total_written = 0;

	while (remain > 0) {
		auto const num_written = fwrite(buf + total_written, 1, remain, stdout);
		if (num_written < 1 || num_written > remain) {
			int res = errno;
			Genode::error((char const *)strerror(res));
			return res;
		}

		remain -= num_written;
		total_written += num_written;
		total += num_written;
	}

	/* send EOF */
	fclose(stdout);

	Genode::log("piped ", total, " bytes");
	return 0;
};
