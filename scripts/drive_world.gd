class_name DriveWorld
extends Node3D
## Original arcade city. Geometry and mission roads are fictional, not navigation data.

signal hit_obstacle(damage: float)
signal near_miss
signal mission_complete

const CarFactory = preload("res://scripts/car_mesh.gd")
const ROADS: Array[float] = [-240.0, -120.0, 0.0, 120.0, 240.0]
const START := Vector3(-4.5, 0, 65)
var speed_kmh: float = 0.0
var distance_m: float = 0.0
var health: float = 100.0
var fuel: float = 100.0
var nitro: float = 100.0
var gear_label: String = "D1"
var automatic: bool = true
var reverse_selected: bool = false
var target_distance: float = 0.0
var completed_targets: int = 0
var total_targets: int = 0
var player_heading: float = 0.0
var running: bool = false
var showroom: bool = true
var player: CharacterBody3D
var camera: Camera3D
var _car: Dictionary = {}
var _level: Dictionary = {}
var _difficulty: Dictionary = {}
var _quality: int = 1
var _model: Node3D
var _show_car: Node3D
var _city: Node3D
var _show_stage: Node3D
var _target_node: Node3D
var _target_ring: MeshInstance3D
var _target_arrow: MeshInstance3D
var _target_pillar: MeshInstance3D
var _targets: Array[Vector2] = []
var _traffic: Array[Dictionary] = []
var _potholes: Array[Node3D] = []
var _materials: Dictionary = {}
var _batches: Dictionary = {}
var _speed: float = 0.0
var _steer: float = 0.0
var _throttle: float = 0.0
var _brake: float = 0.0
var _boost: bool = false
var _gear: int = 1
var _crash_timer: float = 0.0
var _sim_time: float = 0.0
var _park_time: float = 0.0
var _camera_mode: int = 0
var _last_safe := START
var _last_safe_heading: float = 0.0
var _safe_timer: float = 0.0
var _show_time: float = 0.0
var _ready_done: bool = false
var _complete_sent: bool = false
var _rng := RandomNumberGenerator.new()

func _ready() -> void:
	_build_environment()
	_build_city()
	_build_player()
	_build_showroom()
	_build_target()
	_ready_done = true
	set_quality(_quality)
	if not _car.is_empty():
		configure(_car, _level, _difficulty, _quality)
	else:
		_set_car({"style":"sedan", "color":"e0b86c", "category":"Luxury"})
	set_showroom(showroom)

func configure(car: Dictionary, level: Dictionary, difficulty: Dictionary, quality: int) -> void:
	_car = car.duplicate(true)
	_level = level.duplicate(true)
	_difficulty = difficulty.duplicate(true)
	_quality = quality
	if not _ready_done:
		return
	_set_car(_car)
	_targets.clear()
	for target in _level.get("targets", [Vector2(-4.5,-70), Vector2(-4.5,-120), Vector2(115.5,-120)]):
		if target is Vector2:
			_targets.append(target)
		elif target is Vector3:
			_targets.append(Vector2(target.x,target.z))
		elif target is Array and target.size() >= 2:
			_targets.append(Vector2(float(target[0]),float(target[1])))
	total_targets = _targets.size()
	set_quality(quality)
	_rebuild_traffic()
	_rebuild_hazards()
	reset_run()

func begin_run() -> void:
	reset_run()
	set_showroom(false)
	set_running(true)

func set_controls(steer: float, throttle: float, brake: float, boost: bool) -> void:
	_steer = clampf(steer,-1,1)
	_throttle = clampf(throttle,0,1)
	_brake = clampf(brake,0,1)
	_boost = boost

func set_running(value: bool) -> void:
	running = value
	if not value:
		set_controls(0,0,0,false)

