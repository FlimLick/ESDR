extends Control
class_name WaterfallPanel

signal demod_range_changed(node_name: StringName, offset_hz: int, bandwidth_hz: int, center_hz: int, drag_mode: int)
signal demod_selected(node_name: StringName)
signal source_center_changed(center_hz: int)
signal wide_scan_requested(dwell_ms: int, start_hz: int, end_hz: int, loop_enabled: bool)
signal wide_scan_cancelled()
signal wide_scan_completed()
signal demod_context_requested(node_name: StringName, global_position: Vector2i)

const DEFAULT_FFT_SIZE: int = 2048
const HISTORY_ROWS: int = 220
const LABEL_HEIGHT: float = 32.0
const FREQ_LABEL_BAND_HEIGHT: float = 18.0
const HOVER_LABEL_BAND_HEIGHT: float = 14.0
const INFO_LABEL_BAND_HEIGHT: float = 14.0
const DEMOD_CAP_HEIGHT: float = 3.0
const EDGE_HIT_PX: float = 8.0
const MIN_DEMOD_RESIZE_WIDTH_PX: float = (EDGE_HIT_PX * 2.0) + 12.0
const MIN_BANDWIDTH_HZ: float = 1000.0
const SLIDER_PANEL_WIDTH: float = 74.0
const MIN_UPDATE_FPS: float = 8.0
const MAX_UPDATE_FPS: float = 240.0
const MIN_SPAN_DIVISOR: float = 4096.0
const MAX_SPAN_SCALE: float = 2048.0
const MAX_WIDE_SCAN_BINS: int = 32768
const MIN_WIDE_SCAN_BINS: int = 2048
const WIDE_SCAN_TEXTURE_HEIGHT: int = 1
const WIDE_SCAN_DEFAULT_MIN_HZ: float = 0.0
const WIDE_SCAN_DEFAULT_MAX_HZ: float = 1_000_000_000.0
const WIDE_SCAN_PRACTICAL_MAX_HZ: float = 999_000_000_000.0
const WIDE_SCAN_SECTION_EDGE_CROP_RATIO: float = 0.10

const _DRAG_NONE: int = 0
const _DRAG_CENTER: int = 1
const _DRAG_LEFT: int = 2
const _DRAG_RIGHT: int = 3
const _DRAG_PAN: int = 4
const _DRAG_SCAN_MIN: int = 5
const _DRAG_SCAN_MAX: int = 6

var _source_center_hz: float = 0.0
var _source_sample_rate_hz: float = 2400000.0
var _pan_hz: float = 0.0
var _span_scale: float = 0.8
var _normal_pan_hz: float = 0.0
var _normal_span_scale: float = 0.8
var _wide_pan_hz: float = 0.0
var _wide_span_scale: float = 1.0
var _fft_size: int = DEFAULT_FFT_SIZE
var _max_update_fps: float = 60.0
var _show_waterfall: bool = true
var _render_enabled: bool = true
var _full_waterfall_update: bool = true
var _bilinear_filter_enabled: bool = false
var _replay_playback_mode: bool = false
var _frequency_labels_visible: bool = true
var _fft_window_name: String = "Nuttall"
var _auto_range_enabled: bool = false
var _last_update_us: int = 0

var _demodulators: Array[Dictionary] = []
var _latest_frame: PackedVector2Array = PackedVector2Array()

var _waterfall_image: Image = null
var _waterfall_texture: ImageTexture = null
var _fft_real: PackedFloat32Array = PackedFloat32Array()
var _fft_imag: PackedFloat32Array = PackedFloat32Array()
var _fft_mag: PackedFloat32Array = PackedFloat32Array()
var _fft_smooth_db: PackedFloat32Array = PackedFloat32Array()
var _palette_colors: PackedColorArray = PackedColorArray()

var _drag_mode: int = _DRAG_NONE
var _drag_demod: StringName = StringName()
var _drag_retune_source: bool = false
var _hover_mode: int = _DRAG_NONE
var _hover_demod: StringName = StringName()
var _hover_freq_active: bool = false
var _hover_freq_hz: float = 0.0
var _hover_freq_x: float = 0.0

var _display_min_db: float = -70.0
var _display_max_db: float = 10.0
var _source_min_frequency_hz: float = 0.0
var _source_max_frequency_hz: float = 999000000000.0
var _source_min_sample_rate_hz: float = 1000.0
var _source_max_sample_rate_hz: float = 2400000.0

var _wide_scan_mode: bool = false
var _wide_scan_running: bool = false
var _wide_scan_dwell_ms: int = 50
var _wide_scan_start_hz: float = WIDE_SCAN_DEFAULT_MIN_HZ
var _wide_scan_end_hz: float = WIDE_SCAN_DEFAULT_MAX_HZ
var _wide_scan_loop_enabled: bool = false
var _wide_scan_bins: PackedFloat32Array = PackedFloat32Array()
var _wide_scan_sections: Array[Dictionary] = []
var _wide_scan_section_centers_hz: PackedInt64Array = PackedInt64Array()
var _wide_scan_section_index: int = -1
var _wide_scan_section_start_us: int = 0
var _wide_scan_wait_for_tune: bool = false
var _wide_scan_target_center_hz: float = 0.0
var _wide_scan_full_span_hz: float = WIDE_SCAN_DEFAULT_MAX_HZ - WIDE_SCAN_DEFAULT_MIN_HZ
var _wide_scan_center_hz: float = (WIDE_SCAN_DEFAULT_MIN_HZ + WIDE_SCAN_DEFAULT_MAX_HZ) * 0.5
var _wide_scan_min_hz: float = WIDE_SCAN_DEFAULT_MIN_HZ
var _wide_scan_max_hz: float = WIDE_SCAN_DEFAULT_MAX_HZ
var _wide_view_full_span_hz: float = WIDE_SCAN_DEFAULT_MAX_HZ - WIDE_SCAN_DEFAULT_MIN_HZ
var _wide_view_center_hz: float = (WIDE_SCAN_DEFAULT_MIN_HZ + WIDE_SCAN_DEFAULT_MAX_HZ) * 0.5
var _wide_scan_section_sample_rate_hz: float = 2400000.0
var _wide_scan_accumulator: PackedFloat32Array = PackedFloat32Array()
var _wide_scan_accumulator_count: int = 0
var _wide_scan_texture: ImageTexture = null
var _wide_scan_image: Image = null
var _wide_scan_dirty: bool = false
var _wide_scan_bin_written: PackedByteArray = PackedByteArray()
var _wide_scan_bin_floor_db: float = -140.0
var _wide_scan_bin_peak_db: float = -30.0
var _follow_demod_name: StringName = StringName()
var _normal_view_slice_mode: bool = false
var _wide_view_initialized_from_caps: bool = false

var _slider_panel: VBoxContainer = null
var _min_label: Label = null
var _max_label: Label = null
var _min_slider: VSlider = null
var _max_slider: VSlider = null
var _range_title: Label = null
var _min_value_label: Label = null
var _max_value_label: Label = null
var _settings_button: Button = null
var _settings_popup: PopupPanel = null
var _wide_scan_settings_button: Button = null
var _wide_scan_settings_popup: PopupPanel = null
var _fft_option: OptionButton = null
var _window_option: OptionButton = null
var _speed_slider: HSlider = null
var _speed_value_label: Label = null
var _render_toggle_button: Button = null
var _view_mode_option: OptionButton = null
var _auto_range_check: CheckBox = null
var _show_waterfall_check: CheckBox = null
var _full_update_check: CheckBox = null
var _bilinear_filter_check: CheckBox = null
var _wide_scan_dwell_slider: HSlider = null
var _wide_scan_dwell_label: Label = null
var _wide_scan_start_spin: SpinBox = null
var _wide_scan_end_spin: SpinBox = null
var _wide_scan_start_selector: Object = null
var _wide_scan_end_selector: Object = null
var _wide_scan_loop_check: CheckBox = null
var _follow_demod_option: OptionButton = null
var _scan_button: Button = null
var _controls_vbox: VBoxContainer = null
var _top_buttons_row: HBoxContainer = null
var _wide_buttons_row: HBoxContainer = null


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	focus_mode = Control.FOCUS_NONE
	_apply_texture_filter()
	_initialize_buffers()
	_build_slider_ui()
	_build_settings_ui()
	set_process(true)


func _process(_delta: float) -> void:
	if not is_visible_in_tree():
		return
	if not _render_enabled:
		return
	if not _full_waterfall_update:
		var now_us: int = Time.get_ticks_usec()
		if _last_update_us != 0:
			var min_interval_us: int = int(1_000_000.0 / max(_max_update_fps, 1.0))
			if now_us - _last_update_us < min_interval_us:
				return
		_last_update_us = now_us

	var process_live_waterfall: bool = not _wide_scan_mode or _wide_scan_running
	if not _latest_frame.is_empty() and process_live_waterfall:
		var frame_copy: PackedVector2Array = _latest_frame
		_consume_latest_frame(frame_copy)
		if _wide_scan_running:
			_accumulate_wide_scan_frame(frame_copy)
		_latest_frame = PackedVector2Array()
		queue_redraw()
	elif not _latest_frame.is_empty() and not _wide_scan_running:
		# In wide-scan view (not scanning), skip live FFT work to keep UI responsive.
		_latest_frame = PackedVector2Array()

	if _wide_scan_running:
		_tick_wide_scan()
	if _wide_scan_dirty:
		_apply_wide_scan_bins_to_image()
		_wide_scan_dirty = false
		queue_redraw()


func _gui_input(event: InputEvent) -> void:
	var mouse_button: InputEventMouseButton = event as InputEventMouseButton
	if mouse_button != null:
		_handle_mouse_button(mouse_button)
		accept_event()
		return

	var mouse_motion: InputEventMouseMotion = event as InputEventMouseMotion
	if mouse_motion != null:
		_handle_mouse_motion(mouse_motion)
		accept_event()


func _draw() -> void:
	var bounds: Rect2 = Rect2(Vector2.ZERO, size)
	draw_rect(bounds, Color(0.02, 0.02, 0.02, 1.0), true)

	if _show_waterfall:
		var wf_rect: Rect2 = _get_waterfall_rect()
		_draw_header_bands(wf_rect)
		if _wide_scan_mode:
			_draw_wide_scan_texture(wf_rect)
		else:
			var src_rect: Rect2 = _get_waterfall_src_rect()
			if _waterfall_texture != null:
				draw_texture_rect_region(_waterfall_texture, wf_rect, src_rect)
		if _frequency_labels_visible:
			_draw_frequency_grid(wf_rect)
		_draw_hover_marker(wf_rect)
		_draw_drag_readout(wf_rect)
		if _wide_scan_mode:
			_draw_wide_scan_overlays(wf_rect)

	_draw_demodulators(_get_waterfall_rect())
	if _wide_scan_running:
		var status_text: String = "Wide Scan %d/%d" % [max(0, _wide_scan_section_index + 1), max(1, _wide_scan_section_centers_hz.size())]
		_draw_bold_string(Vector2(8.0, size.y - 8.0), status_text, HORIZONTAL_ALIGNMENT_LEFT, -1.0, 14, Color(0.95, 0.95, 0.95, 0.95))


func _draw_wide_scan_texture(wf_rect: Rect2) -> void:
	draw_rect(wf_rect, Color(0.0, 0.0, 0.0, 1.0), true)
	if not _wide_scan_bins.is_empty():
		_draw_wide_scan_hotspot_projection(wf_rect)
		return
	if _wide_scan_texture == null or _wide_scan_image == null:
		return
	if _wide_scan_full_span_hz <= 1.0:
		return

	var view_left_hz: float = _left_edge_hz()
	var view_right_hz: float = view_left_hz + _get_visible_span_hz()
	var scan_left_hz: float = _wide_scan_min_hz
	var scan_right_hz: float = _wide_scan_max_hz
	var overlap_left_hz: float = max(view_left_hz, scan_left_hz)
	var overlap_right_hz: float = min(view_right_hz, scan_right_hz)
	if overlap_right_hz <= overlap_left_hz:
		return

	var scan_span_hz: float = max(_wide_scan_full_span_hz, 1.0)
	var view_span_hz: float = max(_get_visible_span_hz(), 1.0)
	var texture_width: float = float(max(1, _wide_scan_image.get_width()))
	var texture_height: float = float(max(1, _wide_scan_image.get_height()))

	var src_x: float = ((overlap_left_hz - scan_left_hz) / scan_span_hz) * texture_width
	var src_w: float = ((overlap_right_hz - overlap_left_hz) / scan_span_hz) * texture_width
	var dst_x: float = wf_rect.position.x + (((overlap_left_hz - view_left_hz) / view_span_hz) * wf_rect.size.x)
	var dst_w: float = ((overlap_right_hz - overlap_left_hz) / view_span_hz) * wf_rect.size.x
	if src_w <= 0.0 or dst_w <= 0.0:
		return

	var src_rect: Rect2 = Rect2(src_x, 0.0, src_w, texture_height)
	var dst_rect: Rect2 = Rect2(dst_x, wf_rect.position.y, dst_w, wf_rect.size.y)
	draw_texture_rect_region(_wide_scan_texture, dst_rect, src_rect)


func _draw_wide_scan_hotspot_projection(wf_rect: Rect2) -> void:
	if _wide_scan_bins.is_empty():
		return
	if _wide_scan_full_span_hz <= 1.0:
		return

	var bin_count: int = _wide_scan_bins.size()
	if bin_count <= 0:
		return
	var bins_last: int = bin_count - 1

	var min_db: float = _display_min_db
	var max_db: float = _display_max_db
	if _auto_range_enabled:
		min_db = _wide_scan_bin_floor_db
		max_db = _wide_scan_bin_peak_db
		if max_db <= min_db + 4.0:
			max_db = min_db + 4.0
	var range_db: float = max(1.0, max_db - min_db)

	var view_left_hz: float = _left_edge_hz()
	var view_span_hz: float = max(_get_visible_span_hz(), 0.0001)
	var scan_left_hz: float = _wide_scan_min_hz
	var scan_span_hz: float = max(_wide_scan_full_span_hz, 1.0)
	var scan_right_hz: float = scan_left_hz + scan_span_hz

	var pixel_count: int = int(max(1.0, floor(wf_rect.size.x)))
	var hz_per_px: float = view_span_hz / float(pixel_count)
	for px in range(pixel_count):
		var col_left_hz: float = view_left_hz + (float(px) * hz_per_px)
		var col_right_hz: float = col_left_hz + hz_per_px
		var overlap_left_hz: float = max(col_left_hz, scan_left_hz)
		var overlap_right_hz: float = min(col_right_hz, scan_right_hz)
		if overlap_right_hz <= overlap_left_hz:
			continue

		var bin_f0: float = ((overlap_left_hz - scan_left_hz) / scan_span_hz) * float(bins_last)
		var bin_f1: float = ((overlap_right_hz - scan_left_hz) / scan_span_hz) * float(bins_last)
		var bin_i0: int = clamp(int(floor(bin_f0)), 0, bins_last)
		var bin_i1: int = clamp(int(ceil(bin_f1)), bin_i0, bins_last)

		var peak_db: float = -INF
		for bi in range(bin_i0, bin_i1 + 1):
			peak_db = max(peak_db, float(_wide_scan_bins[bi]))
		if not is_finite(peak_db):
			continue

		var norm_value: float = clamp((peak_db - min_db) / range_db, 0.0, 1.0)
		var color: Color = _sdrpp_palette(norm_value)
		var col_x: float = wf_rect.position.x + float(px)
		draw_rect(Rect2(col_x, wf_rect.position.y, 1.0, wf_rect.size.y), color, true)


