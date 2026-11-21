class_name DriveData
extends RefCounted
## Licensed branded car art plus original fictional cars and arcade missions.
## Route names are inspired by Delhi,
## not surveyed road data. Speeds are game balance values, not real specifications.

const CARS: Array[Dictionary] = [
	{"id": "lux_sedan", "name": "BMW 320i E90", "model": "bmw_320i", "category": "Luxury", "color": "e0b86c", "style": "sedan", "top_speed": 188.0, "accel": 7.8, "handling": 1.02, "brake": 17.0, "unlock": 0},
	{"id": "lux_suv", "name": "Mercedes-Benz G-Class", "model": "mercedes_gclass", "category": "Luxury", "color": "154c48", "style": "suv", "top_speed": 196.0, "accel": 7.1, "handling": 0.92, "brake": 17.8, "unlock": 1000},
	{"id": "lux_coupe", "name": "Sultan Coupe", "category": "Luxury", "color": "f2e3cd", "style": "coupe", "top_speed": 238.0, "accel": 9.2, "handling": 1.12, "brake": 19.2, "unlock": 2200},
	{"id": "lux_limo", "name": "Shahi Limousine", "category": "Luxury", "color": "211e30", "style": "limo", "top_speed": 174.0, "accel": 6.4, "handling": 0.78, "brake": 15.5, "unlock": 3400},
	{"id": "lux_wagon", "name": "Mehr Estate", "category": "Luxury", "color": "457f99", "style": "wagon", "top_speed": 218.0, "accel": 8.4, "handling": 1.05, "brake": 18.5, "unlock": 4800},
	{"id": "lux_gt", "name": "Kohinoor Grand Tourer", "category": "Luxury", "color": "8e2940", "style": "grandtourer", "top_speed": 278.0, "accel": 10.6, "handling": 1.13, "brake": 20.2, "unlock": 6500},
	{"id": "sup_gt", "name": "Mercedes-AMG GT", "model": "mercedes_amg_gt", "category": "Supercar", "color": "e96240", "style": "gt", "top_speed": 268.0, "accel": 10.8, "handling": 1.10, "brake": 20.0, "unlock": 0},
	{"id": "sup_mid", "name": "Garuda Mid-Engine", "category": "Supercar", "color": "d8e74a", "style": "mid", "top_speed": 302.0, "accel": 12.0, "handling": 1.16, "brake": 21.0, "unlock": 8000},
	{"id": "sup_hyper", "name": "Vayu Hypercar", "category": "Supercar", "color": "5c96df", "style": "hyper", "top_speed": 344.0, "accel": 13.5, "handling": 1.20, "brake": 22.0, "unlock": 11000},
	{"id": "sup_track", "name": "Agni Track Special", "category": "Supercar", "color": "efaa2c", "style": "track", "top_speed": 326.0, "accel": 13.0, "handling": 1.28, "brake": 23.0, "unlock": 13000},
	{"id": "sup_roadster", "name": "Tara Roadster", "category": "Supercar", "color": "b77fd5", "style": "roadster", "top_speed": 292.0, "accel": 11.6, "handling": 1.22, "brake": 21.0, "unlock": 15000},
	{"id": "sup_proto", "name": "Astra Prototype", "category": "Supercar", "color": "e7f1ef", "style": "prototype", "top_speed": 355.0, "accel": 14.0, "handling": 1.25, "brake": 23.0, "unlock": 18000},
]