func set_showroom(value: bool) -> void:
	showroom = value
	if not _ready_done:
		return
	_city.visible = not value
	player.visible = not value
	_target_node.visible = not value and completed_targets < total_targets
	_show_stage.visible = value
	if value:
		running = false
		camera.fov = 42
		camera.position = Vector3(9,4.7,10)
		camera.look_at(Vector3(2.8,0.7,0))
	else:
		camera.fov = 65
		_update_camera(1.0, true)

func set_quality(value: int) -> void:
	_quality = clampi(value,0,2)
	if camera:
		camera.far = [235.0,330.0,440.0][_quality]
	if get_viewport():
		get_viewport().scaling_3d_scale = [0.65,0.82,1.0][_quality]

func reset_run() -> void:
	if not player:
		return
	player.position = START
	player.rotation = Vector3.ZERO
	player.velocity = Vector3.ZERO
	player_heading = 0.0
	_speed = 0.0
	speed_kmh = 0.0
	distance_m = 0.0
	health = 100.0
	fuel = float(_level.get("fuel_budget",100.0))
	nitro = 100.0
	completed_targets = 0
	_crash_timer = 0.0
	_park_time = 0.0
	_sim_time = 0.0
	_gear = 1
	gear_label = "D1" if automatic else "1"
	reverse_selected = false
	_complete_sent = false
	_last_safe = START
	_last_safe_heading = 0.0
	_safe_timer = 0.0
	set_controls(0,0,0,false)
	for car in _traffic:
		car["node"].position = car["start"]
		car["blocked"] = false
		car["passed"] = false
		car["check_timer"] = 0.0
	for hole in _potholes:
		hole.set_meta("hit",false)
	_update_target()
	if not showroom:
		_update_camera(1.0,true)

func cycle_camera() -> void:
	_camera_mode = (_camera_mode + 1) % 3
	_update_camera(1.0,true)

func toggle_reverse() -> void:
	if speed_kmh < 10:
		reverse_selected = not reverse_selected
		_speed = 0.0
		gear_label = "R" if reverse_selected else ("D1" if automatic else str(_gear))

func toggle_transmission() -> void:
	automatic = not automatic
	_gear = clampi(int(speed_kmh / 42) + 1,1,6)
	gear_label = "R" if reverse_selected else ("D%d" % _gear if automatic else str(_gear))

func shift_gear(delta: int) -> void:
	if not automatic and not reverse_selected:
		_gear = clampi(_gear + delta,1,6)
		gear_label = str(_gear)

func get_player_position() -> Vector3:
	return player.position if player else START

func get_target_position() -> Vector3:
	if completed_targets < _targets.size():
		var p: Vector2 = _targets[completed_targets]
		return Vector3(p.x,0,p.y)
	return get_player_position()

func reset_car() -> void:
	if not running:
		return
	player.position = _last_safe
	player.rotation.y = _last_safe_heading
	player_heading = _last_safe_heading
	_speed = 0.0
	speed_kmh = 0.0
	health = maxf(0.0,health-5.0)
	_crash_timer = 1.5
	hit_obstacle.emit(5.0)
	_update_camera(1.0,true)