func set_source_state(center_hz: int, sample_rate_hz: float) -> void:
	var previous_center_hz: float = _source_center_hz
	_source_center_hz = float(center_hz)
	var applied_delta_hz: float = _source_center_hz - previous_center_hz
	_apply_locked_demod_offsets_for_source_delta(applied_delta_hz)
	_source_sample_rate_hz = max(sample_rate_hz, 1000.0)
	_clamp_view()
	queue_redraw()


func set_source_capabilities(min_frequency_hz: float, max_frequency_hz: float, min_sample_rate_hz: float, max_sample_rate_hz: float) -> void:
	var old_min_hz: float = _source_min_frequency_hz
	var old_max_hz: float = _source_max_frequency_hz
	_source_min_frequency_hz = max(0.0, min_frequency_hz)
	_source_max_frequency_hz = max(_source_min_frequency_hz + 1.0, max_frequency_hz)
	_source_min_sample_rate_hz = max(1000.0, min_sample_rate_hz)
	_source_max_sample_rate_hz = max(_source_min_sample_rate_hz, max_sample_rate_hz)
	if _wide_scan_start_hz < _source_min_frequency_hz or _wide_scan_start_hz > _source_max_frequency_hz:
		_wide_scan_start_hz = _source_min_frequency_hz
	if _wide_scan_end_hz <= _wide_scan_start_hz or _wide_scan_end_hz > _source_max_frequency_hz:
		_wide_scan_end_hz = _source_max_frequency_hz
	var caps_changed: bool = abs(old_min_hz - _source_min_frequency_hz) > 0.5 or abs(old_max_hz - _source_max_frequency_hz) > 0.5
	if not _wide_view_initialized_from_caps or caps_changed:
		_wide_view_full_span_hz = max(1.0, _source_max_frequency_hz - _source_min_frequency_hz)
		_wide_view_center_hz = (_source_min_frequency_hz + _source_max_frequency_hz) * 0.5
		_wide_pan_hz = 0.0
		_wide_span_scale = 1.0
		_wide_view_initialized_from_caps = true
		if _wide_scan_mode:
			_pan_hz = 0.0
			_span_scale = 1.0
	_sanitize_scan_range()
	_update_scan_range_controls()
	_clamp_view()


func is_wide_scan_mode_enabled() -> bool:
	return _wide_scan_mode


func is_wide_scan_running() -> bool:
	return _wide_scan_running


func begin_wide_scan(min_hz: int, max_hz: int, section_sample_rate_hz: float, dwell_ms: int) -> void:
	var previous_bins: PackedFloat32Array = _wide_scan_bins
	var previous_min_hz: float = _wide_scan_min_hz
	var previous_max_hz: float = _wide_scan_max_hz

	_wide_scan_min_hz = min(float(min_hz), float(max_hz))
	_wide_scan_max_hz = max(float(min_hz), float(max_hz))
	_wide_scan_full_span_hz = max(1.0, _wide_scan_max_hz - _wide_scan_min_hz)
	_wide_scan_center_hz = (_wide_scan_min_hz + _wide_scan_max_hz) * 0.5
	_wide_scan_section_sample_rate_hz = clamp(section_sample_rate_hz, _source_min_sample_rate_hz, _source_max_sample_rate_hz)
	_wide_scan_dwell_ms = clamp(dwell_ms, 10, 60000)

	var new_bins: PackedFloat32Array = PackedFloat32Array()
	var view_span_for_bins_hz: float = max(_wide_view_full_span_hz, _wide_scan_full_span_hz)
	var recommended_bins: int = int(round((view_span_for_bins_hz / max(_wide_scan_section_sample_rate_hz, 1.0)) * float(_fft_size)))
	recommended_bins = clamp(recommended_bins, MIN_WIDE_SCAN_BINS, MAX_WIDE_SCAN_BINS)
	new_bins.resize(recommended_bins)
	for i in range(recommended_bins):
		new_bins[i] = -200.0
	var can_reuse_previous_bins: bool = not previous_bins.is_empty() \
		and previous_bins.size() == recommended_bins \
		and abs(previous_min_hz - _wide_scan_min_hz) <= 0.5 \
		and abs(previous_max_hz - _wide_scan_max_hz) <= 0.5
	if can_reuse_previous_bins:
		new_bins = previous_bins
	_wide_scan_bins = new_bins
	_reset_wide_scan_bin_written_mask()
	_wide_scan_bin_floor_db = -200.0
	_wide_scan_bin_peak_db = -30.0
	_wide_scan_dirty = true

	_wide_scan_section_centers_hz = PackedInt64Array()
	var section_span_hz: float = max(_wide_scan_section_sample_rate_hz, 1.0)
	var section_half_hz: float = section_span_hz * 0.5
	var step_hz: float = section_span_hz * (1.0 - (WIDE_SCAN_SECTION_EDGE_CROP_RATIO * 2.0))
	step_hz = clamp(step_hz, section_span_hz * 0.2, section_span_hz)
	var center_hz: float = _wide_scan_min_hz + section_half_hz
	while center_hz - section_half_hz < _wide_scan_max_hz:
		_wide_scan_section_centers_hz.push_back(int(round(center_hz)))
		center_hz += step_hz
	if _wide_scan_section_centers_hz.is_empty():
		_wide_scan_section_centers_hz.push_back(int(round((_wide_scan_min_hz + _wide_scan_max_hz) * 0.5)))
	if _wide_scan_sections.size() != _wide_scan_section_centers_hz.size():
		_wide_scan_sections.clear()

	_wide_scan_running = true
	_wide_scan_section_index = -1
	_wide_scan_wait_for_tune = false
	_wide_scan_section_start_us = 0
	_wide_scan_target_center_hz = 0.0
	_prepare_next_wide_scan_section()
	_refresh_scan_button_style()
	queue_redraw()


func cancel_wide_scan() -> void:
	if not _wide_scan_running:
		return
	_wide_scan_running = false
	_wide_scan_wait_for_tune = false
	_wide_scan_section_index = -1
	_wide_scan_section_start_us = 0
	emit_signal("wide_scan_cancelled")
	_refresh_scan_button_style()
	queue_redraw()


func is_source_retune_drag_active() -> bool:
	if _drag_mode == _DRAG_PAN and _drag_retune_source:
		return true
	# Follow-demod center drag also retunes the source and should keep visual center decoupled.
	if _drag_mode == _DRAG_CENTER and not _wide_scan_running and _drag_demod != StringName() and _drag_demod == _follow_demod_name:
		return true
	return false


func set_demodulators(demodulators: Array[Dictionary]) -> void:
	_demodulators = demodulators.duplicate(true)
	_refresh_follow_demod_options()
	queue_redraw()


func set_replay_playback_mode(enabled: bool) -> void:
	if _replay_playback_mode == enabled:
		return
	_replay_playback_mode = enabled
	queue_redraw()


func set_frequency_labels_visible(enabled: bool) -> void:
	if _frequency_labels_visible == enabled:
		return
	_frequency_labels_visible = enabled
	queue_redraw()


func push_baseband_frame(frame: PackedVector2Array) -> void:
	if frame.is_empty():
		return
	var frame_count: int = frame.size()
	var sample_count: int = min(_fft_size, frame_count)
	if sample_count <= 0:
		return

	var reduced: PackedVector2Array = PackedVector2Array()
	reduced.resize(sample_count)
	if sample_count == 1:
		reduced[0] = frame[frame_count - 1]
		_latest_frame = reduced
		return

	var start_idx: int = max(0, frame_count - _fft_size)
	var span: int = max(1, frame_count - start_idx - 1)
	for i in range(sample_count):
		var t: float = float(i) / float(sample_count - 1)
		var idx: int = start_idx + int(round(t * float(span)))
		idx = clamp(idx, 0, frame_count - 1)
		reduced[i] = frame[idx]
	_latest_frame = reduced


func _initialize_buffers() -> void:
	_waterfall_image = Image.create(_fft_size, HISTORY_ROWS, false, Image.FORMAT_RGBA8)
	_waterfall_image.fill(Color(0.0, 0.0, 0.0, 1.0))
	_waterfall_texture = ImageTexture.create_from_image(_waterfall_image)

	_fft_real.resize(_fft_size)
	_fft_imag.resize(_fft_size)
	_fft_mag.resize(_fft_size)
	_fft_smooth_db.resize(_fft_size)
	for i in range(_fft_size):
		_fft_real[i] = 0.0
		_fft_imag[i] = 0.0
		_fft_mag[i] = 0.0
		_fft_smooth_db[i] = -120.0
	_initialize_palette()


func _initialize_palette() -> void:
	# SDR++ "Classic" colormap (author: Youssef Touil)
	# Source: https://raw.githubusercontent.com/AlexandreRouma/SDRPlusPlus/master/root/res/colormaps/classic.json
	var stops: PackedStringArray = PackedStringArray([
		"#000020", "#000030", "#000050", "#000091", "#1E90FF",
		"#FFFFFF", "#FFFF00", "#FE6D16", "#FE6D16", "#FF0000",
		"#FF0000", "#C60000", "#9F0000", "#750000", "#4A0000",
	])
	_palette_colors.resize(stops.size())
	for i in range(stops.size()):
		_palette_colors[i] = Color.html(stops[i])


