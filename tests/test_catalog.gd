extends SceneTree
## Run: godot --headless --path . --script tests/test_catalog.gd

const Catalog = preload("res://scripts/catalog.gd")

var checks: int = 0
var failures: int = 0


func verify(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error(message)


func on_road(point: Vector2) -> bool:
	if absf(point.x) > 320.0 or absf(point.y) > 320.0:
		return false
	for center in Catalog.ROAD_CENTERS:
		if absf(point.x - center) <= 10.0 or absf(point.y - center) <= 10.0:
			return true
	return false


func route_length(targets: Array) -> float:
	var previous: Vector2 = Catalog.START_POSITION
	var distance: float = 0.0
	for target in targets:
		distance += previous.distance_to(target)
		previous = target
	return distance


func _initialize() -> void:
	verify(Catalog.CARS.size() == 12, "Expected twelve cars")
	verify(Catalog.LEVELS.size() == 12, "Expected twelve missions")
	verify(Catalog.DIFFICULTIES.size() == 4, "Expected four difficulty modes")
	var luxury: int = 0
	var supercars: int = 0
	var free_luxury: bool = false
	var free_supercar: bool = false
	var ids: Dictionary = {}
	var styles: Dictionary = {}
	for index in Catalog.CARS.size():
		var car: Dictionary = Catalog.CARS[index]
		verify(not ids.has(car.id), "Duplicate car id: " + str(car.id))
		verify(not styles.has(car.style), "Duplicate silhouette: " + str(car.style))
		ids[car.id] = true
		styles[car.style] = true
		verify(float(car.accel) > 0.0 and float(car.brake) > 0.0, "Positive vehicle accelerations")
		verify(Color.html_is_valid(str(car.color)), "Valid vehicle paint color")
		verify(Catalog.car_unlocked(index, int(car.unlock)), "Unlock boundary inclusive")
		verify(not Catalog.car_unlocked(index, int(car.unlock) - 1), "Unlock cannot bypass threshold")
		if car.category == "Luxury":
			luxury += 1
			free_luxury = free_luxury or int(car.unlock) == 0
		if car.category == "Supercar":
			supercars += 1
			free_supercar = free_supercar or int(car.unlock) == 0
	verify(luxury == 6 and supercars == 6, "Six luxury cars and six supercars")
	verify(free_luxury and free_supercar, "Both car classes accessible on first launch")
	verify(not Catalog.car_unlocked(-1, 999999), "Reject negative car indices")
	verify(not Catalog.car_unlocked(12, 999999), "Reject out of range car indices")
	verify(Catalog.level_unlocked(0, 0), "First level unlocked")
	verify(not Catalog.level_unlocked(1, 0), "Next level locked until first completion")
	verify(Catalog.level_unlocked(1, 1), "First completion unlocks second level")
	verify(not Catalog.level_unlocked(-1, 12), "Reject negative mission indices")
	verify(not Catalog.level_unlocked(12, 12), "Reject out of range mission indices")
	var previous_reward: int = 0
	var modes: Dictionary = {}
	for level_index in Catalog.LEVELS.size():
		var level: Dictionary = Catalog.LEVELS[level_index]
		verify(int(level.id) == level_index + 1, "Contiguous mission identifiers")
		verify(int(level.reward) > previous_reward, "Mission reward increases")
		verify(int(level.traffic) <= 12 and int(level.hazards) <= 8, "Bounded entity budgets")
		verify(level.type in ["parking", "checkpoints", "precision", "fuel"], "Recognized mission mode")
		verify(level.targets.size() >= 2 and level.targets.size() <= 5, "Two to five ordered targets")
		modes[level.type] = true
		previous_reward = int(level.reward)
		var measured: float = route_length(level.targets)
		verify(absf(measured - float(level.distance)) < 0.15, "HUD route distance matches actual target route")
		var previous: Vector2 = Catalog.START_POSITION
		for target in level.targets:
			verify(target is Vector2, "Targets use world x/z Vector2 coordinates")
			verify(previous.distance_to(target) > 15.0, "Distinct targets cannot overlap collection radii")
			var steps: int = ceili(previous.distance_to(target))
			for step in steps + 1:
				verify(on_road(previous.lerp(target, float(step) / maxf(1.0, float(steps)))), "Route stays on road through intersections: " + str(level.name))
			previous = target
		# At a modest 40 km/h average, allow six seconds per turn and five
		# extra seconds for a parking maneuver. Even Extreme is feasible
		# on this model; real traffic and handling still need playtesting.
		var reference_time: float = measured / (40.0 / 3.6) + 6.0 * float(level.targets.size() - 1)
		if level.type == "parking":
			reference_time += 5.0
			var delta: Vector2 = level.targets[-1] - level.targets[-2]
			var approach_heading: float = rad_to_deg(atan2(-delta.x, -delta.y))
			verify(absf(wrapf(approach_heading - float(level.target_heading), -180.0, 180.0)) < 25.0, "Parking heading matches approach")
		if level.type == "precision":
			verify(float(level.min_health) >= 70.0 and float(level.min_health) <= 90.0, "Precision mode requires careful driving")
		if level.type == "fuel":
			verify(float(level.fuel_budget) > 0.0 and float(level.fuel_budget) < 50.0, "Fuel mode starts with reduced tank")
		for difficulty in Catalog.DIFFICULTIES:
			var limit: float = float(level.time) * float(difficulty.time)
			verify(reference_time < limit, "City driving time feasible on " + str(difficulty.name) + ": " + str(level.name))
		for difficulty_index in Catalog.DIFFICULTIES.size():
			var one: int = Catalog.reward_for_run(level_index, difficulty_index, 1)
			var two: int = Catalog.reward_for_run(level_index, difficulty_index, 2)
			var three: int = Catalog.reward_for_run(level_index, difficulty_index, 3)
			verify(one > 0 and two > one and three > two, "Stars increase reward")
	verify(modes.size() == 4, "All four mission types are represented")
	verify(Catalog.stars_for_run(20.0, 100.0, 80.0) == 3, "Three star threshold")
	verify(Catalog.stars_for_run(8.0, 100.0, 55.0) == 2, "Two star threshold")
	verify(Catalog.stars_for_run(40.0, 100.0, 54.0) == 1, "Damage limits star rating")
	verify(Catalog.stars_for_run(0.0, 0.0, 0.0) == 1, "Invalid timer cannot divide by zero")
	verify(Catalog.reward_for_run(-1, 0, 3) == 0, "Invalid level has no reward")
	verify(Catalog.reward_for_run(0, 4, 3) == 0, "Invalid difficulty has no reward")
	print("Catalog validation: %d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)
