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

#include "pipe.h"
#include "pipe_handle.h"

void Vfs_pipe::Pipe::enqueue(Vfs_pipe::Handle_element& element)
{
	_read_ready_waiters.enqueue(element);
}

/**
 * Check if pipe is referenced, if not, destroy
 */
void Vfs_pipe::Pipe::cleanup()
{
	bool alive = _new_handle_active;
	if (!alive)
		_registry.for_each([&alive] (Pipe_handle&) {
			alive = true; });
	if (!alive)
		destroy(_alloc, this);
}

/**
 * Remove "/new" handle reference
 */
void Vfs_pipe::Pipe::remove_new_handle() {
	_new_handle_active = false; }

/**
 * Detach a handle
 */
void Vfs_pipe::Pipe::remove(Pipe_handle &handle)
{
	if (handle.io_progress_elem.enqueued())
		_io_progress_waiters.remove(handle.io_progress_elem);
	if (handle.read_ready_elem.enqueued())
		_read_ready_waiters.remove(handle.read_ready_elem);
}

/**
 * Decrease _num_writers
 */
void Vfs_pipe::Pipe::remove_writer()
{
	if (_num_writers) {
		_num_writers--;
	}
}

/**
 * Open a write or read handle
 */
Vfs_pipe::Open_result Vfs_pipe::Pipe::open(Vfs::File_system &fs,
                                           Path const &filename,
                                           Vfs::Vfs_handle **handle,
                                           Genode::Allocator &alloc)
{
	if (filename == "/in") {
		*handle = new (alloc)
			Pipe_handle(fs, alloc, Directory_service::OPEN_MODE_WRONLY, _registry, *this);
		_num_writers++;
		_waiting_for_writers = false;
		return Open_result::OPEN_OK;
	}

	if (filename == "/out") {
		*handle = new (alloc)
			Pipe_handle(fs, alloc, Directory_service::OPEN_MODE_RDONLY, _registry, *this);
		return Open_result::OPEN_OK;
	}

	return Open_result::OPEN_ERR_UNACCESSIBLE;
}

/**
 * Use a signal as a hack to defer notifications
 * until the "io_progress_handler".
 */
void Vfs_pipe::Pipe::submit_signal() {
	Genode::Signal_transmitter(_notify_sigh).submit(); }

/**
 * Notify handles waiting for activity
 */
void Vfs_pipe::Pipe::notify()
{
	_io_progress_waiters.dequeue_all([] (Handle_element &elem) {
		elem.object().io_progress_response(); });
	_read_ready_waiters.dequeue_all([] (Handle_element &elem) {
		elem.object().read_ready_response(); });
}

void Vfs_pipe::Pipe::_notify_watchers() {
	Genode::Signal_transmitter(_watch_sigh).submit(); }


Vfs_pipe::Write_result Vfs_pipe::Pipe::write(Pipe_handle &handle,
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

	if (notify) {
		_notify_watchers();
		submit_signal();
	}
	return Write_result::WRITE_OK;
}

Vfs_pipe::Read_result Vfs_pipe::Pipe::read(Pipe_handle &handle,
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

		/* Send only EOF when at least one writer opened the pipe */
		if ((_num_writers == 0) && !_waiting_for_writers) {
			return Read_result::READ_OK; /* EOF */
		}

		_io_progress_waiters.enqueue(handle.io_progress_elem);
		return Read_result::READ_QUEUED;
	}

	if (notify)
		submit_signal();

	return Read_result::READ_OK;
}