func _build_settings_ui() -> void:
	if _controls_vbox != null:
		return

	var controls: VBoxContainer = VBoxContainer.new()
	controls.anchor_left = 0.0
	controls.anchor_top = 0.0
	controls.anchor_right = 0.0
	controls.anchor_bottom = 0.0
	controls.offset_left = 4.0
	controls.offset_top = _get_top_band_height() + 4.0
	controls.offset_right = 250.0
	controls.offset_bottom = _get_top_band_height() + 88.0
	controls.add_theme_constant_override("separation", 4)
	add_child(controls)
	_controls_vbox = controls

	var mode_row: HBoxContainer = HBoxContainer.new()
	mode_row.add_theme_constant_override("separation", 4)
	controls.add_child(mode_row)
	var mode_label: Label = Label.new()
	mode_label.text = "View"
	mode_row.add_child(mode_label)
	var mode_option: OptionButton = OptionButton.new()
	mode_option.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	mode_option.add_item("Waterfall", 0)
	mode_option.add_item("Wide Scan", 1)
	mode_option.item_selected.connect(_on_view_mode_selected)
	mode_row.add_child(mode_option)
	_view_mode_option = mode_option

	var top_row: HBoxContainer = HBoxContainer.new()
	top_row.add_theme_constant_override("separation", 4)
	controls.add_child(top_row)
	_top_buttons_row = top_row

	var waterfall_menu_button: Button = Button.new()
	waterfall_menu_button.text = "Waterfall Menu"
	waterfall_menu_button.custom_minimum_size = Vector2(120.0, 22.0)
	waterfall_menu_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	waterfall_menu_button.tooltip_text = "Waterfall display settings"
	waterfall_menu_button.pressed.connect(_on_settings_button_pressed)
	top_row.add_child(waterfall_menu_button)
	_settings_button = waterfall_menu_button

	var render_toggle: Button = Button.new()
	render_toggle.text = "Waterfall On"
	render_toggle.custom_minimum_size = Vector2(120.0, 22.0)
	render_toggle.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	render_toggle.tooltip_text = "Toggle Waterfall Rendering"
	render_toggle.pressed.connect(_on_render_toggle_pressed)
	top_row.add_child(render_toggle)
	_render_toggle_button = render_toggle
	_refresh_render_toggle_style()

	var wide_row: HBoxContainer = HBoxContainer.new()
	wide_row.add_theme_constant_override("separation", 4)
	controls.add_child(wide_row)
	_wide_buttons_row = wide_row

	var wide_menu_button: Button = Button.new()
	wide_menu_button.text = "Wide Scan Menu"
	wide_menu_button.custom_minimum_size = Vector2(120.0, 22.0)
	wide_menu_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	wide_menu_button.tooltip_text = "Wide scan settings"
	wide_menu_button.pressed.connect(_on_wide_scan_settings_button_pressed)
	wide_row.add_child(wide_menu_button)
	_wide_scan_settings_button = wide_menu_button

	var scan_button: Button = Button.new()
	scan_button.text = "Start Scan"
	scan_button.custom_minimum_size = Vector2(120.0, 22.0)
	scan_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scan_button.tooltip_text = "Start/Stop Wide Field Scan"
	scan_button.pressed.connect(_on_scan_button_pressed)
	wide_row.add_child(scan_button)
	_scan_button = scan_button
	_refresh_scan_button_style()

	var popup: PopupPanel = PopupPanel.new()
	popup.name = "WaterfallSettingsPopup"
	add_child(popup)
	_settings_popup = popup

	var root: VBoxContainer = VBoxContainer.new()
	root.custom_minimum_size = Vector2(340.0, 280.0)
	popup.add_child(root)

	var display_header: Label = Label.new()
	display_header.text = "Display"
	display_header.add_theme_font_size_override("font_size", 14)
	root.add_child(display_header)

	var show_row: HBoxContainer = HBoxContainer.new()
	root.add_child(show_row)
	var show_check: CheckBox = CheckBox.new()
	show_check.text = "Show Waterfall"
	show_check.button_pressed = _show_waterfall
	show_check.toggled.connect(_on_show_waterfall_toggled)
	show_row.add_child(show_check)
	_show_waterfall_check = show_check

	var full_row: HBoxContainer = HBoxContainer.new()
	root.add_child(full_row)
	var full_check: CheckBox = CheckBox.new()
	full_check.text = "Full Waterfall Update"
	full_check.button_pressed = _full_waterfall_update
	full_check.toggled.connect(_on_full_update_toggled)
	full_row.add_child(full_check)
	_full_update_check = full_check

	var bilinear_row: HBoxContainer = HBoxContainer.new()
	root.add_child(bilinear_row)
	var bilinear_check: CheckBox = CheckBox.new()
	bilinear_check.text = "Bilinear Filtering"
	bilinear_check.button_pressed = _bilinear_filter_enabled
	bilinear_check.toggled.connect(_on_bilinear_filter_toggled)
	bilinear_row.add_child(bilinear_check)
	_bilinear_filter_check = bilinear_check

	var fps_row: HBoxContainer = HBoxContainer.new()
	root.add_child(fps_row)
	var fps_label: Label = Label.new()
	fps_label.text = "FFT Framerate"
	fps_row.add_child(fps_label)
	var speed_slider: HSlider = HSlider.new()
	speed_slider.min_value = 0.0
	speed_slider.max_value = 1.0
	speed_slider.step = 0.001
	speed_slider.value = _log_position_from_fps(_max_update_fps)
	speed_slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	speed_slider.value_changed.connect(_on_speed_changed)
	fps_row.add_child(speed_slider)
	_speed_slider = speed_slider

	var fps_value: Label = Label.new()
	fps_value.custom_minimum_size = Vector2(42.0, 0.0)
	fps_value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	fps_row.add_child(fps_value)
	_speed_value_label = fps_value

	var fft_size_row: HBoxContainer = HBoxContainer.new()
	root.add_child(fft_size_row)
	var fft_size_label: Label = Label.new()
	fft_size_label.text = "FFT Size"
	fft_size_row.add_child(fft_size_label)
	var fft_option: OptionButton = OptionButton.new()
	fft_option.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	fft_option.add_item("1024", 1024)
	fft_option.add_item("2048", 2048)
	fft_option.add_item("4096", 4096)
	fft_option.add_item("8192", 8192)
	fft_option.add_item("16384", 16384)
	fft_option.select(1)
	fft_option.item_selected.connect(_on_fft_option_selected)
	fft_size_row.add_child(fft_option)
	_fft_option = fft_option

	var window_row: HBoxContainer = HBoxContainer.new()
	root.add_child(window_row)
	var window_label: Label = Label.new()
	window_label.text = "FFT Window"
	window_row.add_child(window_label)
	var window_option: OptionButton = OptionButton.new()
	window_option.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	window_option.add_item("Nuttall")
	window_option.add_item("Hann")
	window_option.add_item("Blackman-Harris")
	window_option.add_item("Rectangular")
	window_option.select(0)
	window_option.item_selected.connect(_on_window_selected)
	window_row.add_child(window_option)
	_window_option = window_option

	var cmap_row: HBoxContainer = HBoxContainer.new()
	root.add_child(cmap_row)
	var cmap_label: Label = Label.new()
	cmap_label.text = "Color Map"
	cmap_row.add_child(cmap_label)
	var cmap_option: OptionButton = OptionButton.new()
	cmap_option.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	cmap_option.add_item("Classic")
	cmap_option.disabled = true
	cmap_row.add_child(cmap_option)

	var auto_check: CheckBox = CheckBox.new()
	auto_check.text = "Auto Range"
	auto_check.button_pressed = _auto_range_enabled
	auto_check.toggled.connect(_on_auto_range_toggled)
	root.add_child(auto_check)
	_auto_range_check = auto_check

	var wide_popup: PopupPanel = PopupPanel.new()
	wide_popup.name = "WideScanSettingsPopup"
	add_child(wide_popup)
	_wide_scan_settings_popup = wide_popup

	var wide_root: VBoxContainer = VBoxContainer.new()
	wide_root.custom_minimum_size = Vector2(360.0, 240.0)
	wide_popup.add_child(wide_root)

	var scan_header: Label = Label.new()
	scan_header.text = "Wide Scan"
	scan_header.add_theme_font_size_override("font_size", 14)
	wide_root.add_child(scan_header)

	var dwell_row: HBoxContainer = HBoxContainer.new()
	wide_root.add_child(dwell_row)
	var dwell_label: Label = Label.new()
	dwell_label.text = "Dwell / section"
	dwell_row.add_child(dwell_label)
	var dwell_slider: HSlider = HSlider.new()
	dwell_slider.min_value = 10.0
	dwell_slider.max_value = 3000.0
	dwell_slider.step = 10.0
	dwell_slider.value = float(_wide_scan_dwell_ms)
	dwell_slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	dwell_slider.value_changed.connect(_on_wide_scan_dwell_changed)
	dwell_row.add_child(dwell_slider)
	_wide_scan_dwell_slider = dwell_slider

	var dwell_value: Label = Label.new()
	dwell_value.custom_minimum_size = Vector2(52.0, 0.0)
	dwell_value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	dwell_row.add_child(dwell_value)
	_wide_scan_dwell_label = dwell_value
	_update_wide_scan_dwell_label()

	var start_row: HBoxContainer = HBoxContainer.new()
	wide_root.add_child(start_row)
	var start_label: Label = Label.new()
	start_label.text = "Start Frequency"
	start_row.add_child(start_label)
	var start_panel: PanelContainer = PanelContainer.new()
	start_panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	start_row.add_child(start_panel)
	if ClassDB.class_exists("DigitNumberSelector") and ClassDB.can_instantiate("DigitNumberSelector"):
		var start_selector_obj: Object = ClassDB.instantiate("DigitNumberSelector")
		var start_selector_ctrl: Control = start_selector_obj as Control
		if start_selector_ctrl != null:
			start_selector_obj.call("set_limits", int(WIDE_SCAN_DEFAULT_MIN_HZ), int(WIDE_SCAN_PRACTICAL_MAX_HZ))
			start_selector_obj.call("set_digit_count", 12)
			start_selector_obj.call("set_group_size", 3)
			start_selector_obj.call("set_show_group_labels", true)
			start_selector_obj.call("set_show_separators", true)
			start_selector_obj.call("set_suffix_text", "")
			var start_labels: PackedStringArray = PackedStringArray()
			start_labels.push_back("GHz")
			start_labels.push_back("MHz")
			start_labels.push_back("kHz")
			start_labels.push_back("Hz")
			start_selector_obj.call("set_group_labels", start_labels)
			start_selector_obj.call("set_value_no_signal", int(round(_wide_scan_start_hz)))
			start_selector_obj.connect("value_changed", Callable(self, "_on_wide_scan_start_selector_changed"))
			start_panel.add_child(start_selector_ctrl)
			_wide_scan_start_selector = start_selector_obj
		else:
			var start_spin_fallback: SpinBox = SpinBox.new()
			start_spin_fallback.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			start_spin_fallback.min_value = WIDE_SCAN_DEFAULT_MIN_HZ
			start_spin_fallback.max_value = WIDE_SCAN_PRACTICAL_MAX_HZ
			start_spin_fallback.step = 1000.0
			start_spin_fallback.value = _wide_scan_start_hz
			start_spin_fallback.value_changed.connect(_on_wide_scan_start_changed)
			start_panel.add_child(start_spin_fallback)
			_wide_scan_start_spin = start_spin_fallback
	else:
		var start_spin: SpinBox = SpinBox.new()
		start_spin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		start_spin.min_value = WIDE_SCAN_DEFAULT_MIN_HZ
		start_spin.max_value = WIDE_SCAN_PRACTICAL_MAX_HZ
		start_spin.step = 1000.0
		start_spin.value = _wide_scan_start_hz
		start_spin.value_changed.connect(_on_wide_scan_start_changed)
		start_panel.add_child(start_spin)
		_wide_scan_start_spin = start_spin

	var end_row: HBoxContainer = HBoxContainer.new()
	wide_root.add_child(end_row)
	var end_label: Label = Label.new()
	end_label.text = "End Frequency"
	end_row.add_child(end_label)
	var end_panel: PanelContainer = PanelContainer.new()
	end_panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	end_row.add_child(end_panel)
	if ClassDB.class_exists("DigitNumberSelector") and ClassDB.can_instantiate("DigitNumberSelector"):
		var end_selector_obj: Object = ClassDB.instantiate("DigitNumberSelector")
		var end_selector_ctrl: Control = end_selector_obj as Control
		if end_selector_ctrl != null:
			end_selector_obj.call("set_limits", int(WIDE_SCAN_DEFAULT_MIN_HZ), int(WIDE_SCAN_PRACTICAL_MAX_HZ))
			end_selector_obj.call("set_digit_count", 12)
			end_selector_obj.call("set_group_size", 3)
			end_selector_obj.call("set_show_group_labels", true)
			end_selector_obj.call("set_show_separators", true)
			end_selector_obj.call("set_suffix_text", "")
			var end_labels: PackedStringArray = PackedStringArray()
			end_labels.push_back("GHz")
			end_labels.push_back("MHz")
			end_labels.push_back("kHz")
			end_labels.push_back("Hz")
			end_selector_obj.call("set_group_labels", end_labels)
			end_selector_obj.call("set_value_no_signal", int(round(_wide_scan_end_hz)))
			end_selector_obj.connect("value_changed", Callable(self, "_on_wide_scan_end_selector_changed"))
			end_panel.add_child(end_selector_ctrl)
			_wide_scan_end_selector = end_selector_obj
		else:
			var end_spin_fallback: SpinBox = SpinBox.new()
			end_spin_fallback.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			end_spin_fallback.min_value = WIDE_SCAN_DEFAULT_MIN_HZ
			end_spin_fallback.max_value = WIDE_SCAN_PRACTICAL_MAX_HZ
			end_spin_fallback.step = 1000.0
			end_spin_fallback.value = _wide_scan_end_hz
			end_spin_fallback.value_changed.connect(_on_wide_scan_end_changed)
			end_panel.add_child(end_spin_fallback)
			_wide_scan_end_spin = end_spin_fallback
	else:
		var end_spin: SpinBox = SpinBox.new()
		end_spin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		end_spin.min_value = WIDE_SCAN_DEFAULT_MIN_HZ
		end_spin.max_value = WIDE_SCAN_PRACTICAL_MAX_HZ
		end_spin.step = 1000.0
		end_spin.value = _wide_scan_end_hz
		end_spin.value_changed.connect(_on_wide_scan_end_changed)
		end_panel.add_child(end_spin)
		_wide_scan_end_spin = end_spin

	var loop_check: CheckBox = CheckBox.new()
	loop_check.text = "Loop Wide Scan"
	loop_check.button_pressed = _wide_scan_loop_enabled
	loop_check.toggled.connect(_on_wide_scan_loop_toggled)
	wide_root.add_child(loop_check)
	_wide_scan_loop_check = loop_check

	var follow_row: HBoxContainer = HBoxContainer.new()
	wide_root.add_child(follow_row)
	var follow_label: Label = Label.new()
	follow_label.text = "Follow Demod"
	follow_row.add_child(follow_label)
	var follow_option: OptionButton = OptionButton.new()
	follow_option.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	follow_option.item_selected.connect(_on_follow_demod_selected)
	follow_row.add_child(follow_option)
	_follow_demod_option = follow_option
	_refresh_follow_demod_options()

	_sanitize_scan_range()
	_update_scan_range_controls()
	_update_speed_label()
	_refresh_view_mode_controls()


func _build_slider_ui() -> void:
	if _slider_panel != null:
		return

	var panel: VBoxContainer = VBoxContainer.new()
	panel.name = "SliderPanel"
	panel.anchor_left = 1.0
	panel.anchor_top = 0.0
	panel.anchor_right = 1.0
	panel.anchor_bottom = 1.0
	panel.offset_left = -SLIDER_PANEL_WIDTH
	panel.offset_top = 4.0
	panel.offset_right = -4.0
	panel.offset_bottom = -4.0
	panel.alignment = BoxContainer.ALIGNMENT_BEGIN
	panel.add_theme_constant_override("separation", 3)
	panel.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(panel)
	_slider_panel = panel

	var range_title: Label = Label.new()
	range_title.text = "dB"
	range_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	range_title.add_theme_font_size_override("font_size", 11)
	range_title.add_theme_constant_override("outline_size", 1)
	panel.add_child(range_title)
	_range_title = range_title

	var sliders_row: HBoxContainer = HBoxContainer.new()
	sliders_row.size_flags_vertical = Control.SIZE_EXPAND_FILL
	sliders_row.add_theme_constant_override("separation", 6)
	panel.add_child(sliders_row)

	var min_col: VBoxContainer = VBoxContainer.new()
	min_col.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	min_col.size_flags_vertical = Control.SIZE_EXPAND_FILL
	min_col.alignment = BoxContainer.ALIGNMENT_CENTER
	min_col.add_theme_constant_override("separation", 2)
	sliders_row.add_child(min_col)

	var min_slider: VSlider = VSlider.new()
	min_slider.name = "MinDbSlider"
	min_slider.min_value = -140.0
	min_slider.max_value = -20.0
	min_slider.step = 1.0
	min_slider.value = _display_min_db
	min_slider.size_flags_vertical = Control.SIZE_EXPAND_FILL
	min_slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	min_slider.custom_minimum_size = Vector2(16.0, 120.0)
	min_slider.tooltip_text = "Min dB"
	min_slider.value_changed.connect(_on_min_db_changed)
	min_col.add_child(min_slider)
	_min_slider = min_slider

	var min_value_label: Label = Label.new()
	min_value_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	min_value_label.add_theme_font_size_override("font_size", 11)
	min_value_label.add_theme_constant_override("outline_size", 1)
	min_col.add_child(min_value_label)
	_min_value_label = min_value_label

	var min_label: Label = Label.new()
	min_label.text = "MIN"
	min_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	min_label.add_theme_font_size_override("font_size", 9)
	min_label.add_theme_constant_override("outline_size", 1)
	min_col.add_child(min_label)
	_min_label = min_label

	var max_col: VBoxContainer = VBoxContainer.new()
	max_col.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	max_col.size_flags_vertical = Control.SIZE_EXPAND_FILL
	max_col.alignment = BoxContainer.ALIGNMENT_CENTER
	max_col.add_theme_constant_override("separation", 2)
	sliders_row.add_child(max_col)

	var max_slider: VSlider = VSlider.new()
	max_slider.name = "MaxDbSlider"
	max_slider.min_value = -120.0
	max_slider.max_value = 10.0
	max_slider.step = 1.0
	max_slider.value = _display_max_db
	max_slider.size_flags_vertical = Control.SIZE_EXPAND_FILL
	max_slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	max_slider.custom_minimum_size = Vector2(16.0, 120.0)
	max_slider.tooltip_text = "Max dB"
	max_slider.value_changed.connect(_on_max_db_changed)
	max_col.add_child(max_slider)
	_max_slider = max_slider

	var max_value_label: Label = Label.new()
	max_value_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	max_value_label.add_theme_font_size_override("font_size", 11)
	max_value_label.add_theme_constant_override("outline_size", 1)
	max_col.add_child(max_value_label)
	_max_value_label = max_value_label

	var max_label: Label = Label.new()
	max_label.text = "MAX"
	max_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	max_label.add_theme_font_size_override("font_size", 9)
	max_label.add_theme_constant_override("outline_size", 1)
	max_col.add_child(max_label)
	_max_label = max_label

	_refresh_slider_labels()


