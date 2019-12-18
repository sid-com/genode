/*
 * \brief  VFS named pipe plugin
 * \author Sid Hussmann
 * \date   2019-12-13
 */

/*
 * Copyright (C) 2019 gapfruit AG
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VFS_NAMED_PIPE_TYPES_H_
#define _VFS_NAMED_PIPE_TYPES_H_

#include <os/path.h>
#include <os/ring_buffer.h>
#include <vfs/file_system.h>
#include <vfs/file_system_factory.h>

#include "file_system.h"

namespace Vfs_named_pipe {
	using namespace Vfs;

	struct Pipe_handle;
	typedef Genode::Fifo_element<Pipe_handle> Handle_element;
	typedef Genode::Fifo<Handle_element> Handle_fifo;
	typedef Genode::Registry<Pipe_handle> Pipe_handle_registry;

	typedef Vfs::Directory_service::Open_result Open_result;
	typedef Vfs::File_io_service::Write_result Write_result;
	typedef Vfs::File_io_service::Read_result Read_result;
	typedef Genode::Path<32> Path;

	enum { PIPE_BUF_SIZE = 8192U };
	typedef Genode::Ring_buffer<unsigned char, PIPE_BUF_SIZE+1> Pipe_buffer;
}

#endif /* _VFS_NAMED_PIPE_TYPES_H_ */
