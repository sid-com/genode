/*
 * \brief  VFS named pipe plugin
 * \author Sid Hussmann
 * \date   2019-12-13
 */

/*
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <vfs/file_system_factory.h>
#include "types.h"

namespace Vfs_named_pipe {
	using namespace Vfs;

	struct Pipe_handle;
	class File_system;
}


struct Vfs_named_pipe::Pipe_handle : Vfs::Vfs_handle
{

	Handle_element io_progress_elem { *this };
	Handle_element  read_ready_elem { *this };

	bool const writer;

	Vfs_named_pipe::File_system& _file_system;

	Pipe_handle(Vfs_named_pipe::File_system &fs,
	            Genode::Allocator &alloc,
	            unsigned flags)
	:
		Vfs::Vfs_handle(reinterpret_cast<Vfs::File_system&>(fs),
		                reinterpret_cast<Vfs::File_system&>(fs), alloc, flags),
		writer(flags == Directory_service::OPEN_MODE_WRONLY),
		_file_system(fs)
	{ }

	virtual ~Pipe_handle() = default;

	Write_result write(const char *buf,
	                   file_size count,
	                   file_size &out_count);

	Read_result read(char *buf,
	                 file_size count,
	                 file_size &out_count);

	bool read_ready() const;
	bool notify_read_ready();
};
