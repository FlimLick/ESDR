extends Control

const _EXTENSION_PATH: String = "res://esdr.gdextension"
const _WATERFALL_PANEL_SCRIPT: Script = preload("res://scripts/waterfall_panel.gd")

const _NODE_MENU_RTL: int = 1
const _NODE_MENU_NFM: int = 2
const _NODE_MENU_AUDIO_SINK: int = 3
const _NODE_MENU_AUDIO_GENERATOR: int = 4
const _NODE_MENU_AUDIO_MIXER: int = 5
const _NODE_MENU_MATH: int = 6
const _NODE_MENU_CONVERTER: int = 7
const _NODE_MENU_WFM: int = 8
const _NODE_MENU_AM: int = 9
const _NODE_MENU_USB: int = 10
const _NODE_MENU_LSB: int = 11
const _NODE_MENU_DSB: int = 12
const _NODE_MENU_CW: int = 13
const _NODE_MENU_BASEBAND_REPLAY: int = 14
const _NODE_MENU_AUDIO_REPLAY: int = 15
const _NODE_MENU_AUDIO_STEREO_MATRIX: int = 16
const _SIGNAL_BASEBAND_TYPE: int = 8
const _SIGNAL_AUDIO_TYPE: int = 9

const _NODE_MENU_SOURCES: Array[int] = [_NODE_MENU_RTL, _NODE_MENU_BASEBAND_REPLAY]
const _NODE_MENU_DEMODULATORS: Array[int] = [_NODE_MENU_NFM, _NODE_MENU_WFM, _NODE_MENU_AM, _NODE_MENU_USB, _NODE_MENU_LSB, _NODE_MENU_DSB, _NODE_MENU_CW]
const _NODE_MENU_AUDIO: Array[int] = [_NODE_MENU_AUDIO_GENERATOR, _NODE_MENU_AUDIO_MIXER, _NODE_MENU_AUDIO_STEREO_MATRIX, _NODE_MENU_AUDIO_REPLAY]
const _NODE_MENU_SINKS: Array[int] = [_NODE_MENU_AUDIO_SINK]
const _NODE_MENU_MATH_GROUP: Array[int] = [_NODE_MENU_MATH, _NODE_MENU_CONVERTER]

const _NODE_SPECS: Dictionary = {
	_NODE_MENU_RTL: {
		"label": "RTL-SDR Source",
		"factory": "create_rtl_sdr_source",
		"class": "RTLSDRSource",
		"name_prefix": "RTLSDRSource",
		"error": "RTLSDRSource class is not available.",
		"spawn_base": Vector2(200, 0),
		"spawn_step": Vector2(60, 40),
	},
	_NODE_MENU_NFM: {
		"label": "NFM Demodulator",
		"factory": "create_nfm_demodulator",
		"class": "NFMDemodulator",
		"name_prefix": "NFMDemodulator",
		"error": "NFMDemodulator class is not available.",
		"spawn_base": Vector2(800, 200),
		"spawn_step": Vector2(40, 30),
	},
	_NODE_MENU_WFM: {
		"label": "WFM Demodulator",
		"factory": "create_wfm_demodulator",
		"class": "WFMDemodulator",
		"name_prefix": "WFMDemodulator",
		"error": "WFMDemodulator class is not available.",
		"spawn_base": Vector2(1080, 200),
		"spawn_step": Vector2(40, 30),
	},
	_NODE_MENU_AM: {
		"label": "AM Demodulator",
		"factory": "create_am_demodulator",
		"class": "AMDemodulator",
		"name_prefix": "AMDemodulator",
		"error": "AMDemodulator class is not available.",
		"spawn_base": Vector2(1360, 200),
		"spawn_step": Vector2(40, 30),
	},
	_NODE_MENU_USB: {
		"label": "USB Demodulator",
		"factory": "create_usb_demodulator",
		"class": "SSBDemodulator",
		"mode": 0,
		"name_prefix": "USBDemodulator",
		"error": "SSBDemodulator class is not available.",
		"spawn_base": Vector2(1640, 200),
		"spawn_step": Vector2(40, 30),
	},
	_NODE_MENU_LSB: {
		"label": "LSB Demodulator",
		"factory": "create_lsb_demodulator",
		"class": "SSBDemodulator",
		"mode": 1,
		"name_prefix": "LSBDemodulator",
		"error": "SSBDemodulator class is not available.",
		"spawn_base": Vector2(1880, 200),
		"spawn_step": Vector2(40, 30),
	},
	_NODE_MENU_DSB: {
		"label": "DSB Demodulator",
		"factory": "create_dsb_demodulator",
		"class": "SSBDemodulator",
		"mode": 2,
		"name_prefix": "DSBDemodulator",
		"error": "SSBDemodulator class is not available.",
		"spawn_base": Vector2(2120, 200),
		"spawn_step": Vector2(40, 30),
	},
	_NODE_MENU_CW: {
		"label": "CW Demodulator",
		"factory": "create_cw_demodulator",
		"class": "CWDemodulator",
		"name_prefix": "CWDemodulator",
		"error": "CWDemodulator class is not available.",
		"spawn_base": Vector2(2360, 200),
		"spawn_step": Vector2(40, 30),
	},
	_NODE_MENU_AUDIO_SINK: {
		"label": "Audio Sink",
		"factory": "create_audio_sink",
		"class": "AudioSink",
		"name_prefix": "AudioSink",
		"error": "AudioSink class is not available.",
		"spawn_base": Vector2(1200, 200),
		"spawn_step": Vector2(30, 30),
	},
	_NODE_MENU_AUDIO_GENERATOR: {
		"label": "Audio Generator",
		"factory": "create_audio_generator_source",
		"class": "AudioGeneratorSource",
		"name_prefix": "AudioGeneratorSource",
		"error": "AudioGeneratorSource class is not available.",
		"spawn_base": Vector2(500, 420),
		"spawn_step": Vector2(40, 30),
	},
	_NODE_MENU_AUDIO_MIXER: {
		"label": "Audio Mixer",
		"factory": "create_audio_mixer",
		"class": "AudioMixer",
		"name_prefix": "AudioMixer",
		"error": "AudioMixer class is not available.",
		"spawn_base": Vector2(900, 420),
		"spawn_step": Vector2(40, 30),
	},
	_NODE_MENU_AUDIO_STEREO_MATRIX: {
		"label": "Audio Stereo Matrix",
		"factory": "create_audio_stereo_matrix",
		"class": "AudioStereoMatrix",
		"name_prefix": "AudioStereoMatrix",
		"error": "AudioStereoMatrix class is not available.",
		"spawn_base": Vector2(1040, 420),
		"spawn_step": Vector2(40, 30),
	},
	_NODE_MENU_BASEBAND_REPLAY: {
		"label": "Baseband Replay",
		"factory": "create_baseband_replay",
		"class": "BasebandReplay",
		"name_prefix": "BasebandReplay",
		"error": "BasebandReplay class is not available.",
		"spawn_base": Vector2(520, 120),
		"spawn_step": Vector2(36, 28),
	},
	_NODE_MENU_AUDIO_REPLAY: {
		"label": "Audio Replay",
		"factory": "create_audio_replay",
		"class": "AudioReplay",
		"name_prefix": "AudioReplay",
		"error": "AudioReplay class is not available.",
		"spawn_base": Vector2(1220, 420),
		"spawn_step": Vector2(36, 28),
	},
	_NODE_MENU_MATH: {
		"label": "Math",
		"factory": "create_math_operator",
		"class": "MathOperator",
		"name_prefix": "MathOperator",
		"error": "MathOperator class is not available.",
		"spawn_base": Vector2(650, 90),
		"spawn_step": Vector2(30, 30),
	},
	_NODE_MENU_CONVERTER: {
		"label": "Value Converter",
		"factory": "create_value_type_converter",
		"class": "ValueTypeConverter",
		"name_prefix": "ValueTypeConverter",
		"error": "ValueTypeConverter class is not available.",
		"spawn_base": Vector2(760, 120),
		"spawn_step": Vector2(28, 28),
	},
}

const _GRAPH_CTX_ADD_NODE: int = 1
const _GRAPH_CTX_DISCONNECT_SELECTED: int = 2
const _GRAPH_CTX_DELETE_SELECTED: int = 3
const _GRAPH_CTX_COPY_SELECTED: int = 4
const _GRAPH_CTX_PASTE: int = 5
const _GRAPH_CTX_RENAME_SELECTED: int = 6
const _GRAPH_CTX_REPLACE_SELECTED_DEMOD: int = 7

const _MODULE_CTX_DISCONNECT_LINKS: int = 1
const _MODULE_CTX_DELETE_NODE: int = 2
const _MODULE_CTX_COPY_NODE: int = 3
const _MODULE_CTX_RENAME_NODE: int = 4
const _MODULE_CTX_REPLACE_DEMOD: int = 5

const _REPLACE_DEMOD_MENU_BASE: int = 5000

const _OPTION_AUTO_CENTER_NEW_NODES: int = 1
const _OPTION_SHOW_PROBLEM_HIGHLIGHTING: int = 2
const _OPTION_ENABLE_AUDIO_DEBUG_LOGS: int = 3

@export_node_path("GraphEdit") var _graph_edit_path: NodePath = NodePath("VBoxContainer/Control/GraphEdit")
@export_node_path("Button") var _add_node_button_path: NodePath = NodePath("VBoxContainer/Control/Button")
@export_node_path("Button") var _options_button_path: NodePath = NodePath("VBoxContainer/PanelContainer/HBoxContainer/Button2")
@export_node_path("TabContainer") var _waterfall_tabs_path: NodePath = NodePath("VBoxContainer/WaterfallTabs")

@onready var _graph_edit: GraphEdit = get_node_or_null(_graph_edit_path) as GraphEdit
@onready var _add_node_button: Button = get_node_or_null(_add_node_button_path) as Button
@onready var _options_button_ref: Button = get_node_or_null(_options_button_path) as Button
@onready var _waterfall_tabs_ref: TabContainer = get_node_or_null(_waterfall_tabs_path) as TabContainer

var _node_counts: Dictionary = {
	_NODE_MENU_RTL: 0,
	_NODE_MENU_NFM: 0,
	_NODE_MENU_WFM: 0,
	_NODE_MENU_AM: 0,
	_NODE_MENU_USB: 0,
	_NODE_MENU_LSB: 0,
	_NODE_MENU_DSB: 0,
	_NODE_MENU_CW: 0,
	_NODE_MENU_AUDIO_SINK: 0,
	_NODE_MENU_AUDIO_GENERATOR: 0,
	_NODE_MENU_AUDIO_MIXER: 0,
	_NODE_MENU_AUDIO_STEREO_MATRIX: 0,
	_NODE_MENU_BASEBAND_REPLAY: 0,
	_NODE_MENU_AUDIO_REPLAY: 0,
	_NODE_MENU_MATH: 0,
	_NODE_MENU_CONVERTER: 0,
}

var _add_node_menu: PopupMenu = null
var _graph_context_menu: PopupMenu = null
var _module_context_menu: PopupMenu = null
var _replace_demod_menu: PopupMenu = null
var _disconnect_links_button: Button = null
var _options_button: Button = null
var _options_popup: PopupMenu = null
var _rename_dialog: AcceptDialog = null
var _rename_line_edit: LineEdit = null
var _rename_target_node: GraphNode = null

var _context_target_node: GraphNode = null
var _replace_target_node: GraphNode = null
var _context_global_position: Vector2i = Vector2i.ZERO

var _runtime_links: Array[Dictionary] = []
var _audio_mix_inputs: Dictionary = {}
var _problem_nodes: Dictionary = {}
var _control_value_cache: Dictionary = {}
var _pending_spawn_graph_pos: Vector2 = Vector2.ZERO
var _has_pending_spawn_pos: bool = false
var _clipboard_nodes: Array[Dictionary] = []
var _waterfall_tabs: TabContainer = null
var _waterfall_panels: Dictionary = {}
var _waterfall_next_forward_us: Dictionary = {}
var _confirmed_source_center_hz: Dictionary = {}
var _waterfall_visual_center_hz: Dictionary = {}
var _locked_demod_absolute_hz: Dictionary = {}
var _wide_scan_saved_sample_rate_hz: Dictionary = {}
var _wide_scan_saved_center_hz: Dictionary = {}
var _wide_scan_saved_demod_offsets_hz: Dictionary = {}
var _history_stack: Array[Dictionary] = []
var _history_index: int = -1
var _history_restoring: bool = false
var _paste_cascade_index: int = 0
var _last_paste_position: Vector2 = Vector2.ZERO
var _has_last_paste_position: bool = false
var _last_paste_used_mouse: bool = false
var _audio_debug_logs_enabled: bool = false

const _MAX_HISTORY_STATES: int = 200


func _ready() -> void:
	if _add_node_button and not _add_node_button.pressed.is_connected(_on_add_node_pressed):
		_add_node_button.pressed.connect(_on_add_node_pressed)

	if _graph_edit:
		_graph_edit.right_disconnects = true
		if not _graph_edit.connection_request.is_connected(_on_connection_request):
			_graph_edit.connection_request.connect(_on_connection_request)
		if not _graph_edit.disconnection_request.is_connected(_on_disconnection_request):
			_graph_edit.disconnection_request.connect(_on_disconnection_request)
		if not _graph_edit.gui_input.is_connected(_on_graph_gui_input):
			_graph_edit.gui_input.connect(_on_graph_gui_input)

	if not _ensure_extension_loaded():
		push_error("Failed to load ESDR extension; SDR nodes are unavailable.")
		return

	_setup_add_node_menu()
	_setup_context_menus()
	_setup_disconnect_button()
	_setup_options_menu()
	_setup_rename_dialog()
	_setup_waterfall_tabs()

	_add_rtl_node(Vector2(60, 40))
	_add_wfm_node(Vector2(520, 40))
	_add_audio_sink_node(Vector2(980, 40))

	var rtl_node: GraphNode = _find_first_graph_node_by_class("RTLSDRSource")
	var wfm_node: GraphNode = _find_first_graph_node_by_class("WFMDemodulator")
	var sink_node: GraphNode = _find_first_graph_node_by_class("AudioSink")
	if rtl_node != null:
		# Default FM broadcast band center frequency: 88.1 MHz
		rtl_node.call("set_port_value", 1, 88100000)

	_connect_graph_nodes(rtl_node, 2, wfm_node, 2)
	_connect_graph_nodes(wfm_node, 2, sink_node, 1)

	_sync_runtime_connections()
	_record_history_state()
	set_process(true)
	set_process_input(true)
	set_process_unhandled_input(true)


func _process(_delta: float) -> void:
	_propagate_control_connections()
	_flush_audio_mix_buffers()
	_tick_problem_nodes(_delta)
	_update_waterfall_views()


