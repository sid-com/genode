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


#include <base/log.h>
#include <util/string.h>

/* libc includes */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace Genode;

enum { BUF_SIZE = 16*1024 };
static char receive_buffer[BUF_SIZE];



int main(int argc, char *argv[])
{
	/* test values */
	const char* test_data { "random test data microphone check onetwo\n" };

	const char* send_filename { "/dev/pipe/upstream/in" };
	const char* receive_filename { "/dev/pipe/downstream/out" };

	FILE* send_file = nullptr;
	FILE* receive_file = nullptr;

	try {
		receive_file = fopen(receive_filename, "r");
		if (receive_file == nullptr) {
			error("Cannot open receive file ", receive_filename);
			throw Exception();
		}

		send_file = fopen(send_filename, "a");
		if (send_file == nullptr) {
			error("Cannot open send file ", send_filename);
			throw Exception();
		}

		auto const bytes_written = fwrite(test_data, 1, Genode::strlen(test_data), send_file);

		/* send EOF */
		fclose(send_file);

		Genode::log("written ", bytes_written, " bytes");

		if (0 != bytes_written) {

			auto const bytes_read = fread(receive_buffer, 1, BUF_SIZE, receive_file);

			Genode::log("read ", bytes_read, " bytes");
			if (0 != Genode::memcmp(test_data, receive_buffer, bytes_written)) {
				error("Error writing to pipe. Data sent not equal data received.");
				throw Exception();
			}
		} else {
			error("Error writing to pipe bytes_written=0");
			throw Exception();
		}

		fclose(receive_file);
	}
	catch (...)
	{
		if (receive_file != nullptr) {
			fclose(receive_file);
		}

		if (send_file != nullptr) {
			fclose(send_file);
		}
		error("--- test failed ---\n");

		exit(1);
	}

	printf("--- test finished ---\n");

	return 0;
}
