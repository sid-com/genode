/*
 * \brief  VFS pipe plugin
 * \author Emery Hemingway
 * \author Sid Hussmann
 * \date   2019-05-29
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <vfs/file_system_factory.h>
#include <os/path.h>
#include <os/ring_buffer.h>
#include <base/registry.h>
#include <util/list.h>

namespace Vfs_pipe {
	using namespace Vfs;
	typedef Vfs::Directory_service::Open_result Open_result;
	typedef Vfs::File_io_service::Write_result Write_result;
	typedef Vfs::File_io_service::Read_result Read_result;
	enum { MAX_PATH_LENGTH = 256 };
	typedef Genode::Path<MAX_PATH_LENGTH> Path;

	enum { PIPE_BUF_SIZE = 8192U };
	typedef Genode::Ring_buffer<unsigned char, PIPE_BUF_SIZE+1> Pipe_buffer;

	struct Pipe_handle;
	typedef Genode::Fifo_element<Pipe_handle> Handle_element;
	typedef Genode::Fifo<Handle_element> Handle_fifo;
	typedef Genode::Registry<Pipe_handle>::Element Pipe_handle_registry_element;
	typedef Genode::Registry<Pipe_handle> Pipe_handle_registry;

	struct Pipe;
	typedef Genode::Id_space<Pipe> Pipe_space;

	struct New_pipe_handle;

	class File_system;
	class Fifo_file_system;
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
	            Pipe &p)
	:
		Vfs::Vfs_handle(fs, fs, alloc, flags),
		Pipe_handle_registry_element(registry, *this),
		pipe(p),
		writer(flags == Directory_service::OPEN_MODE_WRONLY)
	{ }

	virtual ~Pipe_handle();

	Write_result write(const char *buf,
	                   file_size count,
	                   file_size &out_count);

	Read_result read(char *buf,
	                 file_size count,
	                 file_size &out_count);

	bool read_ready();
	bool notify_read_ready();
};


struct Vfs_pipe::Pipe
{
	Genode::Allocator &alloc;
	Pipe_space::Element space_elem;
	Pipe_buffer buffer { };
	Pipe_handle_registry registry { };
	Handle_fifo io_progress_waiters { };
	Handle_fifo read_ready_waiters { };
	unsigned num_writers { 0 };
	bool waiting_for_writers { true };

	Genode::Signal_context_capability &notify_sigh;

	bool new_handle_active { true };

	Pipe(Genode::Allocator &alloc, Pipe_space &space,
	     Genode::Signal_context_capability &notify_sigh)
	: alloc(alloc), space_elem(*this, space), notify_sigh(notify_sigh) { }

	~Pipe() = default;

	typedef Genode::String<8> Name;
	Name name() const
	{
		return Name(space_elem.id().value);
	}

	/**
	 * Check if pipe is referenced, if not, destroy
	 */
	void cleanup()
	{
		bool alive = new_handle_active;
		if (!alive)
			registry.for_each([&alive] (Pipe_handle&) {
				alive = true; });
		if (!alive)
			destroy(alloc, this);
	}

	/**
	 * Remove "/new" handle reference
	 */
	void remove_new_handle() {
		new_handle_active = false; }

	/**
	 * Detach a handle
	 */
	void remove(Pipe_handle &handle)
	{
		if (handle.io_progress_elem.enqueued())
			io_progress_waiters.remove(handle.io_progress_elem);
		if (handle.read_ready_elem.enqueued())
			read_ready_waiters.remove(handle.read_ready_elem);
	}

	/**
	 * Open a write or read handle
	 */
	Open_result open(Vfs::File_system &fs,
	                 Path const &filename,
	                 Vfs::Vfs_handle **handle,
	                 Genode::Allocator &alloc)
	{
		if (filename == "/in") {

			if (0 == num_writers) {
				/* flush buffer */
				if (!buffer.empty()) {
					Genode::warning("flushing non-empty buffer capacity=", buffer.avail_capacity());
				}
				buffer.reset();
				io_progress_waiters.dequeue_all([] (Handle_element /*&elem*/) { });
			}
			*handle = new (alloc)
				Pipe_handle(fs, alloc, Directory_service::OPEN_MODE_WRONLY, registry, *this);
			num_writers++;
			waiting_for_writers = false;
			return Open_result::OPEN_OK;
		}

		if (filename == "/out") {
			*handle = new (alloc)
				Pipe_handle(fs, alloc, Directory_service::OPEN_MODE_RDONLY, registry, *this);

			if (0 == num_writers && buffer.empty()) {
				waiting_for_writers = true;
			}
			return Open_result::OPEN_OK;
		}

		return Open_result::OPEN_ERR_UNACCESSIBLE;
	}

	/**
	 * Use a signal as a hack to defer notifications
	 * until the "io_progress_handler".
	 */
	void submit_signal() {
		Genode::Signal_transmitter(notify_sigh).submit(); }

	/**
	 * Notify handles waiting for activity
	 */
	void notify()
	{
		io_progress_waiters.dequeue_all([] (Handle_element &elem) {
			elem.object().io_progress_response(); });
		read_ready_waiters.dequeue_all([] (Handle_element &elem) {
			elem.object().read_ready_response(); });
	}

	Write_result write(Pipe_handle &handle,
	                   const char *buf, file_size count,
	                   file_size &out_count)
	{
		file_size out = 0;
		bool notify = buffer.empty();

		while (out < count && 0 < buffer.avail_capacity()) {
			buffer.add(*(buf++));
			++out;
		}

		out_count = out;
		if (out < count)
			io_progress_waiters.enqueue(handle.io_progress_elem);

		if (notify)
			submit_signal();

		return Write_result::WRITE_OK;
	}

	Read_result read(Pipe_handle &handle,
	                 char *buf, file_size count,
	                 file_size &out_count)
	{
		bool notify = buffer.avail_capacity() == 0;

		file_size out = 0;
		while (out < count && !buffer.empty()) {
			*(buf++) = buffer.get();
			++out;
		}

		out_count = out;
		if (!out) {

			/* Send only EOF when at least one writer opened the pipe */
			if ((num_writers == 0) && !waiting_for_writers) {
				return Read_result::READ_OK; /* EOF */
			}

			io_progress_waiters.enqueue(handle.io_progress_elem);
			return Read_result::READ_QUEUED;
		}

		if (notify)
			submit_signal();

		return Read_result::READ_OK;
	}
};