func _ensure_extension_loaded() -> bool:
	if _has_native_factory():
		return true

	var load_result: int = GDExtensionManager.load_extension(_EXTENSION_PATH)
	if load_result != OK and not _has_native_factory():
		return false

	return _has_native_factory()


func _has_native_factory() -> bool:
	return ClassDB.class_exists("ESDRGraphFactory")


func _create_module_node(p_factory_method: String) -> GraphNode:
	if not _has_native_factory():
		return null

	var raw: Variant = ClassDB.class_call_static("ESDRGraphFactory", p_factory_method)
	var object_value: Object = raw as Object
	if object_value == null:
		return null

	return object_value as GraphNode


func _register_graph_node(p_node: GraphNode) -> void:
	if p_node == null:
		return

	_attach_delete_button_to_node(p_node)

	var module_cb: Callable = Callable(self, "_on_module_gui_input").bind(p_node)
	if not p_node.gui_input.is_connected(module_cb):
		p_node.gui_input.connect(module_cb)
	_apply_audio_debug_logging_to_node(p_node)


func _find_first_graph_node_by_class(p_class_name: String) -> GraphNode:
	if _graph_edit == null:
		return null

	for child_value in _graph_edit.get_children():
		var graph_node: GraphNode = child_value as GraphNode
		if graph_node == null:
			continue
		if graph_node.is_class(p_class_name):
			return graph_node

	return null


func _connect_graph_nodes(p_from_node: GraphNode, p_from_port: int, p_to_node: GraphNode, p_to_port: int) -> void:
	if _graph_edit == null or p_from_node == null or p_to_node == null:
		return
	if p_from_port < 0 or p_to_port < 0:
		return

	var from_name: StringName = StringName(String(p_from_node.name))
	var to_name: StringName = StringName(String(p_to_node.name))
	if _graph_edit.is_node_connected(from_name, p_from_port, to_name, p_to_port):
		return

	_on_connection_request(from_name, p_from_port, to_name, p_to_port)


func _attach_delete_button_to_node(p_node: GraphNode) -> void:
	if p_node == null:
		return

	var titlebar: HBoxContainer = p_node.get_titlebar_hbox()
	if titlebar == null:
		return
	titlebar.mouse_filter = Control.MOUSE_FILTER_PASS

	if titlebar.get_node_or_null(NodePath("DeleteNodeButton")) != null:
		return

	if titlebar.get_node_or_null(NodePath("DeleteNodeSpacer")) == null:
		var spacer: Control = Control.new()
		spacer.name = "DeleteNodeSpacer"
		spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		spacer.mouse_filter = Control.MOUSE_FILTER_IGNORE
		spacer.focus_mode = Control.FOCUS_NONE
		titlebar.add_child(spacer)

	var close_button: Button = Button.new()
	close_button.name = "DeleteNodeButton"
	close_button.text = "X"
	close_button.flat = true
	close_button.focus_mode = Control.FOCUS_NONE
	close_button.size_flags_horizontal = Control.SIZE_SHRINK_END
	close_button.custom_minimum_size = Vector2(22, 22)
	close_button.tooltip_text = "Delete Node"
	close_button.mouse_filter = Control.MOUSE_FILTER_PASS
	close_button.pressed.connect(_on_delete_node_button_pressed.bind(p_node))
	titlebar.add_child(close_button)


func _get_node_spec(p_id: int) -> Dictionary:
	var spec_value: Variant = _NODE_SPECS.get(p_id, {})
	return spec_value as Dictionary


func _get_node_count(p_id: int) -> int:
	return int(_node_counts.get(p_id, 0))


func _increment_node_count(p_id: int) -> int:
	var next_count: int = _get_node_count(p_id) + 1
	_node_counts[p_id] = next_count
	return next_count


func _get_default_spawn_offset(p_id: int) -> Vector2:
	var spec: Dictionary = _get_node_spec(p_id)
	if spec.is_empty():
		return Vector2.ZERO

	var base: Vector2 = Vector2(spec.get("spawn_base", Vector2.ZERO))
	var step: Vector2 = Vector2(spec.get("spawn_step", Vector2.ZERO))
	var count: int = _get_node_count(p_id)
	return base + (step * float(count))


func _add_node_by_menu_id(p_id: int, p_position_offset: Vector2) -> void:
	if _graph_edit == null:
		return

	var spec: Dictionary = _get_node_spec(p_id)
	if spec.is_empty():
		return

	var factory_method: String = String(spec.get("factory", ""))
	if factory_method.is_empty():
		return

	var node: GraphNode = _create_module_node(factory_method)
	if node == null:
		push_error(String(spec.get("error", "Module class is not available.")))
		return

	var node_index: int = _increment_node_count(p_id)
	var name_prefix: String = String(spec.get("name_prefix", node.get_class()))
	node.name = "%s%d" % [name_prefix, node_index]
	node.position_offset = p_position_offset
	_graph_edit.add_child(node)
	_register_graph_node(node)
	_sync_runtime_connections()
	_record_history_state()


func _add_rtl_node(p_position_offset: Vector2) -> void:
	_add_node_by_menu_id(_NODE_MENU_RTL, p_position_offset)


func _add_nfm_node(p_position_offset: Vector2) -> void:
	_add_node_by_menu_id(_NODE_MENU_NFM, p_position_offset)


func _add_wfm_node(p_position_offset: Vector2) -> void:
	_add_node_by_menu_id(_NODE_MENU_WFM, p_position_offset)


func _add_am_node(p_position_offset: Vector2) -> void:
	_add_node_by_menu_id(_NODE_MENU_AM, p_position_offset)


func _add_audio_sink_node(p_position_offset: Vector2) -> void:
	_add_node_by_menu_id(_NODE_MENU_AUDIO_SINK, p_position_offset)


func _add_audio_generator_node(p_position_offset: Vector2) -> void:
	_add_node_by_menu_id(_NODE_MENU_AUDIO_GENERATOR, p_position_offset)


func _add_audio_mixer_node(p_position_offset: Vector2) -> void:
	_add_node_by_menu_id(_NODE_MENU_AUDIO_MIXER, p_position_offset)


func _add_audio_stereo_matrix_node(p_position_offset: Vector2) -> void:
	_add_node_by_menu_id(_NODE_MENU_AUDIO_STEREO_MATRIX, p_position_offset)


func _add_math_node(p_position_offset: Vector2) -> void:
	_add_node_by_menu_id(_NODE_MENU_MATH, p_position_offset)


func _add_value_converter_node(p_position_offset: Vector2) -> void:
	_add_node_by_menu_id(_NODE_MENU_CONVERTER, p_position_offset)


func _populate_add_submenu(p_parent: PopupMenu, p_name: String, p_node_ids: Array[int]) -> void:
	var submenu: PopupMenu = PopupMenu.new()
	submenu.name = p_name
	for node_id_value in p_node_ids:
		var node_id: int = int(node_id_value)
		var spec: Dictionary = _get_node_spec(node_id)
		if spec.is_empty():
			continue
		var label: String = String(spec.get("label", "Node"))
		submenu.add_item(label, node_id)
	submenu.id_pressed.connect(_on_add_node_menu_id_pressed)
	p_parent.add_child(submenu)


func _setup_add_node_menu() -> void:
	if _add_node_menu != null:
		return

	var popup: PopupMenu = PopupMenu.new()
	popup.name = "AddNodeMenu"
	_populate_add_submenu(popup, "AddNodeSourcesMenu", _NODE_MENU_SOURCES)
	_populate_add_submenu(popup, "AddNodeDemodulatorsMenu", _NODE_MENU_DEMODULATORS)
	_populate_add_submenu(popup, "AddNodeAudioMenu", _NODE_MENU_AUDIO)
	_populate_add_submenu(popup, "AddNodeSinksMenu", _NODE_MENU_SINKS)
	_populate_add_submenu(popup, "AddNodeMathMenu", _NODE_MENU_MATH_GROUP)

	popup.add_submenu_item("Sources", "AddNodeSourcesMenu")
	popup.add_submenu_item("Demodulators", "AddNodeDemodulatorsMenu")
	popup.add_submenu_item("Audio", "AddNodeAudioMenu")
	popup.add_submenu_item("Sinks", "AddNodeSinksMenu")
	popup.add_submenu_item("math", "AddNodeMathMenu")
	popup.id_pressed.connect(_on_add_node_menu_id_pressed)
	add_child(popup)
	_add_node_menu = popup


func _setup_context_menus() -> void:
	if _graph_context_menu == null:
		var graph_popup: PopupMenu = PopupMenu.new()
		graph_popup.name = "GraphContextMenu"
		graph_popup.add_item("Add Node...", _GRAPH_CTX_ADD_NODE)
		graph_popup.add_separator()
		graph_popup.add_item("Copy Selected", _GRAPH_CTX_COPY_SELECTED)
		graph_popup.add_item("Paste", _GRAPH_CTX_PASTE)
		graph_popup.add_item("Rename Selected", _GRAPH_CTX_RENAME_SELECTED)
		graph_popup.add_item("Replace Selected Demodulator...", _GRAPH_CTX_REPLACE_SELECTED_DEMOD)
		graph_popup.add_separator()
		graph_popup.add_item("Disconnect Selected", _GRAPH_CTX_DISCONNECT_SELECTED)
		graph_popup.add_item("Delete Selected", _GRAPH_CTX_DELETE_SELECTED)
		graph_popup.id_pressed.connect(_on_graph_context_menu_id_pressed)
		add_child(graph_popup)
		_graph_context_menu = graph_popup

	if _module_context_menu == null:
		var module_popup: PopupMenu = PopupMenu.new()
		module_popup.name = "ModuleContextMenu"
		module_popup.add_item("Copy Node", _MODULE_CTX_COPY_NODE)
		module_popup.add_item("Rename Node", _MODULE_CTX_RENAME_NODE)
		module_popup.add_item("Replace Demodulator...", _MODULE_CTX_REPLACE_DEMOD)
		module_popup.add_separator()
		module_popup.add_item("Disconnect Links", _MODULE_CTX_DISCONNECT_LINKS)
		module_popup.add_item("Delete Node", _MODULE_CTX_DELETE_NODE)
		module_popup.id_pressed.connect(_on_module_context_menu_id_pressed)
		add_child(module_popup)
		_module_context_menu = module_popup

	if _replace_demod_menu == null:
		var replace_popup: PopupMenu = PopupMenu.new()
		replace_popup.name = "ReplaceDemodMenu"
		for node_id_value in _NODE_MENU_DEMODULATORS:
			var node_id: int = int(node_id_value)
			var spec: Dictionary = _get_node_spec(node_id)
			if spec.is_empty():
				continue
			var label: String = String(spec.get("label", "Demodulator"))
			replace_popup.add_item(label, _REPLACE_DEMOD_MENU_BASE + node_id)
		replace_popup.id_pressed.connect(_on_replace_demod_menu_id_pressed)
		add_child(replace_popup)
		_replace_demod_menu = replace_popup


func _setup_rename_dialog() -> void:
	if _rename_dialog != null:
		return

	var dialog: AcceptDialog = AcceptDialog.new()
	dialog.name = "RenameNodeDialog"
	dialog.title = "Rename Module"
	dialog.dialog_text = "Enter new module name:"

	var line_edit: LineEdit = LineEdit.new()
	line_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	dialog.add_child(line_edit)
	dialog.register_text_enter(line_edit)
	dialog.confirmed.connect(_on_rename_dialog_confirmed)

	add_child(dialog)
	_rename_dialog = dialog
	_rename_line_edit = line_edit


func _setup_disconnect_button() -> void:
	if _disconnect_links_button != null:
		return

	var parent_control: Control = null
	if _graph_edit != null:
		parent_control = _graph_edit.get_parent() as Control

	if parent_control == null:
		return

	var button: Button = Button.new()
	button.name = "DisconnectLinksButton"
	button.text = "Disconnect"
	button.layout_mode = 1
	button.offset_left = 110.0
	button.offset_top = 66.0
	button.offset_right = 216.0
	button.offset_bottom = 103.0
	button.pressed.connect(_on_disconnect_links_pressed)
	parent_control.add_child(button)
	_disconnect_links_button = button


func _setup_options_menu() -> void:
	if _options_popup != null:
		return

	_options_button = _options_button_ref
	if _options_button == null:
		return

	var popup: PopupMenu = PopupMenu.new()
	popup.name = "OptionsPopupMenu"
	popup.add_check_item("Auto Center New Nodes", _OPTION_AUTO_CENTER_NEW_NODES)
	popup.set_item_checked(0, true)
	popup.add_check_item("Show Problem Highlighting", _OPTION_SHOW_PROBLEM_HIGHLIGHTING)
	popup.set_item_checked(1, true)
	popup.add_check_item("Enable Audio Debug Logs", _OPTION_ENABLE_AUDIO_DEBUG_LOGS)
	popup.set_item_checked(2, _audio_debug_logs_enabled)
	popup.id_pressed.connect(_on_options_popup_id_pressed)
	add_child(popup)
	_options_popup = popup

	if not _options_button.pressed.is_connected(_on_options_button_pressed):
		_options_button.pressed.connect(_on_options_button_pressed)


func _on_options_button_pressed() -> void:
	if _options_popup == null or _options_button == null:
		return

	var x: int = int(round(_options_button.global_position.x))
	var y: int = int(round(_options_button.global_position.y + _options_button.size.y))
	_options_popup.position = Vector2i(x, y)
	_options_popup.popup()


func _on_options_popup_id_pressed(p_id: int) -> void:
	if _options_popup == null:
		return

	var index: int = _options_popup.get_item_index(p_id)
	if index < 0:
		return
	var enabled: bool = not _options_popup.is_item_checked(index)
	_options_popup.set_item_checked(index, enabled)
	if p_id == _OPTION_ENABLE_AUDIO_DEBUG_LOGS:
		_audio_debug_logs_enabled = enabled
		_apply_audio_debug_logging_to_nodes()


func _apply_audio_debug_logging_to_node(p_node: Node) -> void:
	if p_node == null:
		return
	if p_node.has_method("set_debug_logging_enabled"):
		p_node.call("set_debug_logging_enabled", _audio_debug_logs_enabled)


func _apply_audio_debug_logging_to_nodes() -> void:
	if _graph_edit == null:
		return
	for child_value in _graph_edit.get_children():
		var child_node: Node = child_value as Node
		if child_node == null:
			continue
		_apply_audio_debug_logging_to_node(child_node)