func _consume_latest_frame(frame: PackedVector2Array) -> void:
	var frame_count: int = frame.size()
	if frame_count <= 0:
		return

	var start_index: int = max(0, frame_count - _fft_size)
	for i in range(_fft_size):
		var src_index: int = start_index + i
		if src_index >= frame_count:
			src_index = frame_count - 1
		var iq: Vector2 = frame[src_index]
		var window: float = _window_value(i, _fft_size)
		_fft_real[i] = iq.x * window
		_fft_imag[i] = iq.y * window

	_fft_in_place(_fft_real, _fft_imag)
	_compute_shifted_magnitude()
	if not _wide_scan_mode:
		_push_spectrum_row()


func _window_value(i: int, n: int) -> float:
	if n <= 1:
		return 1.0
	var t: float = float(i) / float(n - 1)
	if _fft_window_name == "Rectangular":
		return 1.0
	if _fft_window_name == "Hann":
		return 0.5 - (0.5 * cos(TAU * t))
	if _fft_window_name == "Blackman-Harris":
		var a0: float = 0.35875
		var a1: float = 0.48829
		var a2: float = 0.14128
		var a3: float = 0.01168
		return a0 - (a1 * cos(TAU * t)) + (a2 * cos(2.0 * TAU * t)) - (a3 * cos(3.0 * TAU * t))
	# Nuttall
	var b0: float = 0.355768
	var b1: float = 0.487396
	var b2: float = 0.144232
	var b3: float = 0.012604
	return b0 - (b1 * cos(TAU * t)) + (b2 * cos(2.0 * TAU * t)) - (b3 * cos(3.0 * TAU * t))


func _fft_in_place(real: PackedFloat32Array, imag: PackedFloat32Array) -> void:
	var j: int = 0
	for i in range(1, _fft_size):
		var bit: int = _fft_size >> 1
		while j >= bit and bit > 0:
			j -= bit
			bit >>= 1
		j += bit
		if i < j:
			var temp_r: float = real[i]
			var temp_i: float = imag[i]
			real[i] = real[j]
			imag[i] = imag[j]
			real[j] = temp_r
			imag[j] = temp_i

	var length: int = 2
	while length <= _fft_size:
		var angle: float = -TAU / float(length)
		var wlen_cos: float = cos(angle)
		var wlen_sin: float = sin(angle)
		var half: int = length >> 1
		var base: int = 0
		while base < _fft_size:
			var w_cos: float = 1.0
			var w_sin: float = 0.0
			for k in range(half):
				var i0: int = base + k
				var i1: int = i0 + half
				var u_r: float = real[i0]
				var u_i: float = imag[i0]
				var v_r: float = (real[i1] * w_cos) - (imag[i1] * w_sin)
				var v_i: float = (real[i1] * w_sin) + (imag[i1] * w_cos)
				real[i0] = u_r + v_r
				imag[i0] = u_i + v_i
				real[i1] = u_r - v_r
				imag[i1] = u_i - v_i
				var next_cos: float = (w_cos * wlen_cos) - (w_sin * wlen_sin)
				w_sin = (w_cos * wlen_sin) + (w_sin * wlen_cos)
				w_cos = next_cos
			base += length
		length <<= 1


func _compute_shifted_magnitude() -> void:
	var half: int = _fft_size / 2
	var min_db: float = _display_min_db
	var max_db: float = max(_display_max_db, min_db + 1.0)
	var range_db: float = max_db - min_db

	var max_db_seen: float = -200.0
	var avg_db: float = 0.0
	for x in range(_fft_size):
		var src: int = (x + half) % _fft_size
		var re: float = _fft_real[src]
		var im: float = _fft_imag[src]
		var power: float = ((re * re) + (im * im)) / float(_fft_size * _fft_size)
		var db: float = 10.0 * (log(max(power, 1e-20)) / log(10.0))

		_fft_smooth_db[x] = db
		var norm: float = clamp((db - min_db) / range_db, 0.0, 1.0)
		_fft_mag[x] = norm
		max_db_seen = max(max_db_seen, db)
		avg_db += db

	if _auto_range_enabled and _fft_size > 0:
		avg_db /= float(_fft_size)
		_display_min_db = lerpf(_display_min_db, avg_db - 35.0, 0.08)
		_display_max_db = lerpf(_display_max_db, max_db_seen - 4.0, 0.08)
		if _display_max_db <= _display_min_db + 4.0:
			_display_max_db = _display_min_db + 4.0
		if _min_slider != null:
			_min_slider.set_value_no_signal(_display_min_db)
		if _max_slider != null:
			_max_slider.set_value_no_signal(_display_max_db)
		_refresh_slider_labels()


func _push_spectrum_row() -> void:
	if _waterfall_image == null:
		return

	_waterfall_image.blit_rect(_waterfall_image, Rect2i(0, 0, _fft_size, HISTORY_ROWS - 1), Vector2i(0, 1))

	var full_span_hz: float = max(_source_sample_rate_hz, 1.0)
	var visible_span_hz: float = _get_visible_span_hz()
	_normal_view_slice_mode = visible_span_hz < (full_span_hz * 0.98)
	if _normal_view_slice_mode:
		var full_left_hz: float = _source_center_hz - (full_span_hz * 0.5)
		var left_hz: float = _left_edge_hz()
		var row_last: float = max(1.0, float(_fft_size - 1))
		for x in range(_fft_size):
			var t: float = float(x) / row_last
			var freq_hz: float = left_hz + (visible_span_hz * t)
			var src_norm: float = clamp((freq_hz - full_left_hz) / full_span_hz, 0.0, 1.0)
			var src_pos: float = src_norm * row_last
			var src_i0: int = int(floor(src_pos))
			var src_i1: int = min(_fft_size - 1, src_i0 + 1)
			var frac: float = src_pos - float(src_i0)
			var mag_value: float = lerpf(_fft_mag[src_i0], _fft_mag[src_i1], frac)
			_waterfall_image.set_pixel(x, 0, _sdrpp_palette(mag_value))
	else:
		for x in range(_fft_size):
			var color: Color = _sdrpp_palette(_fft_mag[x])
			_waterfall_image.set_pixel(x, 0, color)

	if _waterfall_texture != null:
		_waterfall_texture.update(_waterfall_image)


func _prepare_next_wide_scan_section() -> void:
	_wide_scan_section_index += 1
	if _wide_scan_section_index >= _wide_scan_section_centers_hz.size():
		_finalize_wide_scan()
		return

	_wide_scan_wait_for_tune = true
	_wide_scan_section_start_us = 0
	_wide_scan_accumulator.resize(_fft_size)
	for i in range(_fft_size):
		_wide_scan_accumulator[i] = -200.0
	_wide_scan_accumulator_count = 0
	_wide_scan_target_center_hz = float(_wide_scan_section_centers_hz[_wide_scan_section_index])
	emit_signal("source_center_changed", int(round(_wide_scan_target_center_hz)))


func _tick_wide_scan() -> void:
	if not _wide_scan_running:
		return
	if _wide_scan_section_index < 0 or _wide_scan_section_index >= _wide_scan_section_centers_hz.size():
		return
	if _wide_scan_wait_for_tune:
		var tune_error_hz: float = abs(_source_center_hz - _wide_scan_target_center_hz)
		var tune_threshold_hz: float = max(250.0, _source_sample_rate_hz * 0.002)
		if tune_error_hz <= tune_threshold_hz:
			_wide_scan_wait_for_tune = false
			_wide_scan_section_start_us = Time.get_ticks_usec()
		return

	if _wide_scan_section_start_us == 0:
		_wide_scan_section_start_us = Time.get_ticks_usec()
		return

	var elapsed_us: int = Time.get_ticks_usec() - _wide_scan_section_start_us
	if elapsed_us < int(_wide_scan_dwell_ms * 1000):
		return

	_finish_wide_scan_section()
	_prepare_next_wide_scan_section()
	queue_redraw()


func _accumulate_wide_scan_frame(_frame: PackedVector2Array) -> void:
	if not _wide_scan_running or _wide_scan_wait_for_tune:
		return
	if _wide_scan_accumulator.size() != _fft_size:
		return
	if _fft_smooth_db.size() != _fft_size:
		return

	for i in range(_fft_size):
		_wide_scan_accumulator[i] = max(_wide_scan_accumulator[i], _fft_smooth_db[i])
	_wide_scan_accumulator_count += 1


func _finish_wide_scan_section() -> void:
	if _wide_scan_section_index < 0 or _wide_scan_section_index >= _wide_scan_section_centers_hz.size():
		return

	var section_center_hz: float = float(_wide_scan_section_centers_hz[_wide_scan_section_index])
	var section_span_hz: float = max(_wide_scan_section_sample_rate_hz, 1.0)
	var section_start_hz: float = section_center_hz - (section_span_hz * 0.5)
	var row_last: int = max(1, _fft_size - 1)
	var section_count: int = _wide_scan_section_centers_hz.size()
	var effective_crop_ratio: float = clamp(WIDE_SCAN_SECTION_EDGE_CROP_RATIO, 0.0, 0.45)
	if section_count <= 1:
		effective_crop_ratio = 0.0
	var crop_bins: int = int(floor(float(_fft_size) * effective_crop_ratio))
	var first_bin: int = clamp(crop_bins, 0, row_last)
	var last_bin: int = clamp(row_last - crop_bins, first_bin, row_last)
	if _wide_scan_section_index <= 0:
		first_bin = 0
	if _wide_scan_section_index >= section_count - 1:
		last_bin = row_last
	if last_bin <= first_bin:
		first_bin = 0
		last_bin = row_last
	var cropped_start_hz: float = section_start_hz + (float(first_bin) / float(row_last)) * section_span_hz
	var cropped_end_hz: float = section_start_hz + (float(last_bin) / float(row_last)) * section_span_hz

	var avg_row: PackedFloat32Array = PackedFloat32Array()
	avg_row.resize(_fft_size)
	for i in range(_fft_size):
		avg_row[i] = _wide_scan_accumulator[i]

	var section_data: Dictionary = {
		"index": _wide_scan_section_index,
		"start_hz": cropped_start_hz,
		"end_hz": cropped_end_hz,
		"center_hz": section_center_hz,
		"sample_rate_hz": section_span_hz,
		"row": avg_row,
	}
	var overwrite_section: bool = _wide_scan_sections.size() == _wide_scan_section_centers_hz.size() \
		and _wide_scan_section_index >= 0 \
		and _wide_scan_section_index < _wide_scan_sections.size()
	if overwrite_section:
		_wide_scan_sections[_wide_scan_section_index] = section_data
	else:
		_wide_scan_sections.append(section_data)

	if _wide_scan_bins.size() <= 1:
		return
	var bins_last: int = _wide_scan_bins.size() - 1
	var span_hz: float = max(_wide_scan_full_span_hz, 1.0)
	for i in range(first_bin, last_bin + 1):
		var freq_hz: float = section_start_hz + (float(i) / float(row_last)) * section_span_hz
		var norm: float = (freq_hz - _wide_scan_min_hz) / span_hz
		var bin_index: int = clamp(int(round(norm * float(bins_last))), 0, bins_last)
		if _wide_scan_bin_written.size() == _wide_scan_bins.size() and _wide_scan_bin_written[bin_index] == 0:
			_wide_scan_bins[bin_index] = avg_row[i]
			_wide_scan_bin_written[bin_index] = 1
		else:
			_wide_scan_bins[bin_index] = max(_wide_scan_bins[bin_index], avg_row[i])

	_fill_wide_scan_bin_gaps()
	_recompute_wide_scan_bin_range()

	_wide_scan_dirty = true


func _finalize_wide_scan() -> void:
	_clear_unwritten_scan_bins_in_range()
	if _wide_scan_loop_enabled:
		_reset_wide_scan_bin_written_mask()
		_wide_scan_running = true
		_wide_scan_wait_for_tune = false
		_wide_scan_section_start_us = 0
		_wide_scan_section_index = -1
		_prepare_next_wide_scan_section()
		_refresh_scan_button_style()
		queue_redraw()
		return
	_wide_scan_running = false
	_wide_scan_wait_for_tune = false
	_wide_scan_section_start_us = 0
	_wide_scan_section_index = -1
	_wide_scan_dirty = true
	emit_signal("wide_scan_completed")
	_refresh_scan_button_style()
	queue_redraw()


func _reset_wide_scan_bin_written_mask() -> void:
	_wide_scan_bin_written = PackedByteArray()
	_wide_scan_bin_written.resize(_wide_scan_bins.size())
	for i in range(_wide_scan_bin_written.size()):
		_wide_scan_bin_written[i] = 0


func _clear_unwritten_scan_bins_in_range() -> void:
	if _wide_scan_bins.size() <= 1:
		return
	if _wide_scan_bin_written.size() != _wide_scan_bins.size():
		return
	var bins_last: int = _wide_scan_bins.size() - 1
	var span_hz: float = max(_wide_scan_full_span_hz, 1.0)
	var start_norm: float = clamp((_wide_scan_start_hz - _wide_scan_min_hz) / span_hz, 0.0, 1.0)
	var end_norm: float = clamp((_wide_scan_end_hz - _wide_scan_min_hz) / span_hz, 0.0, 1.0)
	var start_index: int = clamp(int(floor(min(start_norm, end_norm) * float(bins_last))), 0, bins_last)
	var end_index: int = clamp(int(ceil(max(start_norm, end_norm) * float(bins_last))), start_index, bins_last)
	for i in range(start_index, end_index + 1):
		if _wide_scan_bin_written[i] == 0:
			_wide_scan_bins[i] = -200.0
	_fill_wide_scan_bin_gaps()
	_recompute_wide_scan_bin_range()


