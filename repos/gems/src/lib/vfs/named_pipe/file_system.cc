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

#include "file_system.h"
#include "pipe_handle.h"


Vfs_pipe::File_system::File_system(Vfs::Env &env)
:
	_pipe(env.alloc(), _pipe_space, _notify_cap, _watch_cap),
	_notify_handler(env.env().ep(), *this, &File_system::_notify_any),
	_watch_signal_handler(env.env().ep(), *this,
	                      &Vfs_pipe::File_system::_inform_watchers) { }

Vfs_pipe::File_system::~File_system()
{
	_pipe.cleanup();
}


void Vfs_pipe::File_system::_notify_any()
{
	_pipe.notify();
	_pipe_space.for_each<Pipe&>([] (Pipe &pipe) {
		pipe.notify(); });
}

void Vfs_pipe::File_system::_inform_watchers()
{
	_watch_handle_registry.for_each([this] (Registered_watch_handle &handle) {
		handle.watch_response();
	});
}

/***********************
 ** Directory service **
 ***********************/

Genode::Dataspace_capability Vfs_pipe::File_system::dataspace(char const*) {
	return Genode::Dataspace_capability(); }

void Vfs_pipe::File_system::release(char const*, Dataspace_capability) { }

Vfs_pipe::Open_result Vfs_pipe::File_system::open(const char *cpath,
                                                  unsigned,
                                                  Vfs::Vfs_handle **handle,
                                                  Genode::Allocator &alloc)
{
	Path filename(cpath);
	return _pipe.open(*this, filename, handle, alloc);
}

Vfs_pipe::Opendir_result Vfs_pipe::File_system::opendir(char const *cpath,
                                                        bool create,
                                                        Vfs_handle **handle,
                                                        Allocator &alloc)
{
	/* open dummy handles on directories */
	if (create) return OPENDIR_ERR_PERMISSION_DENIED;
	Path path(cpath);

	if (path == "/") {
		*handle = new (alloc)
			Vfs_handle(*this, *this, alloc, 0);
		return OPENDIR_OK;
	}

	Opendir_result result { OPENDIR_ERR_LOOKUP_FAILED };

	if (path.has_single_element()) {
		Pipe_space::Id id { ~0UL };
		if (ascii_to(path.last_element(), id.value)) try {
			_pipe_space.apply<Pipe&>(id, [&] (Pipe&) {
				*handle = new (alloc)
					Vfs_handle(*this, *this, alloc, 0);
				result = OPENDIR_OK;
			});
		}
		catch (Pipe_space::Unknown_id) { }
	}
	return result;
}

void Vfs_pipe::File_system::close(Vfs::Vfs_handle *vfs_handle)
{
	_pipe.cleanup();
	Pipe *pipe = nullptr;
	if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle)) {
		pipe = &handle->pipe;
		if (handle->writer)
			pipe->remove_writer();
	} else
	if (New_pipe_handle *handle = dynamic_cast<New_pipe_handle*>(vfs_handle))
		pipe = &handle->pipe;

	destroy(vfs_handle->alloc(), vfs_handle);

	if (pipe)
		pipe->cleanup();
}

Vfs_pipe::Stat_result Vfs_pipe::File_system::stat(const char *cpath, Stat &out)
{
	Stat_result result { STAT_ERR_NO_ENTRY };
	Path filename(cpath);

	out = Stat { };

	if (filename == "/in") {
		out = Stat {
			.size              = file_size(_pipe.buffer_avail_capacity()),
			.type              = Node_type::CONTINUOUS_FILE,
			.rwx               = Node_rwx::wo(),
			.inode             = Genode::addr_t(this) + 1,
			.device            = Genode::addr_t(this),
			.modification_time = { }
		};
		result = STAT_OK;
	} else
	if (filename == "/out") {
		out = Stat {
			.size              = file_size(PIPE_BUF_SIZE - _pipe.buffer_avail_capacity()),
			.type              = Node_type::CONTINUOUS_FILE,
			.rwx               = Node_rwx::ro(),
			.inode             = Genode::addr_t(this) + 2,
			.device            = Genode::addr_t(this),
			.modification_time = { }
		};
		result = STAT_OK;
	}
	return result;
}

Vfs_pipe::Unlink_result Vfs_pipe::File_system::unlink(const char*) {
	return UNLINK_ERR_NO_ENTRY; }

Vfs_pipe::Rename_result Vfs_pipe::File_system::rename(const char*, const char*) {
	return RENAME_ERR_NO_ENTRY; }

Vfs::file_size Vfs_pipe::File_system::num_dirent(char const *) {
	return 0; }

bool Vfs_pipe::File_system::directory(char const *cpath)
{
	Path path(cpath);
	if (path == "/") return true;

	if (!path.has_single_element())
		return Open_result::OPEN_ERR_UNACCESSIBLE;

	Pipe_space::Id id { ~0UL };
	if (!ascii_to(path.last_element(), id.value))
		return false;

	bool result = false;
	try {
		_pipe_space.apply<Pipe&>(id, [&] (Pipe &) {
			result = true; });
	}
	catch (Pipe_space::Unknown_id) { }

	return result;
}

const char* Vfs_pipe::File_system::leaf_path(const char *cpath)
{
	Path path(cpath);
	if (path == "/") return cpath;
	if (path == "/in") return cpath;
	if (path == "/out") return cpath;

	return nullptr;
}

void Vfs_pipe::File_system::close(Vfs_watch_handle *handle)
{
	if (handle && (&handle->fs() == this))
		destroy(handle->alloc(), handle);
}

Vfs_pipe::Watch_result Vfs_pipe::File_system::watch(char const *path,
                                                    Vfs_watch_handle **handle,
                                                    Allocator        &alloc)
{
	Path filename(path);
	if (filename != "/out") {
		return WATCH_ERR_UNACCESSIBLE;
	}
	try {
		Vfs_watch_handle &watch_handle = *new (alloc)
			Registered_watch_handle(_watch_handle_registry, *this, alloc);

		*handle = &watch_handle;
		return WATCH_OK;
	}
	catch (Genode::Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
	catch (Genode::Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
}

/**********************
 ** File I/O service **
 **********************/

Vfs_pipe::Write_result Vfs_pipe::File_system::write(Vfs_handle *vfs_handle,
                                                    const char *src,
                                                    file_size count,
                                                    file_size &out_count)
{
	if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle)) {
		return handle->write(src, count, out_count);
	}

	return WRITE_ERR_INVALID;
}

Vfs_pipe::Read_result Vfs_pipe::File_system::complete_read(Vfs_handle *vfs_handle,
                                                           char *dst,
                                                           file_size count,
                                                           file_size &out_count)
{
	if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle))
		return handle->read(dst, count, out_count);

	if (New_pipe_handle *handle = dynamic_cast<New_pipe_handle*>(vfs_handle))
		return handle->read(dst, count, out_count);

	return READ_ERR_INVALID;
}

bool Vfs_pipe::File_system::read_ready(Vfs_handle *vfs_handle)
{
	if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle))
		return handle->read_ready();
	return true;
}

bool Vfs_pipe::File_system::notify_read_ready(Vfs_handle *vfs_handle)
{
	if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle))
		return handle->notify_read_ready();
	return false;
}

Vfs_pipe::Ftruncate_result Vfs_pipe::File_system::ftruncate(Vfs_handle*, file_size) {
	return FTRUNCATE_ERR_NO_PERM; }

Vfs_pipe::Sync_result Vfs_pipe::File_system::complete_sync(Vfs_handle*) {
	return SYNC_OK; }