func _setup_waterfall_tabs() -> void:
	var vbox: VBoxContainer = get_node_or_null(NodePath("VBoxContainer")) as VBoxContainer
	var graph_host: Control = null
	if _graph_edit != null:
		graph_host = _graph_edit.get_parent() as Control
	if vbox == null or graph_host == null:
		return

	var split: VSplitContainer = get_node_or_null(NodePath("VBoxContainer/GraphWaterfallSplit")) as VSplitContainer
	if split == null:
		split = VSplitContainer.new()
		split.name = "GraphWaterfallSplit"
		split.size_flags_vertical = Control.SIZE_EXPAND_FILL
		split.split_offset = 460
		vbox.remove_child(graph_host)
		vbox.add_child(split)
		vbox.move_child(split, 1)
		split.add_child(graph_host)

	if _waterfall_tabs_ref != null:
		_waterfall_tabs = _waterfall_tabs_ref
		if _waterfall_tabs.get_parent() != split:
			if _waterfall_tabs.get_parent() != null:
				_waterfall_tabs.get_parent().remove_child(_waterfall_tabs)
			split.add_child(_waterfall_tabs)
		_waterfall_tabs.visible = false
		return

	var existing_tabs: TabContainer = split.get_node_or_null(NodePath("WaterfallTabs")) as TabContainer
	if existing_tabs != null:
		_waterfall_tabs = existing_tabs
		_waterfall_tabs.visible = false
		return

	var tabs: TabContainer = TabContainer.new()
	tabs.name = "WaterfallTabs"
	tabs.custom_minimum_size = Vector2(0.0, 220.0)
	tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
	tabs.visible = false
	split.add_child(tabs)
	_waterfall_tabs = tabs


func _on_add_node_pressed() -> void:
	if _add_node_menu == null or _add_node_button == null:
		return

	var x: int = int(round(_add_node_button.global_position.x))
	var y: int = int(round(_add_node_button.global_position.y + _add_node_button.size.y))
	_add_node_menu.position = Vector2i(x, y)
	_add_node_menu.popup()


func _on_add_node_menu_id_pressed(p_id: int) -> void:
	if not _NODE_SPECS.has(p_id):
		return

	var spawn_offset: Vector2 = _resolve_spawn_position(_get_default_spawn_offset(p_id))
	_add_node_by_menu_id(p_id, spawn_offset)


func _resolve_spawn_position(p_default: Vector2) -> Vector2:
	if _has_pending_spawn_pos:
		_has_pending_spawn_pos = false
		return _pending_spawn_graph_pos
	return p_default


func _global_to_graph_position(p_global_position: Vector2) -> Vector2:
	if _graph_edit == null:
		return p_global_position

	var local_position: Vector2 = _graph_edit.get_global_transform_with_canvas().affine_inverse() * p_global_position
	return (local_position + _graph_edit.scroll_offset) / _graph_edit.zoom


func _get_factory_method_for_node(p_node: GraphNode) -> String:
	if p_node == null:
		return ""

	var ssb_mode: int = 0
	var has_ssb_mode: bool = false
	if p_node.has_method("get_demod_mode"):
		ssb_mode = int(p_node.call("get_demod_mode"))
		has_ssb_mode = true

	for node_id_value in _NODE_SPECS.keys():
		var node_id: int = int(node_id_value)
		var spec: Dictionary = _get_node_spec(node_id)
		if spec.is_empty():
			continue

		var class_id: String = String(spec.get("class", ""))
		if class_id.is_empty():
			continue
		if not p_node.is_class(class_id):
			continue

		if spec.has("mode"):
			if not has_ssb_mode:
				continue
			if int(spec.get("mode", 0)) != ssb_mode:
				continue

		return String(spec.get("factory", ""))

	return ""


func _copy_selected_nodes_to_clipboard() -> void:
	if _graph_edit == null:
		return

	var collected: Array[Dictionary] = []
	for child_value in _graph_edit.get_children():
		var graph_node: GraphNode = child_value as GraphNode
		if graph_node == null or not graph_node.selected:
			continue

		var factory_method: String = _get_factory_method_for_node(graph_node)
		if factory_method.is_empty():
			continue

		collected.append({
			"factory_method": factory_method,
			"position": graph_node.position_offset,
			"source_name": String(graph_node.name),
		})

	_clipboard_nodes = collected
	_paste_cascade_index = 0
	_has_last_paste_position = false


func _copy_single_node_to_clipboard(p_node: GraphNode) -> void:
	if p_node == null:
		return

	var factory_method: String = _get_factory_method_for_node(p_node)
	if factory_method.is_empty():
		return

	_clipboard_nodes = [{
		"factory_method": factory_method,
		"position": p_node.position_offset,
		"source_name": String(p_node.name),
	}]
	_paste_cascade_index = 0
	_has_last_paste_position = false


func _paste_clipboard_nodes(p_graph_position: Vector2, p_use_mouse_position: bool = true) -> void:
	if _graph_edit == null or _clipboard_nodes.is_empty():
		return

	if _has_last_paste_position:
		var moved: bool = _last_paste_position.distance_to(p_graph_position) > 0.001
		if moved or _last_paste_used_mouse != p_use_mouse_position:
			_paste_cascade_index = 0
			_last_paste_position = p_graph_position
			_last_paste_used_mouse = p_use_mouse_position
	else:
		_last_paste_position = p_graph_position
		_last_paste_used_mouse = p_use_mouse_position
		_has_last_paste_position = true

	var first_entry: Dictionary = _clipboard_nodes[0]
	var first_position: Vector2 = Vector2(first_entry.get("position", Vector2.ZERO))
	var center_position: Vector2 = _graph_view_center()
	var cascade_shift: Vector2 = Vector2(28, 28) * float(_paste_cascade_index)
	var originals_exist: bool = false
	if not p_use_mouse_position:
		for entry_value in _clipboard_nodes:
			var entry_dict: Dictionary = entry_value as Dictionary
			if entry_dict.is_empty():
				continue
			var source_name: String = String(entry_dict.get("source_name", ""))
			if source_name.is_empty():
				continue
			if _graph_edit.get_node_or_null(NodePath(source_name)) != null:
				originals_exist = true
				break

	for entry in _clipboard_nodes:
		var item: Dictionary = entry as Dictionary
		if item.is_empty():
			continue

		var factory_method: String = String(item.get("factory_method", ""))
		if factory_method.is_empty():
			continue

		var original_position: Vector2 = Vector2(item.get("position", p_graph_position))
		var node: GraphNode = _create_module_node(factory_method)
		if node == null:
			continue

		var relative: Vector2 = original_position - first_position
		if p_use_mouse_position:
			node.position_offset = p_graph_position + relative + Vector2(20, 20) + cascade_shift
		elif originals_exist:
			node.position_offset = original_position + Vector2(40, 40) + cascade_shift
		else:
			node.position_offset = center_position + relative + cascade_shift
		node.name = _make_unique_node_name("%sCopy" % node.get_class())
		_graph_edit.add_child(node)
		_register_graph_node(node)

	_sync_runtime_connections()
	_refresh_waterfall_tabs()
	_record_history_state()
	_paste_cascade_index += 1
	_last_paste_position = p_graph_position
	_last_paste_used_mouse = p_use_mouse_position


func _input(p_event: InputEvent) -> void:
	_handle_graph_shortcuts(p_event)


func _unhandled_input(p_event: InputEvent) -> void:
	_handle_graph_shortcuts(p_event)


func _handle_graph_shortcuts(p_event: InputEvent) -> void:
	var key_event: InputEventKey = p_event as InputEventKey
	if key_event == null or not key_event.pressed or key_event.echo:
		return
	if _should_ignore_graph_shortcuts():
		return

	var is_copy: bool = key_event.ctrl_pressed and (key_event.keycode == KEY_C or key_event.physical_keycode == KEY_C)
	var is_paste: bool = key_event.ctrl_pressed and (key_event.keycode == KEY_V or key_event.physical_keycode == KEY_V)
	var is_undo: bool = key_event.ctrl_pressed and (key_event.keycode == KEY_Z or key_event.physical_keycode == KEY_Z)
	var is_redo: bool = (key_event.ctrl_pressed and (key_event.keycode == KEY_Y or key_event.physical_keycode == KEY_Y)) or (key_event.ctrl_pressed and key_event.shift_pressed and (key_event.keycode == KEY_Z or key_event.physical_keycode == KEY_Z))
	var is_delete: bool = key_event.keycode == KEY_DELETE or key_event.physical_keycode == KEY_DELETE
	var is_rename: bool = key_event.keycode == KEY_F2 or key_event.physical_keycode == KEY_F2

	if is_copy:
		_copy_selected_nodes_to_clipboard()
		get_viewport().set_input_as_handled()
		return

	if is_undo:
		_undo_graph_state()
		get_viewport().set_input_as_handled()
		return

	if is_redo:
		_redo_graph_state()
		get_viewport().set_input_as_handled()
		return

	if is_paste:
		if _is_mouse_over_graph():
			var paste_position: Vector2 = _global_to_graph_position(get_global_mouse_position())
			_paste_clipboard_nodes(paste_position, true)
		else:
			_paste_clipboard_nodes(_graph_view_center(), false)
		get_viewport().set_input_as_handled()
		return

	if is_delete:
		_delete_selected_nodes()
		get_viewport().set_input_as_handled()
		return

	if is_rename:
		var selected_node: GraphNode = _get_first_selected_node()
		if selected_node != null:
			_begin_rename_node(selected_node)
			get_viewport().set_input_as_handled()
		return


func _should_ignore_graph_shortcuts() -> bool:
	var focused: Control = get_viewport().gui_get_focus_owner()
	if focused == null:
		return false
	return focused is LineEdit or focused is TextEdit or focused is CodeEdit or focused is SpinBox


func _on_graph_gui_input(p_event: InputEvent) -> void:
	var mouse_event: InputEventMouseButton = p_event as InputEventMouseButton
	if mouse_event == null:
		return
	if not mouse_event.pressed:
		return
	if mouse_event.button_index != MOUSE_BUTTON_RIGHT:
		return

	if _find_node_at_global_position(mouse_event.global_position) != null:
		return

	_context_target_node = null
	_show_graph_context_menu(mouse_event.global_position)
	accept_event()


func _on_module_gui_input(p_event: InputEvent, p_node: GraphNode) -> void:
	var mouse_event: InputEventMouseButton = p_event as InputEventMouseButton
	if mouse_event == null:
		return
	if not mouse_event.pressed:
		return
	if mouse_event.button_index != MOUSE_BUTTON_RIGHT:
		return

	_context_target_node = p_node
	_show_module_context_menu(mouse_event.global_position)
	p_node.accept_event()


func _show_graph_context_menu(p_global_position: Vector2) -> void:
	if _graph_context_menu == null:
		return

	_context_global_position = Vector2i(int(round(p_global_position.x)), int(round(p_global_position.y)))
	var replace_index: int = _graph_context_menu.get_item_index(_GRAPH_CTX_REPLACE_SELECTED_DEMOD)
	if replace_index >= 0:
		_graph_context_menu.set_item_disabled(replace_index, _get_first_selected_demod_node() == null)
	_graph_context_menu.position = _context_global_position
	_graph_context_menu.popup()


func _show_module_context_menu(p_global_position: Vector2) -> void:
	if _module_context_menu == null:
		return

	_context_global_position = Vector2i(int(round(p_global_position.x)), int(round(p_global_position.y)))
	var replace_index: int = _module_context_menu.get_item_index(_MODULE_CTX_REPLACE_DEMOD)
	if replace_index >= 0:
		_module_context_menu.set_item_disabled(replace_index, not _is_demodulator_node(_context_target_node))
	_module_context_menu.position = _context_global_position
	_module_context_menu.popup()


func _on_graph_context_menu_id_pressed(p_id: int) -> void:
	if p_id == _GRAPH_CTX_ADD_NODE:
		if _add_node_menu != null:
			_pending_spawn_graph_pos = _global_to_graph_position(Vector2(_context_global_position))
			_has_pending_spawn_pos = true
			_add_node_menu.position = _context_global_position
			_add_node_menu.popup()
		return

	if p_id == _GRAPH_CTX_COPY_SELECTED:
		_copy_selected_nodes_to_clipboard()
		return

	if p_id == _GRAPH_CTX_PASTE:
		_paste_clipboard_nodes(_global_to_graph_position(Vector2(_context_global_position)))
		return

	if p_id == _GRAPH_CTX_RENAME_SELECTED:
		var selected_node: GraphNode = _get_first_selected_node()
		if selected_node != null:
			_begin_rename_node(selected_node)
		return

	if p_id == _GRAPH_CTX_REPLACE_SELECTED_DEMOD:
		var selected_demod: GraphNode = _get_first_selected_demod_node()
		if selected_demod != null:
			_show_replace_demod_menu(Vector2(_context_global_position), selected_demod)
		return

	if p_id == _GRAPH_CTX_DISCONNECT_SELECTED:
		_on_disconnect_links_pressed()
		return

	if p_id == _GRAPH_CTX_DELETE_SELECTED:
		_delete_selected_nodes()


func _on_module_context_menu_id_pressed(p_id: int) -> void:
	if _context_target_node == null:
		return

	if p_id == _MODULE_CTX_COPY_NODE:
		_copy_single_node_to_clipboard(_context_target_node)
		return

	if p_id == _MODULE_CTX_RENAME_NODE:
		_begin_rename_node(_context_target_node)
		return

	if p_id == _MODULE_CTX_REPLACE_DEMOD:
		_show_replace_demod_menu(Vector2(_context_global_position), _context_target_node)
		return

	if p_id == _MODULE_CTX_DISCONNECT_LINKS:
		var removed_links: int = _disconnect_links_for_node(_context_target_node)
		if removed_links > 0:
			_sync_runtime_connections()
			_record_history_state()
		return

	if p_id == _MODULE_CTX_DELETE_NODE:
		_remove_graph_node(_context_target_node)


func _on_delete_node_button_pressed(p_node: GraphNode) -> void:
	_remove_graph_node(p_node)


func _find_node_at_global_position(p_global_position: Vector2) -> GraphNode:
	if _graph_edit == null:
		return null

	for child_value in _graph_edit.get_children():
		var graph_node: GraphNode = child_value as GraphNode
		if graph_node == null:
			continue

		if graph_node.get_global_rect().has_point(p_global_position):
			return graph_node

	return null


func _remove_graph_node(p_node: GraphNode) -> void:
	_remove_graph_node_internal(p_node, true)


func _remove_graph_node_internal(p_node: GraphNode, p_record_history: bool) -> void:
	if _graph_edit == null or p_node == null:
		return

	_disconnect_links_for_node(p_node)
	if p_node.get_parent() == _graph_edit:
		_graph_edit.remove_child(p_node)
		p_node.queue_free()

	if _context_target_node == p_node:
		_context_target_node = null

	_sync_runtime_connections()
	if p_record_history:
		_record_history_state()


func _delete_selected_nodes() -> void:
	if _graph_edit == null:
		return

	var to_delete: Array[GraphNode] = []
	for child_value in _graph_edit.get_children():
		var graph_node: GraphNode = child_value as GraphNode
		if graph_node != null and graph_node.selected:
			to_delete.append(graph_node)

	for graph_node in to_delete:
		_remove_graph_node_internal(graph_node, false)

	if not to_delete.is_empty():
		_record_history_state()