func _apply_wide_scan_bins_to_image() -> void:
	if _wide_scan_bins.is_empty():
		return
	var width: int = _wide_scan_bins.size()
	if width <= 0:
		return
	if _wide_scan_image == null or _wide_scan_image.get_width() != width or _wide_scan_image.get_height() != WIDE_SCAN_TEXTURE_HEIGHT:
		_wide_scan_image = Image.create(width, WIDE_SCAN_TEXTURE_HEIGHT, false, Image.FORMAT_RGBA8)
		_wide_scan_texture = ImageTexture.create_from_image(_wide_scan_image)

	var min_db: float = _display_min_db
	var max_db: float = _display_max_db
	if _auto_range_enabled:
		min_db = _wide_scan_bin_floor_db
		max_db = _wide_scan_bin_peak_db
		if max_db <= min_db + 4.0:
			max_db = min_db + 4.0
	var range_db: float = max(1.0, max_db - min_db)
	for x in range(width):
		var db_value: float = _wide_scan_bins[x]
		var norm_value: float = clamp((db_value - min_db) / range_db, 0.0, 1.0)
		_wide_scan_image.set_pixel(x, 0, _sdrpp_palette(norm_value))

	if _wide_scan_texture != null:
		_wide_scan_texture.update(_wide_scan_image)


func _recompute_wide_scan_bin_range() -> void:
	if _wide_scan_bins.is_empty():
		_wide_scan_bin_floor_db = -140.0
		_wide_scan_bin_peak_db = -30.0
		return
	var min_db: float = INF
	var max_db: float = -INF
	for value in _wide_scan_bins:
		var db_value: float = float(value)
		min_db = min(min_db, db_value)
		max_db = max(max_db, db_value)
	if not is_finite(min_db) or not is_finite(max_db):
		_wide_scan_bin_floor_db = -140.0
		_wide_scan_bin_peak_db = -30.0
		return
	_wide_scan_bin_floor_db = min_db
	_wide_scan_bin_peak_db = max_db


func _sdrpp_palette(t: float) -> Color:
	if _palette_colors.size() <= 1:
		return Color(0.0, 0.0, 0.0, 1.0)
	var p: float = clamp(t, 0.0, 1.0)
	var pos: float = p * float(_palette_colors.size() - 1)
	var idx0: int = int(floor(pos))
	var idx1: int = min(idx0 + 1, _palette_colors.size() - 1)
	var frac: float = pos - float(idx0)
	return _palette_colors[idx0].lerp(_palette_colors[idx1], frac)


func _get_waterfall_rect() -> Rect2:
	var height: float = max(10.0, size.y - LABEL_HEIGHT - _get_top_band_height())
	var width: float = max(10.0, size.x - SLIDER_PANEL_WIDTH)
	return Rect2(0.0, _get_top_band_height(), width, height)


func _get_waterfall_src_rect() -> Rect2:
	if not _wide_scan_mode and _normal_view_slice_mode:
		return Rect2(0.0, 0.0, float(_fft_size), float(HISTORY_ROWS))

	var full_span_hz: float = _get_full_span_hz()
	var visible_span_hz: float = _get_visible_span_hz()
	var src_width_norm: float = min(1.0, visible_span_hz / full_span_hz)
	var texture_width: float = _get_active_texture_width()
	var src_width_px: float = max(1.0, src_width_norm * texture_width)

	var center_norm: float = 0.5 + (_pan_hz / full_span_hz)
	var src_x: float = (center_norm * texture_width) - (src_width_px * 0.5)
	if not _wide_scan_mode:
		src_x = clamp(src_x, 0.0, texture_width - src_width_px)
	return Rect2(src_x, 0.0, src_width_px, _get_active_texture_height())


func _get_visible_span_hz() -> float:
	var full_span_hz: float = _get_full_span_hz()
	if _wide_scan_mode:
		return max(full_span_hz * _span_scale, 0.0001)
	var min_span_hz: float = full_span_hz / MIN_SPAN_DIVISOR
	var max_span_hz: float = full_span_hz
	return clamp(full_span_hz * _span_scale, min_span_hz, max_span_hz)


func _left_edge_hz() -> float:
	return _get_base_center_hz() + _pan_hz - (_get_visible_span_hz() * 0.5)


func _clamp_view() -> void:
	var full_span_hz: float = _get_full_span_hz()
	if _wide_scan_mode:
		var visible_span_hz_wide: float = max(full_span_hz * _span_scale, 0.0001)
		_span_scale = visible_span_hz_wide / full_span_hz
		var left_hz_wide: float = _get_base_center_hz() + _pan_hz - (visible_span_hz_wide * 0.5)
		if left_hz_wide < 0.0:
			_pan_hz -= left_hz_wide
		_persist_active_view_to_mode()
		return
	var min_span_hz: float = full_span_hz / MIN_SPAN_DIVISOR
	var max_span_hz: float = full_span_hz
	var clamped_span_hz: float = clamp(full_span_hz * _span_scale, min_span_hz, max_span_hz)
	_span_scale = clamped_span_hz / full_span_hz
	var visible_span_hz: float = clamped_span_hz
	var max_pan: float = max(0.0, (full_span_hz - visible_span_hz) * 0.5)
	_pan_hz = clamp(_pan_hz, -max_pan, max_pan)
	_persist_active_view_to_mode()


func _get_full_span_hz() -> float:
	if _wide_scan_mode and _wide_view_full_span_hz > 1.0:
		return _wide_view_full_span_hz
	return max(_source_sample_rate_hz, 1000.0)


func _get_base_center_hz() -> float:
	if _wide_scan_mode and _wide_view_full_span_hz > 1.0:
		return _wide_view_center_hz
	return _source_center_hz


func _get_active_texture_width() -> float:
	if _wide_scan_mode and _wide_scan_image != null:
		return float(max(1, _wide_scan_image.get_width()))
	return float(max(1, _fft_size))


func _get_active_texture_height() -> float:
	if _wide_scan_mode and _wide_scan_image != null:
		return float(max(1, _wide_scan_image.get_height()))
	return float(HISTORY_ROWS)


func _freq_to_x(freq_hz: float) -> float:
	var span_hz: float = _get_visible_span_hz()
	var left_hz: float = _left_edge_hz()
	return ((freq_hz - left_hz) / span_hz) * max(1.0, _get_waterfall_rect().size.x)


func _x_to_freq(x_pos: float) -> float:
	var span_hz: float = _get_visible_span_hz()
	var left_hz: float = _left_edge_hz()
	return left_hz + ((x_pos / max(1.0, _get_waterfall_rect().size.x)) * span_hz)


func _nice_frequency_step(raw_step_hz: float) -> float:
	var safe_raw: float = max(raw_step_hz, 1.0)
	var power10: float = pow(10.0, floor(log(safe_raw) / log(10.0)))
	var mantissa: float = safe_raw / power10
	if mantissa <= 1.0:
		return 1.0 * power10
	if mantissa <= 2.0:
		return 2.0 * power10
	if mantissa <= 5.0:
		return 5.0 * power10
	return 10.0 * power10


func _draw_frequency_grid(wf_rect: Rect2) -> void:
	var span_hz: float = _get_visible_span_hz()
	var left_hz: float = _left_edge_hz()
	var right_hz: float = left_hz + span_hz
	var target_major_divisions: float = 16.0
	var major_step_hz: float = _nice_frequency_step(span_hz / target_major_divisions)
	var quarter_step_hz: float = max(major_step_hz * 0.25, 1.0)
	var label_band_top: float = _get_ticker_band_top()
	var label_baseline_y: float = label_band_top + FREQ_LABEL_BAND_HEIGHT - 4.0

	var start_tick_hz: float = floor(left_hz / quarter_step_hz) * quarter_step_hz
	var max_ticks: int = 2048
	var tick_hz: float = start_tick_hz
	var tick_index: int = 0
	while tick_hz <= right_hz + quarter_step_hz and tick_index < max_ticks:
		var x: float = ((tick_hz - left_hz) / max(span_hz, 1.0)) * wf_rect.size.x
		if x >= -2.0 and x <= wf_rect.size.x + 2.0:
			var major_ratio: float = tick_hz / major_step_hz
			var is_major: bool = abs(major_ratio - round(major_ratio)) < 0.001
			var grid_alpha: float = 0.11
			var grid_width: float = 1.0
			var label_alpha: float = 0.94
			if is_major:
				grid_alpha = 0.22
				grid_width = 1.35
				label_alpha = 1.0
			draw_line(Vector2(x, wf_rect.position.y), Vector2(x, wf_rect.position.y + wf_rect.size.y), Color(1, 1, 1, grid_alpha), grid_width)
			if is_major:
				var text: String = "%.3f MHz" % (tick_hz / 1_000_000.0)
				_draw_bold_string(Vector2(x + 2.0, label_baseline_y), text, HORIZONTAL_ALIGNMENT_LEFT, -1.0, 12, Color(0.97, 0.97, 0.97, label_alpha))
		tick_hz += quarter_step_hz
		tick_index += 1


func _draw_wide_scan_overlays(wf_rect: Rect2) -> void:
	if _wide_scan_full_span_hz <= 1.0:
		return

	var tuned_left_hz: float = _source_center_hz - (_source_sample_rate_hz * 0.5)
	var tuned_right_hz: float = _source_center_hz + (_source_sample_rate_hz * 0.5)
	var tuned_x0: float = _freq_to_x(tuned_left_hz)
	var tuned_x1: float = _freq_to_x(tuned_right_hz)
	var tuned_left_px: float = max(wf_rect.position.x, min(tuned_x0, tuned_x1))
	var tuned_right_px: float = min(wf_rect.end.x, max(tuned_x0, tuned_x1))
	if tuned_right_px > tuned_left_px:
		var tuned_rect: Rect2 = Rect2(tuned_left_px, wf_rect.position.y, tuned_right_px - tuned_left_px, wf_rect.size.y)
		draw_rect(tuned_rect, Color(0.2, 1.0, 0.2, 0.12), true)
		draw_line(Vector2(tuned_rect.position.x, tuned_rect.position.y), Vector2(tuned_rect.position.x, tuned_rect.end.y), Color(0.2, 1.0, 0.2, 0.5), 1.0)
		draw_line(Vector2(tuned_rect.end.x, tuned_rect.position.y), Vector2(tuned_rect.end.x, tuned_rect.end.y), Color(0.2, 1.0, 0.2, 0.5), 1.0)

	var scan_min_x: float = _freq_to_x(_wide_scan_start_hz)
	var scan_max_x: float = _freq_to_x(_wide_scan_end_hz)
	var min_active: bool = _drag_mode == _DRAG_SCAN_MIN or _hover_mode == _DRAG_SCAN_MIN
	var max_active: bool = _drag_mode == _DRAG_SCAN_MAX or _hover_mode == _DRAG_SCAN_MAX
	var min_color: Color = Color(0.55, 1.0, 0.55, 0.95 if min_active else 0.80)
	var max_color: Color = Color(1.0, 0.7, 0.35, 0.95 if max_active else 0.80)
	if scan_min_x >= wf_rect.position.x and scan_min_x <= wf_rect.end.x:
		draw_line(Vector2(scan_min_x, wf_rect.position.y), Vector2(scan_min_x, wf_rect.end.y), min_color, 2.0)
		_draw_bold_string(
			Vector2(scan_min_x + 3.0, _get_info_band_baseline_y()),
			"Scan Min",
			HORIZONTAL_ALIGNMENT_LEFT,
			80.0,
			13,
			Color(0.96, 1.0, 0.96, 1.0)
		)
	if scan_max_x >= wf_rect.position.x and scan_max_x <= wf_rect.end.x:
		draw_line(Vector2(scan_max_x, wf_rect.position.y), Vector2(scan_max_x, wf_rect.end.y), max_color, 2.0)
		_draw_bold_string(
			Vector2(scan_max_x + 3.0, _get_info_band_baseline_y()),
			"Scan Max",
			HORIZONTAL_ALIGNMENT_LEFT,
			80.0,
			13,
			Color(1.0, 0.97, 0.92, 1.0)
		)


func _is_wide_demod_line_mode() -> bool:
	return false


func _draw_demodulators(wf_rect: Rect2) -> void:
	_draw_source_selector(wf_rect)
	var line_mode: bool = _is_wide_demod_line_mode()
	for demod_value in _demodulators:
		var demod: Dictionary = demod_value as Dictionary
		var node_name: StringName = StringName(demod.get("node_name", StringName()))
		if node_name == StringName():
			continue

		var label: String = String(demod.get("label", "Demod"))
		if label.strip_edges().is_empty():
			label = String(demod.get("node_name", "Demod"))
		var offset_hz: float = float(demod.get("offset_hz", 0.0))
		var bandwidth_hz: float = max(float(demod.get("bandwidth_hz", 150000.0)), MIN_BANDWIDTH_HZ)
		var is_selected: bool = bool(demod.get("selected", false))

		var center_hz: float = _source_center_hz + offset_hz
		var x_center: float = _freq_to_x(center_hz)
		var width_px: float = max(6.0, abs((bandwidth_hz / _get_visible_span_hz()) * wf_rect.size.x))
		var cap_y: float = _get_demod_cap_y()
		var bar_top: float = wf_rect.position.y
		var raw_left: float = x_center - (width_px * 0.5)
		var raw_right: float = x_center + (width_px * 0.5)
		var draw_left: float = max(raw_left, wf_rect.position.x)
		var draw_right: float = min(raw_right, wf_rect.end.x)
		if draw_right <= draw_left:
			continue
		var bar_rect: Rect2 = Rect2(draw_left, bar_top, draw_right - draw_left, max(10.0, wf_rect.end.y - bar_top))
		var is_hovered: bool = _hover_demod == node_name

		if line_mode:
			var line_x: float = clamp(x_center, wf_rect.position.x, wf_rect.end.x)
			var line_color: Color = Color(0.2, 1.0, 0.2, 0.85)
			var line_width: float = 1.2
			if is_selected:
				line_color = Color(0.2, 1.0, 0.2, 1.0)
				line_width = 2.0
			elif is_hovered:
				line_color = Color(0.9, 1.0, 0.9, 0.95)
				line_width = 1.6
			draw_line(Vector2(line_x, wf_rect.position.y), Vector2(line_x, wf_rect.end.y), line_color, line_width)
			continue

		var body_alpha: float = 0.24
		var cap_alpha: float = 0.8
		if is_hovered:
			body_alpha = 0.45
		if is_selected:
			cap_alpha = 1.0

		draw_rect(bar_rect, Color(1.0, 1.0, 1.0, body_alpha), true)
		draw_rect(Rect2(bar_rect.position.x, cap_y, bar_rect.size.x, DEMOD_CAP_HEIGHT), Color(0.2, 1.0, 0.2, cap_alpha), true)
		var label_width: float = clamp(max(140.0, bar_rect.size.x + 24.0), 64.0, wf_rect.size.x)
		var label_x: float = (bar_rect.position.x + (bar_rect.size.x * 0.5)) - (label_width * 0.5)
		var label_rect: Rect2 = Rect2(label_x, wf_rect.end.y + 2.0, label_width, LABEL_HEIGHT - 4.0)
		var label_viewport: Rect2 = Rect2(wf_rect.position.x, wf_rect.end.y + 2.0, wf_rect.size.x, LABEL_HEIGHT - 4.0)
		var clipped_label_rect: Rect2 = label_rect.intersection(label_viewport)
		if clipped_label_rect.size.x <= 2.0:
			continue
		draw_rect(clipped_label_rect, Color(0.02, 0.02, 0.02, 0.80), true)
		var draw_text: String = _elide_text(label, clipped_label_rect.size.x - 8.0, 12)
		_draw_bold_string(
			Vector2(clipped_label_rect.position.x + 4.0, clipped_label_rect.position.y + (clipped_label_rect.size.y * 0.70)),
			draw_text,
			HORIZONTAL_ALIGNMENT_CENTER,
			clipped_label_rect.size.x - 8.0,
			12,
			Color(1.0, 1.0, 1.0, 0.95)
		)
		if is_hovered and width_px >= MIN_DEMOD_RESIZE_WIDTH_PX:
			_draw_hover_arrows(bar_rect)


