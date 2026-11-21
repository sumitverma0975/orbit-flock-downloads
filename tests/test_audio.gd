extends SceneTree

const AudioCode = preload("res://scripts/engine_audio.gd")


func _initialize() -> void:
	call_deferred("run_checks")


func run_checks() -> void:
	var sound = AudioCode.new()
	root.add_child(sound)
	await process_frame
	sound.set_engine(22.0, 0.8, true)
	sound._process(0.016)
	var energy: float = 0.0
	for index in range(4096):
		var frame: Vector2 = sound._synthesize_frame()
		assert(is_finite(frame.x) and is_finite(frame.y), "Audio contains non-finite samples")
		assert(absf(frame.x) <= 0.8 and absf(frame.y) <= 0.8, "Audio exceeds limiter")
		energy += frame.length_squared()
	assert(energy > 1.0, "Running engine is silent")
	sound.impact()
	sound.horn()
	for index in range(12000):
		var frame: Vector2 = sound._synthesize_frame()
		assert(is_finite(frame.x) and absf(frame.x) <= 0.8, "Mixed effects exceed limiter")
	sound.set_muted(true)
	sound.impact()
	sound.horn()
	for index in range(128):
		assert(sound._synthesize_frame() == Vector2.ZERO, "Mute does not silence audio")
	sound.set_muted(false)
	sound.set_engine(NAN, INF, false)
	sound._process(0.016)
	var last_frame := Vector2.ZERO
	for index in range(12000):
		last_frame = sound._synthesize_frame()
	assert(last_frame.length_squared() < 0.0000001, "Stopped engine did not fade to silence")
	sound.queue_free()
	await process_frame
	# Allow the audio mix thread to release its stopped stream reference.
	await create_timer(0.15).timeout
	print("PASS: synthesized engine/effects, finite bounded output, immediate mute, invalid-input handling, stopped fade, and headless generator lifecycle")
	quit(0)