## Targets are world (x, z), on a five-by-five road grid spaced 120 meters.
## Heading is Godot yaw in degrees: 0 north (-Z), 90 west, -90 east, 180 south.
## Distance is a route reference only; completion requires every ordered target.
const START_POSITION := Vector2(-4.5, 65.0)
const ROAD_CENTERS: Array[float] = [-240.0, -120.0, 0.0, 120.0, 240.0]
const LEVELS: Array[Dictionary] = [
	{"id": 1, "name": "First Park", "subtitle": "Drive forward, brake, then settle in the green bay", "type": "parking", "targets": [Vector2(-4.5, 10), Vector2(-7.3, -45)], "target_heading": 0.0, "distance": 110.1, "time": 55.0, "traffic": 2, "hazards": 0, "reward": 500, "fuel_budget": 100.0, "min_health": 0.0},
	{"id": 2, "name": "Around the Block", "subtitle": "Two left turns through real intersections", "type": "checkpoints", "targets": [Vector2(-4.5, 4.5), Vector2(-115.5, 4.5), Vector2(-115.5, 90)], "target_heading": 180.0, "distance": 257.0, "time": 70.0, "traffic": 3, "hazards": 1, "reward": 650, "fuel_budget": 100.0, "min_health": 0.0},
	{"id": 3, "name": "Showroom Delivery", "subtitle": "Keep at least 75% condition on arrival", "type": "precision", "targets": [Vector2(-4.5, -115.5), Vector2(-124.5, -115.5), Vector2(-124.5, -210)], "target_heading": 0.0, "distance": 395.0, "time": 85.0, "traffic": 4, "hazards": 2, "reward": 800, "fuel_budget": 100.0, "min_health": 75.0},
	{"id": 4, "name": "Embassy Parking", "subtitle": "Turn right, turn left, park facing north", "type": "parking", "targets": [Vector2(-4.5, -124.5), Vector2(115.5, -124.5), Vector2(112.7, -205)], "target_heading": 0.0, "distance": 390.0, "time": 100.0, "traffic": 4, "hazards": 2, "reward": 950, "fuel_budget": 100.0, "min_health": 0.0},
	{"id": 5, "name": "Economy Class", "subtitle": "A limited tank: coast early and avoid detours", "type": "fuel", "targets": [Vector2(-4.5, -235.5), Vector2(-235.5, -235.5), Vector2(-235.5, -50)], "target_heading": 180.0, "distance": 717.0, "time": 125.0, "traffic": 5, "hazards": 3, "reward": 1100, "fuel_budget": 32.0, "min_health": 0.0},
	{"id": 6, "name": "Market Maneuver", "subtitle": "Three corners and a west-facing curbside bay", "type": "parking", "targets": [Vector2(-4.5, -115.5), Vector2(-115.5, -115.5), Vector2(-115.5, 124.5), Vector2(-210, 127.3)], "target_heading": 90.0, "distance": 626.0, "time": 130.0, "traffic": 6, "hazards": 3, "reward": 1250, "fuel_budget": 100.0, "min_health": 0.0},
	{"id": 7, "name": "Capital Courier", "subtitle": "Five checkpoints around the eastern avenues", "type": "checkpoints", "targets": [Vector2(-4.5, -244.5), Vector2(235.5, -244.5), Vector2(244.5, 4.5), Vector2(115.5, 4.5), Vector2(115.5, -90)], "target_heading": 0.0, "distance": 1022.2, "time": 165.0, "traffic": 7, "hazards": 4, "reward": 1450, "fuel_budget": 100.0, "min_health": 0.0},
	{"id": 8, "name": "No Scratch Service", "subtitle": "Bring the car home with at least 80% condition", "type": "precision", "targets": [Vector2(-4.5, -115.5), Vector2(-244.5, -115.5), Vector2(-244.5, -244.5), Vector2(-115.5, -244.5), Vector2(-115.5, -180)], "target_heading": 180.0, "distance": 743.0, "time": 145.0, "traffic": 8, "hazards": 4, "reward": 1650, "fuel_budget": 100.0, "min_health": 80.0},
	{"id": 9, "name": "Last Quarter Tank", "subtitle": "The long way around on a 26% fuel budget", "type": "fuel", "targets": [Vector2(-4.5, -235.5), Vector2(-235.5, -235.5), Vector2(-235.5, 115.5), Vector2(115.5, 115.5), Vector2(115.5, 35)], "target_heading": 0.0, "distance": 1314.0, "time": 210.0, "traffic": 8, "hazards": 5, "reward": 1850, "fuel_budget": 26.0, "min_health": 0.0},
	{"id": 10, "name": "Executive Arrival", "subtitle": "Navigate the busy eastern blocks, then park", "type": "parking", "targets": [Vector2(-4.5, -124.5), Vector2(244.5, -124.5), Vector2(244.5, 115.5), Vector2(115.5, 124.5), Vector2(112.7, 45)], "target_heading": 0.0, "distance": 887.4, "time": 180.0, "traffic": 9, "hazards": 5, "reward": 2100, "fuel_budget": 100.0, "min_health": 0.0},
	{"id": 11, "name": "Concierge Master", "subtitle": "A full city crossing with 85% condition intact", "type": "precision", "targets": [Vector2(-4.5, -244.5), Vector2(244.5, -244.5), Vector2(244.5, 244.5), Vector2(-124.5, 244.5), Vector2(-124.5, 155)], "target_heading": 0.0, "distance": 1506.0, "time": 235.0, "traffic": 10, "hazards": 6, "reward": 2400, "fuel_budget": 100.0, "min_health": 85.0},
	{"id": 12, "name": "Crown of the Capital", "subtitle": "The final city drive ends in a precise south-facing park", "type": "parking", "targets": [Vector2(-4.5, -235.5), Vector2(-235.5, -235.5), Vector2(-235.5, 115.5), Vector2(235.5, 115.5), Vector2(247.3, 210)], "target_heading": 180.0, "distance": 1448.7, "time": 240.0, "traffic": 12, "hazards": 8, "reward": 2800, "fuel_budget": 100.0, "min_health": 0.0},
]

const DIFFICULTIES: Array[Dictionary] = [
	{"name": "Easy", "traffic": 0.65, "time": 1.30, "damage": 0.60, "reward": 0.80},
	{"name": "Medium", "traffic": 1.00, "time": 1.00, "damage": 1.00, "reward": 1.00},
	{"name": "Hard", "traffic": 1.20, "time": 0.87, "damage": 1.25, "reward": 1.35},
	{"name": "Extreme", "traffic": 1.40, "time": 0.75, "damage": 1.55, "reward": 1.75},
]


static func car_unlocked(car_index: int, credits: int) -> bool:
	return car_index >= 0 and car_index < CARS.size() and credits >= int(CARS[car_index]["unlock"])


static func level_unlocked(index: int, completed_level_count: int) -> bool:
	return index >= 0 and index < LEVELS.size() and index <= maxi(0, completed_level_count)


## Only call for a completed mission. A successful finish always earns one star.
static func stars_for_run(time_left: float, time_limit: float, health: float) -> int:
	var remaining: float = clampf(time_left / maxf(time_limit, 0.001), 0.0, 1.0)
	if health >= 80.0 and remaining >= 0.20:
		return 3
	if health >= 55.0 and remaining >= 0.08:
		return 2
	return 1


static func reward_for_run(level_index: int, difficulty_index: int, stars: int) -> int:
	if level_index < 0 or level_index >= LEVELS.size() or difficulty_index < 0 or difficulty_index >= DIFFICULTIES.size():
		return 0
	var base: float = float(LEVELS[level_index]["reward"])
	var multiplier: float = float(DIFFICULTIES[difficulty_index]["reward"])
	var star_bonus: float = 0.70 + 0.15 * clampi(stars, 1, 3)
	return roundi(base * multiplier * star_bonus)
