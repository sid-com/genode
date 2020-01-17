/*
 * \brief  VFS pipe plugin
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

#include "pipe_handle.h"
#include "pipe.h"

Vfs_pipe::Pipe_handle::Pipe_handle(Vfs::File_system &fs,
                                   Genode::Allocator &alloc,
                                   unsigned flags,
                                   Pipe_handle_registry &registry,
                                   Pipe &p)
:
	Vfs::Vfs_handle(fs, fs, alloc, flags),
	Pipe_handle_registry_element(registry, *this),
	pipe(p),
	writer(flags == Directory_service::OPEN_MODE_WRONLY) { }

Vfs_pipe::Pipe_handle::~Pipe_handle() {
	pipe.remove(*this); }

Vfs_pipe::Write_result
Vfs_pipe::Pipe_handle::write(const char *buf,
                             file_size count,
                             file_size &out_count) {
	return pipe.write(*this, buf, count, out_count); }


Vfs_pipe::Read_result
Vfs_pipe::Pipe_handle::read(char *buf,
                            file_size count,
                            file_size &out_count) {
	return pipe.read(*this, buf, count, out_count); }


bool
Vfs_pipe::Pipe_handle::read_ready() const {
	return !pipe.buffer_empty(); }


bool
Vfs_pipe::Pipe_handle::notify_read_ready()
{
	if (!read_ready_elem.enqueued())
		pipe.enqueue(read_ready_elem);
	return true;
}

Vfs_pipe::New_pipe_handle::New_pipe_handle(Vfs::File_system &fs,
                                           Genode::Allocator &alloc,
                                           unsigned flags,
                                           Pipe_space &pipe_space,
                                           Genode::Signal_context_capability &notify_sigh)
: Vfs::Vfs_handle(fs, fs, alloc, flags),
  pipe(*(new (alloc) Pipe(alloc, pipe_space, notify_sigh))) { }

Vfs_pipe::New_pipe_handle::~New_pipe_handle()
{
	pipe.remove_new_handle();
}

Vfs_pipe::Read_result
Vfs_pipe::New_pipe_handle::read(char *buf,
                                file_size count,
                                file_size &out_count)
{
	auto name = Genode::String<8>(pipe.id().value);
	if (name.length() < count) {
		memcpy(buf, name.string(), name.length());
		out_count = name.length();
		return Read_result::READ_OK;
	}
	return Read_result::READ_ERR_INVALID;
}
