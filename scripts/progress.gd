class_name DriveProgress
extends RefCounted
## Local offline career data. The temporary file preserves the last valid save.
var path: String = "user://career.cfg"
var total_score: int = 0
var completed_count: int = 0
var best_stars: Array[int] = []
var selected_car: int = 0
var selected_level: int = 0
var difficulty: int = 0
var quality: int = 1
var muted: bool = false

func _init(save_path: String = "user://career.cfg") -> void:
	path = save_path
	best_stars.resize(12)
	best_stars.fill(0)

func load_data() -> void:
	var config := ConfigFile.new()
	if config.load(path) != OK:
		return
	total_score = clampi(int(config.get_value("career", "score", 0)), 0, 100000000)
	selected_car = clampi(int(config.get_value("settings", "car", 0)), 0, 11)
	difficulty = clampi(int(config.get_value("settings", "difficulty", 0)), 0, 3)
	quality = clampi(int(config.get_value("settings", "quality", 1)), 0, 2)
	muted = bool(config.get_value("settings", "muted", false))
	var stars: Variant = config.get_value("career", "stars", [])
	if stars is Array or stars is PackedInt32Array:
		for i in mini(stars.size(), 12):
			best_stars[i] = clampi(int(stars[i]), 0, 3)
	completed_count = 0
	while completed_count < 12 and best_stars[completed_count] > 0:
		completed_count += 1
	selected_level = clampi(int(config.get_value("settings", "level", 0)), 0, mini(completed_count, 11))
	if not DriveData.car_unlocked(selected_car, total_score):
		selected_car = 0

func save_data() -> bool:
	var config := ConfigFile.new()
	config.set_value("career", "score", total_score)
	config.set_value("career", "stars", best_stars)
	config.set_value("settings", "car", selected_car)
	config.set_value("settings", "level", selected_level)
	config.set_value("settings", "difficulty", difficulty)
	config.set_value("settings", "quality", quality)
	config.set_value("settings", "muted", muted)
	if config.save(path + ".tmp") != OK:
		return false
	return DirAccess.rename_absolute(path + ".tmp", path) == OK

func record_win(level: int, stars: int, reward: int) -> void:
	if level < 0 or level >= 12 or level > completed_count:
		return
	best_stars[level] = maxi(best_stars[level], clampi(stars, 1, 3))
	total_score = mini(100000000, total_score + maxi(0, reward))
	completed_count = maxi(completed_count, mini(12, level + 1))
	save_data()
