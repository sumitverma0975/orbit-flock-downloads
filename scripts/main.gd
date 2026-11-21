extends Node
## Delhi Drive: an original offline driving challenge game.
const BG := Color("101d2b")
const INK := Color("edf3f5")
const MUTED := Color("9eafbd")
const GOLD := Color("efbc76")
const MINT := Color("80dfbf")
const FONT = preload("res://assets/fonts/Regular.ttf")
const BOLD = preload("res://assets/fonts/Bold.ttf")
var progress := DriveProgress.new()
var world: DriveWorld
var sound: DriveAudio
var ui: Control
var overlay: DriveOverlay
var state: String = "menu"
var time_left: float = 0.0
var time_limit: float = 1.0
var touch_holds: Dictionary = {}
var hold_rects: Dictionary = {}
var hold_buttons: Dictionary = {}
var speed_label: Label
var time_label: Label
var target_label: Label
var gear_label: Label
var distance_label: Label
var gear_button: Button
var reverse_button: Button
var result_success: bool = false
var earned_stars: int = 0
var earned_reward: int = 0
var result_reason: String = ""
var throttle_value: float = 0.0
var steer_value: float = 0.0
var help_return_state: String = "menu"

func _ready() -> void:
	Engine.max_fps = 60
	progress.load_data()
	world = DriveWorld.new()
	world.name = "DriveWorld"
	add_child(world)
	world.hit_obstacle.connect(_on_collision)
	world.mission_complete.connect(_win)
	sound = DriveAudio.new()
	add_child(sound)
	sound.set_muted(progress.muted)
	var layer := CanvasLayer.new()
	add_child(layer)
	ui = Control.new()
	ui.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	ui.mouse_filter = Control.MOUSE_FILTER_IGNORE
	layer.add_child(ui)
	var theme := Theme.new()
	theme.default_font = FONT
	theme.default_font_size = 18
	ui.theme = theme
	show_menu()

func _clear_ui() -> void:
	for child in ui.get_children():
		ui.remove_child(child)
		child.queue_free()
	touch_holds.clear()
	hold_rects.clear()
	hold_buttons.clear()

func _style(color: Color, border: Color = Color(1, 1, 1, 0.12), radius: int = 12) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = color
	style.border_color = border
	style.set_border_width_all(1)
	style.set_corner_radius_all(radius)
	return style

func _panel(rect: Rect2, color: Color = Color(0.04, 0.075, 0.12, 0.94)) -> Panel:
	var panel := Panel.new()
	panel.position = rect.position
	panel.size = rect.size
	panel.add_theme_stylebox_override("panel", _style(color))
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ui.add_child(panel)
	return panel

func _label(text: String, rect: Rect2, font_size: int = 20, color: Color = INK, bold: bool = false, centered: bool = false) -> Label:
	var label := Label.new()
	label.text = text
	label.position = rect.position
	label.size = rect.size
	label.add_theme_color_override("font_color", color)
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_font_override("font", BOLD if bold else FONT)
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	if centered:
		label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ui.add_child(label)
	return label

func _button(text: String, rect: Rect2, action: Callable, accent: bool = false, font_size: int = 18) -> Button:
	var button := Button.new()
	button.text = text
	button.position = rect.position
	button.size = rect.size
	button.focus_mode = Control.FOCUS_NONE
	button.add_theme_font_override("font", BOLD)
	button.add_theme_font_size_override("font_size", font_size)
	button.add_theme_color_override("font_color", BG if accent else INK)
	button.add_theme_color_override("font_hover_color", BG if accent else INK)
	button.add_theme_color_override("font_pressed_color", BG if accent else INK)
	button.add_theme_color_override("font_disabled_color", MUTED)
	button.add_theme_stylebox_override("normal", _style(GOLD if accent else Color(0.09, 0.15, 0.21, 0.9)))
	button.add_theme_stylebox_override("hover", _style(GOLD.lightened(0.15) if accent else Color("2d4559"), GOLD))
	button.add_theme_stylebox_override("pressed", _style(GOLD.darkened(0.2) if accent else Color("385b6c")))
	button.add_theme_stylebox_override("disabled", _style(Color("23303c")))
	button.pressed.connect(action)
	ui.add_child(button)
	return button

