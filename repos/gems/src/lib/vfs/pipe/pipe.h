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

#ifndef _VFS_PIPE_H_
#define _VFS_PIPE_H_

#include "types.h"

namespace Vfs_pipe {
	using namespace Vfs;

	class Pipe;
}



class Vfs_pipe::Pipe
{
private:
	Genode::Allocator &_alloc;
	Pipe_space::Element _space_elem;
	Pipe_buffer _buffer { };
	Pipe_handle_registry _registry { };
	Handle_fifo _io_progress_waiters { };
	Handle_fifo _read_ready_waiters { };
	unsigned _num_writers = 0;

	Genode::Signal_context_capability &_notify_sigh;

	bool _new_handle_active { true };
	bool _waiting_for_writers { true };

public:
	Pipe(Genode::Allocator &alloc, Pipe_space &space,
       Genode::Signal_context_capability &notify_sigh)
: _alloc(alloc), _space_elem(*this, space),
  _notify_sigh(notify_sigh) { }

	~Pipe() = default;

	Pipe_space::Id id() const
	{
		return _space_elem.id();
	}

	bool buffer_empty() const
	{
		return _buffer.empty();
	}

	bool buffer_avail_capacity() const
	{
		return _buffer.avail_capacity();
	}

	void enqueue(Handle_element& element);

	/**
	 * Check if pipe is referenced, if not, destroy
	 */
	void cleanup();

	/**
	 * Remove "/new" handle reference
	 */
	void remove_new_handle();

	/**
	 * Detach a handle
	 */
	void remove(Pipe_handle &handle);

	/**
	 * Decrease _num_writers
	 */
	void remove_writer();

	/**
	 * Open a write or read handle
	 */
	Open_result open(Vfs::File_system &fs,
	                 Path const &filename,
	                 Vfs::Vfs_handle **handle,
	                 Genode::Allocator &alloc);

	/**
	 * Use a signal as a hack to defer notifications
	 * until the "io_progress_handler".
	 */
	void submit_signal();

	/**
	 * Notify handles waiting for activity
	 */
	void notify();

	Write_result write(Pipe_handle &handle,
	                   const char *buf, file_size count,
	                   file_size &out_count);

	Read_result read(Pipe_handle &handle,
	                 char *buf, file_size count,
	                 file_size &out_count);
};

#endif /* _VFS_PIPE_H_ */