func _draw_source_selector(wf_rect: Rect2) -> void:
	if not _wide_scan_mode:
		return
	var source_span_hz: float = max(_source_sample_rate_hz, 1.0)
	var x_center: float = _freq_to_x(_source_center_hz)
	var width_px: float = max(6.0, abs((source_span_hz / _get_visible_span_hz()) * wf_rect.size.x))
	var left_x: float = x_center - (width_px * 0.5)
	var right_x: float = x_center + (width_px * 0.5)
	var draw_left: float = max(wf_rect.position.x, left_x)
	var draw_right: float = min(wf_rect.end.x, right_x)
	if draw_right <= draw_left:
		return
	var bar_rect: Rect2 = Rect2(draw_left, wf_rect.position.y, draw_right - draw_left, max(10.0, wf_rect.size.y))
	var alpha: float = 0.10 if _drag_retune_source else 0.06
	draw_rect(bar_rect, Color(1.0, 0.2, 0.2, alpha), true)
	draw_rect(Rect2(bar_rect.position.x, _get_demod_cap_y(), bar_rect.size.x, DEMOD_CAP_HEIGHT), Color(1.0, 0.18, 0.18, 0.95), true)
	_draw_bold_string(
		Vector2(max(2.0, bar_rect.position.x + 3.0), _get_info_band_baseline_y()),
		"SDR Source",
		HORIZONTAL_ALIGNMENT_LEFT,
		140.0,
		14,
		Color(1.0, 0.98, 0.98, 1.0)
	)


func _draw_hover_arrows(bar_rect: Rect2) -> void:
	var y_mid: float = bar_rect.position.y + (bar_rect.size.y * 0.5)
	var left_x: float = bar_rect.position.x
	var right_x: float = bar_rect.position.x + bar_rect.size.x
	var arrow_color: Color = Color(0.95, 0.95, 0.95, 0.9)

	var left_arrow: PackedVector2Array = PackedVector2Array()
	left_arrow.push_back(Vector2(left_x - 6.0, y_mid))
	left_arrow.push_back(Vector2(left_x + 2.0, y_mid - 5.0))
	left_arrow.push_back(Vector2(left_x + 2.0, y_mid + 5.0))
	draw_colored_polygon(left_arrow, arrow_color)

	var right_arrow: PackedVector2Array = PackedVector2Array()
	right_arrow.push_back(Vector2(right_x + 6.0, y_mid))
	right_arrow.push_back(Vector2(right_x - 2.0, y_mid - 5.0))
	right_arrow.push_back(Vector2(right_x - 2.0, y_mid + 5.0))
	draw_colored_polygon(right_arrow, arrow_color)


func _handle_mouse_button(mouse_button: InputEventMouseButton) -> void:
	if mouse_button.button_index == MOUSE_BUTTON_WHEEL_UP and mouse_button.pressed:
		_zoom_at(mouse_button.position.x, 0.88)
		return
	if mouse_button.button_index == MOUSE_BUTTON_WHEEL_DOWN and mouse_button.pressed:
		_zoom_at(mouse_button.position.x, 1.14)
		return

	if mouse_button.button_index == MOUSE_BUTTON_MIDDLE:
		if mouse_button.pressed:
			_drag_mode = _DRAG_PAN
			_drag_demod = StringName()
			_drag_retune_source = not _wide_scan_running and _get_waterfall_rect().has_point(mouse_button.position)
			mouse_default_cursor_shape = Control.CURSOR_MOVE if _drag_retune_source else Control.CURSOR_DRAG
		else:
			_drag_mode = _DRAG_NONE
			_drag_retune_source = false
			mouse_default_cursor_shape = Control.CURSOR_ARROW
		return

	if mouse_button.button_index == MOUSE_BUTTON_RIGHT:
		if mouse_button.pressed:
			var right_hit: Dictionary = _hit_test_demod(mouse_button.position)
			if not right_hit.is_empty():
				var hit_name: StringName = StringName(right_hit.get("node_name", StringName()))
				if hit_name != StringName():
					emit_signal("demod_selected", hit_name)
					emit_signal("demod_context_requested", hit_name, Vector2i(int(round(mouse_button.global_position.x)), int(round(mouse_button.global_position.y))))
				_drag_mode = _DRAG_NONE
				_drag_demod = StringName()
				_drag_retune_source = false
				mouse_default_cursor_shape = Control.CURSOR_ARROW
				return
			_drag_mode = _DRAG_PAN
			_drag_demod = StringName()
			_drag_retune_source = not _wide_scan_running and _get_waterfall_rect().has_point(mouse_button.position)
			mouse_default_cursor_shape = Control.CURSOR_MOVE if _drag_retune_source else Control.CURSOR_DRAG
		else:
			_drag_mode = _DRAG_NONE
			_drag_retune_source = false
			mouse_default_cursor_shape = Control.CURSOR_ARROW
		return

	if mouse_button.button_index != MOUSE_BUTTON_LEFT:
		return

	var scan_hit_mode: int = _get_scan_range_hit_mode(mouse_button.position)
	if mouse_button.pressed and scan_hit_mode != _DRAG_NONE:
		_drag_mode = scan_hit_mode
		_drag_demod = StringName()
		_drag_retune_source = false
		mouse_default_cursor_shape = Control.CURSOR_HSIZE
		return

	if not mouse_button.pressed:
		_drag_mode = _DRAG_NONE
		_drag_demod = StringName()
		_drag_retune_source = false
		mouse_default_cursor_shape = Control.CURSOR_ARROW
		return

	var hit: Dictionary = _hit_test_demod(mouse_button.position)
	if not hit.is_empty():
		_drag_demod = StringName(hit.get("node_name", StringName()))
		if _drag_demod != StringName():
			emit_signal("demod_selected", _drag_demod)
		_drag_mode = int(hit.get("mode", _DRAG_NONE))
		_drag_retune_source = false
		return

	if _is_source_ticker_drag_hit(mouse_button.position):
		_drag_mode = _DRAG_PAN
		_drag_demod = StringName()
		_drag_retune_source = true
		mouse_default_cursor_shape = Control.CURSOR_MOVE
		return

	if _hit_test_source(mouse_button.position):
		_drag_mode = _DRAG_PAN
		_drag_demod = StringName()
		_drag_retune_source = true
		mouse_default_cursor_shape = Control.CURSOR_MOVE
		return

	_drag_mode = _DRAG_PAN
	_drag_demod = StringName()
	_drag_retune_source = false
	mouse_default_cursor_shape = Control.CURSOR_DRAG


func _handle_mouse_motion(mouse_motion: InputEventMouseMotion) -> void:
	_update_hover_tracking(mouse_motion.position)
	if _drag_mode == _DRAG_SCAN_MIN or _drag_mode == _DRAG_SCAN_MAX:
		mouse_default_cursor_shape = Control.CURSOR_HSIZE
		var drag_hz: float = clamp(_x_to_freq(mouse_motion.position.x), _source_min_frequency_hz, _source_max_frequency_hz)
		if _drag_mode == _DRAG_SCAN_MIN:
			_wide_scan_start_hz = drag_hz
		else:
			_wide_scan_end_hz = drag_hz
		_sanitize_scan_range()
		_update_scan_range_controls()
		queue_redraw()
		return

	if _drag_mode == _DRAG_PAN:
		mouse_default_cursor_shape = Control.CURSOR_MOVE if _drag_retune_source else Control.CURSOR_DRAG
		var span_hz: float = _get_visible_span_hz()
		var hz_per_px: float = span_hz / max(1.0, _get_waterfall_rect().size.x)
		if _drag_retune_source:
			var previous_center_hz: float = _source_center_hz
			var drag_sign: float = 1.0 if _wide_scan_mode else -1.0
			var delta_hz: float = drag_sign * mouse_motion.relative.x * hz_per_px
			var next_center_hz: float = previous_center_hz + delta_hz
			next_center_hz = _clamp_source_center_to_limits(next_center_hz)
			_source_center_hz = next_center_hz
			var applied_delta_hz: float = _source_center_hz - previous_center_hz
			_apply_locked_demod_offsets_for_source_delta(applied_delta_hz)
			emit_signal("source_center_changed", int(round(_source_center_hz)))
		else:
			_pan_hz -= mouse_motion.relative.x * hz_per_px
			_clamp_view()
		queue_redraw()
		return

	if _drag_mode == _DRAG_NONE or _drag_demod == StringName():
		_update_hover(mouse_motion.position)
		return

	# Keep demod offset dragging on the same drag cursor style as waterfall panning.
	mouse_default_cursor_shape = Control.CURSOR_MOVE

	var count: int = _demodulators.size()
	for i in range(count):
		var demod_value: Variant = _demodulators[i]
		var demod: Dictionary = demod_value as Dictionary
		if demod.is_empty():
			continue

		var node_name: StringName = StringName(demod.get("node_name", StringName()))
		if node_name != _drag_demod:
			continue

		var offset_hz: float = float(demod.get("offset_hz", 0.0))
		var bandwidth_hz: float = max(float(demod.get("bandwidth_hz", 150000.0)), MIN_BANDWIDTH_HZ)
		var center_hz: float = _source_center_hz + offset_hz
		var center_x: float = _freq_to_x(center_hz)

		if _drag_mode == _DRAG_CENTER:
			center_hz = _x_to_freq(mouse_motion.position.x)
			center_hz = clamp(center_hz, _left_edge_hz(), _left_edge_hz() + _get_visible_span_hz())
			if not _wide_scan_running and _follow_demod_name != StringName() and node_name == _follow_demod_name:
				var previous_source_center_hz: float = _source_center_hz
				var current_demod_center_hz: float = _source_center_hz + offset_hz
				var source_half_span_hz: float = _source_sample_rate_hz * 0.5
				var follow_padding_hz: float = min(source_half_span_hz * 0.45, max(2000.0, _source_sample_rate_hz * 0.03))
				var follow_half_limit_hz: float = max(1000.0, source_half_span_hz - follow_padding_hz)
				var out_of_scope_hz: float = 0.0
				if center_hz > _source_center_hz + follow_half_limit_hz:
					out_of_scope_hz = center_hz - (_source_center_hz + follow_half_limit_hz)
				elif center_hz < _source_center_hz - follow_half_limit_hz:
					out_of_scope_hz = center_hz - (_source_center_hz - follow_half_limit_hz)
				if abs(out_of_scope_hz) > 0.0:
					var next_source_center_hz: float = _source_center_hz
					if abs(center_hz - current_demod_center_hz) >= _source_sample_rate_hz:
						next_source_center_hz = _clamp_source_center_to_limits(center_hz)
					else:
						next_source_center_hz = _clamp_source_center_to_limits(_source_center_hz + sign(out_of_scope_hz) * source_half_span_hz)
						if center_hz > next_source_center_hz + follow_half_limit_hz or center_hz < next_source_center_hz - follow_half_limit_hz:
							next_source_center_hz = _clamp_source_center_to_limits(center_hz)
					_source_center_hz = next_source_center_hz
				var applied_source_delta_hz: float = _source_center_hz - previous_source_center_hz
				_apply_locked_demod_offsets_for_source_delta(applied_source_delta_hz, node_name)
				emit_signal("source_center_changed", int(round(_source_center_hz)))
				offset_hz = center_hz - _source_center_hz
			else:
				offset_hz = center_hz - _source_center_hz
		elif _drag_mode == _DRAG_LEFT:
			var right_x: float = center_x + ((bandwidth_hz / _get_visible_span_hz()) * _get_waterfall_rect().size.x * 0.5)
			var new_width_hz: float = abs(_x_to_freq(right_x) - _x_to_freq(mouse_motion.position.x))
			bandwidth_hz = max(new_width_hz, MIN_BANDWIDTH_HZ)
		elif _drag_mode == _DRAG_RIGHT:
			var left_x: float = center_x - ((bandwidth_hz / _get_visible_span_hz()) * _get_waterfall_rect().size.x * 0.5)
			var new_width_hz2: float = abs(_x_to_freq(mouse_motion.position.x) - _x_to_freq(left_x))
			bandwidth_hz = max(new_width_hz2, MIN_BANDWIDTH_HZ)

		demod["offset_hz"] = offset_hz
		demod["bandwidth_hz"] = bandwidth_hz
		_demodulators[i] = demod
		emit_signal(
			"demod_range_changed",
			node_name,
			int(round(offset_hz)),
			int(round(bandwidth_hz)),
			int(round(_source_center_hz + offset_hz)),
			_drag_mode
		)
		queue_redraw()
		return


func _zoom_at(x_pos: float, zoom_factor: float) -> void:
	var clamped_factor: float = clamp(zoom_factor, 0.5, 2.0)
	var before_hz: float = _x_to_freq(x_pos)
	var full_span_hz: float = _get_full_span_hz()
	if _wide_scan_mode:
		_span_scale *= clamped_factor
		if _span_scale <= 0.0:
			_span_scale = 0.000000001
		_clamp_view()
		var after_hz_wide: float = _x_to_freq(x_pos)
		_pan_hz += before_hz - after_hz_wide
		_clamp_view()
		queue_redraw()
		return
	var min_scale: float = (full_span_hz / MIN_SPAN_DIVISOR) / max(1.0, full_span_hz)
	var max_span_hz: float = full_span_hz
	var max_scale: float = max_span_hz / max(1.0, full_span_hz)
	max_scale = max(max_scale, MAX_SPAN_SCALE)
	_span_scale = clamp(_span_scale * clamped_factor, min_scale, max_scale)
	_clamp_view()
	var after_hz: float = _x_to_freq(x_pos)
	_pan_hz += before_hz - after_hz
	_clamp_view()
	queue_redraw()


