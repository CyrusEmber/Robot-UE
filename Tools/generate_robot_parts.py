import bpy
import math
from math import radians

OUT_DIR = "E:/UnrealDocument/Robot/RobotParts"
FBX_DIR = OUT_DIR + "/fbx"

import os
os.makedirs(OUT_DIR, exist_ok=True)
os.makedirs(FBX_DIR, exist_ok=True)

print("Blender version:", bpy.app.version_string)

# ---------- clean scene ----------
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()
for m in list(bpy.data.meshes):
    if m.users == 0:
        bpy.data.meshes.remove(m)

# ---------- materials ----------
def make_mat(name, color, metallic, rough):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = rough
    return mat

M_RUBBER    = make_mat("Rubber",        (0.025, 0.025, 0.028), 0.0, 0.92)
M_STEEL     = make_mat("SteelBrushed",  (0.62, 0.65, 0.68),   1.0, 0.35)
M_CHROME    = make_mat("Chrome",        (0.88, 0.90, 0.92),   1.0, 0.08)
M_GUNMETAL  = make_mat("Gunmetal",      (0.16, 0.17, 0.19),   1.0, 0.45)
M_GOLD      = make_mat("Gold",          (1.00, 0.74, 0.32),   1.0, 0.22)
M_COPPER    = make_mat("Copper",        (0.92, 0.58, 0.45),   1.0, 0.30)
M_RUSTRED   = make_mat("IronRed",       (0.45, 0.12, 0.06),   0.85, 0.55)

def set_mat(obj, mat):
    obj.data.materials.append(mat)

def smooth(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    try:
        bpy.ops.object.shade_auto_smooth()
    except Exception:
        bpy.ops.object.shade_smooth()

def bevel(obj, width, segments=3):
    mod = obj.modifiers.new("Bevel", 'BEVEL')
    mod.width = width
    mod.segments = segments

def parent_to(root, obj):
    obj.parent = root
    obj.matrix_parent_inverse = root.matrix_world.inverted()

def new_root(name, loc):
    bpy.ops.object.empty_add(type='PLAIN_AXES', location=loc)
    e = bpy.context.active_object
    e.name = name
    e.empty_display_size = 0.4
    return e

parts = []

# ---------- 1. Tire ----------
root = new_root("Tire", (0, 0, 0))
cx, cz = 0.0, 1.42
bpy.ops.mesh.primitive_torus_add(align='WORLD', location=(cx, 0, cz),
    rotation=(radians(90), 0, 0), major_radius=1.0, minor_radius=0.42)
tire = bpy.context.active_object
tire.name = "Tire_Rubber"
set_mat(tire, M_RUBBER)
smooth(tire)

bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.72, depth=0.5,
    location=(cx, 0, cz), rotation=(radians(90), 0, 0))
rim = bpy.context.active_object
rim.name = "Tire_Rim"
set_mat(rim, M_STEEL)
smooth(rim)

bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.24, depth=0.64,
    location=(cx, 0, cz), rotation=(radians(90), 0, 0))
hub = bpy.context.active_object
hub.name = "Tire_Hub"
set_mat(hub, M_GUNMETAL)
smooth(hub)

n = 0
for sy in (-1, 1):
    for i in range(6):
        a = i * math.pi / 3
        bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.05, depth=0.1,
            location=(cx + 0.45 * math.cos(a), 0.27 * sy, cz + 0.45 * math.sin(a)),
            rotation=(radians(90), 0, 0))
        bolt = bpy.context.active_object
        bolt.name = "Tire_Bolt_%02d" % n
        set_mat(bolt, M_CHROME)
        smooth(bolt)
        n += 1

for ob in [tire, rim, hub] + [o for o in bpy.data.objects if o.name.startswith("Tire_Bolt")]:
    parent_to(root, ob)
parts.append(("tire", root))

# ---------- 2. Battery ----------
root = new_root("Battery", (6, 0, 0))
bpy.ops.mesh.primitive_cube_add(location=(6, 0, 0.8))
body = bpy.context.active_object
body.name = "Battery_Body"
body.scale = (1.2, 0.7, 0.8)
set_mat(body, M_GUNMETAL)
bevel(body, 0.06)

