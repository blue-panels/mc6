runtime_pluginsdir = $(runtime_plugins_dir)
runtime_plugins_LTLIBRARIES = mc-lua.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(GLIB_CFLAGS) \
	$(LUA_CFLAGS) \
	-DMC_LUA_SYSTEM_SCRIPTS_DIR=\"$(lua_scripts_dir)\" \
	-DMC_LUA_SYSTEM_MODULES_DIR=\"$(lua_modules_dir)\"

mc_lua_la_SOURCES = \
	mc-lua.c
mc_lua_la_LDFLAGS = -module -avoid-version
mc_lua_la_LIBADD = $(GLIB_LIBS) $(LUA_LIBS)

lua_chafadir = $(lua_scripts_dir)/mc/lua-chafa
if ENABLE_LUA_CHAFA
dist_lua_chafa_DATA = \
	scripts/mc/lua-chafa/lua.ini \
	scripts/mc/lua-chafa/init.lua \
	scripts/mc/lua-chafa/help.hlp
endif

lua_sixeldir = $(lua_scripts_dir)/mc/lua-sixel
dist_lua_sixel_DATA = \
	scripts/mc/lua-sixel/lua.ini \
	scripts/mc/lua-sixel/init.lua \
	scripts/mc/lua-sixel/help.hlp

lua_java_classdir = $(lua_scripts_dir)/mc/lua-java-class
if ENABLE_LUA_JAVA_CLASS
dist_lua_java_class_DATA = \
	scripts/mc/lua-java-class/lua.ini \
	scripts/mc/lua-java-class/init.lua
endif

lua_dbfdir = $(lua_scripts_dir)/mc/lua-dbf
dist_lua_dbf_DATA = \
	scripts/mc/lua-dbf/lua.ini \
	scripts/mc/lua-dbf/init.lua \
	scripts/mc/lua-dbf/help.hlp

lua_readelfdir = $(lua_scripts_dir)/mc/lua-readelf
if ENABLE_LUA_READELF
dist_lua_readelf_DATA = \
	scripts/mc/lua-readelf/lua.ini \
	scripts/mc/lua-readelf/init.lua
endif

lua_base64_decodedir = $(lua_scripts_dir)/editor/base64-decode
dist_lua_base64_decode_DATA = \
	scripts/editor/base64-decode/lua.ini \
	scripts/editor/base64-decode/init.lua

lua_insert_literaldir = $(lua_scripts_dir)/editor/insert-literal
dist_lua_insert_literal_DATA = \
	scripts/editor/insert-literal/lua.ini \
	scripts/editor/insert-literal/init.lua

lua_insert_datetimedir = $(lua_scripts_dir)/editor/insert-datetime
dist_lua_insert_datetime_DATA = \
	scripts/editor/insert-datetime/lua.ini \
	scripts/editor/insert-datetime/init.lua

lua_format_paragraphdir = $(lua_scripts_dir)/editor/format-paragraph
dist_lua_format_paragraph_DATA = \
	scripts/editor/format-paragraph/lua.ini \
	scripts/editor/format-paragraph/init.lua

lua_sort_selectiondir = $(lua_scripts_dir)/editor/sort-selection
dist_lua_sort_selection_DATA = \
	scripts/editor/sort-selection/lua.ini \
	scripts/editor/sort-selection/init.lua

lua_insert_command_outputdir = $(lua_scripts_dir)/editor/insert-command-output
dist_lua_insert_command_output_DATA = \
	scripts/editor/insert-command-output/lua.ini \
	scripts/editor/insert-command-output/init.lua

lua_draw_tabledir = $(lua_scripts_dir)/editor/draw-table
dist_lua_draw_table_DATA = \
	scripts/editor/draw-table/lua.ini \
	scripts/editor/draw-table/init.lua

# Teaching examples: installed for reference only, never loaded by the engine.
lua_examples_dir = $(pkgdatadir)/lua/examples

lua_example_demo_paneldir = $(lua_examples_dir)/mc/demo-panel
dist_lua_example_demo_panel_DATA = \
	examples/mc/demo-panel/lua.ini \
	examples/mc/demo-panel/init.lua

lua_example_notify_editor_savedir = $(lua_examples_dir)/editor/notify-editor-save
dist_lua_example_notify_editor_save_DATA = \
	examples/editor/notify-editor-save/lua.ini \
	examples/editor/notify-editor-save/init.lua
