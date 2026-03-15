extends Control

const _EXTENSION_PATH := "res://esdr.gdextension"
const _NODE_MENU_RTL := 1
const _NODE_MENU_NFM := 2

@export_node_path("GraphEdit") var _graph_edit_path: NodePath = NodePath("VBoxContainer/Control/GraphEdit")
@export_node_path("Button") var _add_node_button_path: NodePath = NodePath("VBoxContainer/Control/Button")

@onready var _graph_edit: GraphEdit = get_node_or_null(_graph_edit_path) as GraphEdit
@onready var _add_node_button: Button = get_node_or_null(_add_node_button_path) as Button

var _rtl_count: int = 0
var _nfm_count: int = 0
var _add_node_menu: PopupMenu


func _ready() -> void:
	if _add_node_button and not _add_node_button.pressed.is_connected(_on_add_node_pressed):
		_add_node_button.pressed.connect(_on_add_node_pressed)

	if _graph_edit:
		if not _graph_edit.connection_request.is_connected(_on_connection_request):
			_graph_edit.connection_request.connect(_on_connection_request)
		if not _graph_edit.disconnection_request.is_connected(_on_disconnection_request):
			_graph_edit.disconnection_request.connect(_on_disconnection_request)

	if not _ensure_extension_loaded():
		push_error("Failed to load ESDR extension; RTL nodes are unavailable.")
		return

	_setup_add_node_menu()
	_add_rtl_node(Vector2(200, 0))
	_add_nfm_node(Vector2(800, 200))


func _ensure_extension_loaded() -> bool:
	if _has_native_factory():
		return true

	var load_result := GDExtensionManager.load_extension(_EXTENSION_PATH)
	if load_result != OK and not _has_native_factory():
		return false

	return _has_native_factory()


func _has_native_factory() -> bool:
	return ClassDB.class_exists("ESDRGraphFactory")


func _add_rtl_node(position_offset: Vector2) -> void:
	if _graph_edit == null:
		return

	var node := _create_module_node("create_rtl_sdr_source")
	if node == null:
		push_error("RTLSDRSource class is not available.")
		return

	_rtl_count += 1
	node.name = "RTLSDRSource%d" % _rtl_count
	node.position_offset = position_offset
	_graph_edit.add_child(node)


func _add_nfm_node(position_offset: Vector2) -> void:
	if _graph_edit == null:
		return

	var node := _create_module_node("create_nfm_demodulator")
	if node == null:
		push_error("NFMDemodulator class is not available.")
		return

	_nfm_count += 1
	node.name = "NFMDemodulator%d" % _nfm_count
	node.position_offset = position_offset
	_graph_edit.add_child(node)


func _create_module_node(p_factory_method: String) -> GraphNode:
	if not _has_native_factory():
		return null

	var raw: Variant = ClassDB.class_call_static("ESDRGraphFactory", p_factory_method)
	var object_value: Object = raw as Object
	if object_value == null:
		return null

	return object_value as GraphNode


func _setup_add_node_menu() -> void:
	if _add_node_menu != null:
		return

	var popup := PopupMenu.new()
	popup.name = "AddNodeMenu"
	popup.add_item("RTL-SDR Source", _NODE_MENU_RTL)
	popup.add_item("NFM Demodulator", _NODE_MENU_NFM)
	popup.id_pressed.connect(_on_add_node_menu_id_pressed)
	add_child(popup)
	_add_node_menu = popup


func _on_add_node_pressed() -> void:
	if _add_node_menu == null or _add_node_button == null:
		return

	var x: int = int(round(_add_node_button.global_position.x))
	var y: int = int(round(_add_node_button.global_position.y + _add_node_button.size.y))
	_add_node_menu.position = Vector2i(x, y)
	_add_node_menu.popup()


func _on_add_node_menu_id_pressed(id: int) -> void:
	if id == _NODE_MENU_RTL:
		var offset := Vector2(200 + 60 * _rtl_count, 40 * _rtl_count)
		_add_rtl_node(offset)
		return

	if id == _NODE_MENU_NFM:
		var offset := Vector2(800 + 40 * _nfm_count, 200 + 30 * _nfm_count)
		_add_nfm_node(offset)


func _on_connection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	if from_node == to_node:
		return

	if _graph_edit.is_node_connected(from_node, from_port, to_node, to_port):
		return

	_graph_edit.connect_node(from_node, from_port, to_node, to_port)


func _on_disconnection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	if not _graph_edit.is_node_connected(from_node, from_port, to_node, to_port):
		return

	_graph_edit.disconnect_node(from_node, from_port, to_node, to_port)