func _is_mouse_over_graph() -> bool:
	if _graph_edit == null:
		return false
	return _graph_edit.get_global_rect().has_point(get_global_mouse_position())


func _graph_view_center() -> Vector2:
	if _graph_edit == null:
		return Vector2.ZERO
	var local_center: Vector2 = _graph_edit.size * 0.5
	return (local_center + _graph_edit.scroll_offset) / _graph_edit.zoom


func _make_unique_node_name(p_base: String) -> String:
	if _graph_edit == null:
		return p_base

	var base: String = p_base.strip_edges()
	if base.is_empty():
		base = "Node"
	base = base.replace("/", "_").replace(":", "_").replace("@", "_")

	if _graph_edit.get_node_or_null(NodePath(base)) == null:
		return base

	var index: int = 2
	while true:
		var candidate: String = "%s%d" % [base, index]
		if _graph_edit.get_node_or_null(NodePath(candidate)) == null:
			return candidate
		index += 1

	return base


func _get_first_selected_node() -> GraphNode:
	if _graph_edit == null:
		return null

	for child_value in _graph_edit.get_children():
		var graph_node: GraphNode = child_value as GraphNode
		if graph_node != null and graph_node.selected:
			return graph_node
	return null


func _get_first_selected_demod_node() -> GraphNode:
	var selected_node: GraphNode = _get_first_selected_node()
	if _is_demodulator_node(selected_node):
		return selected_node
	return null


func _show_replace_demod_menu(p_global_position: Vector2, p_target_node: GraphNode) -> void:
	if _replace_demod_menu == null or p_target_node == null:
		return
	if not _is_demodulator_node(p_target_node):
		return
	_replace_target_node = p_target_node
	_replace_demod_menu.position = Vector2i(int(round(p_global_position.x)), int(round(p_global_position.y)))
	_replace_demod_menu.popup()


func _on_replace_demod_menu_id_pressed(p_id: int) -> void:
	if _replace_target_node == null:
		return
	if p_id < _REPLACE_DEMOD_MENU_BASE:
		return
	var node_menu_id: int = p_id - _REPLACE_DEMOD_MENU_BASE
	_replace_demodulator_node(_replace_target_node, node_menu_id)
	_replace_target_node = null


func _replace_demodulator_node(p_old_node: GraphNode, p_new_node_menu_id: int) -> void:
	if _graph_edit == null or p_old_node == null:
		return
	if not _is_demodulator_node(p_old_node):
		return
	if not _NODE_MENU_DEMODULATORS.has(p_new_node_menu_id):
		return

	var spec: Dictionary = _get_node_spec(p_new_node_menu_id)
	if spec.is_empty():
		return
	var factory_method: String = String(spec.get("factory", ""))
	if factory_method.is_empty():
		return

	var new_node: GraphNode = _create_module_node(factory_method)
	if new_node == null:
		push_error(String(spec.get("error", "Could not create demodulator.")))
		return

	var old_name: String = String(p_old_node.name)
	var old_position: Vector2 = p_old_node.position_offset
	var old_title: String = String(p_old_node.title)
	var old_selected: bool = p_old_node.selected
	var old_offset_hz: int = int(p_old_node.call("get_port_value", 0)) if p_old_node.has_method("get_port_value") else 0
	var old_bandwidth_hz: int = int(p_old_node.call("get_port_value", 1)) if p_old_node.has_method("get_port_value") else 150000
	var old_output_volume: float = float(p_old_node.call("get_port_value", 4)) if p_old_node.has_method("get_port_value") else 1.0
	var old_lock_to_source: bool = bool(p_old_node.call("get_lock_to_source_frequency")) if p_old_node.has_method("get_lock_to_source_frequency") else false

	var links: Array[Dictionary] = []
	for connection_value in _graph_edit.get_connection_list():
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue
		var from_name: StringName = StringName(connection.get("from_node", StringName()))
		var to_name: StringName = StringName(connection.get("to_node", StringName()))
		if from_name == StringName(old_name) or to_name == StringName(old_name):
			links.append(connection.duplicate(true))

	_disconnect_links_for_node(p_old_node)
	if p_old_node.get_parent() == _graph_edit:
		_graph_edit.remove_child(p_old_node)
	p_old_node.queue_free()

	new_node.name = old_name
	new_node.position_offset = old_position
	new_node.title = old_title
	new_node.selected = old_selected
	_graph_edit.add_child(new_node)
	_register_graph_node(new_node)

	if new_node.has_method("set_port_value"):
		new_node.call("set_port_value", 0, old_offset_hz)
		new_node.call("set_port_value", 1, old_bandwidth_hz)
		new_node.call("set_port_value", 4, old_output_volume)
	if new_node.has_method("set_lock_to_source_frequency"):
		new_node.call("set_lock_to_source_frequency", old_lock_to_source)

	var previous_history_restore_state: bool = _history_restoring
	_history_restoring = true
	for link_value in links:
		var link: Dictionary = link_value as Dictionary
		if link.is_empty():
			continue
		var from_node: StringName = StringName(link.get("from_node", StringName()))
		var to_node: StringName = StringName(link.get("to_node", StringName()))
		var from_port: int = int(link.get("from_port", -1))
		var to_port: int = int(link.get("to_port", -1))
		if from_port < 0 or to_port < 0:
			continue
		if _graph_edit.get_node_or_null(NodePath(String(from_node))) == null:
			continue
		if _graph_edit.get_node_or_null(NodePath(String(to_node))) == null:
			continue
		_on_connection_request(from_node, from_port, to_node, to_port)
	_history_restoring = previous_history_restore_state

	_sync_runtime_connections()
	_refresh_waterfall_tabs()
	_update_waterfall_views()
	_record_history_state()


func _begin_rename_node(p_node: GraphNode) -> void:
	if p_node == null or _rename_dialog == null or _rename_line_edit == null:
		return

	_rename_target_node = p_node
	_rename_line_edit.text = _get_graph_node_display_label(p_node, String(p_node.name))
	_rename_dialog.popup_centered(Vector2i(360, 110))
	_rename_line_edit.grab_focus()
	_rename_line_edit.select_all()


func _on_rename_dialog_confirmed() -> void:
	if _rename_target_node == null or _rename_line_edit == null:
		return
	if not is_instance_valid(_rename_target_node):
		_rename_target_node = null
		return

	var requested_name: String = _rename_line_edit.text.strip_edges()
	if requested_name.is_empty():
		_rename_target_node = null
		return

	_rename_target_node.title = requested_name
	_rename_target_node = null
	_sync_runtime_connections()
	_refresh_waterfall_tabs()
	_record_history_state()


func _capture_graph_state() -> Dictionary:
	var snapshot: Dictionary = {
		"nodes": [],
		"connections": [],
	}
	if _graph_edit == null:
		return snapshot

	var nodes: Array[Dictionary] = []
	for child_value in _graph_edit.get_children():
		var graph_node: GraphNode = child_value as GraphNode
		if graph_node == null:
			continue

		var factory_method: String = _get_factory_method_for_node(graph_node)
		if factory_method.is_empty():
			continue

		nodes.append({
			"factory_method": factory_method,
			"name": String(graph_node.name),
			"title": String(graph_node.title),
			"position": graph_node.position_offset,
		})

	var connections: Array[Dictionary] = []
	for connection_value in _graph_edit.get_connection_list():
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue

		connections.append({
			"from_node": String(connection.get("from_node", StringName())),
			"from_port": int(connection.get("from_port", -1)),
			"to_node": String(connection.get("to_node", StringName())),
			"to_port": int(connection.get("to_port", -1)),
		})

	snapshot["nodes"] = nodes
	snapshot["connections"] = connections
	return snapshot


func _restore_graph_state(p_snapshot: Dictionary) -> void:
	if _graph_edit == null:
		return

	_history_restoring = true
	_context_target_node = null
	_clear_runtime_links()
	_control_value_cache.clear()
	var existing_nodes: Dictionary = {}
	for child_value in _graph_edit.get_children():
		var graph_node: GraphNode = child_value as GraphNode
		if graph_node == null:
			continue
		existing_nodes[String(graph_node.name)] = graph_node

	var snapshot_nodes_by_name: Dictionary = {}
	var nodes: Array = p_snapshot.get("nodes", []) as Array
	for node_value in nodes:
		var entry: Dictionary = node_value as Dictionary
		if entry.is_empty():
			continue
		var node_name: String = String(entry.get("name", ""))
		if node_name.is_empty():
			continue
		snapshot_nodes_by_name[node_name] = entry

	var to_free: Array[GraphNode] = []
	for node_name_value in existing_nodes.keys():
		var node_name: String = String(node_name_value)
		if snapshot_nodes_by_name.has(node_name):
			continue
		var stale_node: GraphNode = existing_nodes[node_name] as GraphNode
		if stale_node != null:
			to_free.append(stale_node)
	for graph_node in to_free:
		_graph_edit.remove_child(graph_node)
		graph_node.queue_free()
		existing_nodes.erase(String(graph_node.name))

	for node_value in nodes:
		var entry: Dictionary = node_value as Dictionary
		if entry.is_empty():
			continue

		var factory_method: String = String(entry.get("factory_method", ""))
		var node_name: String = String(entry.get("name", ""))
		if factory_method.is_empty() or node_name.is_empty():
			continue

		var node: GraphNode = existing_nodes.get(node_name, null) as GraphNode
		var expected_factory: String = ""
		if node != null:
			expected_factory = _get_factory_method_for_node(node)

		var needs_recreate: bool = node == null or expected_factory != factory_method
		if needs_recreate:
			if node != null:
				_graph_edit.remove_child(node)
				node.queue_free()
			node = _create_module_node(factory_method)
			if node == null:
				continue
			node.name = StringName(node_name)
			_graph_edit.add_child(node)
			_register_graph_node(node)
			existing_nodes[node_name] = node

		node.title = String(entry.get("title", ""))
		node.position_offset = Vector2(entry.get("position", Vector2.ZERO))

	var existing_connections: Array = _graph_edit.get_connection_list()
	for connection_value in existing_connections:
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue
		var from_node_name: StringName = StringName(connection.get("from_node", StringName()))
		var to_node_name: StringName = StringName(connection.get("to_node", StringName()))
		var from_port: int = int(connection.get("from_port", -1))
		var to_port: int = int(connection.get("to_port", -1))
		if from_port < 0 or to_port < 0:
			continue
		if _graph_edit.is_node_connected(from_node_name, from_port, to_node_name, to_port):
			_graph_edit.disconnect_node(from_node_name, from_port, to_node_name, to_port)

	var connections: Array = p_snapshot.get("connections", []) as Array
	for connection_value in connections:
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue

		var from_node_name: String = String(connection.get("from_node", ""))
		var to_node_name: String = String(connection.get("to_node", ""))
		var from_port: int = int(connection.get("from_port", -1))
		var to_port: int = int(connection.get("to_port", -1))
		if from_node_name.is_empty() or to_node_name.is_empty():
			continue
		if from_port < 0 or to_port < 0:
			continue

		var from_node: StringName = StringName(from_node_name)
		var to_node: StringName = StringName(to_node_name)
		if _graph_edit.get_node_or_null(NodePath(from_node_name)) == null:
			continue
		if _graph_edit.get_node_or_null(NodePath(to_node_name)) == null:
			continue
		if not _graph_edit.is_node_connected(from_node, from_port, to_node, to_port):
			_graph_edit.connect_node(from_node, from_port, to_node, to_port)

	_sync_runtime_connections()
	_refresh_waterfall_tabs()
	_history_restoring = false


func _record_history_state() -> void:
	if _history_restoring:
		return

	var snapshot: Dictionary = _capture_graph_state()
	if _history_index < _history_stack.size() - 1:
		_history_stack = _history_stack.slice(0, _history_index + 1)

	_history_stack.append(snapshot)
	if _history_stack.size() > _MAX_HISTORY_STATES:
		_history_stack.remove_at(0)
		_history_index = _history_stack.size() - 1
	else:
		_history_index = _history_stack.size() - 1


func _undo_graph_state() -> void:
	if _history_stack.is_empty():
		return
	if _history_index <= 0:
		return

	_history_index -= 1
	var snapshot: Dictionary = _history_stack[_history_index] as Dictionary
	_restore_graph_state(snapshot)


func _redo_graph_state() -> void:
	if _history_stack.is_empty():
		return
	if _history_index >= _history_stack.size() - 1:
		return

	_history_index += 1
	var snapshot: Dictionary = _history_stack[_history_index] as Dictionary
	_restore_graph_state(snapshot)


func _on_disconnect_links_pressed() -> void:
	if _graph_edit == null:
		return

	var removed: int = _disconnect_links_for_selected_nodes()
	if removed == 0 and _disconnect_closest_link_to_mouse():
		removed = 1

	if removed > 0:
		_sync_runtime_connections()
		_record_history_state()


func _disconnect_links_for_selected_nodes() -> int:
	if _graph_edit == null:
		return 0

	var selected_names: Dictionary = {}
	for child_value in _graph_edit.get_children():
		var graph_node: GraphNode = child_value as GraphNode
		if graph_node != null and graph_node.selected:
			selected_names[String(graph_node.name)] = true

	if selected_names.is_empty():
		return 0

	var removed: int = 0
	var connection_list: Array = _graph_edit.get_connection_list()
	for connection_value in connection_list:
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue

		var from_name: String = String(connection.get("from_node", StringName()))
		var to_name: String = String(connection.get("to_node", StringName()))
		if not selected_names.has(from_name) and not selected_names.has(to_name):
			continue

		var from_port: int = int(connection.get("from_port", -1))
		var to_port: int = int(connection.get("to_port", -1))
		if from_port < 0 or to_port < 0:
			continue

		var from_node: StringName = StringName(from_name)
		var to_node: StringName = StringName(to_name)
		if _graph_edit.is_node_connected(from_node, from_port, to_node, to_port):
			_graph_edit.disconnect_node(from_node, from_port, to_node, to_port)
			removed += 1

	return removed


func _disconnect_links_for_node(p_node: GraphNode) -> int:
	if _graph_edit == null or p_node == null:
		return 0

	var node_name: String = String(p_node.name)
	var removed: int = 0
	var connection_list: Array = _graph_edit.get_connection_list()
	for connection_value in connection_list:
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue

		var from_name: String = String(connection.get("from_node", StringName()))
		var to_name: String = String(connection.get("to_node", StringName()))
		if from_name != node_name and to_name != node_name:
			continue

		var from_port: int = int(connection.get("from_port", -1))
		var to_port: int = int(connection.get("to_port", -1))
		if from_port < 0 or to_port < 0:
			continue

		var from_node: StringName = StringName(from_name)
		var to_node: StringName = StringName(to_name)
		if _graph_edit.is_node_connected(from_node, from_port, to_node, to_port):
			_graph_edit.disconnect_node(from_node, from_port, to_node, to_port)
			removed += 1

	return removed