func _hold(text: String, rect: Rect2, action: String, accent: bool = false) -> void:
	hold_rects[action] = rect
	var b := _button(text, rect, func(): pass, accent, 22)
	b.mouse_filter = Control.MOUSE_FILTER_IGNORE
	b.toggle_mode = true
	hold_buttons[action] = b

func _save() -> void:
	progress.save_data()

func show_menu() -> void:
	state = "menu"
	world.set_running(false)
	_clear_ui()
	world.configure(DriveData.CARS[progress.selected_car], DriveData.LEVELS[progress.selected_level], DriveData.DIFFICULTIES[progress.difficulty], progress.quality)
	world.set_showroom(true)
	Engine.max_fps = 30 if progress.quality == 0 else 60
	var car: Dictionary = DriveData.CARS[progress.selected_car]
	var level: Dictionary = DriveData.LEVELS[progress.selected_level]
	var unlocked: bool = DriveData.car_unlocked(progress.selected_car, progress.total_score)
	var level_unlocked: bool = DriveData.level_unlocked(progress.selected_level, progress.completed_count)
	_panel(Rect2(28, 25, 345, 86), Color(0.035, 0.065, 0.1, 0.86))
	_label("DELHI / DRIVE", Rect2(48, 35, 320, 45), 34, INK, true)
	_label("THE CITY IS YOUR DRIVING TEST", Rect2(50, 78, 320, 20), 11, GOLD, true)
	_panel(Rect2(533, 35, 270, 50), Color(0.04, 0.075, 0.12, 0.88))
	_label("CAREER    %s credits" % progress.total_score, Rect2(548, 40, 245, 38), 16, MINT, true, true)
	_button("‹", Rect2(34, 325, 54, 72), func(): _change_car(-1), false, 38)
	_button("›", Rect2(758, 325, 54, 72), func(): _change_car(1), false, 38)
	_panel(Rect2(30, 528, 783, 143), Color(0.04, 0.075, 0.12, 0.92))
	_label("%02d / 12   •   %s" % [progress.selected_car + 1, str(car.category).to_upper()], Rect2(50, 539, 730, 22), 13, GOLD, true)
	_label(str(car.name), Rect2(48, 562, 725, 40), 29, INK, true)
	_label("POWER   %0.1f     TOP SPEED   %d km/h     HANDLING   %d%%" % [float(car.accel), int(car.top_speed), int(float(car.handling) * 100)], Rect2(51, 608, 720, 24), 15, MUTED)
	_label("READY TO DRIVE" if unlocked else "UNLOCK AT %d CAREER CREDITS" % int(car.unlock), Rect2(51, 641, 720, 20), 12, MINT if unlocked else GOLD, true)
	_panel(Rect2(838, 25, 412, 646))
	_label("CAREER CHALLENGES", Rect2(864, 43, 330, 29), 14, GOLD, true)
	_label("%02d / 12" % (progress.selected_level + 1), Rect2(864, 79, 330, 40), 30, INK, true)
	_label(str(level.name), Rect2(864, 123, 355, 43), 25, INK, true)
	var subtitle := _label(str(level.get("subtitle", "Drive with precision")), Rect2(864, 170, 355, 42), 14, MUTED)
	subtitle.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var kind: String = str(level.get("type", "checkpoints")).to_upper()
	_label(kind + "  /  " + ("★".repeat(progress.best_stars[progress.selected_level]) + "☆".repeat(3 - progress.best_stars[progress.selected_level])), Rect2(864, 218, 355, 27), 17, MINT, true)
	var instructions := "Reach the markers in order before the timer runs out."
	match kind:
		"PARKING":
			instructions = "Follow the markers, line up with the bay, and stop inside it."
		"PRECISION":
			instructions = "Finish the route with at least %d%% vehicle condition." % int(level.min_health)
		"FUEL":
			instructions = "Finish the route on a %d%% tank. Coast into turns to save fuel." % int(level.fuel_budget)
	var description := _label(instructions, Rect2(864, 258, 355, 70), 16, MUTED)
	description.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_button("‹  PREVIOUS", Rect2(864, 350, 165, 46), func(): _change_level(-1), false, 15)
	_button("NEXT  ›", Rect2(1041, 350, 181, 46), func(): _change_level(1), false, 15)
	_button("DIFFICULTY     " + str(DriveData.DIFFICULTIES[progress.difficulty].name).to_upper() + "  ›", Rect2(864, 411, 358, 48), func(): _change_difficulty(), false, 16)
	_button("GRAPHICS    " + ["LOW / 30 FPS", "MEDIUM", "HIGH"][progress.quality] + "  ›", Rect2(864, 472, 358, 44), func(): _change_quality(), false, 15)
	var start := _button("START CHALLENGE  →" if unlocked and level_unlocked else ("CAR LOCKED" if not unlocked else "COMPLETE PREVIOUS LEVEL"), Rect2(864, 539, 358, 62), start_run, true, 19)
	start.disabled = not unlocked or not level_unlocked
	_button("HOW TO DRIVE", Rect2(864, 615, 220, 34), _show_help, false, 12)
	_button("SOUND " + ("OFF" if progress.muted else "ON"), Rect2(1095, 615, 127, 34), _toggle_sound, false, 12)
	_label("OFFLINE • ORIGINAL CITY • NO ADS", Rect2(35, 685, 610, 22), 12, MUTED)
	_label("v1.0   /   Original driving challenges", Rect2(780, 685, 464, 22), 12, MUTED, false, true)