Vfs_pipe::Pipe_handle::~Pipe_handle() {
	pipe.remove(*this); }


Vfs_pipe::Write_result
Vfs_pipe::Pipe_handle::write(const char *buf,
	                         file_size count,
	                         file_size &out_count) {
	return Pipe_handle::pipe.write(*this, buf, count, out_count); }


Vfs_pipe::Read_result
Vfs_pipe::Pipe_handle::read(char *buf,
	                        file_size count,
	                        file_size &out_count) {
	return Pipe_handle::pipe.read(*this, buf, count, out_count); }


bool
Vfs_pipe::Pipe_handle::read_ready() {
	return !writer && !pipe.buffer.empty(); }


bool
Vfs_pipe::Pipe_handle::notify_read_ready()
{
	if (!writer && !read_ready_elem.enqueued())
		pipe.read_ready_waiters.enqueue(read_ready_elem);
	return true;
}


struct Vfs_pipe::New_pipe_handle : Vfs::Vfs_handle
{
	Pipe &pipe;

	New_pipe_handle(Vfs::File_system &fs,
	                Genode::Allocator &alloc,
	                unsigned flags,
	                Pipe_space &pipe_space,
	                Genode::Signal_context_capability &notify_sigh)
	: Vfs::Vfs_handle(fs, fs, alloc, flags),
	  pipe(*(new (alloc) Pipe(alloc, pipe_space, notify_sigh)))
	{ }

	~New_pipe_handle()
	{
		pipe.remove_new_handle();
	}

	Read_result read(char *buf,
	                 file_size count,
	                 file_size &out_count)
	{
		auto name = pipe.name();
		if (name.length() < count) {
			memcpy(buf, name.string(), name.length());
			out_count = name.length();
			return Read_result::READ_OK;
		}
		return Read_result::READ_ERR_INVALID;
	}
};


class Vfs_pipe::File_system : public Vfs::File_system
{
	protected:

		Pipe_space _pipe_space { };

		/*
		 * XXX: a hack to defer cross-thread notifications at
		 * the libc until the io_progress handler
		 */
		Genode::Io_signal_handler<File_system> _notify_handler;
		Genode::Signal_context_capability _notify_cap { _notify_handler };

		void _notify_any()
		{
			_pipe_space.for_each<Pipe&>([] (Pipe &pipe) {
				pipe.notify(); });
		}

