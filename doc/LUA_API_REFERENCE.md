# Lua API reference

This file is generated from annotations in `src/lua/mc-lua.c`.
Do not edit it manually; run `python3 maint/generate-lua-api.py`.

## Workspace `mc`

| Lua method | Description | Capability | Mutation |
|---|---|---|---|
| `mc.panel_provider.register(spec) -> registration\|nil, error?` | Register a virtual panel namespace and its callbacks. | `panel_provider` | yes |
| `panel:chdir(path) -> boolean\|nil, error?` | Change the panel's current directory. | `panel` | yes |
| `panel:current() -> table\|nil, error?` | Return a snapshot of the current entry. | `panel` | no |
| `panel:cwd() -> string\|nil, error?` | Return the panel's current directory. | `panel` | no |
| `panel:refresh() -> boolean\|nil, error?` | Refresh the panel contents. | `panel` | yes |
| `panel:selected() -> table[]\|nil, error?` | Return snapshots of marked entries. | `panel` | no |

## Workspace `mcedit`

| Lua method | Description | Capability | Mutation |
|---|---|---|---|
| `editor:cursor() -> Position\|nil, error?` | Return the current cursor position. | `editor` | no |
| `editor:edit(spec) -> EditResult\|nil, error?` | Apply replacements atomically as one undo operation. | `editor` | yes |
| `editor:get_text() -> string\|nil, error?` | Read the complete buffer using the compatibility API. | `editor` | no |
| `editor:info() -> DocumentInfo\|nil, error?` | Return document metadata and the current revision. | `editor` | no |
| `editor:insert(text) -> boolean\|nil, error?` | Insert text at the cursor using the compatibility API. | `editor` | yes |
| `editor:is_readonly() -> boolean\|nil, error?` | Report whether the document is read-only. | `editor` | no |
| `editor:path() -> string\|nil, error?` | Return the document path. | `editor` | no |
| `editor:replace(range, text) -> EditResult\|nil, error?` | Replace a byte range in the editor buffer. | `editor` | yes |
| `editor:replace_selection(text, options?) -> EditResult\|nil, error?` | Replace the selection, or insert when it is empty. | `editor` | yes |
| `editor:save() -> boolean\|nil, error?` | Save the document with the native editor operation. | `editor` | yes |
| `editor:selected_text() -> string\|nil, error?` | Read selected text using the compatibility API. | `editor` | no |
| `editor:selection() -> Selection\|nil, error?` | Return the current selection snapshot. | `editor` | no |
| `editor:set_cursor(position) -> boolean\|nil, error?` | Move the cursor to a validated position. | `editor` | yes |
| `editor:tab_width() -> integer\|nil, error?` | Return the configured tab width. | `editor` | no |
| `editor:text(range?) -> string\|nil, error?` | Read the complete buffer or a byte range. | `editor` | no |
| `mc.editor.current() -> editor\|nil, error?` | Return the editor associated with the active callback. | `editor` | no |
| `mc.macro(spec) -> boolean\|nil, error?` | Register an editor action with optional key and menu placement. | `events` | yes |

## Workspace `mcview`

| Lua method | Description | Capability | Mutation |
|---|---|---|---|
| `mc.viewer.current() -> viewer\|nil, error?` | Return the viewer associated with the active callback. | `viewer` | no |
| `viewer:goto(offset) -> boolean\|nil, error?` | Move the viewer to a byte offset. | `viewer` | yes |
| `viewer:mode() -> string\|nil, error?` | Return the active viewer mode. | `viewer` | no |
| `viewer:path() -> string\|nil, error?` | Return the viewed file path. | `viewer` | no |
| `viewer:position() -> integer\|nil, error?` | Return the current byte offset. | `viewer` | no |

## Workspace `any`