func _physics_process(delta: float) -> void:
	if not _ready_done:
		return
	if showroom:
		_show_time += delta
		if _show_car:
			_show_car.rotation.y = -0.55 + sin(_show_time*0.18)*0.20
		return
	if not running:
		return
	_sim_time += delta
	_crash_timer = maxf(0,_crash_timer-delta)
	var throttle: float = _throttle if fuel > 0 and health > 0 else 0.0
	var boost_active: bool = _boost and nitro>0 and throttle>0 and not reverse_selected
	var top_speed: float = float(_car.get("top_speed",190))/3.6
	if reverse_selected:
		top_speed = 9.0
	elif not automatic:
		top_speed = minf(top_speed,15.0*_gear)
	if boost_active:
		top_speed *= 1.12
	var acceleration: float = float(_car.get("accel",8.0))*0.62
	var braking: float = float(_car.get("brake",18.0))
	var current_speed: float = absf(_speed)
	var on_road: bool = _is_on_road(player.position,11.5)
	if not on_road:
		top_speed = minf(top_speed,9.0)
		acceleration *= 0.45
	if boost_active:
		acceleration *= 1.45
		nitro = maxf(0,nitro-delta*19)
	else:
		nitro = minf(100,nitro+delta*2.8)
	if _brake>0:
		current_speed = move_toward(current_speed,0,braking*_brake*delta)
	elif throttle>0:
		current_speed = move_toward(current_speed,top_speed,acceleration*throttle*delta)
	else:
		current_speed = move_toward(current_speed,0,delta*(0.65 + current_speed*0.019))
	if current_speed>top_speed:
		current_speed = move_toward(current_speed,top_speed,braking*0.35*delta)
	_speed = current_speed * (-1.0 if reverse_selected else 1.0)
	var turn_rate: float = minf(current_speed*0.19,1.9)/(1.0+current_speed*0.023)
	player_heading -= _steer*turn_rate*float(_car.get("handling",1.0))*delta*(-1.0 if reverse_selected else 1.0)
	player.rotation.y = player_heading
	var forward := Vector3(-sin(player_heading),0,-cos(player_heading))
	var before: Vector3 = player.position
	player.velocity = forward*_speed
	player.move_and_slide()
	player.position.y = 0
	distance_m += before.distance_to(player.position)
	if player.get_slide_collision_count()>0 and current_speed>0.5:
		_damage(clampf(current_speed*1.35,3,38))
		_speed *= 0.12
		# Back the car away very slightly so steering/reverse can free it.
		player.position -= forward*0.05
	player.position.x = clampf(player.position.x,-316,316)
	player.position.z = clampf(player.position.z,-316,316)
	speed_kmh = absf(_speed)*3.6
	if automatic and not reverse_selected:
		_gear = clampi(int(speed_kmh/40)+1,1,6)
	gear_label = "R" if reverse_selected else ("D%d" % _gear if automatic else str(_gear))
	fuel = maxf(0,fuel-delta*(0.035+throttle*0.12+current_speed*0.0025))
	_update_traffic(delta)
	_update_hazards()
	_update_mission(delta)
	_safe_timer += delta
	if _safe_timer>3.0 and on_road and _crash_timer<=0 and current_speed<17:
		_last_safe = player.position
		_last_safe_heading = player_heading
		_safe_timer = 0.0
	if _model:
		_model.rotation.z = lerpf(_model.rotation.z,-_steer*minf(current_speed,15)*0.0025,delta*7)
	_update_camera(delta,false)

func _damage(amount: float) -> void:
	if _crash_timer>0:
		return
	var applied: float = amount*float(_difficulty.get("damage",1.0))
	health = maxf(0,health-applied)
	_crash_timer = 1.1
	hit_obstacle.emit(applied)

func _update_mission(delta: float) -> void:
	if completed_targets>=total_targets or _complete_sent:
		return
	var target: Vector3 = get_target_position()
	target_distance = player.position.distance_to(target)
	var final_parking: bool = str(_level.get("type","checkpoints"))=="parking" and completed_targets==total_targets-1
	var reached: bool = target_distance<9.0
	if final_parking:
		var wanted: float = deg_to_rad(float(_level.get("target_heading",0.0)))
		var angle: float = absf(wrapf(player_heading-wanted,-PI,PI))
		reached = target_distance<3.0 and speed_kmh<3.0 and angle<deg_to_rad(25)
		_park_time = _park_time+delta if reached else 0.0
		reached = _park_time>=0.75
	if reached:
		completed_targets += 1
		_park_time = 0.0
		nitro = minf(100,nitro+15)
		_update_target()
		if completed_targets>=total_targets:
			_complete_sent = true
			running = false
			mission_complete.emit()
	if _target_arrow:
		_target_arrow.position.y = 5.0+sin(_sim_time*3)*0.30
		_target_arrow.rotation.y += delta