func _disconnect_closest_link_to_mouse() -> bool:
	if _graph_edit == null:
		return false

	var local_mouse: Vector2 = _graph_edit.get_local_mouse_position()
	var closest: Dictionary = _graph_edit.get_closest_connection_at_point(local_mouse, 24.0)
	if closest.is_empty():
		return false

	var from_node: StringName = closest.get("from_node", StringName())
	var to_node: StringName = closest.get("to_node", StringName())
	var from_port: int = int(closest.get("from_port", -1))
	var to_port: int = int(closest.get("to_port", -1))

	if from_port < 0 or to_port < 0:
		return false

	if not _graph_edit.is_node_connected(from_node, from_port, to_node, to_port):
		return false

	_graph_edit.disconnect_node(from_node, from_port, to_node, to_port)
	return true


func _on_connection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	if from_node == to_node:
		return

	if _graph_edit.is_node_connected(from_node, from_port, to_node, to_port):
		return

	var from_node_ref: Node = _graph_edit.get_node_or_null(NodePath(String(from_node))) as Node
	var to_node_ref: Node = _graph_edit.get_node_or_null(NodePath(String(to_node))) as Node
	var existing_input_connection: Dictionary = _find_existing_connection_to_input(to_node, to_port)
	var allow_multi_audio_input: bool = _is_audio_connection(from_node_ref as GraphNode, from_port, to_node_ref as GraphNode, to_port)
	if not allow_multi_audio_input and not existing_input_connection.is_empty():
		var existing_from_node: StringName = StringName(existing_input_connection.get("from_node", StringName()))
		var existing_from_port: int = int(existing_input_connection.get("from_port", -1))
		if existing_from_node != from_node or existing_from_port != from_port:
			_mark_node_problem(from_node_ref, "Input already connected.", 2.0)
			_mark_node_problem(to_node_ref, "Input already connected.", 2.0)
			return

	if not _are_ports_compatible(from_node_ref as GraphNode, from_port, to_node_ref as GraphNode, to_port):
		_mark_node_problem(from_node_ref, "Type mismatch link blocked.", 2.5)
		_mark_node_problem(to_node_ref, "Type mismatch link blocked.", 2.5)
		return

	var math_link: bool = _is_math_like_node(from_node_ref) or _is_math_like_node(to_node_ref)
	if math_link and _is_control_link(from_node_ref, to_node_ref) and _would_create_control_cycle(from_node, to_node):
		_mark_node_problem(from_node_ref, "Control loop blocked.", 2.5)
		_mark_node_problem(to_node_ref, "Control loop blocked.", 2.5)
		return

	_graph_edit.connect_node(from_node, from_port, to_node, to_port)
	_clear_node_problem(from_node_ref)
	_clear_node_problem(to_node_ref)
	_sync_runtime_connections()
	_record_history_state()


func _find_existing_connection_to_input(p_to_node: StringName, p_to_port: int) -> Dictionary:
	if _graph_edit == null:
		return {}

	var connection_list: Array = _graph_edit.get_connection_list()
	for connection_value in connection_list:
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue

		var existing_to_node: StringName = StringName(connection.get("to_node", StringName()))
		var existing_to_port: int = int(connection.get("to_port", -1))
		if existing_to_node == p_to_node and existing_to_port == p_to_port:
			return connection

	return {}


func _on_disconnection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	if not _graph_edit.is_node_connected(from_node, from_port, to_node, to_port):
		return

	_graph_edit.disconnect_node(from_node, from_port, to_node, to_port)
	var from_node_ref: Node = _graph_edit.get_node_or_null(NodePath(String(from_node))) as Node
	var to_node_ref: Node = _graph_edit.get_node_or_null(NodePath(String(to_node))) as Node
	_clear_node_problem(from_node_ref)
	_clear_node_problem(to_node_ref)
	_sync_runtime_connections()
	_record_history_state()


func _is_control_link(p_from_node: Node, p_to_node: Node) -> bool:
	if p_from_node == null or p_to_node == null:
		return false
	return p_from_node.has_method("get_port_value") and p_to_node.has_method("set_port_value")


func _are_ports_compatible(p_from_node: GraphNode, p_from_port: int, p_to_node: GraphNode, p_to_port: int) -> bool:
	if p_from_node == null or p_to_node == null:
		return true
	if p_from_port < 0 or p_to_port < 0:
		return false

	var from_type: int = p_from_node.get_output_port_type(p_from_port)
	var to_type: int = p_to_node.get_input_port_type(p_to_port)
	if from_type == 0 or to_type == 0:
		return false
	return from_type == to_type


func _is_audio_connection(p_from_node: GraphNode, p_from_port: int, p_to_node: GraphNode, p_to_port: int) -> bool:
	if p_from_node == null or p_to_node == null:
		return false
	if p_from_port < 0 or p_to_port < 0:
		return false

	var from_type: int = p_from_node.get_output_port_type(p_from_port)
	var to_type: int = p_to_node.get_input_port_type(p_to_port)
	return from_type == _SIGNAL_AUDIO_TYPE and to_type == _SIGNAL_AUDIO_TYPE


func _is_baseband_connection(p_from_node: GraphNode, p_from_port: int, p_to_node: GraphNode, p_to_port: int) -> bool:
	if p_from_node == null or p_to_node == null:
		return false
	if p_from_port < 0 or p_to_port < 0:
		return false

	var from_type: int = p_from_node.get_output_port_type(p_from_port)
	var to_type: int = p_to_node.get_input_port_type(p_to_port)
	return from_type == _SIGNAL_BASEBAND_TYPE and to_type == _SIGNAL_BASEBAND_TYPE


func _is_math_like_node(p_node: Node) -> bool:
	if p_node == null:
		return false
	return p_node.is_class("MathOperator")


func _would_create_control_cycle(p_from_node: StringName, p_to_node: StringName) -> bool:
	if _graph_edit == null:
		return false

	var stack: Array[StringName] = [p_to_node]
	var visited: Dictionary = {}
	var connection_list: Array = _graph_edit.get_connection_list()
	while not stack.is_empty():
		var current: StringName = stack.pop_back()
		var current_key: String = String(current)
		if visited.has(current_key):
			continue
		visited[current_key] = true

		if current == p_from_node:
			return true

		for connection_value in connection_list:
			var connection: Dictionary = connection_value as Dictionary
			if connection.is_empty():
				continue

			var edge_from: StringName = StringName(connection.get("from_node", StringName()))
			if edge_from != current:
				continue

			var edge_to: StringName = StringName(connection.get("to_node", StringName()))
			if edge_to == StringName():
				continue

			var edge_from_node: Node = _graph_edit.get_node_or_null(NodePath(String(edge_from))) as Node
			var edge_to_node: Node = _graph_edit.get_node_or_null(NodePath(String(edge_to))) as Node
			if not _is_control_link(edge_from_node, edge_to_node):
				continue

			stack.append(edge_to)
		

	return false


func _mark_node_problem(p_node: Node, p_message: String, p_seconds: float) -> void:
	if p_node == null:
		return
	if not p_node.has_method("set_problem_state"):
		return

	p_node.call("set_problem_state", true, p_message)
	var key: int = p_node.get_instance_id()
	var entry: Dictionary = {
		"node": p_node,
		"ttl": p_seconds,
	}
	_problem_nodes[key] = entry


func _clear_node_problem(p_node: Node) -> void:
	if p_node == null:
		return

	if p_node.has_method("set_problem_state"):
		p_node.call("set_problem_state", false, "")

	var key: int = p_node.get_instance_id()
	if _problem_nodes.has(key):
		_problem_nodes.erase(key)


func _tick_problem_nodes(p_delta: float) -> void:
	if _problem_nodes.is_empty():
		return

	var to_clear: Array[int] = []
	for key_value in _problem_nodes.keys():
		var key: int = int(key_value)
		var entry: Dictionary = _problem_nodes.get(key, {}) as Dictionary
		if entry.is_empty():
			to_clear.append(key)
			continue

		var node_value: Variant = entry.get("node", null)
		var node: Node = node_value as Node
		if node == null:
			to_clear.append(key)
			continue

		var ttl: float = float(entry.get("ttl", 0.0))
		ttl -= p_delta
		if ttl <= 0.0:
			if node.has_method("set_problem_state"):
				node.call("set_problem_state", false, "")
			to_clear.append(key)
			continue

		entry["ttl"] = ttl
		_problem_nodes[key] = entry
	

	for key in to_clear:
		_problem_nodes.erase(key)
	


func _add_runtime_link(p_source: Object, p_signal: StringName, p_callable: Callable) -> void:
	if not p_source.is_connected(p_signal, p_callable):
		p_source.connect(p_signal, p_callable)

	var link: Dictionary = {
		"source": p_source,
		"signal": p_signal,
		"callable": p_callable,
	}
	_runtime_links.append(link)


func _make_connection_key(p_from_node: StringName, p_from_port: int, p_to_node: StringName, p_to_port: int) -> String:
	return "%s:%d->%s:%d" % [String(p_from_node), p_from_port, String(p_to_node), p_to_port]


func _clear_runtime_links() -> void:
	for link_value in _runtime_links:
		var link: Dictionary = link_value as Dictionary
		if link.is_empty():
			continue

		var source: Object = link.get("source", null) as Object
		if source == null:
			continue

		var signal_name: StringName = link.get("signal", StringName())
		var callable: Callable = link.get("callable", Callable())
		if signal_name == StringName():
			continue

		if source.is_connected(signal_name, callable):
			source.disconnect(signal_name, callable)

	_runtime_links.clear()
	_audio_mix_inputs.clear()


func _on_audio_mix_mono_frame(p_frame: PackedFloat32Array, p_mix_key: String) -> void:
	if p_frame.is_empty():
		return
	var entry: Dictionary = _audio_mix_inputs.get(p_mix_key, {}) as Dictionary
	if entry.is_empty():
		return
	var frames: Array = entry.get("mono_frames", []) as Array
	frames.append(p_frame)
	entry["mono_frames"] = frames
	_audio_mix_inputs[p_mix_key] = entry


func _on_audio_mix_stereo_frame(p_frame: PackedVector2Array, p_mix_key: String) -> void:
	if p_frame.is_empty():
		return
	var entry: Dictionary = _audio_mix_inputs.get(p_mix_key, {}) as Dictionary
	if entry.is_empty():
		return
	var frames: Array = entry.get("stereo_frames", []) as Array
	frames.append(p_frame)
	entry["stereo_frames"] = frames
	_audio_mix_inputs[p_mix_key] = entry


func _is_demod_in_source_scope(p_source_node: Node, p_demod_node: Node) -> bool:
	if p_source_node == null or p_demod_node == null:
		return true
	if not p_demod_node.has_method("get_port_value"):
		return true

	var sample_rate_hz: float = 0.0
	if p_source_node.has_method("get_current_sample_rate_hz"):
		sample_rate_hz = float(p_source_node.call("get_current_sample_rate_hz"))
	elif p_source_node.has_method("get_port_value"):
		sample_rate_hz = float(p_source_node.call("get_port_value", 8))
	if sample_rate_hz <= 0.0:
		return true

	var offset_hz: float = float(p_demod_node.call("get_port_value", 0))
	var half_span_hz: float = sample_rate_hz * 0.5
	return abs(offset_hz) <= half_span_hz


func _on_baseband_frame_for_demod(p_frame: PackedVector2Array, p_source_name: StringName, p_demod_name: StringName) -> void:
	if _graph_edit == null:
		return
	var source_node: Node = _graph_edit.get_node_or_null(NodePath(String(p_source_name))) as Node
	var demod_node: Node = _graph_edit.get_node_or_null(NodePath(String(p_demod_name))) as Node
	if source_node == null or demod_node == null:
		return
	if not demod_node.has_method("push_baseband_frame"):
		return
	if not _is_demod_in_source_scope(source_node, demod_node):
		return
	demod_node.call("push_baseband_frame", p_frame)


func _flush_audio_mix_buffers() -> void:
	if _audio_mix_inputs.is_empty() or _graph_edit == null:
		return

	for mix_key_value in _audio_mix_inputs.keys():
		var mix_key: String = String(mix_key_value)
		var entry: Dictionary = _audio_mix_inputs.get(mix_key, {}) as Dictionary
		if entry.is_empty():
			continue

		var mono_frames: Array = entry.get("mono_frames", []) as Array
		var stereo_frames: Array = entry.get("stereo_frames", []) as Array
		if mono_frames.is_empty() and stereo_frames.is_empty():
			continue

		var to_node_name: String = String(entry.get("to_node_name", ""))
		var to_node: Node = _graph_edit.get_node_or_null(NodePath(to_node_name)) as Node
		if to_node == null:
			entry["mono_frames"] = []
			entry["stereo_frames"] = []
			_audio_mix_inputs[mix_key] = entry
			continue

		var has_stereo_data: bool = not stereo_frames.is_empty()
		if has_stereo_data:
			var max_len_stereo: int = 0
			var contributor_count_stereo: int = 0
			for frame_value in stereo_frames:
				var frame_stereo: PackedVector2Array = frame_value as PackedVector2Array
				if frame_stereo.is_empty():
					continue
				max_len_stereo = max(max_len_stereo, frame_stereo.size())
				contributor_count_stereo += 1
			for frame_value in mono_frames:
				var frame_mono_for_stereo: PackedFloat32Array = frame_value as PackedFloat32Array
				if frame_mono_for_stereo.is_empty():
					continue
				max_len_stereo = max(max_len_stereo, frame_mono_for_stereo.size())
				contributor_count_stereo += 1

			if max_len_stereo > 0 and contributor_count_stereo > 0:
				var mixed_stereo: PackedVector2Array = PackedVector2Array()
				mixed_stereo.resize(max_len_stereo)
				for i in range(max_len_stereo):
					mixed_stereo[i] = Vector2.ZERO

				for frame_value in stereo_frames:
					var frame_stereo: PackedVector2Array = frame_value as PackedVector2Array
					var limit_stereo: int = min(max_len_stereo, frame_stereo.size())
					for i in range(limit_stereo):
						var accum: Vector2 = mixed_stereo[i]
						accum += frame_stereo[i]
						mixed_stereo[i] = accum

				for frame_value in mono_frames:
					var frame_mono_for_stereo: PackedFloat32Array = frame_value as PackedFloat32Array
					var limit_mono_stereo: int = min(max_len_stereo, frame_mono_for_stereo.size())
					for i in range(limit_mono_stereo):
						var sample_mono_stereo: float = frame_mono_for_stereo[i]
						var accum_stereo: Vector2 = mixed_stereo[i]
						accum_stereo += Vector2(sample_mono_stereo, sample_mono_stereo)
						mixed_stereo[i] = accum_stereo

				for i in range(max_len_stereo):
					var v: Vector2 = mixed_stereo[i]
					mixed_stereo[i] = Vector2(clamp(v.x, -1.0, 1.0), clamp(v.y, -1.0, 1.0))

				if to_node.has_method("push_audio_stereo_frame"):
					to_node.call("push_audio_stereo_frame", mixed_stereo)
				elif to_node.has_method("push_audio_stereo_frame_from_source"):
					to_node.call("push_audio_stereo_frame_from_source", mixed_stereo, StringName("PseudoMix"))
		else:
			var max_len_mono: int = 0
			var contributor_count_mono: int = 0
			for frame_value in mono_frames:
				var frame_mono: PackedFloat32Array = frame_value as PackedFloat32Array
				if frame_mono.is_empty():
					continue
				max_len_mono = max(max_len_mono, frame_mono.size())
				contributor_count_mono += 1

			if max_len_mono > 0 and contributor_count_mono > 0:
				var mixed_mono: PackedFloat32Array = PackedFloat32Array()
				mixed_mono.resize(max_len_mono)
				for i in range(max_len_mono):
					mixed_mono[i] = 0.0

				for frame_value in mono_frames:
					var frame_mono: PackedFloat32Array = frame_value as PackedFloat32Array
					var limit_mono: int = min(max_len_mono, frame_mono.size())
					for i in range(limit_mono):
						mixed_mono[i] = mixed_mono[i] + frame_mono[i]

				for i in range(max_len_mono):
					mixed_mono[i] = clamp(mixed_mono[i], -1.0, 1.0)

				if to_node.has_method("push_audio_frame"):
					to_node.call("push_audio_frame", mixed_mono)
				elif to_node.has_method("push_audio_frame_from_source"):
					to_node.call("push_audio_frame_from_source", mixed_mono, StringName("PseudoMix"))

		entry["mono_frames"] = []
		entry["stereo_frames"] = []
		_audio_mix_inputs[mix_key] = entry