for i, tx in enumerate([-0.7, 0.7]):
    bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=0.13, depth=0.26,
        location=(6 + tx, 0, 1.73))
    term = bpy.context.active_object
    term.name = "Battery_Terminal_%d" % i
    set_mat(term, M_COPPER)
    smooth(term)

bpy.ops.mesh.primitive_cube_add(location=(6, -0.715, 0.9))
label = bpy.context.active_object
label.name = "Battery_Label"
label.scale = (0.65, 0.015, 0.275)
set_mat(label, M_GOLD)
bevel(label, 0.01)

bpy.ops.mesh.primitive_cube_add(location=(6, 0, 1.62))
strap = bpy.context.active_object
strap.name = "Battery_Strap"
strap.scale = (1.22, 0.72, 0.03)
set_mat(strap, M_RUSTRED)

for ob in [body, label, strap] + [o for o in bpy.data.objects if o.name.startswith("Battery_Terminal")]:
    parent_to(root, ob)
parts.append(("battery", root))

# ---------- 3. Armor Plate ----------
root = new_root("ArmorPlate", (12, 0, 0))
bpy.ops.mesh.primitive_cube_add(location=(12, 0, 1.1))
plate = bpy.context.active_object
plate.name = "Armor_Plate"
plate.scale = (1.6, 0.175, 1.1)
set_mat(plate, M_STEEL)
bevel(plate, 0.05)

bpy.ops.mesh.primitive_cube_add(location=(12, 0, 1.1))
core = bpy.context.active_object
core.name = "Armor_Core"
core.scale = (1.5, 0.19, 1.0)
set_mat(core, M_GUNMETAL)

n = 0
for fz in [0.5, 1.7]:
    for fx in [-1.25, -0.42, 0.42, 1.25]:
        for sy in [-1, 1]:
            bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.06, depth=0.07,
                location=(12 + fx, 0.19 * sy, fz), rotation=(radians(90), 0, 0))
            riv = bpy.context.active_object
            riv.name = "Armor_Rivet_%02d" % n
            set_mat(riv, M_CHROME)
            smooth(riv)
            parent_to(root, riv)
            n += 1

for ob in [plate, core]:
    parent_to(root, ob)
parts.append(("armor_plate", root))

# ---------- 4. Gear ----------
root = new_root("Gear", (18, 0, 0))
gx, gz = 18.0, 1.11
bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=1.1, depth=0.35,
    location=(gx, 0, gz))
disc = bpy.context.active_object
disc.name = "Gear_Disc"
set_mat(disc, M_GOLD)
smooth(disc)

bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.28, depth=1.0,
    location=(gx, 0, gz))
cutter = bpy.context.active_object
cutter.name = "Gear_Cutter"
mod = disc.modifiers.new("Hole", 'BOOLEAN')
mod.operation = 'DIFFERENCE'
mod.object = cutter
bpy.context.view_layer.objects.active = disc
bpy.ops.object.modifier_apply(modifier="Hole")
bpy.data.objects.remove(cutter, do_unlink=True)

for i in range(12):
    a = i * math.pi / 6
    bpy.ops.mesh.primitive_cube_add(location=(gx + 1.2 * math.cos(a), 1.2 * math.sin(a), gz),
        rotation=(0, 0, a))
    tooth = bpy.context.active_object
    tooth.name = "Gear_Tooth_%02d" % i
    tooth.scale = (0.2, 0.16, 0.175)
    set_mat(tooth, M_GOLD)
    bevel(tooth, 0.02, 2)
    parent_to(root, tooth)

bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.24, depth=0.6,
    location=(gx, 0, gz))
bush = bpy.context.active_object
bush.name = "Gear_Bushing"
set_mat(bush, M_COPPER)
smooth(bush)

parent_to(root, disc)
parts.append(("gear", root))

# ---------- 5. Hydraulic Piston ----------
root = new_root("Piston", (24, 0, 0))
bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.5, depth=3.0,
    location=(24, 0, 0.5), rotation=(0, radians(90), 0))