func _update_target() -> void:
	if not _target_node:
		return
	_target_node.visible = not showroom and completed_targets<total_targets
	if completed_targets>=total_targets:
		target_distance = 0.0
		return
	_target_node.position = get_target_position()
	target_distance = player.position.distance_to(get_target_position())
	var parking: bool = str(_level.get("type","checkpoints"))=="parking" and completed_targets==total_targets-1
	_target_node.rotation.y = deg_to_rad(float(_level.get("target_heading",0))) if parking else 0.0
	_target_ring.scale = Vector3(0.42,1,0.62) if parking else Vector3.ONE
	_target_pillar.visible = not parking

func _update_camera(delta: float, snap: bool) -> void:
	if not camera or not player:
		return
	var direction := Vector3(-sin(player_heading),0,-cos(player_heading))
	var desired: Vector3
	var look: Vector3
	match _camera_mode:
		1:
			desired = player.position+Vector3(0,1.32,0)+direction*0.55
			look = player.position+direction*35+Vector3(0,0.9,0)
		2:
			desired = player.position-direction*3+Vector3(0,28,0)
			look = player.position+direction*6
		_:
			desired = player.position-direction*(9.5+minf(speed_kmh,130)*0.018)+Vector3(0,4.5,0)
			look = player.position+direction*5.5+Vector3(0,1.0,0)
	camera.position = desired if snap else camera.position.lerp(desired,minf(1,delta*(10.0 if _camera_mode==1 else 6.0)))
	camera.look_at(look)

func _is_on_road(point: Vector3, half_width: float = 10.0) -> bool:
	for road in ROADS:
		if absf(point.x-road)<half_width or absf(point.z-road)<half_width:
			return true
	return false

func _set_car(car: Dictionary) -> void:
	if _model:
		_model.free()
	if _show_car:
		_show_car.free()
	_model = _make_car(car,true)
	player.add_child(_model)
	_show_car = _make_car(car,true)
	_show_stage.add_child(_show_car)
	_show_car.rotation.y = -0.55

func _make_car(car: Dictionary, detailed: bool) -> Node3D:
	# Licensed import helper is optional; procedural original cars remain a fallback.
	if detailed and ResourceLoader.exists("res://scripts/model_car.gd"):
		var helper: Script = load("res://scripts/model_car.gd")
		if helper and helper.has_method("build"):
			var result: Variant = helper.call("build",car,detailed)
			if result is Node3D:
				return result
	return CarFactory.build(car,detailed)

func _build_environment() -> void:
	var world_environment := WorldEnvironment.new()
	var environment := Environment.new()
	environment.background_mode = Environment.BG_SKY
	var sky := Sky.new()
	var sky_material := ProceduralSkyMaterial.new()
	sky_material.sky_top_color = Color("719ab1")
	sky_material.sky_horizon_color = Color("f3ceaa")
	sky_material.ground_bottom_color = Color("747f73")
	sky_material.ground_horizon_color = Color("e7c0a0")
	sky.sky_material = sky_material
	environment.sky = sky
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color("c2d0da")
	environment.ambient_light_energy = 0.72
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	environment.fog_enabled = true
	environment.fog_light_color = Color("d7c8b6")
	environment.fog_density = 0.0019
	world_environment.environment = environment
	add_child(world_environment)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-38,-35,0)
	sun.light_color = Color("ffe3bd")
	sun.light_energy = 1.6
	sun.shadow_enabled = false
	add_child(sun)
	camera = Camera3D.new()
	camera.near = 0.12
	camera.current = true
	add_child(camera)

func _mat(color: String, unshaded: bool = false) -> StandardMaterial3D:
	var key: String = color+str(unshaded)
	if _materials.has(key):
		return _materials[key]
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(color)
	material.roughness = 0.85
	if unshaded:
		material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_materials[key] = material
	return material

