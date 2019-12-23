/*
 * \brief  VFS typedefs for the pipe plugin
 * \author Emery Hemingway
 * \author Sid Hussmann
 * \date   2019-12-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VFS_PIPE_TYPES_H_
#define _VFS_PIPE_TYPES_H_

#include <os/path.h>
#include <os/ring_buffer.h>
#include <vfs/file_system.h>
#include <base/registry.h>

namespace Vfs_pipe {
	using namespace Vfs;

	typedef Vfs::Directory_service::Open_result Open_result;
	typedef Vfs::Directory_service::Opendir_result Opendir_result;
	typedef Vfs::Directory_service::Rename_result Rename_result;
	typedef Vfs::Directory_service::Stat_result Stat_result;
	typedef Vfs::Directory_service::Unlink_result Unlink_result;
	typedef Vfs::Directory_service::Watch_result Watch_result;
	typedef Vfs::File_io_service::Ftruncate_result Ftruncate_result;
	typedef Vfs::File_io_service::Read_result Read_result;
	typedef Vfs::File_io_service::Sync_result Sync_result;
	typedef Vfs::File_io_service::Write_result Write_result;
	typedef Genode::Path<32> Path;

	enum { PIPE_BUF_SIZE = 8192U };
	typedef Genode::Ring_buffer<unsigned char, PIPE_BUF_SIZE+1> Pipe_buffer;

	struct Pipe_handle;
	typedef Genode::Fifo_element<Pipe_handle> Handle_element;
	typedef Genode::Fifo<Handle_element> Handle_fifo;

	typedef Genode::Registry<Pipe_handle>::Element Pipe_handle_registry_element;
	typedef Genode::Registry<Pipe_handle> Pipe_handle_registry;
	class Pipe;
	typedef Genode::Id_space<Pipe> Pipe_space;
}

#endif /* _VFS_NAMED_PIPE_TYPES_H_ */
