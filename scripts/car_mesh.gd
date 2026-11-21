class_name DriveCarMesh
extends RefCounted
## Original procedural cars. -Z is forward, Y is up; wheels touch y=0.

static var _materials: Dictionary = {}
static var _boxes: Dictionary = {}
static var _cylinders: Dictionary = {}

static func build(car: Dictionary, detailed: bool = true) -> Node3D:
	var root := Node3D.new()
	root.name = "CarModel"
	var style := str(car.get("style", "sedan")).to_lower()
	var identifier := str(car.get("id", ""))
	if style == "sedan" and identifier.begins_with("sup_"):
		style = identifier.trim_prefix("sup_")
	var body_color := Color(str(car.get("color", "#d4b37a")))
	var paint := _material(body_color, 0.32, 0.38)
	var dark_paint := _material(body_color.darkened(0.34), 0.22, 0.5)
	var glass := _material(Color("182c3c"), 0.38, 0.18)
	var black := _material(Color("15191e"), 0.0, 0.9)
	var chrome := _material(Color("c7d1d7"), 0.65, 0.24)
	var headlight := _material(Color("fff4cb"), 0.0, 0.5, true)
	var taillight := _material(Color("ec4854"), 0.0, 0.6, true)
	var length: float = 4.55
	var width: float = 1.88
	var body_height: float = 0.64
	var cabin_height: float = 0.60
	var cabin_length: float = 2.25
	var cabin_z: float = 0.14
	var wheel_radius: float = 0.35
	var is_sport: bool = str(car.get("category", "Luxury")).to_lower().contains("super")
	match style:
		"suv", "lux_suv":
			length = 4.85; width = 2.04; body_height = 0.88; cabin_height = 0.82; cabin_length = 2.88; cabin_z = 0.27; wheel_radius = 0.41
		"limo", "limousine", "lux_limo":
			length = 5.85; width = 1.94; cabin_length = 3.45; cabin_z = 0.17
		"coupe", "lux_coupe", "grandtourer":
			length = 4.90 if style == "grandtourer" else 4.48; body_height = 0.56; cabin_height = 0.51; cabin_length = 2.16 if style == "grandtourer" else 1.98; cabin_z = 0.22
		"estate", "wagon", "lux_estate":
			length = 4.92; body_height = 0.69; cabin_height = 0.66; cabin_length = 3.0; cabin_z = 0.33
		"cabrio", "convertible", "lux_cabrio":
			length = 4.60; body_height = 0.54; cabin_height = 0.43; cabin_length = 1.62; cabin_z = 0.25
		"gt", "sup_gt":
			length = 4.72; width = 1.99; body_height = 0.46; cabin_height = 0.48; cabin_length = 1.88; cabin_z = 0.43; is_sport = true
		"mid", "mid_engine", "sup_mid":
			length = 4.56; width = 2.02; body_height = 0.43; cabin_height = 0.49; cabin_length = 1.70; cabin_z = -0.15; is_sport = true
		"hyper", "hypercar", "sup_hyper":
			length = 4.84; width = 2.08; body_height = 0.39; cabin_height = 0.46; cabin_length = 1.72; cabin_z = -0.12; is_sport = true
		"track", "sup_track":
			length = 4.50; width = 2.02; body_height = 0.38; cabin_height = 0.44; cabin_length = 1.70; cabin_z = -0.02; is_sport = true
		"roadster", "speedster", "sup_roadster":
			length = 4.24; width = 1.94; body_height = 0.41; cabin_height = 0.32; cabin_length = 1.30; cabin_z = 0.05; is_sport = true
		"prototype", "sup_prototype":
			length = 4.96; width = 2.12; body_height = 0.33; cabin_height = 0.45; cabin_length = 1.40; cabin_z = -0.28; is_sport = true
	if is_sport and style == "sedan":
		body_height = 0.44; cabin_height = 0.46; cabin_length = 1.85
	var base: float = wheel_radius * 0.63
	var belt: float = base + body_height
	var axle_z: float = length * 0.315
	var cabin_width: float = width * (0.73 if is_sport else 0.80)
	var open_top: bool = style in ["cabrio", "convertible", "roadster", "speedster"]

	# A chamfered eight-corner body avoids the blocky look of stacked cubes.
	_prism(root, Vector3(width, body_height, length), Vector3(0, base + body_height * 0.5, 0), paint, 0.17 if is_sport else 0.10)
	_box(root, Vector3(width * 0.95, 0.13, length * 0.88), Vector3(0, base + 0.015, 0.02), dark_paint)
	_cabin(root, cabin_width, cabin_height, cabin_length, belt - 0.01, cabin_z, glass, is_sport)
	if not open_top:
		_box(root, Vector3(cabin_width * 0.79, 0.055, cabin_length * 0.58), Vector3(0, belt + cabin_height + 0.008, cabin_z + cabin_length * 0.04), paint)
	else:
		# A low dark cockpit and narrow windscreen distinguish open cars.
		root.get_child(root.get_child_count() - 1).scale.y = 0.55
		_box(root, Vector3(cabin_width * 0.84, 0.18, 0.10), Vector3(0, belt + cabin_height * 0.72, cabin_z - cabin_length * 0.36), glass)

	var wheels := Node3D.new()
	wheels.name = "Wheels"
	root.add_child(wheels)
	for side in [-1.0, 1.0]:
		for axle in [-1.0, 1.0]:
			var wheel := _wheel(wheel_radius, 0.25 if is_sport else 0.22, black)
			wheel.position = Vector3(side * width * 0.47, wheel_radius, axle * axle_z)
			wheels.add_child(wheel)
			if detailed:
				var rim := _wheel(wheel_radius * 0.61, 0.025, chrome)
				rim.position = wheel.position + Vector3(side * 0.13, 0, 0)
				wheels.add_child(rim)

	# Full width grille and paired running lights are legible at phone resolution.
	_box(root, Vector3(width * (0.68 if is_sport else 0.48), body_height * 0.33, 0.035), Vector3(0, base + body_height * 0.40, -length * 0.5 - 0.005), black)
	for side in [-1.0, 1.0]:
		_box(root, Vector3(width * 0.22, 0.075 if is_sport else 0.12, 0.04), Vector3(side * width * 0.34, belt - body_height * 0.20, -length * 0.485), headlight)
		_box(root, Vector3(width * 0.26, 0.08, 0.045), Vector3(side * width * 0.33, belt - body_height * 0.23, length * 0.485), taillight)
	if detailed:
		# B pillars, mirrors and door sills give luxury cars a finished silhouette.
		for side in [-1.0, 1.0]:
			if not is_sport and not open_top:
				_box(root, Vector3(0.055, cabin_height * 0.78, 0.085), Vector3(side * cabin_width * 0.48, belt + cabin_height * 0.4, cabin_z + 0.07), paint)
			_box(root, Vector3(0.18, 0.11, 0.29), Vector3(side * (width * 0.5 + 0.06), belt + 0.12, cabin_z - cabin_length * 0.38), paint)
			_box(root, Vector3(0.035, 0.035, length * 0.47), Vector3(side * width * 0.496, base + body_height * 0.31, 0.08), chrome if not is_sport else dark_paint)
		_box(root, Vector3(0.41, 0.13, 0.025), Vector3(0, base + body_height * 0.37, length * 0.504), chrome)
		if not is_sport:
			_box(root, Vector3(0.045, body_height * 0.30, 0.045), Vector3(0, base + body_height * 0.41, -length * 0.511), chrome)
		else:
			_box(root, Vector3(width * 0.84, 0.06, 0.28), Vector3(0, base - 0.035, -length * 0.47), black)
			_box(root, Vector3(width * 0.74, 0.10, 0.15), Vector3(0, base + 0.04, length * 0.49), black)
			if style in ["hyper", "hypercar", "track", "sup_track", "prototype", "sup_prototype"]:
				for side in [-1.0, 1.0]:
					_box(root, Vector3(0.07, 0.28, 0.10), Vector3(side * width * 0.29, belt + 0.11, length * 0.36), black)
				_box(root, Vector3(width * 1.04, 0.07, 0.34), Vector3(0, belt + 0.28, length * 0.36), dark_paint)
			if style in ["track", "prototype", "gt"]:
				_box(root, Vector3(width * 0.11, 0.012, length * 0.22), Vector3(0, belt + 0.01, -length * 0.34), chrome)
				_box(root, Vector3(width * 0.11, 0.012, length * 0.17), Vector3(0, belt + 0.01, length * 0.37), chrome)
	return root

