class_name DriveOverlay
extends Control
var world: Node3D
var active: bool = false
var remaining: float = 0.0
var limit: float = 1.0
var steer: float = 0.0
var health_color := Color("7bdbbb")

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE

func _draw() -> void:
	if not active or not is_instance_valid(world):
		return
	# A compact city map shows the grid, player orientation and current target.
	var map_rect := Rect2(1040, 110, 214, 200)
	draw_style_box(_panel(), map_rect)
	var center := Vector2(1147, 211)
	var map_scale: float = 0.26
	for r in [-240, -120, 0, 120, 240]:
		draw_line(center + Vector2(r * map_scale, -83), center + Vector2(r * map_scale, 83), Color("364556"), 6)
		draw_line(center + Vector2(-83, r * map_scale), center + Vector2(83, r * map_scale), Color("364556"), 6)
	var p: Vector3 = world.get_player_position()
	var goal: Vector3 = world.get_target_position()
	var dot: Vector2 = center + Vector2(p.x, p.z) * map_scale
	var target: Vector2 = center + Vector2(goal.x, goal.z) * map_scale
	draw_circle(target, 7, Color("edb96c"))
	draw_circle(target, 3, Color("162430"))
	var direction := Vector2(-sin(world.player_heading), -cos(world.player_heading))
	var side := Vector2(-direction.y, direction.x)
	draw_colored_polygon(PackedVector2Array([dot + direction * 10, dot - direction * 7 + side * 5, dot - direction * 7 - side * 5]), Color("7bdbbb"))
	# Continuous vehicle health, fuel and time bars.
	_bar(Vector2(376, 670), 155, float(world.health) / 100.0, health_color)
	_bar(Vector2(748, 670), 155, float(world.fuel) / 100.0, Color("edb96c"))
	_bar(Vector2(380, 88), 520, remaining / maxf(1.0, limit), Color("edb96c") if remaining > 15 else Color("f0776b"))
	# Steering dial moves with touch and keyboard inputs.
	var wheel := Vector2(166, 524)
	draw_arc(wheel, 43, 0, TAU, 36, Color(0.9, 0.95, 1, 0.22), 6, true)
	var a := steer * 1.7
	var right := Vector2(cos(a), sin(a)) * 37
	var down := Vector2(-sin(a), cos(a)) * 36
	draw_line(wheel - right, wheel + right, Color("9baabd"), 5, true)
	draw_line(wheel, wheel + down, Color("9baabd"), 5, true)
	draw_circle(wheel, 10, Color("192b3a"))

func _bar(origin: Vector2, width: float, ratio: float, color: Color) -> void:
	draw_style_box(_solid(Color(0.02, 0.04, 0.07, 0.75)), Rect2(origin, Vector2(width, 5)))
	draw_style_box(_solid(color), Rect2(origin, Vector2(width * clampf(ratio, 0, 1), 5)))

func _solid(color: Color) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = color
	style.set_corner_radius_all(3)
	return style

func _panel() -> StyleBoxFlat:
	var style := _solid(Color(0.045, 0.075, 0.11, 0.84))
	style.border_color = Color(1, 1, 1, 0.1)
	style.set_border_width_all(1)
	style.set_corner_radius_all(12)
	return style