		virtual bool _get_pipe_id(const char* cpath, Pipe_space::Id &id) const
		{
			return 0 != ascii_to(cpath, id.value);
		}

	public:

		File_system(Vfs::Env &env)
		: _notify_handler(env.env().ep(), *this, &File_system::_notify_any) { }

		const char* type() override { return "pipe"; }

		/***********************
		 ** Directory service **
		 ***********************/

		Genode::Dataspace_capability dataspace(char const*) override {
			return Genode::Dataspace_capability(); }

		void release(char const*, Dataspace_capability) override { }

		Open_result open(const char *cpath,
		                 unsigned mode,
		                 Vfs::Vfs_handle **handle,
		                 Genode::Allocator &alloc) override
		{
			Path path(cpath);

			if (path == "/new") {
				if ((Directory_service::OPEN_MODE_ACCMODE & mode) == Directory_service::OPEN_MODE_WRONLY)
					return Open_result::OPEN_ERR_NO_PERM;
				*handle = new (alloc)
					New_pipe_handle(*this, alloc, mode, _pipe_space, _notify_cap);
				return Open_result::OPEN_OK;
			}

			path.strip_last_element();
			if (!path.has_single_element())
				return Open_result::OPEN_ERR_UNACCESSIBLE;

			Pipe_space::Id id { ~0UL };
			Open_result result { Open_result::OPEN_ERR_UNACCESSIBLE };
			if (_get_pipe_id(path.last_element(), id)) {
				try {
					_pipe_space.apply<Pipe&>(id, [&] (Pipe &pipe) {
						Path filename(cpath);
						filename.keep_only_last_element();
						result = pipe.open(*this, filename, handle, alloc);
					});
				}
				catch (Pipe_space::Unknown_id) { }
			}

			return result;
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

			if (path.has_single_element()) {
				Pipe_space::Id id { ~0UL };
				if (_get_pipe_id(path.last_element(), id)) {
					try {
						_pipe_space.apply<Pipe&>(id, [&] (Pipe&) {
							*handle = new (alloc)
								Vfs_handle(*this, *this, alloc, 0);
							result = OPENDIR_OK;
						});
					}
					catch (Pipe_space::Unknown_id) { }
				}
			}

			return result;
		}