func _hit_test_demod(mouse_pos: Vector2) -> Dictionary:
	var wf_rect: Rect2 = _get_waterfall_rect()
	if mouse_pos.y < wf_rect.position.y or mouse_pos.y > wf_rect.end.y:
		return {}

	var count: int = _demodulators.size()
	for i in range(count):
		var demod_value: Variant = _demodulators[i]
		var demod: Dictionary = demod_value as Dictionary
		if demod.is_empty():
			continue

		var node_name: StringName = StringName(demod.get("node_name", StringName()))
		if node_name == StringName():
			continue

		var offset_hz: float = float(demod.get("offset_hz", 0.0))
		var bandwidth_hz: float = max(float(demod.get("bandwidth_hz", 150000.0)), MIN_BANDWIDTH_HZ)
		var center_hz: float = _source_center_hz + offset_hz
		var x_center: float = _freq_to_x(center_hz)
		var width_px: float = max(6.0, abs((bandwidth_hz / _get_visible_span_hz()) * wf_rect.size.x))
		var left_x: float = x_center - (width_px * 0.5)
		var right_x: float = x_center + (width_px * 0.5)
		var bar_top: float = wf_rect.position.y
		var bar_rect: Rect2 = Rect2(left_x, bar_top, width_px, max(10.0, wf_rect.end.y - bar_top))
		if mouse_pos.x < left_x or mouse_pos.x > right_x:
			continue
		if mouse_pos.y < bar_rect.position.y or mouse_pos.y > bar_rect.end.y:
			continue

		var mode: int = _DRAG_CENTER
		var allow_resize_edges: bool = width_px >= MIN_DEMOD_RESIZE_WIDTH_PX
		if allow_resize_edges:
			if abs(mouse_pos.x - left_x) <= EDGE_HIT_PX:
				mode = _DRAG_LEFT
			elif abs(mouse_pos.x - right_x) <= EDGE_HIT_PX:
				mode = _DRAG_RIGHT
		return {"node_name": node_name, "mode": mode}

	return {}


func _hit_test_source(mouse_pos: Vector2) -> bool:
	if not _wide_scan_mode or _wide_scan_running:
		return false
	var wf_rect: Rect2 = _get_waterfall_rect()
	if mouse_pos.y < wf_rect.position.y or mouse_pos.y > wf_rect.end.y:
		return false
	var source_span_hz: float = max(_source_sample_rate_hz, 1.0)
	var x_center: float = _freq_to_x(_source_center_hz)
	var width_px: float = max(6.0, abs((source_span_hz / _get_visible_span_hz()) * wf_rect.size.x))
	var left_x: float = x_center - (width_px * 0.5)
	var right_x: float = x_center + (width_px * 0.5)
	return mouse_pos.x >= left_x and mouse_pos.x <= right_x


func _get_source_ticker_rect() -> Rect2:
	var wf_rect: Rect2 = _get_waterfall_rect()
	return Rect2(wf_rect.position.x, _get_ticker_band_top() - 2.0, wf_rect.size.x, FREQ_LABEL_BAND_HEIGHT + 4.0)


func _is_source_ticker_drag_hit(mouse_pos: Vector2) -> bool:
	if _wide_scan_running:
		return false
	return _get_source_ticker_rect().has_point(mouse_pos)


func _get_scan_range_hit_mode(mouse_pos: Vector2) -> int:
	if not _wide_scan_mode:
		return _DRAG_NONE
	var wf_rect: Rect2 = _get_waterfall_rect()
	if not wf_rect.has_point(mouse_pos):
		return _DRAG_NONE
	var hit_tolerance_px: float = 7.0
	var start_x: float = _freq_to_x(_wide_scan_start_hz)
	var end_x: float = _freq_to_x(_wide_scan_end_hz)
	if abs(mouse_pos.x - start_x) <= hit_tolerance_px:
		return _DRAG_SCAN_MIN
	if abs(mouse_pos.x - end_x) <= hit_tolerance_px:
		return _DRAG_SCAN_MAX
	return _DRAG_NONE


func _update_hover(mouse_pos: Vector2) -> void:
	_update_hover_tracking(mouse_pos)
	var hit: Dictionary = _hit_test_demod(mouse_pos)
	if hit.is_empty():
		if _hover_mode != _DRAG_NONE or _hover_demod != StringName():
			_hover_mode = _DRAG_NONE
			_hover_demod = StringName()
			queue_redraw()
		var scan_hit_mode: int = _get_scan_range_hit_mode(mouse_pos)
		if scan_hit_mode != _DRAG_NONE:
			_hover_mode = scan_hit_mode
			_hover_demod = StringName()
			mouse_default_cursor_shape = Control.CURSOR_HSIZE
			queue_redraw()
			return
		if _hit_test_source(mouse_pos):
			mouse_default_cursor_shape = Control.CURSOR_MOVE
		elif _is_source_ticker_drag_hit(mouse_pos):
			# Top frequency strip drag retunes source center in live mode.
			mouse_default_cursor_shape = Control.CURSOR_HSIZE
		elif _get_waterfall_rect().has_point(mouse_pos):
			# Open-hand style fallback while hovering pan area.
			mouse_default_cursor_shape = Control.CURSOR_DRAG
		else:
			mouse_default_cursor_shape = Control.CURSOR_ARROW
		return

	_hover_mode = int(hit.get("mode", _DRAG_NONE))
	_hover_demod = StringName(hit.get("node_name", StringName()))
	if _hover_mode == _DRAG_CENTER:
		# Offset selector hover should read as "move/tune".
		mouse_default_cursor_shape = Control.CURSOR_DRAG
	else:
		# Edge handles are bandwidth resize.
		mouse_default_cursor_shape = Control.CURSOR_HSIZE
	queue_redraw()


func _notification(what: int) -> void:
	if what == NOTIFICATION_MOUSE_EXIT:
		_hover_mode = _DRAG_NONE
		_hover_demod = StringName()
		_hover_freq_active = false
		if _drag_mode == _DRAG_NONE:
			mouse_default_cursor_shape = Control.CURSOR_ARROW
		queue_redraw()


func _on_min_db_changed(value: float) -> void:
	_auto_range_enabled = false
	if _auto_range_check != null:
		_auto_range_check.set_pressed_no_signal(false)
	_display_min_db = min(value, _display_max_db - 1.0)
	if _min_slider != null and abs(_min_slider.value - _display_min_db) > 0.001:
		_min_slider.value = _display_min_db
	_refresh_slider_labels()
	if _wide_scan_mode and not _wide_scan_bins.is_empty():
		_wide_scan_dirty = true
	queue_redraw()


func _on_max_db_changed(value: float) -> void:
	_auto_range_enabled = false
	if _auto_range_check != null:
		_auto_range_check.set_pressed_no_signal(false)
	_display_max_db = max(value, _display_min_db + 1.0)
	if _max_slider != null and abs(_max_slider.value - _display_max_db) > 0.001:
		_max_slider.value = _display_max_db
	_refresh_slider_labels()
	if _wide_scan_mode and not _wide_scan_bins.is_empty():
		_wide_scan_dirty = true
	queue_redraw()


func _refresh_slider_labels() -> void:
	if _min_label != null:
		_min_label.text = "MIN"
	if _min_value_label != null:
		_min_value_label.text = "%.0f" % _display_min_db
	if _max_label != null:
		_max_label.text = "MAX"
	if _max_value_label != null:
		_max_value_label.text = "%.0f" % _display_max_db


func _on_settings_button_pressed() -> void:
	if _settings_popup == null or _settings_button == null:
		return
	if _settings_popup.visible:
		_settings_popup.hide()
		return
	if _wide_scan_settings_popup != null and _wide_scan_settings_popup.visible:
		_wide_scan_settings_popup.hide()
	_settings_popup.position = Vector2i(int(_settings_button.global_position.x), int(_settings_button.global_position.y + _settings_button.size.y + 2.0))
	_settings_popup.popup()


func _on_wide_scan_settings_button_pressed() -> void:
	if _wide_scan_settings_popup == null or _wide_scan_settings_button == null:
		return
	if _wide_scan_settings_popup.visible:
		_wide_scan_settings_popup.hide()
		return
	if _settings_popup != null and _settings_popup.visible:
		_settings_popup.hide()
	_wide_scan_settings_popup.position = Vector2i(int(_wide_scan_settings_button.global_position.x), int(_wide_scan_settings_button.global_position.y + _wide_scan_settings_button.size.y + 2.0))
	_wide_scan_settings_popup.popup()


func _on_view_mode_selected(index: int) -> void:
	var wants_wide_scan: bool = index == 1
	if wants_wide_scan == _wide_scan_mode:
		return
	_on_wide_scan_button_pressed()


func _on_fft_option_selected(index: int) -> void:
	if _fft_option == null:
		return
	var id_value: int = _fft_option.get_item_id(index)
	if id_value <= 0:
		return
	_fft_size = id_value
	_initialize_buffers()
	_clamp_view()
	queue_redraw()


func _on_window_selected(index: int) -> void:
	if _window_option == null:
		return
	_fft_window_name = _window_option.get_item_text(index)


func _on_speed_changed(value: float) -> void:
	_max_update_fps = _fps_from_log_position(value)
	_update_speed_label()


func _on_render_toggle_pressed() -> void:
	_render_enabled = not _render_enabled
	_refresh_render_toggle_style()


func _on_wide_scan_button_pressed() -> void:
	_persist_active_view_to_mode()
	_wide_scan_mode = not _wide_scan_mode
	_drag_mode = _DRAG_NONE
	_drag_demod = StringName()
	_drag_retune_source = false
	_hover_mode = _DRAG_NONE
	_hover_demod = StringName()
	if _wide_scan_mode:
		_normal_view_slice_mode = false
		var has_scan_data: bool = not _wide_scan_sections.is_empty() or not _wide_scan_bins.is_empty()
		if not has_scan_data:
			_wide_scan_section_centers_hz = PackedInt64Array()
			_wide_scan_section_index = -1
			_wide_scan_image = Image.create(1, WIDE_SCAN_TEXTURE_HEIGHT, false, Image.FORMAT_RGBA8)
			_wide_scan_image.fill(Color(0.0, 0.0, 0.0, 1.0))
			if _wide_scan_texture == null:
				_wide_scan_texture = ImageTexture.create_from_image(_wide_scan_image)
			else:
				_wide_scan_texture.update(_wide_scan_image)
	if not _wide_scan_mode and _wide_scan_running:
		cancel_wide_scan()
	if _settings_popup != null and _settings_popup.visible:
		_settings_popup.hide()
	if _wide_scan_settings_popup != null and _wide_scan_settings_popup.visible:
		_wide_scan_settings_popup.hide()
	_sync_active_view_from_mode()
	_refresh_wide_scan_button_style()
	_refresh_scan_button_style()
	_clamp_view()
	queue_redraw()


func _on_show_waterfall_toggled(enabled: bool) -> void:
	_show_waterfall = enabled
	queue_redraw()


func _on_full_update_toggled(enabled: bool) -> void:
	_full_waterfall_update = enabled


func _on_bilinear_filter_toggled(enabled: bool) -> void:
	_bilinear_filter_enabled = enabled
	_apply_texture_filter()
	queue_redraw()


func _on_auto_range_toggled(enabled: bool) -> void:
	_auto_range_enabled = enabled
	if _wide_scan_mode and not _wide_scan_bins.is_empty():
		_wide_scan_dirty = true


func _on_wide_scan_dwell_changed(value: float) -> void:
	_wide_scan_dwell_ms = int(round(clamp(value, 10.0, 60000.0)))
	_update_wide_scan_dwell_label()


func _on_wide_scan_start_selector_changed(value: int) -> void:
	_wide_scan_start_hz = clamp(float(value), _source_min_frequency_hz, _source_max_frequency_hz)
	_sanitize_scan_range()
	_update_scan_range_controls()
	queue_redraw()


func _on_wide_scan_end_selector_changed(value: int) -> void:
	_wide_scan_end_hz = clamp(float(value), _source_min_frequency_hz, _source_max_frequency_hz)
	_sanitize_scan_range()
	_update_scan_range_controls()
	queue_redraw()


func _on_wide_scan_start_changed(value: float) -> void:
	_wide_scan_start_hz = clamp(value, _source_min_frequency_hz, _source_max_frequency_hz)
	_sanitize_scan_range()
	_update_scan_range_controls()
	queue_redraw()


func _on_wide_scan_end_changed(value: float) -> void:
	_wide_scan_end_hz = clamp(value, _source_min_frequency_hz, _source_max_frequency_hz)
	_sanitize_scan_range()
	_update_scan_range_controls()
	queue_redraw()


func _on_wide_scan_loop_toggled(enabled: bool) -> void:
	_wide_scan_loop_enabled = enabled


func _on_follow_demod_selected(index: int) -> void:
	if _follow_demod_option == null:
		_follow_demod_name = StringName()
		return
	var id_value: int = _follow_demod_option.get_item_id(index)
	if id_value <= 0:
		_follow_demod_name = StringName()
		return
	_follow_demod_name = StringName(_follow_demod_option.get_item_metadata(index))


func _fps_from_log_position(p: float) -> float:
	var clamped: float = clamp(p, 0.0, 1.0)
	var ratio: float = MAX_UPDATE_FPS / MIN_UPDATE_FPS
	return MIN_UPDATE_FPS * pow(ratio, clamped)


func _log_position_from_fps(fps: float) -> float:
	var clamped_fps: float = clamp(fps, MIN_UPDATE_FPS, MAX_UPDATE_FPS)
	var ratio: float = MAX_UPDATE_FPS / MIN_UPDATE_FPS
	return log(clamped_fps / MIN_UPDATE_FPS) / log(ratio)


func _update_speed_label() -> void:
	if _speed_value_label != null:
		_speed_value_label.text = str(int(round(_max_update_fps)))


func _update_wide_scan_dwell_label() -> void:
	if _wide_scan_dwell_label != null:
		_wide_scan_dwell_label.text = "%d ms" % _wide_scan_dwell_ms


func _sanitize_scan_range() -> void:
	var min_limit_hz: float = _source_min_frequency_hz
	var max_limit_hz: float = _source_max_frequency_hz
	_wide_scan_start_hz = clamp(_wide_scan_start_hz, min_limit_hz, max_limit_hz)
	_wide_scan_end_hz = clamp(_wide_scan_end_hz, min_limit_hz, max_limit_hz)
	if _wide_scan_end_hz <= _wide_scan_start_hz:
		var width_hz: float = max(_source_sample_rate_hz, 1_000_000.0)
		_wide_scan_end_hz = min(max_limit_hz, _wide_scan_start_hz + width_hz)
	if _wide_scan_end_hz <= _wide_scan_start_hz:
		_wide_scan_start_hz = min_limit_hz
		_wide_scan_end_hz = max_limit_hz


