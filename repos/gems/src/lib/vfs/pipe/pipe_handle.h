/*
 * \brief  VFS pipe handle for pipe plugin
 * \author Emery Hemingway
 * \author Sid Hussmann
 * \date   2019-05-29
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VFS_PIPE_PIPE_HANDLE_H_
#define _VFS_PIPE_PIPE_HANDLE_H_

#include "types.h"

namespace Vfs_pipe {
	using namespace Vfs;

	class Pipe;
	struct Pipe_handle;
	struct New_pipe_handle;
}


struct Vfs_pipe::Pipe_handle : Vfs::Vfs_handle, private Pipe_handle_registry_element
{
	Pipe &pipe;

	Handle_element io_progress_elem { *this };
	Handle_element  read_ready_elem { *this };

	bool const writer;

	Pipe_handle(Vfs::File_system &fs,
	            Genode::Allocator &alloc,
	            unsigned flags,
	            Pipe_handle_registry &registry,
	            Vfs_pipe::Pipe& p);


	~Pipe_handle();

	Write_result write(const char *buf,
	                   file_size count,
	                   file_size &out_count);

	Read_result read(char *buf,
	                 file_size count,
	                 file_size &out_count);

	bool read_ready() const;
	bool notify_read_ready();
};


struct Vfs_pipe::New_pipe_handle : Vfs::Vfs_handle
{
	Pipe &pipe;

	New_pipe_handle(Vfs::File_system &fs,
	                Genode::Allocator &alloc,
	                unsigned flags,
	                Pipe_space &pipe_space,
	                Genode::Signal_context_capability &notify_sigh);

	~New_pipe_handle();

	Read_result read(char *buf,
	                 file_size count,
	                 file_size &out_count);
};

#endif /* _VFS_PIPE_PIPE_HANDLE_H_ */