func _sync_runtime_connections() -> void:
	_clear_runtime_links()
	_control_value_cache.clear()

	if _graph_edit == null:
		return

	var mixer_input_counts: Dictionary = {}
	var math_input_counts: Dictionary = {}
	var audio_input_counts: Dictionary = {}

	var connection_list: Array = _graph_edit.get_connection_list()
	var removed_mismatches: int = 0
	for connection_value in connection_list:
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue

		var from_node_name: StringName = StringName(connection.get("from_node", StringName()))
		var to_node_name: StringName = StringName(connection.get("to_node", StringName()))
		var from_port: int = int(connection.get("from_port", -1))
		var to_port: int = int(connection.get("to_port", -1))
		if from_port < 0 or to_port < 0:
			continue

		var from_graph_node: GraphNode = _graph_edit.get_node_or_null(NodePath(String(from_node_name))) as GraphNode
		var to_graph_node: GraphNode = _graph_edit.get_node_or_null(NodePath(String(to_node_name))) as GraphNode
		if _are_ports_compatible(from_graph_node, from_port, to_graph_node, to_port):
			continue
		if _graph_edit.is_node_connected(from_node_name, from_port, to_node_name, to_port):
			_graph_edit.disconnect_node(from_node_name, from_port, to_node_name, to_port)
			removed_mismatches += 1

	if removed_mismatches > 0:
		connection_list = _graph_edit.get_connection_list()

	for connection_value in connection_list:
		var connection_count: Dictionary = connection_value as Dictionary
		if connection_count.is_empty():
			continue
		var from_node_name_count: StringName = StringName(connection_count.get("from_node", StringName()))
		var to_node_name_count: StringName = StringName(connection_count.get("to_node", StringName()))
		var from_port_count: int = int(connection_count.get("from_port", -1))
		var to_port_count: int = int(connection_count.get("to_port", -1))
		if from_port_count < 0 or to_port_count < 0:
			continue
		var from_graph_node_count: GraphNode = _graph_edit.get_node_or_null(NodePath(String(from_node_name_count))) as GraphNode
		var to_graph_node_count: GraphNode = _graph_edit.get_node_or_null(NodePath(String(to_node_name_count))) as GraphNode
		if not _is_audio_connection(from_graph_node_count, from_port_count, to_graph_node_count, to_port_count):
			continue
		var audio_key: String = "%s:%d" % [String(to_node_name_count), to_port_count]
		audio_input_counts[audio_key] = int(audio_input_counts.get(audio_key, 0)) + 1

	var seen_input_ports: Dictionary = {}
	var removed_multi_input_links: int = 0
	for connection_value in connection_list:
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue

		var from_node_name: StringName = StringName(connection.get("from_node", StringName()))
		var to_node_name: StringName = StringName(connection.get("to_node", StringName()))
		var from_port: int = int(connection.get("from_port", -1))
		var to_port: int = int(connection.get("to_port", -1))
		if from_port < 0 or to_port < 0:
			continue

		var input_key: String = "%s:%d" % [String(to_node_name), to_port]
		if seen_input_ports.has(input_key) and int(audio_input_counts.get(input_key, 0)) <= 1:
			if _graph_edit.is_node_connected(from_node_name, from_port, to_node_name, to_port):
				_graph_edit.disconnect_node(from_node_name, from_port, to_node_name, to_port)
				removed_multi_input_links += 1
			continue

		seen_input_ports[input_key] = true

	if removed_multi_input_links > 0:
		connection_list = _graph_edit.get_connection_list()

	for child_value in _graph_edit.get_children():
		var source_child: GraphNode = child_value as GraphNode
		if source_child == null:
			continue
		if not _is_source_node(source_child):
			continue
		if not source_child.has_signal("frequency_tuned_changed"):
			continue
		var source_name_key: StringName = StringName(String(source_child.name))
		var tuned_callable: Callable = Callable(self, "_on_source_frequency_tuned_changed").bind(source_name_key)
		_add_runtime_link(source_child, &"frequency_tuned_changed", tuned_callable)

	for connection_value in connection_list:
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue

		if not connection.has("from_node") or not connection.has("to_node"):
			continue

		var from_node_name: StringName = StringName(connection.get("from_node", StringName()))
		var to_node_name: StringName = StringName(connection.get("to_node", StringName()))
		var from_port: int = int(connection.get("from_port", -1))
		var to_port: int = int(connection.get("to_port", -1))
		if from_port < 0 or to_port < 0:
			continue

		var from_graph_node: GraphNode = _graph_edit.get_node_or_null(NodePath(String(from_node_name))) as GraphNode
		var to_graph_node: GraphNode = _graph_edit.get_node_or_null(NodePath(String(to_node_name))) as GraphNode
		if from_graph_node == null or to_graph_node == null:
			continue

		if _is_audio_connection(from_graph_node, from_port, to_graph_node, to_port) and from_graph_node.has_signal("audio_frame"):
			var input_key: String = "%s:%d" % [String(to_node_name), to_port]
			var audio_count_for_input: int = int(audio_input_counts.get(input_key, 0))
			var use_pseudo_mix: bool = audio_count_for_input > 1
			var source_name: StringName = StringName(String(from_graph_node.name))
			var has_stereo_link: bool = false
			var stereo_enabled: bool = true
			if from_graph_node.has_method("get_stereo_enabled"):
				stereo_enabled = bool(from_graph_node.call("get_stereo_enabled"))

			if use_pseudo_mix:
				if not _audio_mix_inputs.has(input_key):
					_audio_mix_inputs[input_key] = {
						"to_node_name": String(to_node_name),
						"to_port": to_port,
						"mono_frames": [],
						"stereo_frames": [],
					}
				if stereo_enabled and from_graph_node.has_signal("audio_stereo_frame"):
					var mixed_stereo_callable: Callable = Callable(self, "_on_audio_mix_stereo_frame").bind(input_key)
					_add_runtime_link(from_graph_node, &"audio_stereo_frame", mixed_stereo_callable)
					has_stereo_link = true
				if not has_stereo_link:
					var mixed_mono_callable: Callable = Callable(self, "_on_audio_mix_mono_frame").bind(input_key)
					_add_runtime_link(from_graph_node, &"audio_frame", mixed_mono_callable)
			else:
				if stereo_enabled and from_graph_node.has_signal("audio_stereo_frame") and to_graph_node.has_method("push_audio_stereo_frame_from_source"):
					var stereo_callable: Callable = Callable(to_graph_node, "push_audio_stereo_frame_from_source").bind(source_name)
					_add_runtime_link(from_graph_node, &"audio_stereo_frame", stereo_callable)
					has_stereo_link = true
				elif stereo_enabled and from_graph_node.has_signal("audio_stereo_frame") and to_graph_node.has_method("push_audio_stereo_frame"):
					_add_runtime_link(from_graph_node, &"audio_stereo_frame", Callable(to_graph_node, "push_audio_stereo_frame"))
					has_stereo_link = true

				if to_graph_node.has_method("push_audio_frame_from_source"):
					if not has_stereo_link:
						var mono_callable: Callable = Callable(to_graph_node, "push_audio_frame_from_source").bind(source_name)
						_add_runtime_link(from_graph_node, &"audio_frame", mono_callable)
				elif to_graph_node.has_method("push_audio_frame"):
					if not has_stereo_link:
						_add_runtime_link(from_graph_node, &"audio_frame", Callable(to_graph_node, "push_audio_frame"))

		if to_graph_node.has_method("push_audio_input"):
			var input_callable: Callable = Callable(to_graph_node, "push_audio_input").bind(StringName(String(from_graph_node.name)))
			_add_runtime_link(from_graph_node, &"audio_frame", input_callable)

			var mix_key: String = String(to_graph_node.name)
			var mix_count: int = int(mixer_input_counts.get(mix_key, 0))
			mixer_input_counts[mix_key] = mix_count + 1

		if _is_baseband_connection(from_graph_node, from_port, to_graph_node, to_port) and from_graph_node.has_signal("baseband_frame"):
			if to_graph_node.has_method("push_baseband_frame"):
				if _is_demodulator_node(to_graph_node):
					var scoped_baseband_callable: Callable = Callable(self, "_on_baseband_frame_for_demod").bind(StringName(String(from_graph_node.name)), StringName(String(to_graph_node.name)))
					_add_runtime_link(from_graph_node, &"baseband_frame", scoped_baseband_callable)
				else:
					_add_runtime_link(from_graph_node, &"baseband_frame", Callable(to_graph_node, "push_baseband_frame"))

			if to_graph_node.has_method("set_input_sample_rate_hz"):
				var source_rate: Variant = null
				if from_graph_node.has_method("get_current_sample_rate_hz"):
					source_rate = from_graph_node.call("get_current_sample_rate_hz")
				elif from_graph_node.has_method("get_port_value"):
					source_rate = from_graph_node.call("get_port_value", 8)

				if source_rate != null:
					to_graph_node.call("set_input_sample_rate_hz", source_rate)

				if from_graph_node.has_signal("sample_rate_changed"):
					_add_runtime_link(from_graph_node, &"sample_rate_changed", Callable(to_graph_node, "set_input_sample_rate_hz"))

			if to_graph_node.has_signal("passthrough_frequency_request") and from_graph_node.has_method("set_port_value"):
				var passthrough_request_callable: Callable = Callable(self, "_on_passthrough_frequency_request").bind(
					StringName(String(from_graph_node.name)),
					StringName(String(to_graph_node.name))
				)
				_add_runtime_link(to_graph_node, &"passthrough_frequency_request", passthrough_request_callable)

			if _is_demodulator_node(to_graph_node) and to_graph_node.has_signal("offset_user_changed"):
				var user_offset_callable: Callable = Callable(self, "_on_demod_offset_user_changed").bind(StringName(String(to_graph_node.name)), StringName(String(from_graph_node.name)))
				_add_runtime_link(to_graph_node, &"offset_user_changed", user_offset_callable)

			if from_graph_node.has_signal("frequency_tuned_changed") and to_graph_node.has_method("set_upstream_tuned_frequency_hz"):
				_add_runtime_link(from_graph_node, &"frequency_tuned_changed", Callable(to_graph_node, "set_upstream_tuned_frequency_hz"))
				if from_graph_node.has_method("get_port_value"):
					to_graph_node.call("set_upstream_tuned_frequency_hz", int(from_graph_node.call("get_port_value", 1)))

		if to_graph_node.has_method("set_connected_input_count"):
			var target_port: int = to_port
			if target_port >= 0:
				var is_math_input: bool = false
				if to_graph_node.has_method("is_input_port"):
					is_math_input = bool(to_graph_node.call("is_input_port", target_port))
				if is_math_input:
					var math_key: String = String(to_graph_node.name)
					var math_count: int = int(math_input_counts.get(math_key, 0))
					math_input_counts[math_key] = math_count + 1

	for child_value in _graph_edit.get_children():
		var child: Node = child_value as Node
		if child == null:
			continue

		if child.has_method("set_connected_source_count"):
			var connected_audio_count: int = int(mixer_input_counts.get(String(child.name), 0))
			child.call("set_connected_source_count", connected_audio_count)

		if child.has_method("set_connected_input_count"):
			var connected_math_count: int = int(math_input_counts.get(String(child.name), 0))
			child.call("set_connected_input_count", connected_math_count)

	_clear_stale_locked_demod_absolute_entries()
	_sync_waterfall_runtime_links()


func _propagate_control_connections() -> void:
	if _graph_edit == null:
		return

	var connection_list: Array = _graph_edit.get_connection_list()
	var needs_resync: bool = false
	var active_control_keys: Dictionary = {}
	for connection_value in connection_list:
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue

		var from_node_name: StringName = StringName(connection.get("from_node", StringName()))
		var to_node_name: StringName = StringName(connection.get("to_node", StringName()))
		var from_port: int = int(connection.get("from_port", -1))
		var to_port: int = int(connection.get("to_port", -1))
		if from_port < 0 or to_port < 0:
			continue

		var from_node: Node = _graph_edit.get_node_or_null(NodePath(String(from_node_name))) as Node
		var to_node: Node = _graph_edit.get_node_or_null(NodePath(String(to_node_name))) as Node
		if from_node == null or to_node == null:
			continue
		var from_graph_node: GraphNode = from_node as GraphNode
		var to_graph_node: GraphNode = to_node as GraphNode
		if not _are_ports_compatible(from_graph_node, from_port, to_graph_node, to_port):
			if _graph_edit.is_node_connected(from_node_name, from_port, to_node_name, to_port):
				_graph_edit.disconnect_node(from_node_name, from_port, to_node_name, to_port)
				needs_resync = true
			continue
		if from_graph_node == null:
			continue

		var signal_type: int = from_graph_node.get_output_port_type(from_port)
		if signal_type == _SIGNAL_BASEBAND_TYPE or signal_type == _SIGNAL_AUDIO_TYPE:
			continue
		if not from_node.has_method("get_port_value") or not to_node.has_method("set_port_value"):
			continue

		var value: Variant = from_node.call("get_port_value", from_port)
		if value == null:
			continue

		var connection_key: String = _make_connection_key(from_node_name, from_port, to_node_name, to_port)
		active_control_keys[connection_key] = true
		if to_port == 1 and _is_source_node(to_graph_node) and _is_source_scan_locked(to_node_name):
			_control_value_cache.erase(connection_key)
			continue
		if _control_value_cache.has(connection_key) and _control_value_cache[connection_key] == value:
			continue

		to_node.call("set_port_value", to_port, value)
		_control_value_cache[connection_key] = value

	var stale_keys: Array = []
	for cache_key_value in _control_value_cache.keys():
		var cache_key: String = String(cache_key_value)
		if active_control_keys.has(cache_key):
			continue
		stale_keys.append(cache_key)

	for stale_key_value in stale_keys:
		var stale_key: String = String(stale_key_value)
		_control_value_cache.erase(stale_key)

	if needs_resync:
		_sync_runtime_connections()


