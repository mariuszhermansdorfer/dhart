from humanfactorspy.raytracer import raytracer_native_functions
from humanfactorspy.geometry import MeshInfo
from typing import *
from ctypes import c_void_p

__all__ = ['EmbreeBVH']

class EmbreeBVH(object):
    """ An acceelerated datastructure optimized for high performane ray intersections
    
    Note:
        A BVH is an accelerated data structure that's used for high performance
        ray intersections. All functions that ustilize the embree ray tracer
        require the mesh first to be converted to this format. 

    """

    pointer: Union[None, c_void_p] = None  # Pointer to the underlying c-object

    def __init__(self, geometry: Union[MeshInfo, List[MeshInfo]]):
        """ Create a new BVH from an existing mesh 

        Args:
            geometry: The mesh to create the BVH from. 
        
        Example:
            Creating a BVH from a plane object

            >>> from humanfactorspy.geometry import ConstructPlane
            >>> from humanfactorspy.raytracer import EmbreeBVH

            >>> MI = ConstructPlane()
            >>> BVH = EmbreeBVH(MI)

        """
        
        if isinstance(geometry, List):
            pointers = [mesh._MeshInfo__internal_ptr for mesh in geometry]
        else:
            pointers = geometry._MeshInfo__internal_ptr
        self.pointer = raytracer_native_functions.CreateRayTracer(
            pointers
        )


    def __del__(self):
        if self.pointer != 0:
            raytracer_native_functions.DestroyRayTracer(self.pointer)

