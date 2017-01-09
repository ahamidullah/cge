bl_info = {
	"name":         "ahh",
	"author":       "me",
	"blender":      (2,6,2),
	"version":      (0,0,1),
	"location":     "File > Import-Export",
	"description":  "Export custom data format",
	"category":     "Import-Export"
}

import bpy
import bmesh
from bpy_extras.io_utils import ExportHelper

# /usr/share/blender/scripts/addons/io_mesh_myformat/__init__.py
class ExportMyFormat(bpy.types.Operator, ExportHelper):
	bl_idname       = "export_my_format.ahh";
	bl_label        = "ahh";
	bl_options      = {'PRESET'};

	filename_ext    = ".ahh";

	def execute(self, context):
		out = open(self.properties.filepath, 'w')
		sce = context.scene
		ob = sce.objects.active
		for mat_slot in ob.material_slots:
			for mtex_slot in mat_slot.material.texture_slots:
				if mtex_slot:
					#print("\t%s" % mtex_slot)
					if hasattr(mtex_slot.texture , 'image'):
                    				out.write('tf %s\n' % bpy.path.abspath(mtex_slot.texture.image.filepath))

		bm = bmesh.new()
		bm.from_mesh(ob.data)
		bmesh.ops.triangulate(bm, faces=bm.faces)

		verts = []
		inds = []
		cur_ind = 0
		uv_lay = bm.loops.layers.uv.active
		for f in bm.faces:
			out.write('f')
			for l in f.loops:
				v =  (l.vert, l[uv_lay].uv.copy().freeze())
				if v not in verts:
					verts.append(v)
					inds.append(cur_ind)
					out.write(' %i' % (cur_ind))
					cur_ind += 1
				else:
					i = verts.index(v)
					out.write(' %i' % (inds[i]))
			out.write('\n')
		for v in verts:
			out.write( 'v %f %f %f\n' % (v[0].co.x, v[0].co.z, v[0].co.y) )
		for v in verts:
			out.write( 'vn %f %f %f\n' % (v[0].normal.x, v[0].normal.z, v[0].normal.y) )
		for v in verts:
			out.write( 'vt %f %f\n' % (v[1].x, v[1].y))
		out.close()
		bm.free()
		return {'FINISHED'}

def menu_func(self, context):
	self.layout.operator(ExportMyFormat.bl_idname, text="ahh(.ahh)");

def register():
	bpy.utils.register_module(__name__);
	bpy.types.INFO_MT_file_export.append(menu_func);

def unregister():
	bpy.utils.unregister_module(__name__);
	bpy.types.INFO_MT_file_export.remove(menu_func);

if __name__ == "__main__":
	register()
