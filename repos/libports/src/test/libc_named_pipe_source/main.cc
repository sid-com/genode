/*
 * \brief  libc_named_pipe test
 * \author Sid Hussmann
 * \date   2019-12-12
 */

/*
 * Copyright (C) 2018-2020 gapfruit AG
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

// Genode includes
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/signal.h>
#include <libc/component.h>
#include <os/vfs.h>
#include <util/string.h>
#include <util/xml_node.h>
#include <vfs/vfs_handle.h>

/* libc includes */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace Named_pipe_source {
	using namespace Genode;
	class Main;
}


class Named_pipe_source::Main : Vfs::Watch_response_handler
{
		using Signal_handler = Genode::Signal_handler<Main>;

		/* test values */
		const char* _test_data { "random test data microphone check onetwo\n" };

		Env&                    _env;
		Heap                    _heap        { _env.ram(), _env.rm() };
		Attached_rom_dataspace  _config      { _env, "config" };

		FILE*           _receive_file        { nullptr };
		Directory::Path _output_filename     { "/dev/pipe/downstream/out" };
		Signal_handler  _output_data_handler { _env.ep(), *this, &Main::_handle_output_data };

		void watch_response() override
		{
			Genode::Signal_transmitter(_output_data_handler).submit();
		}

		struct Vfs_env : Vfs::Env
		{
			Main &_main;

			Vfs_env(Main &main) : _main(main) { }

			Genode::Env&       env()      override { return _main._env; }
			Genode::Allocator& alloc()    override { return _main._heap; }
			Vfs::File_system&  root_dir() override { return _main._root_dir_fs; }

		} _vfs_env { *this };

		Vfs::Global_file_system_factory _fs_factory { _heap };

		Vfs::Dir_file_system _root_dir_fs {
			_vfs_env, _config.xml().sub_node("vfs"), _fs_factory };

		Genode::Directory _root_dir { _vfs_env };
		Genode::Watcher _watcher    { _root_dir, _output_filename, *this };
		void _handle_output_data();
		void send_data();

	public:

		Main(Env &env) : _env(env)
		{
			Libc::with_libc([&] () {
				_receive_file = fopen(_output_filename.string(), "r");
				if (_receive_file == nullptr) {
					error("Cannot open receive file ", _output_filename);
					exit(1);
				}
				send_data();
			});
		}

		~Main() = default;
};

void Named_pipe_source::Main::_handle_output_data()
{
	Genode::log("Named_pipe_source::Main::", __func__, "() ");
	Libc::with_libc([&] () {
		if (_receive_file != nullptr) {
			size_t bytes_read = 0;
			enum { BUF_SIZE = 16*1024 };
			static char receive_buffer[BUF_SIZE];
			Genode::log("Named_pipe_source::Main::", __func__, "() before fread");
			bytes_read = fread(receive_buffer, 1, BUF_SIZE, _receive_file);
			Genode::log("Named_pipe_source::Main::", __func__, "() after fread. bytes_read=", bytes_read);
// 			Genode::log("read ", bytes_read, " bytes");
			if (0 != Genode::memcmp(_test_data, receive_buffer, bytes_read)) {
				error("Error writing to pipe. Data sent not equal data received.");
				throw Exception();
			}
		} else {
			error("Closed receive file ", _output_filename);
			throw Exception();
		}
	});

	log("--- test succeeded ---");
}

void Named_pipe_source::Main::send_data()
{
	const char* send_filename { "/dev/pipe/upstream/in" };

	FILE* send_file = nullptr;

	try {
		Genode::log("Named_pipe_source::Main::", __func__, "() fopen. send_filename=", send_filename);
		send_file = fopen(send_filename, "a");
		if (send_file == nullptr) {
			error("Cannot open send file ", send_filename);
			throw Exception();
		}

		Genode::log("Named_pipe_source::Main::", __func__, "() before fwrite. send_filename=", send_filename);
		auto const bytes_written = fwrite(_test_data, 1, Genode::strlen(_test_data), send_file);
		Genode::log("Named_pipe_source::Main::", __func__, "() after fwrite. bytes_written=", bytes_written);

		Genode::log("Named_pipe_source::Main::", __func__, "() before fclose. send_filename=", send_filename);
		/* send EOF */
		fclose(send_file);
		Genode::log("Named_pipe_source::Main::", __func__, "() after fclose. send_filename=", send_filename);

		log("written ", bytes_written, " bytes");

		if (0 == bytes_written) {
			error("Error writing to pipe bytes_written=0");
			throw Exception();
		}
	}
	catch (...)
	{
		if (send_file != nullptr) {
			fclose(send_file);
		}
		error("--- test failed ---");

		exit(1);
	}
}


void Libc::Component::construct(Libc::Env& env) { static Named_pipe_source::Main main(env); }