func _box(parent: Node3D, size: Vector3, pos: Vector3, color: String, collision: bool = false) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	var item := MeshInstance3D.new()
	item.mesh = mesh
	item.material_override = _mat(color)
	item.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	item.position = pos
	parent.add_child(item)
	if collision:
		_static_collider(parent,size,pos)
	return item

func _static_collider(parent: Node3D, size: Vector3, pos: Vector3) -> void:
	var body := StaticBody3D.new()
	var shape := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = size
	shape.shape = box
	body.position = pos
	body.add_child(shape)
	parent.add_child(body)

func _batch_box(key: String, size: Vector3, pos: Vector3, color: Color, rotation_y: float = 0.0) -> void:
	if not _batches.has(key):
		_batches[key] = []
	var transform := Transform3D(Basis(Vector3.UP,rotation_y).scaled(size),pos)
	_batches[key].append([transform,color])

func _finish_batches() -> void:
	for key in _batches:
		var entries: Array = _batches[key]
		var multi := MultiMesh.new()
		multi.transform_format = MultiMesh.TRANSFORM_3D
		multi.use_colors = true
		var cube := BoxMesh.new()
		cube.size = Vector3.ONE
		multi.mesh = cube
		multi.instance_count = entries.size()
		for i in range(entries.size()):
			multi.set_instance_transform(i,entries[i][0])
			multi.set_instance_color(i,entries[i][1])
		var instance := MultiMeshInstance3D.new()
		instance.multimesh = multi
		var material := StandardMaterial3D.new()
		material.vertex_color_use_as_albedo = true
		material.roughness = 0.88
		instance.material_override = material
		instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		_city.add_child(instance)
	_batches.clear()

