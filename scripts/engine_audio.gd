class_name DriveAudio
extends Node
## Lightweight original synthesized sounds. Speed passed to set_engine is metres/second.
## No microphone, downloaded audio, or sample files are used.

const SAMPLE_RATE: float = 22050.0
const SAMPLE_STEP: float = 1.0 / SAMPLE_RATE
const BUFFER_SECONDS: float = 0.12
const MAX_REFILL_FRAMES: int = 4096
const GEAR_SPEEDS: Array[float] = [10.0, 18.0, 29.0, 42.0, 58.0]
const GEAR_RATIOS: Array[float] = [3.5, 2.3, 1.6, 1.2, 1.0, 0.8]

var _player: AudioStreamPlayer
var _playback: AudioStreamGeneratorPlayback
var _speed: float = 0.0
var _throttle: float = 0.0
var _running: bool = false
var _muted: bool = false
var _rpm: float = 900.0
var _rpm_target: float = 900.0
var _engine_gain: float = 0.0
var _engine_target: float = 0.0
var _engine_phase: float = 0.0
var _horn_phase_a: float = 0.0
var _horn_phase_b: float = 0.0
var _impact_phase: float = 0.0
var _horn_remaining: float = 0.0
var _impact_remaining: float = 0.0
var _noise_low: float = 0.0
var _noise_state: int = 984721


func _ready() -> void:
	# Refill while the scene tree is paused so the device buffer never starves.
	# Engine output fades to silence when the tree is paused.
	process_mode = Node.PROCESS_MODE_ALWAYS
	_player = AudioStreamPlayer.new()
	_player.name = "SynthesizedVehicleAudio"
	var generator := AudioStreamGenerator.new()
	generator.mix_rate = SAMPLE_RATE
	generator.buffer_length = BUFFER_SECONDS
	_player.stream = generator
	_player.volume_db = -80.0 if _muted else -5.0
	add_child(_player)
	_player.play()
	_playback = _player.get_stream_playback() as AudioStreamGeneratorPlayback
	_refill_buffer()


func _exit_tree() -> void:
	if is_instance_valid(_player):
		_player.stop()
		_player.stream = null
	_playback = null


func set_engine(speed: float, throttle: float, running: bool) -> void:
	_speed = clampf(absf(speed), 0.0, 150.0) if is_finite(speed) else 0.0
	_throttle = clampf(throttle, 0.0, 1.0) if is_finite(throttle) else 0.0
	_running = running
	var gear: int = 0
	while gear < GEAR_SPEEDS.size() and _speed >= GEAR_SPEEDS[gear]:
		gear += 1
	# A six-speed transmission gives audible RPM drops as road speed increases.
	var wheel_rpm: float = _speed * 60.0 / (TAU * 0.34)
	_rpm_target = clampf(wheel_rpm * GEAR_RATIOS[gear] * 3.4 + _throttle * 550.0, 900.0, 7400.0)


func impact() -> void:
	if not _muted:
		_impact_remaining = 0.34


func horn() -> void:
	if not _muted:
		_horn_remaining = 0.42


func set_muted(muted: bool) -> void:
	_muted = muted
	if is_instance_valid(_player):
		# Gain changes immediately, including sound already queued in the buffer.
		_player.volume_db = -80.0 if muted else -5.0
	if muted:
		_horn_remaining = 0.0
		_impact_remaining = 0.0
		_engine_gain = 0.0


func _process(_delta: float) -> void:
	_engine_target = 1.0 if _running and not get_tree().paused else 0.0
	_refill_buffer()


func _refill_buffer() -> void:
	if _playback == null:
		return
	# A hard refill budget prevents a delayed frame causing unbounded audio work.
	# Godot's Dummy audio driver also supports this path for headless validation.
	var count: int = mini(_playback.get_frames_available(), MAX_REFILL_FRAMES)
	if count <= 0:
		return
	var samples := PackedVector2Array()
	samples.resize(count)
	if not _muted and (_engine_target > 0.0 or _engine_gain > 0.0 or _horn_remaining > 0.0 or _impact_remaining > 0.0):
		for index in range(count):
			samples[index] = _synthesize_frame()
	_playback.push_buffer(samples)


func _synthesize_frame() -> Vector2:
	if _muted or (_engine_target <= 0.0 and _engine_gain <= 0.0 and _horn_remaining <= 0.0 and _impact_remaining <= 0.0):
		return Vector2.ZERO
	_rpm = lerpf(_rpm, _rpm_target, 0.0007)
	_engine_gain = move_toward(_engine_gain, _engine_target, SAMPLE_STEP * 5.0)
	_engine_phase = fmod(_engine_phase + TAU * (_rpm / 60.0 * 3.0) * SAMPLE_STEP, TAU)
	_noise_state = (_noise_state * 1664525 + 1013904223) & 0xffffffff
	var noise: float = float((_noise_state >> 8) & 0xffffff) / 8388607.5 - 1.0
	_noise_low = lerpf(_noise_low, noise, 0.065)
	var engine: float = sin(_engine_phase) + 0.40 * sin(_engine_phase * 2.0) + 0.16 * sin(_engine_phase * 3.0)
	engine = (engine * (0.045 + _throttle * 0.065) + _noise_low * _throttle * 0.025) * _engine_gain
	var road: float = (noise - _noise_low) * minf(_speed / 85.0, 1.0) * 0.016 * _engine_gain
	var effect: float = 0.0
	if _horn_remaining > 0.0:
		_horn_phase_a = fmod(_horn_phase_a + TAU * 440.0 * SAMPLE_STEP, TAU)
		_horn_phase_b = fmod(_horn_phase_b + TAU * 554.37 * SAMPLE_STEP, TAU)
		var horn_envelope: float = minf(_horn_remaining / 0.035, 1.0) * minf((0.42 - _horn_remaining) / 0.015, 1.0)
		effect += (sin(_horn_phase_a) + 0.7 * sin(_horn_phase_b)) * 0.12 * horn_envelope
		_horn_remaining = maxf(0.0, _horn_remaining - SAMPLE_STEP)
	if _impact_remaining > 0.0:
		var impact_envelope: float = _impact_remaining / 0.34
		_impact_phase = fmod(_impact_phase + TAU * (55.0 + impact_envelope * 65.0) * SAMPLE_STEP, TAU)
		effect += (sin(_impact_phase) * 0.18 + noise * 0.22) * impact_envelope * impact_envelope
		_impact_remaining = maxf(0.0, _impact_remaining - SAMPLE_STEP)
	# Modest stereo road ambience, centered engine/horn, bounded before device gain.
	return Vector2(clampf(engine + effect + road, -0.8, 0.8), clampf(engine + effect + road * 0.8, -0.8, 0.8))