static func _material(color: Color, metallic: float = 0.0, roughness: float = 0.6, emissive: bool = false) -> StandardMaterial3D:
	var key := "%s:%.2f:%.2f:%s" % [color.to_html(), metallic, roughness, str(emissive)]
	if _materials.has(key):
		return _materials[key]
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = metallic
	material.roughness = roughness
	if emissive:
		material.emission_enabled = true
		material.emission = color * 0.45
	_materials[key] = material
	return material

static func _add_mesh(parent: Node3D, mesh: Mesh, position: Vector3, material: Material) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.position = position
	instance.material_override = material
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(instance)
	return instance

static func _box(parent: Node3D, size: Vector3, position: Vector3, material: Material) -> MeshInstance3D:
	var key := str(size)
	if not _boxes.has(key):
		var mesh := BoxMesh.new()
		mesh.size = size
		_boxes[key] = mesh
	return _add_mesh(parent, _boxes[key], position, material)

static func _wheel(radius: float, width: float, material: Material) -> MeshInstance3D:
	var key := "%.3f:%.3f" % [radius, width]
	if not _cylinders.has(key):
		var mesh := CylinderMesh.new()
		mesh.top_radius = radius
		mesh.bottom_radius = radius
		mesh.height = width
		mesh.radial_segments = 10
		mesh.rings = 1
		_cylinders[key] = mesh
	var instance := MeshInstance3D.new()
	instance.mesh = _cylinders[key]
	instance.material_override = material
	instance.rotation.z = PI * 0.5
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return instance

