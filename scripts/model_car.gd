class_name LicensedCarMesh
extends RefCounted
## Attributed model derivatives, normalized to metres with -Z forward and Y=0 ground.
## Other catalog cars use the original procedural mesh factory.

const Factory = preload("res://scripts/car_mesh.gd")
const MODEL_PATHS := {
	"bmw_320i": "res://assets/models/bmw_320i.res",
	"mercedes_gclass": "res://assets/models/mercedes_gclass.res",
	"mercedes_amg_gt": "res://assets/models/mercedes_amg_gt.res",
}
static var _meshes: Dictionary = {}

static func build(car: Dictionary, detailed: bool = true) -> Node3D:
	var model_id := str(car.get("model", ""))
	if not MODEL_PATHS.has(model_id):
		return Factory.build(car, detailed)
	if not _meshes.has(model_id):
		_meshes[model_id] = load(str(MODEL_PATHS[model_id])) as Mesh
	var mesh: Mesh = _meshes[model_id]
	if mesh == null:
		push_error("Missing licensed car mesh: " + model_id)
		return Factory.build(car, detailed)
	var root := Node3D.new()
	root.name = "LicensedCarModel"
	var body := MeshInstance3D.new()
	body.name = "VehicleBody"
	body.mesh = mesh
	body.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	root.add_child(body)
	return root