func _change_car(delta: int) -> void:
	progress.selected_car = posmod(progress.selected_car + delta, 12)
	_save()
	show_menu()

func _change_level(delta: int) -> void:
	progress.selected_level = posmod(progress.selected_level + delta, 12)
	_save()
	show_menu()

func _change_difficulty() -> void:
	progress.difficulty = (progress.difficulty + 1) % 4
	_save()
	show_menu()

func _change_quality() -> void:
	progress.quality = (progress.quality + 1) % 3
	_save()
	show_menu()

func _toggle_sound() -> void:
	progress.muted = not progress.muted
	sound.set_muted(progress.muted)
	_save()
	if state == "menu":
		show_menu()
	elif state == "paused":
		_pause_screen()

func start_run() -> void:
	if not DriveData.car_unlocked(progress.selected_car, progress.total_score) or not DriveData.level_unlocked(progress.selected_level, progress.completed_count):
		return
	_save()
	_clear_ui()
	world.configure(DriveData.CARS[progress.selected_car], DriveData.LEVELS[progress.selected_level], DriveData.DIFFICULTIES[progress.difficulty], progress.quality)
	world.set_showroom(false)
	world.begin_run()
	time_limit = float(DriveData.LEVELS[progress.selected_level].time) * float(DriveData.DIFFICULTIES[progress.difficulty].time)
	time_left = time_limit
	state = "playing"
	_build_hud()

func _build_hud() -> void:
	_clear_ui()
	overlay = DriveOverlay.new()
	overlay.world = world
	overlay.active = true
	ui.add_child(overlay)
	_panel(Rect2(20, 16, 1240, 82), Color(0.035, 0.065, 0.1, 0.91))
	_button("Ⅱ", Rect2(34, 29, 58, 54), pause_run, false, 23)
	_label("%02d  /  %s" % [progress.selected_level + 1, str(DriveData.LEVELS[progress.selected_level].name).to_upper()], Rect2(108, 24, 420, 27), 17, GOLD, true)
	target_label = _label("FOLLOW THE GOLD MARKER", Rect2(108, 53, 450, 27), 15, INK)
	time_label = _label("00:00", Rect2(564, 21, 155, 48), 36, INK, true, true)
	_button("CAMERA", Rect2(916, 30, 110, 52), func(): world.cycle_camera(), false, 14)
	_button("HORN", Rect2(1038, 30, 98, 52), func(): sound.horn(), false, 14)
	_button("HELP", Rect2(1148, 30, 98, 52), _show_help, false, 14)
	_label("CITY / GPS", Rect2(1059, 117, 180, 20), 11, MINT, true)
	distance_label = _label("", Rect2(1048, 287, 196, 24), 12, GOLD, true, true)
	_hold("‹", Rect2(30, 575, 125, 113), "left")
	_hold("›", Rect2(177, 575, 125, 113), "right")
	_hold("BRAKE", Rect2(976, 576, 123, 112), "brake")
	_hold("GAS", Rect2(1116, 514, 133, 174), "gas", true)
	reverse_button = _button("D / R", Rect2(836, 621, 121, 67), func(): world.toggle_reverse(); touch_holds.clear(), false, 19)
	gear_button = _button("AUTO", Rect2(836, 567, 121, 42), _toggle_gear_mode, false, 13)
	_button("−", Rect2(836, 517, 55, 40), func(): world.shift_gear(-1), false, 22)
	_button("+", Rect2(902, 517, 55, 40), func(): world.shift_gear(1), false, 22)
	_button("RECOVER", Rect2(30, 116, 106, 40), _recover, false, 12)
	speed_label = _label("0", Rect2(544, 550, 190, 80), 63, INK, true, true)
	_label("KM/H", Rect2(573, 631, 130, 25), 12, MUTED, true, true)
	gear_label = _label("D1", Rect2(731, 580, 96, 48), 30, GOLD, true, true)
	_label("CONDITION", Rect2(376, 640, 160, 24), 11, MINT, true)
	_label("FUEL", Rect2(748, 640, 100, 24), 11, GOLD, true)
	_label("STEERING", Rect2(102, 463, 130, 22), 11, MUTED, true, true)

