extends SceneTree
## Exercise the first challenge through the game scene and its touch controls.

const Game := preload("res://main.tscn")
const SAVE_PATH := "user://mobile-release-smoke.cfg"
const GAS := Vector2(1180, 600)
const BRAKE := Vector2(1030, 628)
var failures: int = 0

func _initialize() -> void:
	call_deferred("run")

func check(passed: bool, message: String) -> void:
	if not passed:
		failures += 1
		push_error(message)

func touch(index: int, position: Vector2, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = position
	event.pressed = pressed
	root.push_input(event, true)

func run() -> void:
	if FileAccess.file_exists(SAVE_PATH):
		DirAccess.remove_absolute(SAVE_PATH)
	var game = Game.instantiate()
	game.progress.path = SAVE_PATH
	root.add_child(game)
	await process_frame
	check(game.state == "menu", "The game opens in the garage")
	game.start_run()
	check(game.state == "playing", "Start enters the first challenge")
	touch(0, GAS, true)
	touch(1, Vector2(230, 627), true)
	check(game.touch_holds.get(0) == "gas" and game.touch_holds.get(1) == "right", "Two fingers can steer and accelerate together")
	touch(1, Vector2(230, 627), false)
	for frame in 120:
		await physics_frame
	check(game.world.speed_kmh > 25.0, "Holding the gas pad accelerates the car")
	touch(0, GAS, false)
	game.pause_run()
	var time_before: float = game.time_left
	var position_before: Vector3 = game.world.get_player_position()
	for frame in 30:
		await physics_frame
	check(game.time_left == time_before and game.world.get_player_position() == position_before, "Pause freezes the timer and car")
	game.resume_run()
	var active_pad := Vector2.ZERO
	for frame in 2200:
		if game.state != "playing":
			break
		var world: DriveWorld = game.world
		var remaining: float = world.get_player_position().z - world.get_target_position().z
		var desired_speed: float = 11.0
		if world.completed_targets == world.total_targets - 1:
			desired_speed = minf(desired_speed, sqrt(maxf(0.0, remaining - 0.35) * 17.0))
		var next_pad := BRAKE if world.speed_kmh / 3.6 > desired_speed or remaining < 0.35 else GAS
		if active_pad != next_pad:
			if active_pad != Vector2.ZERO:
				touch(0, active_pad, false)
			touch(0, next_pad, true)
			active_pad = next_pad
		await physics_frame
	if active_pad != Vector2.ZERO:
		touch(0, active_pad, false)
	check(game.state == "result" and game.result_success, "Driving and braking completes the parking mission")
	check(game.earned_stars > 0 and game.earned_reward > 0, "The completion awards stars and credits")
	var saved := DriveProgress.new(SAVE_PATH)
	saved.load_data()
	check(saved.completed_count == 1 and saved.total_score == game.earned_reward, "The reward and next challenge survive reloading")
	game._next_level()
	check(game.state == "playing" and game.progress.selected_level == 1, "Next Challenge starts the unlocked second level")
	game.queue_free()
	await process_frame
	OS.delay_msec(170) # Allow the audio thread to close the stopped stream.
	await process_frame
	if FileAccess.file_exists(SAVE_PATH):
		DirAccess.remove_absolute(SAVE_PATH)
	print("MOBILE FLOW: %s" % ("garage, touch driving, pause, parking, saved credits, and next challenge passed" if failures == 0 else "%d failures" % failures))
	quit(1 if failures else 0)
