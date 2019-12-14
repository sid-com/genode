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

#include <vfs/file_system_factory.h>
#include <base/registry.h>

#include "file_system.h"
#include "pipe_handle.h"

namespace Vfs_named_pipe {
	using namespace Vfs;
}




Vfs_named_pipe::Write_result
Vfs_named_pipe::Pipe_handle::write(const char *buf,
	                                 file_size count,
	                                 file_size &out_count) {
	return _file_system.write(*this, buf, count, out_count); }


Vfs_named_pipe::Read_result
Vfs_named_pipe::Pipe_handle::read(char *buf,
	                                file_size count,
	                                file_size &out_count) {
	return _file_system.read(*this, buf, count, out_count); }


bool
Vfs_named_pipe::Pipe_handle::read_ready() const {
	return !_file_system.buffer_empty(); }


bool
Vfs_named_pipe::Pipe_handle::notify_read_ready()
{
	if (!read_ready_elem.enqueued())
		_file_system.enqueue(read_ready_elem);
	return true;
}


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node) override
		{
			return new (env.alloc())
				Vfs_named_pipe::File_system(env);
		}
	};

	static Factory f;
	return &f;
}
