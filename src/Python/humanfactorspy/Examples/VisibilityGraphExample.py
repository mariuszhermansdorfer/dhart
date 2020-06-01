
from humanfactorspy.geometry import LoadOBJ, CommonRotations
from humanfactorspy.raytracer import EmbreeBVH
from humanfactorspy.visibilitygraph import VisibilityGraphAllToAll

obj_path = "H:\\HumanMetrics\\Codebase\\HumanFactors\\out\\install\\x64-Debug\\Example Models\\plane.obj"
loaded_obj = LoadOBJ(obj_path, rotation=CommonRotations.Yup_to_Zup)
bvh = EmbreeBVH(loaded_obj)

# The model is a flat plane, so only nodes 0,2 should connect.
points = [(0,0,1), (0,0,-10), (0,2,0)]
height = 1.7
VG = VisibilityGraphAllToAll(
    bvh,
    points,
    height
)

print(VG.CompressToCSR())