func _toggle_gear_mode() -> void:
	world.toggle_transmission()

func _recover() -> void:
	if state == "playing":
		world.reset_car()
		time_left = maxf(0.0, time_left - 5.0)
		touch_holds.clear()

func _process(delta: float) -> void:
	if not is_instance_valid(world):
		return
	var active: bool = state == "playing"
	if active:
		for action in hold_buttons:
			hold_buttons[action].set_pressed_no_signal(_held(action))
		var gas: float = 1.0 if _held("gas") or Input.is_physical_key_pressed(KEY_W) or Input.is_physical_key_pressed(KEY_UP) else 0.0
		var brake: float = 1.0 if _held("brake") or Input.is_physical_key_pressed(KEY_S) or Input.is_physical_key_pressed(KEY_DOWN) or Input.is_physical_key_pressed(KEY_SPACE) else 0.0
		var steer: float = float(_held("right") or Input.is_physical_key_pressed(KEY_D) or Input.is_physical_key_pressed(KEY_RIGHT)) - float(_held("left") or Input.is_physical_key_pressed(KEY_A) or Input.is_physical_key_pressed(KEY_LEFT))
		steer_value = move_toward(steer_value, steer, delta * 5.0)
		throttle_value = gas
		world.set_controls(steer_value, gas, brake, Input.is_physical_key_pressed(KEY_SHIFT))
		time_left = maxf(0.0, time_left - delta)
		speed_label.text = str(roundi(world.speed_kmh))
		gear_label.text = world.gear_label
		gear_button.text = "AUTO" if world.automatic else "MANUAL"
		reverse_button.text = "REVERSE" if world.reverse_selected else "DRIVE"
		time_label.text = "%02d:%02d" % [floori(time_left / 60.0), int(time_left) % 60]
		time_label.add_theme_color_override("font_color", Color("f48d7b") if time_left < 15 else INK)
		var kind: String = str(DriveData.LEVELS[progress.selected_level].get("type", "checkpoints"))
		var is_final_parking := kind == "parking" and world.completed_targets == world.total_targets - 1
		target_label.text = "PARK: ALIGN & STOP IN THE BAY" if is_final_parking else "CHECKPOINT %d / %d" % [mini(world.completed_targets + 1, world.total_targets), world.total_targets]
		distance_label.text = "%d m TO TARGET" % roundi(world.target_distance)
		overlay.remaining = time_left
		overlay.limit = time_limit
		overlay.steer = steer_value
		overlay.queue_redraw()
		if world.health <= 0:
			_finish(false, "VEHICLE DAMAGED", 0, 0)
		elif world.fuel <= 0:
			_finish(false, "OUT OF FUEL", 0, 0)
		elif time_left <= 0:
			_finish(false, "TIME'S UP", 0, 0)
		elif kind == "precision" and world.health < float(DriveData.LEVELS[progress.selected_level].get("min_health", 60)):
			_finish(false, "CONDITION LIMIT", 0, 0)
	else:
		throttle_value = 0.0
		world.set_controls(0, 0, 1, false)
	sound.set_engine(float(world.speed_kmh) / 3.6, throttle_value, state == "playing")

func _held(action: String) -> bool:
	return touch_holds.values().has(action)