func _build_city() -> void:
	_city = Node3D.new()
	_city.name = "City"
	add_child(_city)
	_box(_city,Vector3(1500,0.1,1500),Vector3(0,-0.12,0),"8d9470")
	for road in ROADS:
		_box(_city,Vector3(20,0.10,640),Vector3(road,-0.015,0),"414952")
		_box(_city,Vector3(640,0.10,20),Vector3(0,-0.01,road),"414952")
		for side in [-1.0,1.0]:
			_box(_city,Vector3(2,0.12,640),Vector3(road+side*11,0.005,0),"c0b6a4")
			_box(_city,Vector3(640,0.12,2),Vector3(0,0.008,road+side*11),"c0b6a4")
		for dash in range(-310,311,12):
			if _near_junction(float(dash),14):
				continue
			_batch_box("markings",Vector3(0.16,0.015,5),Vector3(road,0.055,dash),Color("efd691"))
			_batch_box("markings",Vector3(5,0.015,0.16),Vector3(dash,0.06,road),Color("efd691"))
			for side in [-1.0,1.0]:
				_batch_box("markings",Vector3(0.12,0.015,10),Vector3(road+side*8.6,0.06,dash),Color("d8d7cb"))
				_batch_box("markings",Vector3(10,0.015,0.12),Vector3(dash,0.065,road+side*8.6),Color("d8d7cb"))
	# Junction markings and traffic lights. No raised kerb across an intersection.
	for x in ROADS:
		for z in ROADS:
			_box(_city,Vector3(24,0.13,24),Vector3(x,0.015,z),"414952")
			for offset in [-7,-4,-1,2,5]:
				for sign_value in [-1.0,1.0]:
					_batch_box("crosswalks",Vector3(1.2,0.02,2.6),Vector3(x+offset,0.09,z+sign_value*8),Color("ebe4cf"))
					_batch_box("crosswalks",Vector3(2.6,0.02,1.2),Vector3(x+sign_value*8,0.09,z+offset),Color("ebe4cf"))
			for side in [-1.0,1.0]:
				_batch_box("poles",Vector3(0.16,4.5,0.16),Vector3(x+side*12,2.25,z-12),Color("35424a"))
				_batch_box("poles",Vector3(0.48,1.15,0.42),Vector3(x+side*12,4.0,z-12),Color("29353d"))
				_batch_box("signals",Vector3(0.22,0.22,0.44),Vector3(x+side*12,4.26,z-12),Color("e37e4f"))
	var city_rng := RandomNumberGenerator.new()
	city_rng.seed = 8831
	var colors: Array[Color] = [Color("cfba9e"),Color("aeb7b2"),Color("c4a27d"),Color("d4c7b1"),Color("b8b5a5")]
	for block_x in [-180.0,-60.0,60.0,180.0]:
		for block_z in [-180.0,-60.0,60.0,180.0]:
			for sx in [-1.0,1.0]:
				for sz in [-1.0,1.0]:
					var width: float = city_rng.randf_range(25,34)
					var depth: float = city_rng.randf_range(24,35)
					var height: float = city_rng.randf_range(10,37)
					var center := Vector3(block_x+sx*24,height*0.5,block_z+sz*24)
					_batch_box("buildings",Vector3(width,height,depth),center,colors[city_rng.randi_range(0,colors.size()-1)])
					_static_collider(_city,Vector3(width,height,depth),center)
					_batch_box("roofs",Vector3(width+0.5,0.55,depth+0.5),Vector3(center.x,height,center.z),Color("9f988c"))
					for floor_index in range(1,int(height/3.6)):
						for col in [-0.3,0.0,0.3]:
							for face in [-1.0,1.0]:
								_batch_box("windows",Vector3(width*0.19,1.55,0.08),Vector3(center.x+col*width,floor_index*3.6,center.z+face*(depth*0.5+0.06)),Color("526c76"))
								_batch_box("windows",Vector3(0.08,1.55,depth*0.19),Vector3(center.x+face*(width*0.5+0.06),floor_index*3.6,center.z+col*depth),Color("526c76"))
	# Repeated trees and street lamps are instanced in only a handful of draws.
	for road in ROADS:
		for coordinate in range(-295,296,30):
			if _near_junction(float(coordinate),18):
				continue
			for side in [-1.0,1.0]:
				for transpose in [false,true]:
					var p := Vector3(road+side*14,0,coordinate)
					if transpose:
						p = Vector3(coordinate,0,road+side*14)
					_batch_box("trees",Vector3(0.32,2.6,0.32),p+Vector3(0,1.3,0),Color("716044"))
					_batch_box("trees",Vector3(3.3,3.8,3.3),p+Vector3(0,3.8,0),Color("607d4e"),0.4)
					p += Vector3(0,0,6) if not transpose else Vector3(6,0,0)
					_batch_box("lamps",Vector3(0.13,6.2,0.13),p+Vector3(0,3.1,0),Color("4d5b5c"))
					_batch_box("lamps",Vector3(1.1,0.18,0.5),p+Vector3(0,6.2,0),Color("ece4bf"))
	for side in [-1.0,1.0]:
		_box(_city,Vector3(1,1.2,640),Vector3(side*320,0.6,0),"b3a89b",true)
		_box(_city,Vector3(640,1.2,1),Vector3(0,0.6,side*320),"b3a89b",true)
	_finish_batches()
	_add_sign(Vector3(-15,3.4,42),"CAPITAL DISTRICT",0)
	_add_sign(Vector3(109,3.4,-18),"MARKET ROAD",PI*0.5)

func _near_junction(coordinate: float, margin: float) -> bool:
	for road in ROADS:
		if absf(coordinate-road)<margin:
			return true
	return false

func _add_sign(pos: Vector3, text: String, yaw: float) -> void:
	var sign_root := Node3D.new()
	_city.add_child(sign_root)
	sign_root.position = pos
	sign_root.rotation.y = yaw
	_box(sign_root,Vector3(5.8,1.1,0.14),Vector3.ZERO,"245c58")
	var label := Label3D.new()
	label.text = text
	label.font_size = 48
	label.pixel_size = 0.012
	label.position.z = 0.08
	label.no_depth_test = false
	label.outline_size = 0
	sign_root.add_child(label)