static func _cabin(parent: Node3D, width: float, height: float, length: float, base: float, offset: float, material: Material, sport: bool) -> void:
	var bottom := PackedVector3Array([
		Vector3(-width * 0.5, 0, -length * 0.5), Vector3(width * 0.5, 0, -length * 0.5),
		Vector3(width * 0.5, 0, length * 0.5), Vector3(-width * 0.5, 0, length * 0.5)])
	var top := PackedVector3Array([
		Vector3(-width * 0.39, height, -length * (0.20 if sport else 0.27)), Vector3(width * 0.39, height, -length * (0.20 if sport else 0.27)),
		Vector3(width * 0.39, height, length * 0.31), Vector3(-width * 0.39, height, length * 0.31)])
	_surface(parent, bottom, top, Vector3(0, base, offset), material)

static func _prism(parent: Node3D, size: Vector3, position: Vector3, material: Material, bevel: float) -> void:
	var x: float = size.x * 0.5
	var z: float = size.z * 0.5
	var y: float = size.y * 0.5
	var cut: float = size.x * bevel
	var bottom := PackedVector3Array([
		Vector3(-x + cut, -y, -z), Vector3(x - cut, -y, -z), Vector3(x, -y, -z + cut), Vector3(x, -y, z - cut),
		Vector3(x - cut, -y, z), Vector3(-x + cut, -y, z), Vector3(-x, -y, z - cut), Vector3(-x, -y, -z + cut)])
	var top := PackedVector3Array()
	for p in bottom:
		top.append(Vector3(p.x * 0.97, y, p.z * 0.98))
	_surface(parent, bottom, top, position, material)

static func _surface(parent: Node3D, bottom: PackedVector3Array, top: PackedVector3Array, position: Vector3, material: Material) -> void:
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	surface.set_smooth_group(-1)
	# Clockwise face winding as viewed from the outside (Godot front faces).
	var count := bottom.size()
	for i in range(count):
		var j: int = (i + 1) % count
		for vertex in [bottom[i], bottom[j], top[j], bottom[i], top[j], top[i]]:
			surface.add_vertex(vertex)
	for i in range(1, count - 1):
		for vertex in [top[0], top[i], top[i + 1]]:
			surface.add_vertex(vertex)
		for vertex in [bottom[0], bottom[i + 1], bottom[i]]:
			surface.add_vertex(vertex)
	surface.generate_normals()
	_add_mesh(parent, surface.commit(), position, material)