barrel = bpy.context.active_object
barrel.name = "Piston_Barrel"
set_mat(barrel, M_GUNMETAL)
smooth(barrel)

bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.27, depth=2.8,
    location=(26.45, 0, 0.5), rotation=(0, radians(90), 0))
rod = bpy.context.active_object
rod.name = "Piston_Rod"
set_mat(rod, M_CHROME)
smooth(rod)

bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.65, depth=0.25,
    location=(22.55, 0, 0.5), rotation=(0, radians(90), 0))
flange = bpy.context.active_object
flange.name = "Piston_Flange"
set_mat(flange, M_STEEL)
smooth(flange)

for i, (ex, mat) in enumerate([(22.3, M_STEEL), (28.05, M_STEEL)]):
    bpy.ops.mesh.primitive_torus_add(align='WORLD', location=(ex, 0, 0.5),
        rotation=(0, radians(90), 0), major_radius=0.24, minor_radius=0.08)
    eye = bpy.context.active_object
    eye.name = "Piston_Eye_%d" % i
    set_mat(eye, mat)
    smooth(eye)

for ob in [barrel, rod, flange] + [o for o in bpy.data.objects if o.name.startswith("Piston_Eye")]:
    parent_to(root, ob)
parts.append(("piston", root))

# ---------- ground ----------
bpy.ops.mesh.primitive_plane_add(size=80, location=(12, 0, 0))
ground = bpy.context.active_object
ground.name = "Ground"
gm = bpy.data.materials.new("Ground")
gm.use_nodes = True
gb = gm.node_tree.nodes["Principled BSDF"]
gb.inputs["Base Color"].default_value = (0.09, 0.09, 0.10, 1.0)
gb.inputs["Roughness"].default_value = 0.7
ground.data.materials.append(gm)

# ---------- lights & camera ----------
bpy.ops.object.light_add(type='SUN', location=(10, -10, 12))
sun = bpy.context.active_object
sun.data.energy = 5.0
sun.rotation_euler = (radians(45), radians(10), radians(35))

bpy.ops.object.light_add(type='AREA', location=(12, -7, 8))
key = bpy.context.active_object
key.data.energy = 2600
key.data.size = 8
key.rotation_euler = (radians(55), 0, 0)

bpy.ops.object.light_add(type='AREA', location=(12, 7, 6))
fill = bpy.context.active_object
fill.data.energy = 900
fill.data.size = 5
fill.rotation_euler = (radians(-45), 0, 0)

bpy.ops.object.empty_add(type='PLAIN_AXES', location=(12.5, 0, 1.5))
target = bpy.context.active_object
target.name = "CamTarget"

bpy.ops.object.camera_add(location=(12.5, -27, 9.5))
cam = bpy.context.active_object
cam.data.lens = 28
con = cam.constraints.new('TRACK_TO')
con.target = target
con.track_axis = 'TRACK_NEGATIVE_Z'
con.up_axis = 'UP_Y'
bpy.context.scene.camera = cam

world = bpy.data.worlds["World"]
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.05, 0.05, 0.06, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.2

# ---------- render preview ----------
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 64
scene.cycles.use_denoising = True
scene.render.resolution_x = 1600
scene.render.resolution_y = 900
try:
    scene.view_settings.look = 'AgX - Punchy'
except Exception:
    pass
scene.render.filepath = OUT_DIR + "/preview.png"
scene.render.image_settings.file_format = 'PNG'
bpy.ops.render.render(write_still=True)
print("Rendered:", scene.render.filepath)

# ---------- save blend ----------
bpy.ops.wm.save_as_mainfile(filepath=OUT_DIR + "/robot_parts.blend")
print("Saved blend:", OUT_DIR + "/robot_parts.blend")

# ---------- export fbx per part ----------
for name, root in parts:
    bpy.ops.object.select_all(action='DESELECT')
    root.select_set(True)
    for c in root.children_recursive:
        c.select_set(True)
    bpy.context.view_layer.objects.active = root
    path = FBX_DIR + "/" + name + ".fbx"
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True)
    print("Exported:", path)

print("DONE")