func _build_player() -> void:
	player = CharacterBody3D.new()
	player.name = "PlayerCar"
	player.collision_layer = 2
	player.collision_mask = 1|4
	player.motion_mode = CharacterBody3D.MOTION_MODE_FLOATING
	var collider := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = Vector3(1.75,1.15,4.1)
	collider.shape = shape
	collider.position.y = 0.75
	player.add_child(collider)
	add_child(player)
	player.position = START

func _build_showroom() -> void:
	_show_stage = Node3D.new()
	_show_stage.name = "Garage"
	add_child(_show_stage)
	_box(_show_stage,Vector3(1000,0.1,1000),Vector3(0,-0.12,0),"33444c")
	var mesh := CylinderMesh.new()
	mesh.top_radius = 5
	mesh.bottom_radius = 5
	mesh.height = 0.12
	mesh.radial_segments = 64
	var pedestal := MeshInstance3D.new()
	pedestal.mesh = mesh
	pedestal.material_override = _mat("69716d")
	pedestal.position.y = -0.035
	_show_stage.add_child(pedestal)
	for i in range(14):
		var height: float = 6.0+float(i%5)*3.3
		_box(_show_stage,Vector3(4,height,4),Vector3(-34+i*5,height*0.5,-30-float(i%3)*6),"7f8b89")

func _build_target() -> void:
	_target_node = Node3D.new()
	_target_node.name = "MissionTarget"
	add_child(_target_node)
	var ring := TorusMesh.new()
	ring.inner_radius = 5.6
	ring.outer_radius = 6.0
	ring.rings = 32
	ring.ring_segments = 8
	_target_ring = MeshInstance3D.new()
	_target_ring.mesh = ring
	_target_ring.material_override = _mat("81e2c2",true)
	_target_ring.position.y = 0.18
	_target_node.add_child(_target_ring)
	var marker := CylinderMesh.new()
	marker.top_radius = 0
	marker.bottom_radius = 0.9
	marker.height = 1.5
	marker.radial_segments = 4
	_target_arrow = MeshInstance3D.new()
	_target_arrow.mesh = marker
	_target_arrow.material_override = _mat("f4cf73",true)
	_target_arrow.position.y = 5
	_target_arrow.rotation.z = PI
	_target_node.add_child(_target_arrow)
	_target_pillar = _box(_target_node,Vector3(0.3,16,0.3),Vector3(0,8,0),"81e2c2")
	# Thin lines make a final parking bay and its required heading readable.
	for side in [-1.0,1.0]:
		_box(_target_node,Vector3(0.12,0.03,6),Vector3(side*1.7,0.13,0),"f4cf73")
	_box(_target_node,Vector3(3.5,0.03,0.12),Vector3(0,0.13,-3),"f4cf73")

func _rebuild_traffic() -> void:
	for car in _traffic:
		car["node"].free()
	_traffic.clear()
	_rng.seed = 1039+int(_level.get("id",1))*71
	var count: int = clampi(roundi(float(_level.get("traffic",6))*float(_difficulty.get("traffic",1))),0,22)
	var colors: Array[String] = ["c9bfa8","4b757d","b65d4e","7a8c68","e6d9ba"]
	for i in range(count):
		var body := CharacterBody3D.new()
		body.collision_layer = 4
		body.collision_mask = 1|2|4
		body.motion_mode = CharacterBody3D.MOTION_MODE_FLOATING
		var collider := CollisionShape3D.new()
		var shape := BoxShape3D.new()
		shape.size = Vector3(1.7,1.2,4.0)
		collider.shape = shape
		collider.position.y = 0.8
		body.add_child(collider)
		body.add_child(CarFactory.build({"style":"suv" if i%5==0 else "sedan","color":colors[i%colors.size()]},false))
		_city.add_child(body)
		var vertical: bool = i%2==0
		var direction: float = -1.0 if i%4<2 else 1.0
		var road: float = ROADS[(i/2)%ROADS.size()]
		var lane: float = road+direction*4.5 if vertical else road-direction*4.5
		var along: float = -275.0+float(i)*29.7
		var start: Vector3 = Vector3(lane,0,along) if vertical else Vector3(along,0,lane)
		if start.distance_to(START)<24:
			start.z -= 50
		body.position = start
		body.rotation.y = 0.0 if vertical and direction<0 else (PI if vertical else (PI*0.5 if direction<0 else -PI*0.5))
		_traffic.append({"node":body,"start":start,"vertical":vertical,"direction":direction,"speed":_rng.randf_range(5.5,9.5),"blocked":false,"passed":false,"check_timer":0.0})

