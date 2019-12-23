/*
 * \brief  VFS file system for pipe plugin
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

#ifndef _VFS_PIPE_FILE_SYSTEM_H_
#define _VFS_PIPE_FILE_SYSTEM_H_

#include <base/registry.h>
#include <vfs/env.h>
#include <vfs/file_system.h>

#include "types.h"
#include "pipe.h"

namespace Vfs_pipe {
	typedef Genode::Registered<Vfs_watch_handle>      Registered_watch_handle;
	typedef Genode::Registry<Registered_watch_handle> Watch_handle_registry;
	
	class File_system;
}


class Vfs_pipe::File_system : public Vfs::File_system
{
private:

	Pipe_space _pipe_space { };
	Pipe _pipe;

	/*
	 * XXX: a hack to defer cross-thread notifications at
	 * the libc until the io_progress handler
	 */
	Genode::Io_signal_handler<File_system> _notify_handler;
	Genode::Signal_context_capability _notify_cap { _notify_handler };

	void _notify_any();

	Watch_handle_registry _watch_handle_registry { };
	Genode::Io_signal_handler<File_system> _watch_signal_handler;
	Genode::Signal_context_capability _watch_cap { _watch_signal_handler };

	void _inform_watchers();


public:

	File_system(Vfs::Env &env);

	~File_system();
	const char* type() override { return "named_pipe"; }

	/***********************
	 ** Directory service **
	 ***********************/

	Genode::Dataspace_capability dataspace(char const*) override;

	void release(char const*, Dataspace_capability) override;

	Open_result open(const char *cpath,
                   unsigned mode,
                   Vfs::Vfs_handle **handle,
                   Genode::Allocator &alloc) override;

	Opendir_result opendir(char const *cpath, bool create,
                         Vfs_handle **handle,
                         Allocator &alloc) override;

	void close(Vfs::Vfs_handle *vfs_handle) override;

	Stat_result stat(const char *cpath, Stat &out) override;

	Unlink_result unlink(const char*) override;

	Rename_result rename(const char*, const char*) override;

	file_size num_dirent(char const *) override;

	bool directory(char const *cpath) override;

	const char* leaf_path(const char *cpath) override;

	void close(Vfs_watch_handle *handle) override;

	Watch_result watch(char const       *path,
                     Vfs_watch_handle **handle,
                     Allocator        &alloc) override;

	/**********************
	 ** File I/O service **
	 **********************/

	Write_result write(Vfs_handle *vfs_handle,
                     const char *src, file_size count,
                     file_size &out_count) override;

	Read_result complete_read(Vfs_handle *vfs_handle,
                            char *dst, file_size count,
                            file_size &out_count) override;

	bool read_ready(Vfs_handle *vfs_handle) override;

	bool notify_read_ready(Vfs_handle *vfs_handle) override;

	Ftruncate_result ftruncate(Vfs_handle*, file_size) override;

	Sync_result complete_sync(Vfs_handle*) override;

};


#endif /* _VFS_PIPE_FILE_SYSTEM_H_ */