		void close(Vfs::Vfs_handle *vfs_handle) override
		{
			Pipe *pipe = nullptr;
			if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle)) {
				pipe = &handle->pipe;
				if (handle->writer) {
					pipe->num_writers--;

					/* trigger reattempt of read to deliver EOF */
					if (pipe->num_writers == 0)
						pipe->submit_signal();
				}
			} else
			if (New_pipe_handle *handle = dynamic_cast<New_pipe_handle*>(vfs_handle))
				pipe = &handle->pipe;

			destroy(vfs_handle->alloc(), vfs_handle);

			if (pipe)
				pipe->cleanup();
		}

		Stat_result stat(const char *cpath, Stat &out) override
		{
			Stat_result result { STAT_ERR_NO_ENTRY };
			Path path(cpath);

			out = Stat { };

			if (path == "/new") {
				out = Stat {
					.size              = 1,
					.type              = Node_type::TRANSACTIONAL_FILE,
					.rwx               = Node_rwx::wo(),
					.inode             = Genode::addr_t(this),
					.device            = Genode::addr_t(this),
					.modification_time = { }
				};
				return STAT_OK;
			}

			if (path.has_single_element()) {
				Pipe_space::Id id { ~0UL };
				if (_get_pipe_id(path.last_element(), id)) {
					try {
						_pipe_space.apply<Pipe&>(id, [&] (Pipe &pipe) {
							out = Stat {
								.size              = 2,
								.type              = Node_type::DIRECTORY,
								.rwx               = Node_rwx::rwx(),
								.inode             = Genode::addr_t(&pipe),
								.device            = Genode::addr_t(this),
								.modification_time = { }
							};
							result = STAT_OK;
						});
					}
					catch (Pipe_space::Unknown_id) { }
				}
			} else {
				/* maybe this is /N/in or /N/out */
				path.strip_last_element();
				if (!path.has_single_element())
					/* too many directory levels */
					return result;

				Pipe_space::Id id { ~0UL };
				if (_get_pipe_id(path.last_element(), id)) {
					try {
						_pipe_space.apply<Pipe&>(id, [&] (Pipe &pipe) {
							Path filename(cpath);
							filename.keep_only_last_element();
							if (filename == "/in") {
								out = Stat {
									.size              = file_size(pipe.buffer.avail_capacity()),
									.type              = Node_type::CONTINUOUS_FILE,
									.rwx               = Node_rwx::wo(),
									.inode             = Genode::addr_t(&pipe) + 1,
									.device            = Genode::addr_t(this),
									.modification_time = { }
								};
								result = STAT_OK;
							} else
							if (filename == "/out") {
								out = Stat {
									.size              = file_size(PIPE_BUF_SIZE
									                             - pipe.buffer.avail_capacity()),
									.type              = Node_type::CONTINUOUS_FILE,
									.rwx               = Node_rwx::ro(),
									.inode             = Genode::addr_t(&pipe) + 2,
									.device            = Genode::addr_t(this),
									.modification_time = { }
								};
								result = STAT_OK;
							}
						});
					}
					catch (Pipe_space::Unknown_id) { }
				}
			}

			return result;
		}

		Unlink_result unlink(const char*) override {
			return UNLINK_ERR_NO_ENTRY; }

		Rename_result rename(const char*, const char*) override {
			return RENAME_ERR_NO_ENTRY; }

		file_size num_dirent(char const *) override { return 0; }

		bool directory(char const *cpath) override
		{
			Path path(cpath);
			if (path == "/") return true;

			if (!path.has_single_element()) return false;

			bool result = false;
			Pipe_space::Id id { ~0UL };
			if (_get_pipe_id(path.last_element(), id)) {
				try {
					_pipe_space.apply<Pipe&>(id, [&] (Pipe &) {
						result = true; });
				}
				catch (Pipe_space::Unknown_id) { }
			}

			return result;
		}

		const char* leaf_path(const char *cpath) override
		{
			Path path(cpath);
			if (path == "/") return cpath;
			if (path == "/new") return cpath;

			char const *result = nullptr;
			if (!path.has_single_element()) {
				/* maybe this is /N/in or /N/out */
				path.strip_last_element();
				if (!path.has_single_element()) {
					/* too many directory levels */
					return nullptr;
				}

				Path filename(cpath);
				filename.keep_only_last_element();
				if (filename != "/in" && filename != "/out") {
					/* not a pipe file */
					return nullptr;
				}
			}

			Pipe_space::Id id { ~0UL };
			if (_get_pipe_id(path.last_element(), id)) {
				try {
					/* check if the pipe directory exists */
					_pipe_space.apply<Pipe&>(id, [&] (Pipe &) {
						result = cpath; });
				}
				catch (Pipe_space::Unknown_id) { }
			}

			return result;
		}

		/**********************
		 ** File I/O service **
		 **********************/

		Write_result write(Vfs_handle *vfs_handle,
		                   const char *src, file_size count,
		                   file_size &out_count) override
		{
			if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle))
				return handle->write(src, count, out_count);

			return WRITE_ERR_INVALID;
		}

		Read_result complete_read(Vfs_handle *vfs_handle,
		                          char *dst, file_size count,
		                          file_size &out_count) override
		{
			if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle))
				return handle->read(dst, count, out_count);

			if (New_pipe_handle *handle = dynamic_cast<New_pipe_handle*>(vfs_handle))
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


class Vfs_pipe::Fifo_file_system : public Vfs_pipe::File_system
{
	private:

		class Fifo_element : public Genode::List<Fifo_element>::Element
		{
			private:
				Path _path;
				Pipe_space::Id _id;
			public:
				Fifo_element(Path const &path, Pipe_space::Id const &id)
				: _path(path), _id(id) { }
				Path const &path() const { return _path; }
				Pipe_space::Id const &id() const { return _id; }
		};
		Genode::List<Fifo_element> _elements { };
		Genode::Mutex mutable      _mutex    { };
		Vfs::Env                  &_env;

	protected:

		virtual bool _get_pipe_id(const char* cpath, Pipe_space::Id &id) const override
		{
			Genode::Mutex::Guard guard(_mutex);
			for (auto element=_elements.first(); element; element=element->next()) {
				if (element->path() == Path(cpath)) {
					id = element->id();
					return true;
				}
			}
			return false;
		}

	public:

		Fifo_file_system(Vfs::Env &env, Genode::Xml_node const &config)
		: File_system(env), _env(env)
		{
			config.for_each_sub_node("fifo", [&env, this] (Xml_node const &fifo) {
				auto path = fifo.attribute_value("name", Genode::String<MAX_PATH_LENGTH>());

				Pipe* pipe = new (env.alloc())
					Pipe(env.alloc(), _pipe_space, _notify_cap);
				if (pipe) {
					Genode::Mutex::Guard guard(_mutex);
					auto element = new (env.alloc())
						Fifo_element(path, pipe->space_elem.id());
					if (element) {
						_elements.insert(element);
					} else {
						Genode::error("could not allocate fifo element");
					}
				} else {
					Genode::error("could not allocate fifo pipe");
				}
			});
		}

		~Fifo_file_system()
		{
			Genode::Mutex::Guard guard(_mutex);
			for (auto element=_elements.first(); element; element=element->next()) {
				_elements.remove(element);
				destroy(_env.alloc(), element);
			}
		}

		virtual Open_result open(const char *cpath,
		                         unsigned mode,
		                         Vfs::Vfs_handle **handle,
		                         Genode::Allocator &alloc) override
		{
			if (mode & OPEN_MODE_CREATE) {
				Genode::warning("cannot open fifo pipe with OPEN_MODE_CREATE");
				return OPEN_ERR_NO_PERM;
			}

			Path const path { cpath };
			if (!path.has_single_element()) {
				return Open_result::OPEN_ERR_UNACCESSIBLE;
			}
			if (!((mode == Open_mode::OPEN_MODE_RDONLY) ||
			      (mode == Open_mode::OPEN_MODE_WRONLY))) {
				Genode::error("fifo pipe only supports opening with WO or RO mode");
				return Open_result::OPEN_ERR_UNACCESSIBLE;
			}

			Pipe_space::Id id { ~0UL };
			Open_result result { Open_result::OPEN_ERR_UNACCESSIBLE };
			if (_get_pipe_id(cpath, id)) {
				try {
					_pipe_space.apply<Pipe&>(id, [&] (Pipe &pipe) {
						auto type { (mode == Open_mode::OPEN_MODE_RDONLY) ? "/out" : "/in" };
						result = pipe.open(*this, type, handle, alloc);
					});
				}
				catch (Pipe_space::Unknown_id) { }
			}

			return result;
		}

		Stat_result stat(const char *cpath, Stat &out) override
		{
			out = Stat { };
			Stat_result result { STAT_ERR_NO_ENTRY };
			Path const path { cpath };

			if (path.has_single_element()) {
				Pipe_space::Id id { ~0UL };
				if (_get_pipe_id(cpath, id)) {
					try {
						_pipe_space.apply<Pipe&>(id, [&out, &result, this] (Pipe &pipe) {
							out = Stat {
								.size              = file_size(0),
								.type              = Node_type::CONTINUOUS_FILE,
								.rwx               = Node_rwx::rw(),
								.inode             = Genode::addr_t(&pipe),
								.device            = Genode::addr_t(this),
								.modification_time = { }
							};
							result = STAT_OK;
						});
					}
					catch (Pipe_space::Unknown_id) { }
				}
			}

			return result;
		}

		bool directory(char const *cpath) override
		{
			Path const path { cpath };
			if (path == "/") return true;
			if (!path.has_single_element()) return false;

			bool result { true };
			Pipe_space::Id id { ~0UL };
			if (_get_pipe_id(cpath, id)) {
				try {
					_pipe_space.apply<Pipe&>(id, [&result, &cpath] (Pipe &) {
						result = false; });
				}
				catch (Pipe_space::Unknown_id) { }
			}

			return result;
		}

		char const *leaf_path(char const *cpath) override
		{
			Path path { cpath };
			if (path == "/") return cpath;
			if (!path.has_single_element()) return nullptr;

			char const *result { nullptr };
			Pipe_space::Id id { ~0UL };
			if (_get_pipe_id(path.last_element(), id)) {
				try {
					/* check if the pipe exists */
					_pipe_space.apply<Pipe&>(id, [&result, &cpath] (Pipe &) {
						result = cpath; });
				}
				catch (Pipe_space::Unknown_id) { }
			}

			return result;
		}

};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node node) override
		{
			if (node.has_sub_node("fifo")) {
				return new (env.alloc()) Vfs_pipe::Fifo_file_system(env, node);
			} else {
				return new (env.alloc()) Vfs_pipe::File_system(env);
			}
		}
	};

	static Factory f;
	return &f;
}
