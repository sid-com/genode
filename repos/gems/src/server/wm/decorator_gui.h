/*
 * \brief  GUI service provided to decorator
 * \author Norman Feske
 * \date   2014-02-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DECORATOR_GUI_H_
#define _DECORATOR_GUI_H_

/* Genode includes */
#include <util/string.h>
#include <input_session/client.h>
#include <input/event.h>
#include <input/component.h>

/* local includes */
#include <types.h>
#include <window_registry.h>
#include <pointer.h>
#include <real_gui.h>

namespace Wm {

	class  Main;
	struct Decorator_gui_session;
	struct Decorator_content_callback;
	struct Decorator_content_registry;
}


struct Wm::Decorator_content_callback : Interface
{
	virtual void content_geometry(Window_registry::Id win_id, Rect rect) = 0;

	virtual Gui::View_capability content_view(Window_registry::Id win_id) = 0;

	virtual void update_content_child_views(Window_registry::Id win_id) = 0;

	virtual void hide_content_child_views(Window_registry::Id win_id) = 0;
};


struct Wm::Decorator_gui_session : Session_object<Gui::Session>,
                                   private List<Decorator_gui_session>::Element,
                                   private Upgradeable
{
	friend class List<Decorator_gui_session>;
	using List<Decorator_gui_session>::Element::next;

	using View_capability = Gui::View_capability;
	using View_id         = Gui::View_id;
	using Command_buffer  = Gui::Session::Command_buffer;

	struct Content_view_ref : Gui::View_ref
	{
		Gui::View_ids::Element id;

		Window_registry::Id win_id;

		Content_view_ref(Window_registry::Id win_id, Gui::View_ids &ids, View_id id)
		: id(*this, ids, id), win_id(win_id) { }
	};

	Gui::View_ids _content_view_ids { };

	Env &_env;

	Constrained_ram_allocator _ram { _env.ram(), _ram_quota_guard(), _cap_quota_guard() };

	Sliced_heap _session_alloc { _ram, _env.rm() };

	Slab<Content_view_ref, 4000> _content_view_ref_alloc { _session_alloc };

	Real_gui _real_gui { _env, "decorator" };

	Input::Session_client _input_session { _env.rm(), _real_gui.session.input() };

	Signal_context_capability _mode_sigh { };

	Attached_ram_dataspace _client_command_ds { _ram, _env.rm(), sizeof(Command_buffer) };

	Command_buffer &_client_command_buffer = *_client_command_ds.local_addr<Command_buffer>();

	Pointer::State _pointer_state;

	Input::Session_component &_window_layouter_input;

	Decorator_content_callback &_content_callback;

	struct Dummy_input_action : Input::Session_component::Action
	{
		void exclusive_input_requested(bool) override { };

	} _input_action { };

	/* Gui::Connection requires a valid input session */
	Input::Session_component _dummy_input_component {
		_env.ep(), _env.ram(), _env.rm(), _input_action };

	Signal_handler<Decorator_gui_session>
		_input_handler { _env.ep(), *this, &Decorator_gui_session::_handle_input };

	void _with_win_id_from_title(Gui::Title const &title, auto const &fn)
	{
		unsigned value = 0;
		if (ascii_to(title.string(), value))
			fn(Window_registry::Id { value });
	}

	Decorator_gui_session(Env                        &env,
	                      Resources            const &resources,
	                      Label                const &label,
	                      Diag                 const &diag,
	                      Pointer::Tracker           &pointer_tracker,
	                      Input::Session_component   &window_layouter_input,
	                      Decorator_content_callback &content_callback)
	:
		Session_object<Gui::Session>(env.ep(), resources, label, diag),
		_env(env),
		_pointer_state(pointer_tracker),
		_window_layouter_input(window_layouter_input),
		_content_callback(content_callback)
	{
		_input_session.sigh(_input_handler);
	}

	~Decorator_gui_session()
	{
		while (_content_view_ids.apply_any<Content_view_ref>([&] (Content_view_ref &view_ref) {
			destroy(_content_view_ref_alloc, &view_ref); }));
	}

	void upgrade_local_or_remote(Resources const &resources)
	{
		_upgrade_local_or_remote(resources, *this, _real_gui);
	}

	void _handle_input()
	{
		while (_input_session.pending())
			_input_session.for_each_event([&] (Input::Event const &ev) {
				_pointer_state.apply_event(ev);
				_window_layouter_input.submit(ev); });
	}

	void _execute_command(Command const &cmd)
	{
		switch (cmd.opcode) {

		case Command::GEOMETRY:

			/*
			 * If the content view changes position, propagate the new position
			 * to the GUI service to properly transform absolute input
			 * coordinates.
			 */
			_content_view_ids.apply<Content_view_ref const>(cmd.geometry.view,
				[&] (Content_view_ref const &view_ref) {
					_content_callback.content_geometry(view_ref.win_id, cmd.geometry.rect); },
				[&] { });

			/* forward command */
			_real_gui.enqueue(cmd);
			return;

		case Command::OFFSET:

			/*
			 * If non-content views change their offset (if the lookup
			 * fails), propagate the event
			 */
			_content_view_ids.apply<Content_view_ref const>(cmd.geometry.view,
				[&] (Content_view_ref const &) { },
				[&] { _real_gui.enqueue(cmd); });
			return;

		case Command::FRONT:
		case Command::BACK:
		case Command::FRONT_OF:
		case Command::BEHIND_OF:

			_real_gui.enqueue(cmd);
			_content_view_ids.apply<Content_view_ref const>(cmd.front.view,
				[&] (Content_view_ref const &view_ref) {
					_real_gui.execute();
					_content_callback.update_content_child_views(view_ref.win_id); },
				[&] { });

			return;

		case Command::TITLE:
		case Command::BACKGROUND:
		case Command::NOP:

			_real_gui.enqueue(cmd);
			return;
		}
	}

	Pointer::Position last_observed_pointer_pos() const
	{
		return _pointer_state.last_observed_pos();
	}


	/***************************
	 ** GUI session interface **
	 ***************************/
	
	Framebuffer::Session_capability framebuffer() override
	{
		return _real_gui.session.framebuffer();
	}

	Input::Session_capability input() override
	{
		/*
		 * Deny input to the decorator. User input referring to the
		 * window decorations is routed to the window manager.
		 */
		return _dummy_input_component.cap();
	}

	Info_result info() override
	{
		return _real_gui.session.info();
	}

	View_result view(View_id id, View_attr const &attr) override
	{
		/*
		 * The decorator marks a content view by specifying the window ID
		 * as view title. For such views, we import the view from the
		 * corresponding GUI cient instead of creating a new view.
		 */
		bool out_of_ram = false, out_of_caps = false, associated = false;
		_with_win_id_from_title(attr.title, [&] (Window_registry::Id win_id) {
			try {
				Content_view_ref &view_ref_ptr = *new (_content_view_ref_alloc)
					Content_view_ref(Window_registry::Id(win_id), _content_view_ids, id);

				View_capability view_cap = _content_callback.content_view(win_id);
				Associate_result result = _real_gui.session.associate(id, view_cap);
				if (result != Associate_result::OK)
					destroy(_content_view_ref_alloc, &view_ref_ptr);

				switch (result) {
				case Associate_result::OUT_OF_RAM:  out_of_ram  = true; break;
				case Associate_result::OUT_OF_CAPS: out_of_caps = true; break;
				case Associate_result::OK:          associated  = true; break;
				case Associate_result::INVALID:     break; /* fall back to regular view */
				};
			}
			catch (Out_of_ram)  { _starved_for_ram  = out_of_ram  = true; }
			catch (Out_of_caps) { _starved_for_caps = out_of_caps = true; }
		});

		if (out_of_ram)  return View_result::OUT_OF_RAM;
		if (out_of_caps) return View_result::OUT_OF_CAPS;
		if (associated)  return View_result::OK;

		return _real_gui.session.view(id, attr);
	}

	Child_view_result child_view(View_id id, View_id parent, View_attr const &attr) override
	{
		return _real_gui.session.child_view(id, parent, attr);
	}

	void destroy_view(View_id view) override
	{
		/*
		 * Reset view geometry when destroying a content view
		 */
		_content_view_ids.apply<Content_view_ref>(view,
			[&] (Content_view_ref &view_ref) {

				_content_callback.hide_content_child_views(view_ref.win_id);

				Gui::Rect rect(Gui::Point(0, 0), Gui::Area(0, 0));
				_real_gui.enqueue<Gui::Session::Command::Geometry>(view, rect);
				_real_gui.execute();

				destroy(_content_view_ref_alloc, &view_ref);
			},
			[&] { });

		_real_gui.session.destroy_view(view);
	}

	Associate_result associate(View_id id, View_capability view_cap) override
	{
		return _real_gui.session.associate(id, view_cap);
	}

	View_capability_result view_capability(View_id view) override
	{
		return _real_gui.session.view_capability(view);
	}

	void release_view_id(View_id view) override
	{
		_content_view_ids.apply<Content_view_ref>(view,
			[&] (Content_view_ref &view_ref) { destroy(_content_view_ref_alloc, &view_ref); },
			[&] { });

		_real_gui.session.release_view_id(view);
	}

	Dataspace_capability command_dataspace() override
	{
		return _client_command_ds.cap();
	}

	void execute() override
	{
		for (unsigned i = 0; i < _client_command_buffer.num(); i++)
			_execute_command(_client_command_buffer.get(i));

		_real_gui.execute();
	}

	Buffer_result buffer(Framebuffer::Mode mode) override
	{
		return _real_gui.session.buffer(mode);
	}

	void focus(Capability<Gui::Session>) override { }
};

#endif /* _DECORATOR_GUI_H_ */