| Lua method | Description | Capability | Mutation |
|---|---|---|---|
| `definition:create(argument, params?) -> controller` | Create a viewer-source controller and its package session. | `viewer_source` | yes |
| `mc.log.debug(message) -> nil` | Write a debug message to the Lua runtime log. | `—` | no |
| `mc.log.error(message) -> nil` | Write an error to the Lua runtime log. | `—` | no |
| `mc.log.info(message) -> nil` | Write an informational message to the Lua runtime log. | `—` | no |
| `mc.log.warn(message) -> nil` | Write a warning to the Lua runtime log. | `—` | no |
| `mc.off(subscription) -> boolean` | Remove an event subscription owned by the package. | `events` | yes |
| `mc.on(event, callback, options?) -> integer\|nil, error?` | Subscribe the package to a named MC event. | `events` | yes |
| `mc.panel.active() -> panel\|nil, error?` | Return a handle to the active panel. | `panel` | no |
| `mc.panel.passive() -> panel\|nil, error?` | Return a handle to the passive panel. | `panel` | no |
| `mc.process.run(spec) -> ProcessResult\|nil, error?` | Run a shell command and capture its bounded output. | `process` | yes |
| `mc.source.bytes(spec) -> Source` | Describe an in-memory byte source. | `—` | no |
| `mc.source.file(spec) -> Source` | Describe a local-file source and its ownership. | `—` | no |
| `mc.source.pipeline(stages) -> Source` | Compose process sources into one pipeline. | `—` | no |
| `mc.source.process(spec) -> Source` | Describe a process source using argv and an optional working directory. | `—` | no |
| `mc.ui.dialog(spec) -> DialogResult\|nil, error?` | Show a declarative native modal dialog. | `ui` | yes |
| `mc.ui.indicator(spec) -> boolean\|nil, error?` | Set or replace a package-owned persistent UI indicator. | `ui` | yes |
| `mc.ui.indicator_clear(id, area?) -> boolean\|nil, error?` | Remove a package-owned UI indicator. | `ui` | yes |
| `mc.ui.message(title, text) -> boolean\|nil, error?` | Show a native informational message box. | `ui` | yes |
| `mc.ui.open_diff(spec) -> boolean\|nil, error?` | Compare two byte strings in the native diff viewer. | `diff` | yes |
| `mc.ui.open_viewer(spec) -> boolean\|nil, error?` | Open the native viewer and transfer a controller to MC. | `viewer_source` | yes |
| `mc.ui.status(text) -> boolean\|nil, error?` | Display transient text in the MC status area. | `ui` | yes |
| `mc.ui.text_width(text) -> integer\|nil, error?` | Measure UTF-8 text using terminal display columns. | `ui` | no |
| `mc.viewer_source.define(spec) -> definition\|nil, error?` | Define a reusable family of managed viewer sources. | `viewer_source` | yes |

## Callback contracts

| Callback | Workspace | Capability |
|---|---|---|
| `action(event) -> mc.PASS\|mc.CONSUME` | `mcedit` | `events` |
| `close(instance)` | `mc` | `panel_provider` |
| `close(session)` | `any` | `viewer_source` |
| `copy_connection(host, connection) -> Connection` | `mc` | `panel_provider` |
| `delete_connection(host, connection) -> boolean` | `mc` | `panel_provider` |
| `edit_connection(host, connection) -> Connection` | `mc` | `panel_provider` |
| `enter(instance, entry, revision) -> OperationResult` | `mc` | `panel_provider` |
| `event(snapshot) -> nil` | `any` | `events` |
| `initial_params(session, params) -> params` | `any` | `viewer_source` |
| `invoke_action(instance, request) -> OperationResult` | `mc` | `panel_provider` |
| `list(instance) -> PanelView` | `mc` | `panel_provider` |
| `navigate(instance, request) -> OperationResult` | `mc` | `panel_provider` |
| `new_connection(host) -> Connection` | `mc` | `panel_provider` |
| `open(argument) -> session` | `any` | `viewer_source` |
| `open(host, connection?) -> instance` | `mc` | `panel_provider` |
| `open_read(instance, entry) -> Source` | `mc` | `panel_provider` |
| `options(session, params) -> params?` | `any` | `viewer_source` |
| `prepare(session, params) -> ViewerSpec` | `any` | `viewer_source` |
| `reload(instance) -> OperationResult` | `mc` | `panel_provider` |
| `rename_connection(host, connection) -> Connection` | `mc` | `panel_provider` |
| `view(instance, entry, request) -> ViewerSpec` | `mc` | `panel_provider` |