func _update_traffic(delta: float) -> void:
	for data in _traffic:
		var body: CharacterBody3D = data["node"]
		var vertical: bool = data["vertical"]
		var direction: float = data["direction"]
		var forward := Vector3(0,0,direction) if vertical else Vector3(direction,0,0)
		data["check_timer"] = float(data["check_timer"])-delta
		if float(data["check_timer"])<=0:
			data["check_timer"] = 0.18
			var query := PhysicsRayQueryParameters3D.create(body.position+Vector3(0,0.8,0)+forward*2.4,body.position+Vector3(0,0.8,0)+forward*10.0,1|2|4,[body.get_rid()])
			data["blocked"] = not get_world_3d().direct_space_state.intersect_ray(query).is_empty()
		var coordinate: float = body.position.z if vertical else body.position.x
		var red: bool = (int(_sim_time/9.0)%2==0) != vertical
		var stop_light: bool = false
		if red:
			for road in ROADS:
				var ahead: float = (road-coordinate)*direction
				if ahead>11 and ahead<19:
					stop_light = true
		if not data["blocked"] and not stop_light:
			body.velocity = forward*float(data["speed"])
			body.move_and_slide()
		if absf(coordinate)>302:
			if vertical:
				body.position.z = -coordinate*0.98
			else:
				body.position.x = -coordinate*0.98
			data["passed"] = false
		var separation: float = body.position.distance_to(player.position)
		if separation<4.5 and separation>2.2 and speed_kmh>48 and not data["passed"]:
			data["passed"] = true
			near_miss.emit()
			nitro = minf(100,nitro+5)

func _rebuild_hazards() -> void:
	for hole in _potholes:
		hole.free()
	_potholes.clear()
	var count: int = clampi(int(_level.get("hazards",3)),0,16)
	for i in range(count):
		var road: float = ROADS[i%ROADS.size()]
		var along: float = _rng.randf_range(-270,270)
		if _near_junction(along,17):
			along += 24
		var p := Vector3(road+(-4.5 if i%2==0 else 4.5),0.08,along)
		if i%3==0:
			p = Vector3(along,0.08,road+4.5)
		var hole := Node3D.new()
		hole.position = p
		hole.set_meta("hit",false)
		var mesh := CylinderMesh.new()
		mesh.top_radius = 1.05
		mesh.bottom_radius = 1.15
		mesh.height = 0.025
		mesh.radial_segments = 12
		var patch := MeshInstance3D.new()
		patch.mesh = mesh
		patch.material_override = _mat("292e30")
		hole.add_child(patch)
		_city.add_child(hole)
		_potholes.append(hole)

func _update_hazards() -> void:
	for hole in _potholes:
		var distance: float = player.position.distance_to(hole.position)
		if distance<1.65 and not hole.get_meta("hit",false):
			hole.set_meta("hit",true)
			if speed_kmh>15:
				_damage(3.0+speed_kmh*0.045)
				_speed *= 0.82
		elif distance>12:
			hole.set_meta("hit",false)