func _is_source_node(p_node: GraphNode) -> bool:
	if p_node == null:
		return false
	return p_node.has_signal("baseband_frame") and p_node.has_method("get_current_sample_rate_hz") and p_node.has_method("get_port_value")


func _is_source_scan_locked(p_source_name: StringName) -> bool:
	var panel: Control = _waterfall_panels.get(String(p_source_name), null) as Control
	if panel == null:
		return false
	if not panel.has_method("is_wide_scan_running"):
		return false
	return bool(panel.call("is_wide_scan_running"))


func _set_source_frequency_scan_lock(p_source_name: StringName, p_locked: bool) -> void:
	if _graph_edit == null:
		return
	var source_node: Node = _graph_edit.get_node_or_null(NodePath(String(p_source_name))) as Node
	if source_node == null:
		return
	if source_node.has_method("set_frequency_control_locked"):
		source_node.call("set_frequency_control_locked", p_locked)


func _is_demodulator_node(p_node: GraphNode) -> bool:
	if p_node == null:
		return false
	return p_node.is_class("WFMDemodulator") or p_node.is_class("NFMDemodulator") or p_node.is_class("AMDemodulator") or p_node.is_class("SSBDemodulator") or p_node.is_class("CWDemodulator")


func _get_source_nodes() -> Array[GraphNode]:
	var sources: Array[GraphNode] = []
	if _graph_edit == null:
		return sources

	for child_value in _graph_edit.get_children():
		var graph_node: GraphNode = child_value as GraphNode
		if not _is_source_node(graph_node):
			continue
		sources.append(graph_node)
	return sources


func _create_waterfall_panel_for_source(p_source_name: StringName) -> Control:
	if _waterfall_tabs == null:
		return null
	var source_node: GraphNode = _graph_edit.get_node_or_null(NodePath(String(p_source_name))) as GraphNode
	var panel: Control = _WATERFALL_PANEL_SCRIPT.new() as Control
	if panel == null:
		return null
	panel.name = String(p_source_name) + "_Waterfall"
	panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	if panel.has_signal("demod_range_changed"):
		var changed_callable: Callable = Callable(self, "_on_waterfall_demod_range_changed").bind(p_source_name)
		panel.connect("demod_range_changed", changed_callable)
	if panel.has_signal("demod_selected"):
		var select_callable: Callable = Callable(self, "_on_waterfall_demod_selected")
		if not panel.is_connected("demod_selected", select_callable):
			panel.connect("demod_selected", select_callable)
	if panel.has_signal("demod_context_requested"):
		var demod_context_callable: Callable = Callable(self, "_on_waterfall_demod_context_requested").bind(p_source_name)
		panel.connect("demod_context_requested", demod_context_callable)
	if panel.has_signal("source_center_changed"):
		var center_callable: Callable = Callable(self, "_on_waterfall_source_center_changed").bind(p_source_name)
		panel.connect("source_center_changed", center_callable)
	if panel.has_signal("wide_scan_requested"):
		var wide_scan_request_callable: Callable = Callable(self, "_on_wide_scan_requested").bind(p_source_name)
		panel.connect("wide_scan_requested", wide_scan_request_callable)
	if panel.has_signal("wide_scan_cancelled"):
		var wide_scan_cancel_callable: Callable = Callable(self, "_on_wide_scan_cancelled").bind(p_source_name)
		panel.connect("wide_scan_cancelled", wide_scan_cancel_callable)
	if panel.has_signal("wide_scan_completed"):
		var wide_scan_complete_callable: Callable = Callable(self, "_on_wide_scan_completed").bind(p_source_name)
		panel.connect("wide_scan_completed", wide_scan_complete_callable)
	_waterfall_tabs.add_child(panel)
	_waterfall_tabs.set_tab_title(_waterfall_tabs.get_tab_count() - 1, _get_graph_node_display_label(source_node, String(p_source_name)))
	_waterfall_panels[String(p_source_name)] = panel
	return panel


func _refresh_waterfall_tabs() -> void:
	if _waterfall_tabs == null:
		return

	var sources: Array[GraphNode] = _get_source_nodes()
	_waterfall_tabs.visible = not sources.is_empty()
	if sources.is_empty():
		for child_value in _waterfall_tabs.get_children():
			var child: Node = child_value as Node
			if child == null:
				continue
			_waterfall_tabs.remove_child(child)
			child.queue_free()
		_waterfall_panels.clear()
		_waterfall_next_forward_us.clear()
		return

	var source_names: Dictionary = {}
	for source_node in sources:
		source_names[String(source_node.name)] = true

	for panel_key_value in _waterfall_panels.keys():
		var panel_key: String = String(panel_key_value)
		if source_names.has(panel_key):
			continue
		var stale_panel: Control = _waterfall_panels.get(panel_key, null) as Control
		if stale_panel != null and stale_panel.get_parent() == _waterfall_tabs:
			_waterfall_tabs.remove_child(stale_panel)
			stale_panel.queue_free()
		_waterfall_panels.erase(panel_key)
		_waterfall_next_forward_us.erase(panel_key)

	for source_node in sources:
		var source_name: StringName = StringName(String(source_node.name))
		var panel: Control = _waterfall_panels.get(String(source_name), null) as Control
		if panel == null:
			panel = _create_waterfall_panel_for_source(source_name)
		if panel == null:
			continue
		var tab_index: int = panel.get_index()
		if tab_index >= 0:
			_waterfall_tabs.set_tab_title(tab_index, _get_graph_node_display_label(source_node, String(source_name)))


func _get_graph_node_display_label(p_node: GraphNode, p_fallback: String = "") -> String:
	if p_node == null:
		return p_fallback
	var label: String = String(p_node.title).strip_edges()
	if label.is_empty():
		label = String(p_node.name).strip_edges()
	if label.is_empty():
		label = p_fallback
	return label


func _collect_demodulators_for_source(p_source_name: StringName) -> Array[Dictionary]:
	var demods: Array[Dictionary] = []
	if _graph_edit == null:
		return demods

	var connection_list: Array = _graph_edit.get_connection_list()
	for connection_value in connection_list:
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue

		var from_node_name: StringName = StringName(connection.get("from_node", StringName()))
		var to_node_name: StringName = StringName(connection.get("to_node", StringName()))
		if from_node_name != p_source_name:
			continue

		var from_port: int = int(connection.get("from_port", -1))
		var to_port: int = int(connection.get("to_port", -1))
		if from_port < 0 or to_port < 0:
			continue

		var from_node: GraphNode = _graph_edit.get_node_or_null(NodePath(String(from_node_name))) as GraphNode
		var to_node: GraphNode = _graph_edit.get_node_or_null(NodePath(String(to_node_name))) as GraphNode
		if not _is_baseband_connection(from_node, from_port, to_node, to_port):
			continue
		if not _is_demodulator_node(to_node):
			continue

		var offset_value: int = 0
		if to_node.has_method("get_offset_hz"):
			offset_value = int(round(float(to_node.call("get_offset_hz"))))
		elif to_node.has_method("get_port_value"):
			offset_value = int(to_node.call("get_port_value", 0))
		var bandwidth_value: int = 150000
		if to_node.has_method("get_bandwidth_hz"):
			bandwidth_value = int(round(float(to_node.call("get_bandwidth_hz"))))
		elif to_node.has_method("get_port_value"):
			bandwidth_value = int(to_node.call("get_port_value", 1))
		var lock_to_source: bool = false
		if to_node.has_method("get_lock_to_source_frequency"):
			lock_to_source = bool(to_node.call("get_lock_to_source_frequency"))

		demods.append({
			"node_name": StringName(String(to_node.name)),
			"label": String(to_node.title).strip_edges(),
			"offset_hz": offset_value,
			"bandwidth_hz": max(1000, bandwidth_value),
			"lock_to_source": lock_to_source,
			"selected": to_node.selected,
		})

	return demods


func _get_demod_offset_hz(p_demod_node: Node) -> int:
	if p_demod_node == null:
		return 0
	if p_demod_node.has_method("get_offset_hz"):
		return int(round(float(p_demod_node.call("get_offset_hz"))))
	if p_demod_node.has_method("get_port_value"):
		return int(p_demod_node.call("get_port_value", 0))
	return 0


func _set_demod_offset_hz(p_demod_node: Node, p_offset_hz: int) -> void:
	if p_demod_node == null:
		return
	if p_demod_node.has_method("set_offset_hz"):
		p_demod_node.call("set_offset_hz", p_offset_hz)
	elif p_demod_node.has_method("set_port_value"):
		p_demod_node.call("set_port_value", 0, p_offset_hz)


func _enforce_locked_demod_absolute_for_source(p_source_name: StringName, p_source_center_hz: int) -> void:
	if _graph_edit == null:
		return

	var connection_list: Array = _graph_edit.get_connection_list()
	for connection_value in connection_list:
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue

		var from_node_name: StringName = StringName(connection.get("from_node", StringName()))
		var to_node_name: StringName = StringName(connection.get("to_node", StringName()))
		if from_node_name != p_source_name:
			continue

		var from_port: int = int(connection.get("from_port", -1))
		var to_port: int = int(connection.get("to_port", -1))
		if from_port < 0 or to_port < 0:
			continue

		var from_node: GraphNode = _graph_edit.get_node_or_null(NodePath(String(from_node_name))) as GraphNode
		var to_node: GraphNode = _graph_edit.get_node_or_null(NodePath(String(to_node_name))) as GraphNode
		if not _is_baseband_connection(from_node, from_port, to_node, to_port):
			continue
		if not _is_demodulator_node(to_node):
			continue

		var lock_key: String = String(p_source_name) + "::" + String(to_node_name)
		var lock_enabled: bool = false
		if to_node.has_method("get_lock_to_source_frequency"):
			lock_enabled = bool(to_node.call("get_lock_to_source_frequency"))

		if not lock_enabled:
			if _locked_demod_absolute_hz.has(lock_key):
				_locked_demod_absolute_hz.erase(lock_key)
			continue

		var current_offset_hz: int = _get_demod_offset_hz(to_node)
		var target_absolute_hz: int = p_source_center_hz + current_offset_hz
		if _locked_demod_absolute_hz.has(lock_key):
			target_absolute_hz = int(_locked_demod_absolute_hz[lock_key])
		else:
			_locked_demod_absolute_hz[lock_key] = target_absolute_hz

		var target_offset_hz: int = target_absolute_hz - p_source_center_hz
		if target_offset_hz != current_offset_hz:
			_set_demod_offset_hz(to_node, target_offset_hz)


func _clear_stale_locked_demod_absolute_entries() -> void:
	if _graph_edit == null:
		_locked_demod_absolute_hz.clear()
		return

	var valid_keys: Dictionary = {}
	var connection_list: Array = _graph_edit.get_connection_list()
	for connection_value in connection_list:
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue
		var from_node_name: StringName = StringName(connection.get("from_node", StringName()))
		var to_node_name: StringName = StringName(connection.get("to_node", StringName()))
		var from_port: int = int(connection.get("from_port", -1))
		var to_port: int = int(connection.get("to_port", -1))
		if from_port < 0 or to_port < 0:
			continue
		var from_node: GraphNode = _graph_edit.get_node_or_null(NodePath(String(from_node_name))) as GraphNode
		var to_node: GraphNode = _graph_edit.get_node_or_null(NodePath(String(to_node_name))) as GraphNode
		if not _is_baseband_connection(from_node, from_port, to_node, to_port):
			continue
		if not _is_demodulator_node(to_node):
			continue
		valid_keys[String(from_node_name) + "::" + String(to_node_name)] = true

	var stale_keys: Array = []
	for key_value in _locked_demod_absolute_hz.keys():
		var key: String = String(key_value)
		if valid_keys.has(key):
			continue
		stale_keys.append(key)
	for key_value in stale_keys:
		var key: String = String(key_value)
		_locked_demod_absolute_hz.erase(key)


func _get_source_effective_center_hz(p_source_name: StringName, p_source_node: Node) -> int:
	var source_key: String = String(p_source_name)
	if _confirmed_source_center_hz.has(source_key):
		return int(_confirmed_source_center_hz[source_key])
	if p_source_node != null and p_source_node.has_method("get_port_value"):
		return int(p_source_node.call("get_port_value", 1))
	return 0


func _update_waterfall_views() -> void:
	if _waterfall_tabs == null:
		return
	if not _waterfall_tabs.visible:
		return

	for source_node in _get_source_nodes():
		var source_name: String = String(source_node.name)
		var panel: Control = _waterfall_panels.get(source_name, null) as Control
		if panel == null:
			continue

		var center_hz: int = _get_source_effective_center_hz(StringName(source_name), source_node)
		var visual_center_hz: int = int(_waterfall_visual_center_hz.get(source_name, center_hz))
		_enforce_locked_demod_absolute_for_source(StringName(source_name), visual_center_hz)
		var sample_rate_hz: float = 2400000.0
		if source_node.has_method("get_current_sample_rate_hz"):
			sample_rate_hz = float(source_node.call("get_current_sample_rate_hz"))
		var min_frequency_hz: float = 0.0
		var max_frequency_hz: float = 999000000000.0
		var min_sample_rate_hz: float = 1000.0
		var max_sample_rate_hz: float = sample_rate_hz
		if source_node.has_method("get_min_frequency_hz"):
			min_frequency_hz = float(source_node.call("get_min_frequency_hz"))
		if source_node.has_method("get_max_frequency_hz"):
			max_frequency_hz = float(source_node.call("get_max_frequency_hz"))
		if source_node.has_method("get_min_sample_rate_hz"):
			min_sample_rate_hz = float(source_node.call("get_min_sample_rate_hz"))
		if source_node.has_method("get_max_sample_rate_hz"):
			max_sample_rate_hz = float(source_node.call("get_max_sample_rate_hz"))
		var replay_playing: bool = false
		if source_node.has_method("is_replay_playing"):
			replay_playing = bool(source_node.call("is_replay_playing"))
		var replay_recording: bool = false
		if source_node.has_method("is_record_enabled"):
			replay_recording = bool(source_node.call("is_record_enabled"))
		var passthrough_enabled: bool = true
		if source_node.has_method("is_passthrough_enabled"):
			passthrough_enabled = bool(source_node.call("is_passthrough_enabled"))
		var show_frequency_labels: bool = passthrough_enabled and not replay_playing and not replay_recording

		if panel.has_method("set_source_state"):
			panel.call("set_source_state", visual_center_hz, sample_rate_hz)
		if panel.has_method("set_source_capabilities"):
			panel.call("set_source_capabilities", min_frequency_hz, max_frequency_hz, min_sample_rate_hz, max_sample_rate_hz)
		if panel.has_method("set_replay_playback_mode"):
			panel.call("set_replay_playback_mode", replay_playing)
		if panel.has_method("set_frequency_labels_visible"):
			panel.call("set_frequency_labels_visible", show_frequency_labels)
		if panel.has_method("set_demodulators"):
			panel.call("set_demodulators", _collect_demodulators_for_source(StringName(source_name)))


