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

#include <timer_session/connection.h>

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
	private:
		using Signal_handler = Genode::Signal_handler<Main>;

		/* test values */
		enum { BUF_SIZE = 16*1024 };
		char _test_data[BUF_SIZE];
		char _receive_buffer[BUF_SIZE];

		Env&                    _env;
		Timer::Connection       _timer       { _env };
		Heap                    _heap        { _env.ram(), _env.rm() };
		Attached_rom_dataspace  _config      { _env, "config" };

		FILE*           _test_data_file      { nullptr };
		Directory::Path _test_data_filename  { "/ro/test-data.bin" };
		FILE*           _receive_file        { nullptr };
		Directory::Path _output_filename     { "/dev/pipe/downstream/out" };
		Signal_handler  _output_data_handler { _env.ep(), *this, &Main::_handle_output_data };

		void watch_response() override
		{
			Genode::log("watch_response");
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
			Genode::log("started");
			Libc::with_libc([&] () {
				_receive_file = fopen(_output_filename.string(), "r");
				if (_receive_file == nullptr) {
					error("Cannot open receive file ", _output_filename);
					exit(1);
				}
				_test_data_file = fopen(_test_data_filename.string(), "r");
				if (_test_data_file == nullptr) {
					error("Cannot open test data file ", _test_data_filename);
					exit(1);
				}
			});

			// poll once for data
			watch_response();
		}

		~Main() = default;
};

void Named_pipe_source::Main::_handle_output_data()
{
	Libc::with_libc([&] () {
		if (_receive_file != nullptr) {
			while (true) {
				// read test data
				size_t test_data_num = fread(_test_data, 1, BUF_SIZE, _test_data_file);

				size_t pipe_data_num = fread(_receive_buffer, 1, BUF_SIZE, _receive_file);
				log("Received data. test_data_num=", test_data_num, " pipe_data_num=", pipe_data_num);
				if (pipe_data_num) {
					if (0 != Genode::memcmp(_test_data, _receive_buffer, pipe_data_num)) {
						error("Error writing to pipe. Data sent not equal data received.");
						throw Exception();
					}
				}
				if (feof(_receive_file)) {
					log("--- test succeeded ---");
					return;
				}
			}
		} else {
			error("Closed receive file ", _output_filename);
			throw Exception();
		}
	});
}


void Libc::Component::construct(Libc::Env& env) { static Named_pipe_source::Main main(env); }