func _input(event: InputEvent) -> void:
	if state == "playing":
		if event is InputEventScreenTouch:
			if event.pressed:
				_capture_hold(event.index, event.position)
			else:
				touch_holds.erase(event.index)
		elif event is InputEventScreenDrag:
			if touch_holds.has(event.index):
				_capture_hold(event.index, event.position)
		elif event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
			if event.pressed:
				_capture_hold(1000, event.position)
			else:
				touch_holds.erase(1000)
		elif event is InputEventMouseMotion and touch_holds.has(1000):
			_capture_hold(1000, event.position)
	if event is InputEventKey and event.pressed and not event.echo:
		match state:
			"menu":
				match event.keycode:
					KEY_LEFT: _change_car(-1)
					KEY_RIGHT: _change_car(1)
					KEY_UP: _change_level(1)
					KEY_DOWN: _change_level(-1)
					KEY_G: _change_quality()
					KEY_ENTER: start_run()
			"playing":
				match event.keycode:
					KEY_ESCAPE, KEY_P: pause_run()
					KEY_C: world.cycle_camera()
					KEY_R: world.toggle_reverse()
					KEY_M: world.toggle_transmission()
					KEY_E: world.shift_gear(1)
					KEY_Q: world.shift_gear(-1)
					KEY_H: sound.horn()
			"paused":
				if event.keycode == KEY_ESCAPE or event.keycode == KEY_P:
					resume_run()
			"result":
				if event.keycode == KEY_ENTER: start_run()
				elif event.keycode == KEY_N: _next_level()
				elif event.keycode == KEY_ESCAPE: show_menu()
			"help":
				if event.keycode == KEY_ESCAPE: _close_help()

func _capture_hold(index: int, point: Vector2) -> void:
	touch_holds.erase(index)
	for action in hold_rects:
		if hold_rects[action].has_point(point):
			touch_holds[index] = action
			get_viewport().set_input_as_handled()
			return

func pause_run() -> void:
	if state != "playing": return
	state = "paused"
	world.set_running(false)
	touch_holds.clear()
	_pause_screen()

func _pause_screen() -> void:
	_clear_ui()
	_panel(Rect2(0, 0, 1280, 720), Color(0.02, 0.035, 0.06, 0.8))
	_panel(Rect2(419, 145, 442, 429))
	_label("TAKE A BREATHER", Rect2(446, 181, 388, 50), 30, INK, true, true)
	_label("Your timer and car are paused.", Rect2(446, 236, 388, 30), 16, MUTED, false, true)
	_button("RESUME DRIVE", Rect2(459, 298, 362, 58), resume_run, true)
	_button("RESTART", Rect2(459, 373, 173, 52), start_run)
	_button("GARAGE", Rect2(648, 373, 173, 52), show_menu)
	_button("SOUND " + ("OFF" if progress.muted else "ON"), Rect2(459, 453, 362, 44), _toggle_sound)

func resume_run() -> void:
	state = "playing"
	world.set_running(true)
	_build_hud()

func _win() -> void:
	if state != "playing": return
	var level: Dictionary = DriveData.LEVELS[progress.selected_level]
	if world.fuel <= 0:
		_finish(false, "OUT OF FUEL", 0, 0)
		return
	if world.health <= 0 or world.health < float(level.min_health):
		_finish(false, "CONDITION LIMIT", 0, 0)
		return
	var stars: int = DriveData.stars_for_run(time_left, time_limit, world.health)
	var reward: int = DriveData.reward_for_run(progress.selected_level, progress.difficulty, stars)
	progress.record_win(progress.selected_level, stars, reward)
	_finish(true, "CHALLENGE COMPLETE", stars, reward)

