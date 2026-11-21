extends SceneTree
## Rebuild normalized mobile meshes from the attributed GLB source assets.
## Usage: godot --headless --path . --script tools/bake_models.gd

const MODEL_SPECS := [
	{"id": "bmw_320i", "file": "bmw-320i.glb", "yaw": 180.0, "length": 4.53},
	{"id": "mercedes_gclass", "file": "mercedes-gclass.glb", "yaw": 180.0, "length": 4.82},
	{"id": "mercedes_amg_gt", "file": "mercedes-benz-amg-gt-import.glb", "yaw": 257.0, "length": 4.55},
]

func _init() -> void:
	for spec in MODEL_SPECS:
		bake(spec)
	quit()

func collect(node: Node, parent_transform: Transform3D, output: Array[Dictionary], skip_floor: bool) -> void:
	var transform := parent_transform
	if node is Node3D:
		transform = transform * node.transform
	if node is MeshInstance3D and node.mesh:
		# The AMG author's scene includes a large presentation floor, not car geometry.
		if not (skip_floor and str(node.name) == "Object_4"):
			output.append({"node": node, "transform": transform})
	for child in node.get_children():
		collect(child, transform, output, skip_floor)

func bake(spec: Dictionary) -> void:
	var state := GLTFState.new()
	var document := GLTFDocument.new()
	var error := document.append_from_file("res://SourceAssets/Vehicles/" + str(spec.file), state)
	assert(error == OK, "Cannot read source: " + str(spec.file))
	var original := document.generate_scene(state)
	assert(original != null)
	var parts: Array[Dictionary] = []
	var orientation := Transform3D(Basis(Vector3.UP, deg_to_rad(float(spec.yaw))), Vector3.ZERO)
	collect(original, orientation, parts, spec.id == "mercedes_amg_gt")
	var lower := Vector3(INF, INF, INF)
	var upper := Vector3(-INF, -INF, -INF)
	for part in parts:
		var mesh: Mesh = part.node.mesh
		for surface in mesh.get_surface_count():
			var vertices: PackedVector3Array = mesh.surface_get_arrays(surface)[Mesh.ARRAY_VERTEX]
			for vertex in vertices:
				var point: Vector3 = part.transform * vertex
				lower = lower.min(point)
				upper = upper.max(point)
	assert(lower.is_finite() and upper.is_finite())
	var model_scale: float = float(spec.length) / (upper.z - lower.z)
	var origin := Vector3((lower.x + upper.x) * 0.5, lower.y, (lower.z + upper.z) * 0.5)
	var normalization := Transform3D(Basis.IDENTITY.scaled(Vector3.ONE * model_scale), -origin * model_scale)
	# Merge disconnected source parts by material, retaining UVs, normals and colors.
	# This changes 49 BMW mesh nodes into 11 material surfaces on one mesh node.
	var groups: Dictionary = {}
	for part in parts:
		var mesh: Mesh = part.node.mesh
		for surface in mesh.get_surface_count():
			var material: Material = part.node.get_active_material(surface)
			var key: int = material.get_instance_id() if material != null else 0
			if not groups.has(key):
				var builder := SurfaceTool.new()
				builder.begin(Mesh.PRIMITIVE_TRIANGLES)
				builder.set_material(material)
				groups[key] = builder
			var builder: SurfaceTool = groups[key]
			builder.append_from(mesh, surface, normalization * part.transform)
	var baked := ArrayMesh.new()
	for key in groups:
		var builder: SurfaceTool = groups[key]
		builder.index()
		builder.commit(baked)
	var path: String = "res://assets/models/" + str(spec.id) + ".res"
	assert(ResourceSaver.save(baked, path, ResourceSaver.FLAG_COMPRESS) == OK)
	print("BAKED ", spec.id, " | ", baked.get_surface_count(), " material surfaces | ", baked.get_aabb())
	original.free()