func _on_waterfall_source_frame(p_frame: PackedVector2Array, p_source_name: StringName) -> void:
	if _waterfall_tabs == null or not _waterfall_tabs.visible:
		return
	var panel: Control = _waterfall_panels.get(String(p_source_name), null) as Control
	if panel == null:
		return
	var selected_tab_index: int = _waterfall_tabs.current_tab
	if selected_tab_index < 0:
		return
	var selected_panel: Control = _waterfall_tabs.get_tab_control(selected_tab_index) as Control
	var panel_is_active: bool = selected_panel != null and selected_panel == panel
	var allow_background_wide_scan: bool = false
	if not panel_is_active and panel.has_method("is_wide_scan_running"):
		allow_background_wide_scan = bool(panel.call("is_wide_scan_running"))
	if not panel_is_active and not allow_background_wide_scan:
		return

	var forward_fps: float = 0.0
	if panel.has_method("get_forward_fps_hint"):
		forward_fps = float(panel.call("get_forward_fps_hint"))
	if forward_fps <= 0.0:
		return

	var source_key: String = String(p_source_name)
	var now_us: int = Time.get_ticks_usec()
	var next_allowed_us: int = int(_waterfall_next_forward_us.get(source_key, 0))
	if now_us < next_allowed_us:
		return
	var interval_us: int = int(1_000_000.0 / max(forward_fps, 1.0))
	_waterfall_next_forward_us[source_key] = now_us + interval_us

	if panel.has_method("push_baseband_frame"):
		panel.call("push_baseband_frame", p_frame)


func _sync_waterfall_runtime_links() -> void:
	_refresh_waterfall_tabs()
	if _waterfall_tabs == null:
		return
	if not _waterfall_tabs.visible:
		return

	for source_node in _get_source_nodes():
		var source_name: StringName = StringName(String(source_node.name))
		if source_node.has_signal("baseband_frame"):
			var frame_callable: Callable = Callable(self, "_on_waterfall_source_frame").bind(source_name)
			_add_runtime_link(source_node, &"baseband_frame", frame_callable)

	_update_waterfall_views()


func _on_waterfall_demod_range_changed(p_node_name: StringName, p_offset_hz: int, p_bandwidth_hz: int, _p_center_hz: int, _p_drag_mode: int, _p_source_name: StringName) -> void:
	if _graph_edit == null:
		return

	var demod_node: Node = _graph_edit.get_node_or_null(NodePath(String(p_node_name))) as Node
	if demod_node == null:
		return
	if not demod_node.has_method("set_port_value") and not demod_node.has_method("set_offset_hz"):
		return

	if demod_node.has_method("set_offset_hz"):
		demod_node.call("set_offset_hz", p_offset_hz)
	elif demod_node.has_method("set_port_value"):
		demod_node.call("set_port_value", 0, p_offset_hz)
	if demod_node.has_method("set_bandwidth_hz"):
		demod_node.call("set_bandwidth_hz", p_bandwidth_hz)
	elif demod_node.has_method("set_port_value"):
		demod_node.call("set_port_value", 1, p_bandwidth_hz)
	if demod_node.has_method("get_lock_to_source_frequency") and bool(demod_node.call("get_lock_to_source_frequency")):
		var source_node: Node = _graph_edit.get_node_or_null(NodePath(String(_p_source_name))) as Node
		if source_node != null:
			var source_key: String = String(_p_source_name)
			var tuned_center_hz: int = int(_waterfall_visual_center_hz.get(source_key, _get_source_effective_center_hz(_p_source_name, source_node)))
			var lock_key: String = String(_p_source_name) + "::" + String(p_node_name)
			_locked_demod_absolute_hz[lock_key] = tuned_center_hz + p_offset_hz


func _on_demod_offset_user_changed(p_offset_hz: int, p_demod_name: StringName, p_source_name: StringName) -> void:
	if _graph_edit == null:
		return
	var demod_node: Node = _graph_edit.get_node_or_null(NodePath(String(p_demod_name))) as Node
	if demod_node == null:
		return
	if not demod_node.has_method("get_lock_to_source_frequency"):
		return
	if not bool(demod_node.call("get_lock_to_source_frequency")):
		return
	var source_node: Node = _graph_edit.get_node_or_null(NodePath(String(p_source_name))) as Node
	var source_key: String = String(p_source_name)
	var source_center_hz: int = int(_waterfall_visual_center_hz.get(source_key, _get_source_effective_center_hz(p_source_name, source_node)))
	var lock_key: String = String(p_source_name) + "::" + String(p_demod_name)
	_locked_demod_absolute_hz[lock_key] = source_center_hz + p_offset_hz


func _on_waterfall_demod_selected(p_node_name: StringName) -> void:
	if _graph_edit == null:
		return

	var target: GraphNode = _graph_edit.get_node_or_null(NodePath(String(p_node_name))) as GraphNode
	if target == null:
		return

	for child_value in _graph_edit.get_children():
		var graph_node: GraphNode = child_value as GraphNode
		if graph_node == null:
			continue
		graph_node.selected = graph_node == target
	_update_waterfall_views()


func _on_waterfall_demod_context_requested(p_node_name: StringName, p_global_position: Vector2i, _p_source_name: StringName) -> void:
	if _graph_edit == null:
		return
	var demod_node: GraphNode = _graph_edit.get_node_or_null(NodePath(String(p_node_name))) as GraphNode
	if demod_node == null:
		return
	_show_replace_demod_menu(Vector2(p_global_position), demod_node)


func _on_waterfall_source_center_changed(p_center_hz: int, p_source_name: StringName) -> void:
	if _graph_edit == null:
		return

	var source_key: String = String(p_source_name)
	_waterfall_visual_center_hz[source_key] = p_center_hz

	var source_node: Node = _graph_edit.get_node_or_null(NodePath(String(p_source_name))) as Node
	if source_node == null:
		return
	if not source_node.has_method("set_port_value"):
		return

	if _is_source_scan_locked(p_source_name):
		if source_node.has_method("set_scan_frequency_hz"):
			source_node.call("set_scan_frequency_hz", p_center_hz)
		else:
			source_node.call("set_port_value", 1, p_center_hz)
		return
	source_node.call("set_port_value", 1, p_center_hz)


func _restore_source_sample_rate_from_wide_scan(p_source_name: StringName) -> void:
	_set_source_frequency_scan_lock(p_source_name, false)
	if _graph_edit == null:
		return
	var source_key: String = String(p_source_name)
	var has_saved_sample_rate: bool = _wide_scan_saved_sample_rate_hz.has(source_key)
	var has_saved_offsets: bool = _wide_scan_saved_demod_offsets_hz.has(source_key)
	if not has_saved_sample_rate and not has_saved_offsets:
		return
	var source_node: Node = _graph_edit.get_node_or_null(NodePath(source_key)) as Node
	if source_node == null:
		_wide_scan_saved_sample_rate_hz.erase(source_key)
		_wide_scan_saved_center_hz.erase(source_key)
		_wide_scan_saved_demod_offsets_hz.erase(source_key)
		return
	if has_saved_sample_rate and source_node.has_method("set_port_value"):
		source_node.call("set_port_value", 8, int(_wide_scan_saved_sample_rate_hz[source_key]))
		if _wide_scan_saved_center_hz.has(source_key):
			var restored_center_hz: int = int(_wide_scan_saved_center_hz[source_key])
			source_node.call("set_port_value", 1, restored_center_hz)
			_waterfall_visual_center_hz[source_key] = restored_center_hz
			_confirmed_source_center_hz[source_key] = restored_center_hz
	if has_saved_offsets:
		var saved_offsets: Dictionary = _wide_scan_saved_demod_offsets_hz[source_key] as Dictionary
		for demod_key_value in saved_offsets.keys():
			var demod_name: String = String(demod_key_value)
			var demod_node: Node = _graph_edit.get_node_or_null(NodePath(demod_name)) as Node
			if demod_node == null:
				continue
			if not demod_node.has_method("set_port_value"):
				continue
			demod_node.call("set_port_value", 1, int(saved_offsets[demod_key_value]))
	_wide_scan_saved_sample_rate_hz.erase(source_key)
	_wide_scan_saved_center_hz.erase(source_key)
	_wide_scan_saved_demod_offsets_hz.erase(source_key)


func _on_wide_scan_requested(p_dwell_ms: int, p_start_hz: int, p_end_hz: int, _p_loop_enabled: bool, p_source_name: StringName) -> void:
	if _graph_edit == null:
		return

	var source_key: String = String(p_source_name)
	var source_node: Node = _graph_edit.get_node_or_null(NodePath(source_key)) as Node
	if source_node == null:
		return
	var panel: Control = _waterfall_panels.get(source_key, null) as Control
	if panel == null:
		return
	if not panel.has_method("begin_wide_scan"):
		return

	var current_sample_rate_hz: int = 2400000
	if source_node.has_method("get_current_sample_rate_hz"):
		current_sample_rate_hz = int(source_node.call("get_current_sample_rate_hz"))
	var current_center_hz: int = _get_source_effective_center_hz(p_source_name, source_node)
	_wide_scan_saved_sample_rate_hz[source_key] = current_sample_rate_hz
	_wide_scan_saved_center_hz[source_key] = current_center_hz
	var saved_demod_offsets: Dictionary = {}
	var connection_list: Array = _graph_edit.get_connection_list()
	for connection_value in connection_list:
		var connection: Dictionary = connection_value as Dictionary
		if connection.is_empty():
			continue
		var from_node_name: StringName = StringName(connection.get("from_node", StringName()))
		if from_node_name != p_source_name:
			continue
		var to_node_name: StringName = StringName(connection.get("to_node", StringName()))
		var to_graph_node: GraphNode = _graph_edit.get_node_or_null(NodePath(String(to_node_name))) as GraphNode
		if not _is_demodulator_node(to_graph_node):
			continue
		var demod_node: Node = to_graph_node as Node
		if demod_node == null or not demod_node.has_method("get_port_value"):
			continue
		saved_demod_offsets[String(to_node_name)] = int(demod_node.call("get_port_value", 1))
	_wide_scan_saved_demod_offsets_hz[source_key] = saved_demod_offsets
	var scan_sample_rate_hz: int = clamp(current_sample_rate_hz, 1000, 64000000)

	var min_frequency_hz: int = p_start_hz
	var max_frequency_hz: int = p_end_hz
	if min_frequency_hz > max_frequency_hz:
		var tmp_hz: int = min_frequency_hz
		min_frequency_hz = max_frequency_hz
		max_frequency_hz = tmp_hz
	if source_node.has_method("get_min_frequency_hz"):
		min_frequency_hz = max(min_frequency_hz, int(source_node.call("get_min_frequency_hz")))
	if source_node.has_method("get_max_frequency_hz"):
		max_frequency_hz = min(max_frequency_hz, int(source_node.call("get_max_frequency_hz")))
	if max_frequency_hz <= min_frequency_hz:
		min_frequency_hz = max(0, current_center_hz - scan_sample_rate_hz)
		max_frequency_hz = current_center_hz + scan_sample_rate_hz
	var span_hz: int = max_frequency_hz - min_frequency_hz
	if span_hz > 0:
		var section_count: int = int(ceil(float(span_hz) / float(scan_sample_rate_hz)))
		if section_count > 4096:
			max_frequency_hz = min_frequency_hz + (scan_sample_rate_hz * 4096)

	_set_source_frequency_scan_lock(p_source_name, true)
	panel.call("begin_wide_scan", min_frequency_hz, max_frequency_hz, float(scan_sample_rate_hz), p_dwell_ms)


func _on_wide_scan_cancelled(p_source_name: StringName) -> void:
	_restore_source_sample_rate_from_wide_scan(p_source_name)


func _on_wide_scan_completed(p_source_name: StringName) -> void:
	_restore_source_sample_rate_from_wide_scan(p_source_name)


func _on_passthrough_frequency_request(p_frequency_hz: int, p_upstream_source_name: StringName, p_demod_name: StringName) -> void:
	if _graph_edit == null:
		return
	if _is_source_scan_locked(p_upstream_source_name):
		return

	var upstream_node: Node = _graph_edit.get_node_or_null(NodePath(String(p_upstream_source_name))) as Node
	if upstream_node == null:
		return
	if not upstream_node.has_method("set_port_value"):
		return

	var source_center_before_hz: int = _get_source_effective_center_hz(p_upstream_source_name, upstream_node)
	upstream_node.call("set_port_value", 1, p_frequency_hz)
	var source_delta_hz: int = p_frequency_hz - source_center_before_hz
	if source_delta_hz == 0:
		return

	var demod_node: Node = _graph_edit.get_node_or_null(NodePath(String(p_demod_name))) as Node
	if demod_node == null:
		return
	var demod_graph_node: GraphNode = demod_node as GraphNode
	if not _is_demodulator_node(demod_graph_node):
		return
	if demod_node.has_method("get_lock_to_source_frequency") and bool(demod_node.call("get_lock_to_source_frequency")):
		return

	var current_offset_hz: int = _get_demod_offset_hz(demod_node)
	var adjusted_offset_hz: int = current_offset_hz - source_delta_hz
	if adjusted_offset_hz != current_offset_hz:
		_set_demod_offset_hz(demod_node, adjusted_offset_hz)

func _on_source_frequency_tuned_changed(p_tuned_hz: int, p_source_name: StringName) -> void:
	if _graph_edit == null:
		return

	var source_node: Node = _graph_edit.get_node_or_null(NodePath(String(p_source_name))) as Node
	if source_node == null:
		return

	var source_key: String = String(p_source_name)
	_confirmed_source_center_hz[source_key] = p_tuned_hz

	var should_update_visual_center: bool = true
	var panel: Control = _waterfall_panels.get(source_key, null) as Control
	if panel != null and panel.has_method("is_source_retune_drag_active"):
		should_update_visual_center = not bool(panel.call("is_source_retune_drag_active"))
	if should_update_visual_center:
		_waterfall_visual_center_hz[source_key] = p_tuned_hz

	_update_waterfall_views()