func _finish(success: bool, reason: String, stars: int, reward: int) -> void:
	if state != "playing": return
	state = "result"
	result_success = success
	earned_stars = stars
	earned_reward = reward
	result_reason = reason
	world.set_running(false)
	_clear_ui()
	_panel(Rect2(0, 0, 1280, 720), Color(0.02, 0.035, 0.06, 0.7))
	_panel(Rect2(335, 104, 610, 516))
	_label("DRIVE / %02d" % (progress.selected_level + 1), Rect2(365, 126, 550, 30), 15, GOLD, true, true)
	_label(reason, Rect2(360, 174, 560, 54), 30, INK, true, true)
	_label("★".repeat(stars) + "☆".repeat(3 - stars), Rect2(380, 242, 520, 62), 48, GOLD, true, true)
	_label("+ %d CREDITS" % reward if success else "A clean run is one retry away.", Rect2(365, 325, 550, 38), 23, MINT if success else MUTED, true, true)
	_label("Condition %d%%   •   Time left %ds   •   Career %d" % [roundi(world.health), roundi(time_left), progress.total_score], Rect2(365, 377, 550, 32), 16, MUTED, false, true)
	if success and progress.selected_level < 11:
		_button("NEXT CHALLENGE  →", Rect2(391, 440, 498, 58), _next_level, true)
	else:
		_button("DRIVE AGAIN", Rect2(391, 440, 498, 58), start_run, true)
	_button("RETRY", Rect2(391, 518, 241, 51), start_run)
	_button("GARAGE", Rect2(648, 518, 241, 51), show_menu)

func _next_level() -> void:
	if result_success and progress.selected_level < 11:
		progress.selected_level += 1
		_save()
		start_run()

func _on_collision(_damage: float) -> void:
	sound.impact()

func _show_help() -> void:
	help_return_state = state
	if state == "playing":
		world.set_running(false)
	state = "help"
	_clear_ui()
	_panel(Rect2(0, 0, 1280, 720), Color(0.02, 0.035, 0.06, 0.9))
	_label("THE DRIVER'S HANDBOOK", Rect2(92, 58, 1090, 55), 34, INK, true)
	_label("PRECISE INPUTS. CLEAN BODYWORK. PERFECT PARKING.", Rect2(94, 117, 1100, 29), 14, GOLD, true)
	var copy := "Hold GAS or BRAKE to control speed. Press the left or right pad to steer.\nFollow the target on the minimap. At a parking bay, align the car and stop below 3 km/h.\nTap DRIVE / REVERSE after stopping. AUTO changes gears. Use + / − to shift in MANUAL.\nCollisions reduce vehicle condition. RECOVER costs five seconds and some condition.\nEarn credits and stars to open new cars and challenges. Progress saves on this device.\n\nKEYBOARD: W/S pedals, A/D steer, R reverse, M transmission, Q/E gears, C camera, H horn, P pause."
	var help := _label(copy, Rect2(94, 176, 1090, 316), 18, MUTED)
	help.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_label("VEHICLE ART / CC BY 4.0", Rect2(94, 521, 1090, 26), 13, GOLD, true)
	var credits := _label("BMW 320i E90 by rifdanzz   •   Mercedes G-Class by Lexyc16   •   Mercedes-AMG GT by Yan Carvalho\nModels adapted from Sketchfab. Tap a model to open the original.\nCreative Commons Attribution 4.0 International   •   Godot Engine / MIT", Rect2(94, 545, 1050, 65), 13, MUTED)
	credits.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_button("BMW 320i", Rect2(94, 620, 140, 44), func(): OS.shell_open("https://sketchfab.com/3d-models/28f0bc2f0083417a8e521150529034d6"), false, 13)
	_button("G-Class", Rect2(245, 620, 140, 44), func(): OS.shell_open("https://sketchfab.com/3d-models/1a2a52b16cad4e618af347461817895c"), false, 13)
	_button("AMG GT", Rect2(396, 620, 140, 44), func(): OS.shell_open("https://sketchfab.com/3d-models/661dcab94455463784651a3ebc63cfb9"), false, 13)
	_button("CC BY 4.0", Rect2(547, 620, 140, 44), func(): OS.shell_open("https://creativecommons.org/licenses/by/4.0/"), false, 13)
	_button("BACK", Rect2(1060, 615, 156, 58), _close_help, true)

func _close_help() -> void:
	if help_return_state == "playing":
		resume_run()
	elif help_return_state == "paused":
		state = "paused"
		_pause_screen()
	elif help_return_state == "result":
		state = "playing"
		_finish(result_success, result_reason, earned_stars, earned_reward)
	else:
		show_menu()

func _notification(what: int) -> void:
	if what == NOTIFICATION_APPLICATION_PAUSED or what == NOTIFICATION_APPLICATION_FOCUS_OUT:
		if state == "playing": pause_run()
	elif what == NOTIFICATION_WM_GO_BACK_REQUEST:
		if state == "playing": pause_run()
		elif state == "paused": show_menu()
		elif state == "help": _close_help()
		elif state == "result": show_menu()