func _update_scan_range_controls() -> void:
	var min_limit_hz: float = _source_min_frequency_hz
	var max_limit_hz: float = _source_max_frequency_hz
	if _wide_scan_start_selector != null:
		_wide_scan_start_selector.call("set_limits", int(min_limit_hz), int(max_limit_hz))
		_wide_scan_start_selector.call("set_value_no_signal", int(round(_wide_scan_start_hz)))
	if _wide_scan_end_selector != null:
		_wide_scan_end_selector.call("set_limits", int(min_limit_hz), int(max_limit_hz))
		_wide_scan_end_selector.call("set_value_no_signal", int(round(_wide_scan_end_hz)))
	if _wide_scan_start_spin != null:
		_wide_scan_start_spin.min_value = min_limit_hz
		_wide_scan_start_spin.max_value = max_limit_hz
		_wide_scan_start_spin.set_value_no_signal(_wide_scan_start_hz)
	if _wide_scan_end_spin != null:
		_wide_scan_end_spin.min_value = min_limit_hz
		_wide_scan_end_spin.max_value = max_limit_hz
		_wide_scan_end_spin.set_value_no_signal(_wide_scan_end_hz)
	if _wide_scan_loop_check != null:
		_wide_scan_loop_check.set_pressed_no_signal(_wide_scan_loop_enabled)


func _refresh_follow_demod_options() -> void:
	if _follow_demod_option == null:
		return
	var old_follow: StringName = _follow_demod_name
	_follow_demod_option.clear()
	_follow_demod_option.add_item("None", 0)
	_follow_demod_option.set_item_metadata(0, StringName())
	var selected_index: int = 0
	var idx: int = 1
	for demod_value in _demodulators:
		var demod: Dictionary = demod_value as Dictionary
		if demod.is_empty():
			continue
		var node_name: StringName = StringName(demod.get("node_name", StringName()))
		if node_name == StringName():
			continue
		var label: String = String(demod.get("label", String(node_name)))
		_follow_demod_option.add_item(label, idx)
		_follow_demod_option.set_item_metadata(idx, node_name)
		if node_name == old_follow:
			selected_index = idx
		idx += 1
	_follow_demod_option.select(selected_index)
	if selected_index == 0:
		_follow_demod_name = StringName()
	else:
		_follow_demod_name = StringName(_follow_demod_option.get_item_metadata(selected_index))


func _find_demod_index_by_name(p_node_name: StringName) -> int:
	if p_node_name == StringName():
		return -1
	for i in range(_demodulators.size()):
		var demod: Dictionary = _demodulators[i] as Dictionary
		if demod.is_empty():
			continue
		if StringName(demod.get("node_name", StringName())) == p_node_name:
			return i
	return -1


func _clamp_source_center_to_limits(center_hz: float) -> float:
	var half_span_hz: float = _source_sample_rate_hz * 0.5
	var min_center_hz: float = _source_min_frequency_hz + half_span_hz
	var max_center_hz: float = _source_max_frequency_hz - half_span_hz
	if max_center_hz <= min_center_hz:
		return clamp(center_hz, _source_min_frequency_hz, _source_max_frequency_hz)
	return clamp(center_hz, min_center_hz, max_center_hz)


func _apply_locked_demod_offsets_for_source_delta(p_source_delta_hz: float, p_skip_node_name: StringName = StringName()) -> void:
	if abs(p_source_delta_hz) <= 0.0:
		return

	for i in range(_demodulators.size()):
		var demod: Dictionary = _demodulators[i] as Dictionary
		if demod.is_empty():
			continue
		if not bool(demod.get("lock_to_source", false)):
			continue

		var node_name: StringName = StringName(demod.get("node_name", StringName()))
		if node_name == StringName():
			continue
		if node_name == p_skip_node_name:
			continue

		var bandwidth_hz: float = max(float(demod.get("bandwidth_hz", 150000.0)), MIN_BANDWIDTH_HZ)
		var offset_hz: float = float(demod.get("offset_hz", 0.0)) - p_source_delta_hz
		demod["offset_hz"] = offset_hz
		_demodulators[i] = demod
		emit_signal(
			"demod_range_changed",
			node_name,
			int(round(offset_hz)),
			int(round(bandwidth_hz)),
			int(round(_source_center_hz + offset_hz)),
			_DRAG_CENTER
		)


func _refresh_wide_scan_button_style() -> void:
	_refresh_view_mode_controls()


func _refresh_scan_button_style() -> void:
	if _scan_button == null:
		return
	_scan_button.text = "Stop Scan" if _wide_scan_running else "Start Scan"
	var color: Color = Color(1.0, 0.25, 0.25, 1.0) if _wide_scan_running else Color(0.95, 0.95, 0.95, 1.0)
	_scan_button.add_theme_color_override("font_color", color)
	_scan_button.disabled = false
	_refresh_view_mode_controls()


func _on_scan_button_pressed() -> void:
	if _wide_scan_running:
		cancel_wide_scan()
		return
	if not _wide_scan_mode:
		var current_pan_hz: float = _pan_hz
		var current_span_scale: float = _span_scale
		_persist_active_view_to_mode()
		_wide_scan_mode = true
		_wide_pan_hz = current_pan_hz
		_wide_span_scale = current_span_scale
		_sync_active_view_from_mode()
		_refresh_wide_scan_button_style()
	_sanitize_scan_range()
	_update_scan_range_controls()
	emit_signal("wide_scan_requested", _wide_scan_dwell_ms, int(round(_wide_scan_start_hz)), int(round(_wide_scan_end_hz)), _wide_scan_loop_enabled)


func get_forward_fps_hint() -> float:
	if not _render_enabled:
		return 0.0
	if _wide_scan_mode and not _wide_scan_running:
		return 0.0
	if _full_waterfall_update:
		return MAX_UPDATE_FPS
	return _max_update_fps


func _elide_text(p_text: String, p_max_width: float, p_font_size: int) -> String:
	var font: Font = get_theme_default_font()
	if font == null:
		return p_text
	if p_max_width <= 6.0:
		return ""
	if font.get_string_size(p_text, HORIZONTAL_ALIGNMENT_LEFT, -1.0, p_font_size).x <= p_max_width:
		return p_text
	var ellipsis: String = "..."
	var out: String = p_text
	while out.length() > 1:
		out = out.left(out.length() - 1)
		var candidate: String = out + ellipsis
		if font.get_string_size(candidate, HORIZONTAL_ALIGNMENT_LEFT, -1.0, p_font_size).x <= p_max_width:
			return candidate
	return ellipsis


func _get_demod_cap_y() -> float:
	return _get_waterfall_rect().position.y - DEMOD_CAP_HEIGHT


func _draw_bold_string(
	p_position: Vector2,
	p_text: String,
	p_alignment: HorizontalAlignment,
	p_width: float,
	p_font_size: int,
	p_color: Color
) -> void:
	var font: Font = get_theme_default_font()
	if font == null:
		return
	var outline: Color = Color(0.0, 0.0, 0.0, min(1.0, p_color.a))
	var offsets: Array[Vector2] = [
		Vector2(-1.0, 0.0),
		Vector2(1.0, 0.0),
		Vector2(0.0, -1.0),
		Vector2(0.0, 1.0),
		Vector2(-1.0, -1.0),
		Vector2(1.0, -1.0),
		Vector2(-1.0, 1.0),
		Vector2(1.0, 1.0),
		Vector2(-2.0, 0.0),
		Vector2(2.0, 0.0),
	]
	for offset in offsets:
		draw_string(font, p_position + offset, p_text, p_alignment, p_width, p_font_size, outline)
	draw_string(font, p_position, p_text, p_alignment, p_width, p_font_size, p_color)


func _refresh_render_toggle_style() -> void:
	if _render_toggle_button == null:
		return
	_render_toggle_button.text = "Waterfall On" if _render_enabled else "Waterfall Off"
	var color: Color = Color(0.15, 0.95, 0.15, 1.0) if _render_enabled else Color(0.95, 0.25, 0.25, 1.0)
	_render_toggle_button.add_theme_color_override("font_color", color)


func _refresh_view_mode_controls() -> void:
	if _view_mode_option != null:
		_view_mode_option.select(1 if _wide_scan_mode else 0)
	if _wide_buttons_row != null:
		_wide_buttons_row.visible = _wide_scan_mode
	if _top_buttons_row != null:
		_top_buttons_row.visible = not _wide_scan_mode
	if _settings_button != null:
		_settings_button.text = "Waterfall Menu"
	if _wide_scan_settings_button != null:
		_wide_scan_settings_button.text = "Wide Scan Menu"


func _apply_texture_filter() -> void:
	texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR if _bilinear_filter_enabled else CanvasItem.TEXTURE_FILTER_NEAREST


func _get_ticker_band_top() -> float:
	return 0.0


func _get_hover_band_top() -> float:
	return FREQ_LABEL_BAND_HEIGHT


func _get_info_band_top() -> float:
	return FREQ_LABEL_BAND_HEIGHT + HOVER_LABEL_BAND_HEIGHT


func _get_top_band_height() -> float:
	return FREQ_LABEL_BAND_HEIGHT + HOVER_LABEL_BAND_HEIGHT + INFO_LABEL_BAND_HEIGHT


func _get_info_band_baseline_y() -> float:
	return _get_info_band_top() + INFO_LABEL_BAND_HEIGHT - 6.0


func _draw_header_bands(wf_rect: Rect2) -> void:
	draw_rect(Rect2(0.0, _get_ticker_band_top(), wf_rect.size.x, FREQ_LABEL_BAND_HEIGHT), Color(0.01, 0.01, 0.01, 0.82), true)
	draw_rect(Rect2(0.0, _get_hover_band_top(), wf_rect.size.x, HOVER_LABEL_BAND_HEIGHT), Color(0.02, 0.02, 0.02, 0.88), true)
	draw_rect(Rect2(0.0, _get_info_band_top(), wf_rect.size.x, INFO_LABEL_BAND_HEIGHT), Color(0.02, 0.02, 0.02, 0.88), true)


func _update_hover_tracking(mouse_pos: Vector2) -> void:
	var wf_rect: Rect2 = _get_waterfall_rect()
	var was_active: bool = _hover_freq_active
	var old_x: float = _hover_freq_x
	var old_freq: float = _hover_freq_hz
	if wf_rect.has_point(mouse_pos):
		_hover_freq_active = true
		_hover_freq_x = clamp(mouse_pos.x, wf_rect.position.x, wf_rect.end.x)
		_hover_freq_hz = _x_to_freq(_hover_freq_x)
	else:
		_hover_freq_active = false
	if was_active != _hover_freq_active or abs(old_x - _hover_freq_x) > 0.25 or abs(old_freq - _hover_freq_hz) >= 1.0:
		queue_redraw()


func _draw_hover_marker(wf_rect: Rect2) -> void:
	if not _hover_freq_active:
		return
	var marker_x: float = clamp(_hover_freq_x, wf_rect.position.x, wf_rect.end.x)
	draw_line(Vector2(marker_x, wf_rect.position.y), Vector2(marker_x, wf_rect.end.y), Color(1.0, 1.0, 1.0, 0.62), 2.0)

func _draw_drag_readout(wf_rect: Rect2) -> void:
	if not _hover_freq_active:
		return
	var readout: String = _format_frequency_readout(_hover_freq_hz)
	var text_width: float = clamp(max(140.0, float(readout.length()) * 8.0), 140.0, wf_rect.size.x - 8.0)
	var text_x: float = clamp(_hover_freq_x - (text_width * 0.5), 2.0, wf_rect.size.x - text_width - 2.0)
	var text_rect: Rect2 = Rect2(text_x, _get_hover_band_top(), text_width, HOVER_LABEL_BAND_HEIGHT)
	draw_rect(text_rect, Color(0.01, 0.01, 0.01, 0.94), true)
	_draw_bold_string(
		Vector2(text_rect.position.x + 4.0, text_rect.position.y + HOVER_LABEL_BAND_HEIGHT - 2.0),
		readout,
		HORIZONTAL_ALIGNMENT_CENTER,
		text_rect.size.x - 8.0,
		12,
		Color(0.98, 0.98, 0.98, 1.0)
	)


func _format_frequency_readout(freq_hz: float) -> String:
	var abs_hz: float = abs(freq_hz)
	if abs_hz >= 1_000_000_000.0:
		return "%.6f GHz" % (freq_hz / 1_000_000_000.0)
	if abs_hz >= 1_000_000.0:
		return "%.3f MHz" % (freq_hz / 1_000_000.0)
	if abs_hz >= 1_000.0:
		return "%.3f kHz" % (freq_hz / 1_000.0)
	return "%.0f Hz" % freq_hz


func _fill_wide_scan_bin_gaps() -> void:
	if _wide_scan_bins.is_empty():
		return
	var n: int = _wide_scan_bins.size()
	var unset_threshold: float = -199.5
	var i: int = 0
	while i < n:
		if _wide_scan_bins[i] > unset_threshold:
			i += 1
			continue
		var start: int = i
		while i < n and _wide_scan_bins[i] <= unset_threshold:
			i += 1
		var left: int = start - 1
		var right: int = i
		var left_valid: bool = left >= 0 and _wide_scan_bins[left] > unset_threshold
		var right_valid: bool = right < n and _wide_scan_bins[right] > unset_threshold
		if left_valid and right_valid:
			var left_value: float = _wide_scan_bins[left]
			var right_value: float = _wide_scan_bins[right]
			var span: float = float(right - left)
			for j in range(start, right):
				var t: float = float(j - left) / span
				_wide_scan_bins[j] = lerpf(left_value, right_value, t)
		elif left_valid:
			var fill_left: float = _wide_scan_bins[left]
			for j in range(start, right):
				_wide_scan_bins[j] = fill_left
		elif right_valid:
			var fill_right: float = _wide_scan_bins[right]
			for j in range(start, right):
				_wide_scan_bins[j] = fill_right


func _persist_active_view_to_mode() -> void:
	if _wide_scan_mode:
		_wide_pan_hz = _pan_hz
		_wide_span_scale = _span_scale
	else:
		_normal_pan_hz = _pan_hz
		_normal_span_scale = _span_scale


func _sync_active_view_from_mode() -> void:
	if _wide_scan_mode:
		_pan_hz = _wide_pan_hz
		_span_scale = _wide_span_scale
	else:
		_pan_hz = _normal_pan_hz
		_span_scale = _normal_span_scale
