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

#include "pipe_handle.h"
#include "types.h"

namespace Vfs_named_pipe {
	using namespace Vfs;

	class File_system;
}


class Vfs_named_pipe::File_system : public Vfs::File_system
{
	private:

		Pipe_buffer _buffer { };
		Vfs_named_pipe::Handle_fifo _io_progress_waiters { };
		Vfs_named_pipe::Handle_fifo _read_ready_waiters { };
		unsigned _num_writers = 0;
		bool _waiting_for_writers = true;

		/*
		 * XXX: a hack to defer cross-thread notifications at
		 * the libc until the io_progress handler
		 */
		Genode::Io_signal_handler<File_system> _notify_handler;
		Genode::Signal_context_capability _notify_cap { _notify_handler };

		void _notify_any()
		{
			_io_progress_waiters.dequeue_all([] (Handle_element &elem) {
				elem.object().io_progress_response(); });
			_read_ready_waiters.dequeue_all([] (Handle_element &elem) {
				elem.object().read_ready_response(); });
		}



	public:

		File_system(Vfs::Env &env)
		: _notify_handler(env.env().ep(), *this, &File_system::_notify_any) { }

		const char* type() override { return "named_pipe"; }

		bool buffer_empty() const
		{
			return _buffer.empty();
		}

		void enqueue(Handle_element& element)
		{
			_read_ready_waiters.enqueue(element);
		}

		/***********************
		 ** Directory service **
		 ***********************/

		Genode::Dataspace_capability dataspace(char const*) override {
			return Genode::Dataspace_capability(); }

		void release(char const*, Dataspace_capability) override { }

		Open_result open(const char *cpath,
		                 unsigned,
		                 Vfs::Vfs_handle **handle,
		                 Genode::Allocator &alloc) override
		{
			Path filename(cpath);

			if (filename == "/in") {
				*handle = new (alloc)
					Pipe_handle(*this, alloc, Directory_service::OPEN_MODE_WRONLY);
				_num_writers++;
				_waiting_for_writers = false;
				return Open_result::OPEN_OK;
			}

			if (filename == "/out") {
				*handle = new (alloc)
					Pipe_handle(*this, alloc, Directory_service::OPEN_MODE_RDONLY);
				return Open_result::OPEN_OK;
			}

			return Open_result::OPEN_ERR_UNACCESSIBLE;
		}

		/**
		 * Use a signal as a hack to defer notifications
		 * until the "io_progress_handler".
		 */
		void submit_signal() {
			Genode::Signal_transmitter(_notify_cap).submit(); }

		Write_result write(Vfs_handle *vfs_handle,
		                   char const *buf, file_size buf_size,
		                   file_size &out_count) override
		{
			if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle))
				return handle->write(buf, buf_size, out_count);

			return WRITE_ERR_INVALID;
		}

		Write_result write(Vfs_named_pipe::Pipe_handle &handle,
		                   const char *buf, file_size count,
		                   file_size &out_count)
		{
			file_size out = 0;
			bool notify = _buffer.empty();

			while (out < count && 0 < _buffer.avail_capacity()) {
				_buffer.add(*(buf++));
				++out;
			}

			out_count = out;
			if (out < count)
				_io_progress_waiters.enqueue(handle.io_progress_elem);

			if (notify)
				submit_signal();

			return Write_result::WRITE_OK;
		}

		Read_result read(Pipe_handle &handle,
		                 char *buf, file_size count,
		                 file_size &out_count)
		{
			bool notify = _buffer.avail_capacity() == 0;

			file_size out = 0;
			while (out < count && !_buffer.empty()) {
				*(buf++) = _buffer.get();
				++out;
			}

			out_count = out;
			if (!out) {

				/* Send only EOF when at least one writer opened the named pipe */
				if ((_num_writers == 0) && !_waiting_for_writers)
					return Read_result::READ_OK; /* EOF */

				_io_progress_waiters.enqueue(handle.io_progress_elem);
				return Read_result::READ_QUEUED;
			}

			if (notify)
				submit_signal();

			return Read_result::READ_OK;
		}

		Opendir_result opendir(char const *cpath, bool create,
	                           Vfs_handle **handle,
	                           Allocator &alloc) override
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

			return result;
		}

		void close(Vfs::Vfs_handle *vfs_handle) override
		{
			if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle)) {
				if (handle->writer) {
					_num_writers--;
				} else {
					_waiting_for_writers = true;
				}
			}

			destroy(vfs_handle->alloc(), vfs_handle);
		}

		Stat_result stat(const char *cpath, Stat &out) override
		{
			Stat_result result { STAT_ERR_NO_ENTRY };
			Path filename(cpath);

			out = Stat { };

			if (filename == "/in") {
				out = Stat {
					.size              = file_size(_buffer.avail_capacity()),
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
					.size              = file_size(PIPE_BUF_SIZE - _buffer.avail_capacity()),
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

		Unlink_result unlink(const char*) override {
			return UNLINK_ERR_NO_ENTRY; }

		Rename_result rename(const char*, const char*) override {
			return RENAME_ERR_NO_ENTRY; }

		file_size num_dirent(char const *) override {
			return ~0UL; }

		bool directory(char const *cpath) override
		{
			Path path(cpath);
			if (path == "/") return true;

			if (!path.has_single_element())
				return Open_result::OPEN_ERR_UNACCESSIBLE;

			return false;
		}

		const char* leaf_path(const char *cpath) override
		{
			Path path(cpath);
			if (path == "/") return cpath;
			if (path == "/in") return cpath;
			if (path == "/out") return cpath;

			return nullptr;
		}

		/**********************
		 ** File I/O service **
		 **********************/

		Read_result complete_read(Vfs_handle *vfs_handle,
		                          char *dst, file_size count,
		                          file_size &out_count) override
		{
			if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle))
				return handle->read(dst, count, out_count);

			return READ_ERR_INVALID;
		}

		bool read_ready(Vfs_handle *vfs_handle) override
		{
			if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle))
				return handle->read_ready();
			return true;
		}

		bool notify_read_ready(Vfs_handle *vfs_handle) override
		{
			if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle))
				return handle->notify_read_ready();
			return false;
		}

		Ftruncate_result ftruncate(Vfs_handle*, file_size) override {
			return FTRUNCATE_ERR_NO_PERM; }

		Sync_result complete_sync(Vfs_handle*) override {
			return SYNC_OK; }
};

