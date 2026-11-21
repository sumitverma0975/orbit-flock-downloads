extends SceneTree
const World = preload("res://scripts/drive_world.gd")
const Data = preload("res://scripts/catalog.gd")
var checks: int = 0
var failures: int = 0
func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error(message)
func frames(count: int) -> void:
	for i in range(count):
		await physics_frame
func _initialize() -> void:
	call_deferred("run")
func run() -> void:
	var world = World.new()
	root.add_child(world)
	await frames(2)
	var level: Dictionary = {"id":1,"type":"checkpoints","targets":[Vector2(-4.5,-220)],"traffic":0,"hazards":0,"fuel_budget":32.0}
	world.configure(Data.CARS[0],level,Data.DIFFICULTIES[0],0)
	check(is_equal_approx(world.fuel,32.0),"Fuel mission must apply budget")
	check(world.total_targets==1,"Mission targets parsed")
	check(world._model.name=="LicensedCarModel","BMW must use licensed mesh factory")
	var initial_fuel: float = world.fuel
	await frames(30)
	check(is_equal_approx(world.fuel,initial_fuel),"No fuel consumption in garage")
	world.begin_run()
	world.set_controls(0,1,0,false)
	await frames(120)
	check(world.speed_kmh>20,"Accelerator moves car")
	check(world.player.position.z<60,"Car moves toward -Z")
	check(world.distance_m>5,"Distance tracks actual movement")
	check(world.fuel<initial_fuel,"Driving burns fuel")
	world.set_controls(0,0,1,false)
	await frames(100)
	check(world.speed_kmh<0.1,"Brake stops the car")
	var stopped_z: float = world.player.position.z
	world.toggle_reverse()
	world.set_controls(0,1,0,false)
	await frames(60)
	check(world.player.position.z>stopped_z,"Reverse moves toward +Z")
	check(world.gear_label=="R","Reverse gear label")
	world.set_running(false)
	var paused_position: Vector3 = world.player.position
	var paused_fuel: float = world.fuel
	await frames(30)
	check(world.player.position.is_equal_approx(paused_position),"Pause freezes vehicle")
	check(is_equal_approx(world.fuel,paused_fuel),"Pause freezes fuel")
	world.reset_run()
	world.set_running(true)
	world.toggle_transmission()
	check(not world.automatic,"Manual mode toggle")
	world.shift_gear(2)
	check(world.gear_label=="3","Manual shifting")
	world.set_controls(1,1,0,false)
	await frames(80)
	check(world.player_heading<0,"Right steering turns yaw clockwise")
	world.set_running(false)
	# Verify mission success semantics without a long drive.
	var parking: Dictionary = {"id":1,"type":"parking","targets":[Vector2(-4.5,65)],"target_heading":0.0,"traffic":0,"hazards":0}
	world.configure(Data.CARS[0],parking,Data.DIFFICULTIES[0],0)
	world.begin_run()
	world.player_heading = PI/2
	world.player.rotation.y = PI/2
	await frames(60)
	check(world.completed_targets==0,"Parking rejects wrong heading")
	world.player_heading = 0
	world.player.rotation.y = 0
	await frames(60)
	check(world.completed_targets==1 and not world.running,"Aligned stopped parking completes once")
	# Drive the actual first mission with accelerator/brake, no teleporting. The
	# easy intro parking bay accepts the initial lane, then demands a full stop.
	world.configure(Data.CARS[0],Data.LEVELS[0],Data.DIFFICULTIES[0],0)
	world.begin_run()
	var elapsed_frames: int = 0
	while world.running and elapsed_frames<3000:
		var target_z: float = world.get_target_position().z
		var remaining: float = world.player.position.z-target_z
		var parking_final: bool = world.completed_targets==world.total_targets-1
		var desired: float = 11.0
		if parking_final:
			desired = minf(11.0,sqrt(maxf(0.0,remaining-0.15)*2.0*9.0))
		var velocity_mps: float = world.speed_kmh/3.6
		if remaining<0.3 and parking_final:
			world.set_controls(0,0,1,false)
		elif velocity_mps>desired:
			world.set_controls(0,0,0.65,false)
		else:
			world.set_controls(0,0.65,0,false)
		await frames(1)
		elapsed_frames += 1
	check(world.completed_targets==world.total_targets,"First parking challenge is drivable without teleporting")
	check(elapsed_frames/60.0<float(Data.LEVELS[0]["time"])*float(Data.DIFFICULTIES[0]["time"]),"Intro challenge finishes inside its timer")
	print("INTRO_PARKING elapsed=%.2fs health=%.1f speed=%.2f position=%s" % [elapsed_frames/60.0,world.health,world.speed_kmh,str(world.player.position)])
	# Heavy traffic creation and all physics paths must run without runtime errors.
	world.configure(Data.CARS[6],Data.LEVELS[-1],Data.DIFFICULTIES[-1],1)
	world.begin_run()
	await frames(180)
	check(world._traffic.size()>0 and world._traffic.size()<=22,"Traffic is bounded")
	world.set_running(false)
	world.queue_free()
	await process_frame
	print("WORLD_SMOKE checks=%d failures=%d" % [checks,failures])
	quit(1 if failures else 0)
