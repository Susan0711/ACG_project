"""Build the Tidewatch scene. The normal, offline run uses the supplied Kenney derivatives."""
import argparse
import hashlib
import io
import json
import math
import random
from pathlib import Path
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets" / "tidewatch"
SCENE = ROOT / "scene" / "tidewatch.json"
ARCHIVE_SHA256 = "921f3f73927bb23106cae34bc21d5ab4b033a9fc120475e96f714a406e3169df"
# name used by the scene, original Castle Kit model, baked XYZ scale and yaw (degrees).
KIT = [
    ("beacon-base", "tower-hexagon-base", (1.68, 1.42, 1.68), 20),
    ("beacon-gallery", "tower-hexagon-mid", (1.70, 1.46, 1.70), 20),
    ("beacon-eaves", "tower-hexagon-top", (2.00, 1.25, 2.00), 20),
    ("beacon-roof", "tower-hexagon-roof", (1.92, 1.25, 1.92), 20),
    ("keep-base", "tower-square-base", (1.16, 1.05, 1.16), 0),
    ("keep-windows", "tower-square-mid-windows", (1.16, 0.90, 1.16), 0),
    ("keep-roof", "tower-square-top-roof-high-windows", (1.38, 1.00, 1.38), 0),
    ("gate", "wall-doorway", (0.75, 0.92, 1.55), -90),
    ("wall-front", "wall-narrow", (0.75, 0.68, 1.32), -90),
    ("wall-side", "wall-narrow", (0.75, 0.68, 1.00), 0),
    ("pennant", "flag-pennant", (0.80, 0.80, 0.80), -64),
    ("cypress", "tree-large", (0.86, 1.15, 0.86), 17),
    ("sapling", "tree-small", (0.68, 0.80, 0.68), 63),
    ("reefs", "rocks-large", (0.92, 0.96, 0.92), 20),
]


def write_text(path, text):
    path.write_bytes((text.rstrip() + "\n").encode("utf-8"))


def import_kit(path):
    from PIL import Image  # only required when rebuilding the downloaded derivatives
    if hashlib.sha256(path.read_bytes()).hexdigest() != ARCHIVE_SHA256:
        raise ValueError("Expected Kenney Castle Kit 2.0 from the source listed in ASSET_SOURCES.md")
    with ZipFile(path) as archive:
        prefix = "Models/OBJ format/"
        image = Image.open(io.BytesIO(archive.read(prefix + "Textures/colormap.png"))).convert("RGB")
        image = image.resize((128, 128), Image.Resampling.NEAREST)
        pixels = ["%d %d %d" % image.getpixel((x,y)) for y in range(image.height) for x in range(image.width)]
        write_text(ASSETS / "castle-palette.ppm", "P3\n128 128\n255\n" + "\n".join(pixels))
        for name, source, scale, yaw in KIT:
            angle = math.radians(yaw)
            lines, faces = [f"# Kenney Castle Kit 2.0 / {source} / CC0-1.0"], set()
            vertices = []
            for line in archive.read(prefix + source + ".obj").decode("utf-8-sig").splitlines():
                fields = line.split()
                if not fields:
                    continue
                if fields[0] == "v":
                    x, y, z = (float(v) * factor for v, factor in zip(fields[1:4], scale))
                    px = x * math.cos(angle) + z * math.sin(angle)
                    pz = z * math.cos(angle) - x * math.sin(angle)
                    vertex = f"{px:.9g} {y:.9g} {pz:.9g}"
                    vertices.append(tuple(map(float,vertex.split())))
                    lines.append("v " + vertex)
                elif fields[0] == "vt":
                    lines.append(" ".join(fields[:3]))
                elif fields[0] == "f":
                    corners = ["%s/%s" % tuple(c.split("/")[:2]) for c in fields[1:]]
                    if len(corners) != 3:
                        raise ValueError(f"Expected a triangular face in {source}")
                    a,b,c = [vertices[int(corner.split("/")[0])-1] for corner in corners]
                    ab = [b[i]-a[i] for i in range(3)]
                    ac = [c[i]-a[i] for i in range(3)]
                    area2 = math.hypot(ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2],
                                       ab[0]*ac[1]-ab[1]*ac[0])
                    if area2 < 1e-12:
                        continue
                    face = "f " + " ".join(corners)
                    if face not in faces:
                        faces.add(face)
                        lines.append(face)
            write_text(ASSETS / (name + ".obj"), "\n".join(lines))


class Mesh:
    def __init__(self):
        self.vertices = []
        self.faces = []

    def ring(self, radius, height, count, phase=0):
        return [(radius * math.cos(i * math.tau / count + phase), height,
                 radius * math.sin(i * math.tau / count + phase)) for i in range(count)]

    def polygon(self, points):
        start = len(self.vertices) + 1
        self.vertices.extend(points)
        for i in range(1, len(points) - 1):
            self.faces.append((start, start + i, start + i + 1))

    def join(self, lower, upper):
        for i in range(len(lower)):
            j = (i + 1) % len(lower)
            self.polygon([lower[j], lower[i], upper[i], upper[j]])

    def vertex_normals(self):
        sums = [[0.0, 0.0, 0.0] for _ in self.vertices]
        for face in self.faces:
            a, b, c = (self.vertices[i - 1] for i in face)
            ux, uy, uz = (q - p for p, q in zip(a, b))
            vx, vy, vz = (q - p for p, q in zip(a, c))
            # The cross product weights each face by twice its area.
            nx, ny, nz = uy*vz - uz*vy, uz*vx - ux*vz, ux*vy - uy*vx
            for index in face:
                total = sums[index - 1]
                total[0] += nx
                total[1] += ny
                total[2] += nz
        normals = []
        for total in sums:
            length = math.hypot(*total)
            normals.append(tuple(component / length for component in total))
        return normals

    def save(self, name, uvs=None, *, smooth=False):
        lines = ["# Tidewatch / project-authored geometry"]
        lines += ["v %.8g %.8g %.8g" % p for p in self.vertices]
        if uvs is not None:
            lines += ["vt %.8g %.8g" % uv for uv in uvs]
        if smooth:
            lines += ["vn %.10g %.10g %.10g" % n for n in self.vertex_normals()]
        corner_format = "{0}/{0}" if uvs is not None else "{0}"
        if smooth:
            corner_format = "{0}/{0}/{0}" if uvs is not None else "{0}//{0}"
        for face in self.faces:
            corners = [corner_format.format(i) for i in face]
            lines.append("f " + " ".join(corners))
        write_text(ASSETS / (name + ".obj"), "\n".join(lines))


def cylinder(name, bottom, top, height, sides):
    mesh = Mesh()
    a, b = mesh.ring(bottom, 0, sides), mesh.ring(top, height, sides)
    mesh.join(a, b)
    mesh.polygon(a)
    mesh.polygon(b[::-1])
    mesh.save(name)


def pier_board():
    """A deck board with grain running along its width."""
    name, size = "pier-board", (1.00, .095, .19)
    w,h,d = size
    mesh = Mesh()
    low = [(-w/2,0,-d/2), (w/2,0,-d/2), (w/2,0,d/2), (-w/2,0,d/2)]
    high = [(x,h,z) for x,_,z in low]
    mesh.join(low,high)
    mesh.polygon(low)
    mesh.polygon(high[::-1])
    uvs = [(x + 0.55, z*1.4 + y*1.4 + 0.5) for x,y,z in mesh.vertices]
    mesh.save(name, uvs)


def terrain():
    rng = random.Random(132)
    mesh = Mesh()
    coast = [(math.cos(a * math.tau/44) * (3.18 + rng.uniform(-0.15,0.15)),
              math.sin(a * math.tau/44) * (2.17 + rng.uniform(-0.13,0.13))) for a in range(44)]
    foot = [(1.08*x,-0.2,1.08*z) for x,z in coast]
    shoulder = [(1.04*x,0.22+rng.uniform(-0.11,0.11),1.04*z) for x,z in coast]
    lip = [(x,0.61,z) for x,z in coast]
    mesh.join(foot,shoulder)
    mesh.join(shoulder,lip)
    mesh.save("sea-cliff")
    grass = Mesh()
    grass.join(lip,[(x*.98,.66,z*.98) for x,_,z in lip])
    for i in range(len(lip)):
        x,_,z = lip[i]
        u,_,v = lip[(i+1)%len(lip)]
        grass.polygon([(0,.66,0),(u*.98,.66,v*.98),(x*.98,.66,z*.98)])
    grass.save("headland")
    # A nonuniform grid resolves little wave crests near the jetty and larger ones on the horizon.
    positive = [i*.18 for i in range(39)] + [7.3,7.7,8.2,9,10,12,15,19,24,32,48,70]
    coords = [-x for x in reversed(positive[1:])] + positive
    sea = Mesh()
    for z in coords:
        for x in coords:
            y = .012*math.sin(5*z + .7*math.sin(2*x)) + .008*math.sin(2.8*x+3.8*z)
            sea.vertices.append((x,y,z))
    for row in range(len(coords)-1):
        for col in range(len(coords)-1):
            a = row*len(coords)+col+1
            c = a+len(coords)
            sea.faces.extend([(a,c,c+1),(a,c+1,a+1)])
    sea.save("tide", smooth=True)
    # The background ridges run across the view, behind the island.
    for name,depth,base,amplitude in [("outer-isles",26,1.1,1.4),("far-isles",40,2.0,1.8)]:
        ridge = Mesh()
        old = None
        for i in range(-25,26):
            s = i*1.5
            h = base + amplitude*(.46*math.sin(s*.28)+.34*math.sin(s*.59+2)+.20*math.cos(s*1.3))
            x,z = .87*s-.49*depth, -.49*s-.87*depth
            section = ((x,-.15,z),(x,h,z))
            if old:
                ridge.polygon([old[0],section[0],section[1],old[1]])
            old = section
        ridge.save(name)
    sky = Mesh()
    # Inward-facing emissive surface supplies a constant sky and its reflection in the water.
    low,high = sky.ring(58,-1,40),sky.ring(58,58,40)
    sky.join(high,low)
    sky.polygon(high)
    sky.save("sky")


def details():
    pier_board()
    cylinder("mooring-post",.055,.049,.95,8)
    cylinder("gallery-post",.035,.035,.67,8)
    cylinder("signal-core",.22,.17,.47,12)
    glazing = Mesh()
    bottom = glazing.ring(.65,0,8)
    top = glazing.ring(.65,.67,8)
    inner_bottom = glazing.ring(.635,0,8)
    inner_top = glazing.ring(.635,.67,8)
    glazing.join(bottom,top)
    glazing.join(inner_top,inner_bottom)
    glazing.join(top,inner_top)
    glazing.join(inner_bottom,bottom)
    glazing.save("lantern-glass")
    cylinder("column-cap",.12,.12,.08,8)
    # A closed, tapered hull and two triangular sails share the vessel's world-space velocity.
    hull = Mesh()
    outline = [(0,1),(.34,.48),(.35,-.35),(.22,-.86),(-.22,-.86),(-.35,-.35),(-.34,.48)]
    keel = [(x*.48,0,z*.78) for x,z in outline]
    gunwale = [(x,.22,z) for x,z in outline]
    hull.join(keel[::-1],gunwale[::-1])
    hull.polygon(gunwale)
    hull.save("cutter-hull")
    sail = Mesh()
    sail.polygon([(.02,.34,.05),(.04,1.52,.06),(.48,.46,-.55)])
    sail.save("mainsail")
    jib = Mesh()
    jib.polygon([(0,.34,.81),(0,1.34,.12),(.14,.39,.18)])
    jib.save("jib")
    cylinder("mast",.021,.013,1.52,8)
    wakes = Mesh()
    for side in (-1,1):
        for begin,end in [(0,.44),(.53,.82),(.95,1.21)]:
            previous = None
            for i in range(9):
                t = i/8
                length = begin+(end-begin)*t
                x = side*(.22+.35*length+.07*length*length)
                z = -.64-length
                width = .008+.033*math.sin(t*math.pi)
                section = ((x-width,0,z),(x+width,0,z))
                if previous:
                    wakes.polygon([previous[0],previous[1],section[1],section[0]])
                previous = section
    wakes.save("wake")
    ivy = Mesh()
    ivy.polygon([(-.31,0,.11),(.31,0,.11),(.31,.54,0),(-.31,.54,0)])
    # One complete UV tile, inverted vertically so the stems trail down from the parapet.
    ivy.save("ivy", [(0,1), (1,1), (1,0), (0,0)])
    sun = Mesh()
    sun.polygon([(1.64*.87*math.cos(a*math.tau/64),1.64*math.sin(a*math.tau/64),
                  -1.64*.49*math.cos(a*math.tau/64)) for a in range(64)])
    sun.save("sun")


def assemble():
    objects = []
    def place(mesh, position, material="kit", velocity=None):
        entry = {"mesh": f"../assets/tidewatch/{mesh}.obj", "position": [round(x,5) for x in position],
                 "material": material}
        if velocity is not None:
            entry["velocity"] = velocity
        objects.append(entry)

    materials = {"kit":{"type":"diffuse","texture":"../assets/tidewatch/castle-palette.ppm"},
                 "boards":{"type":"diffuse","texture":"../assets/wood-soft-v05.ppm"},
                 "rock":{"type":"diffuse","color":[.32,.26,.25]},
                 "turf":{"type":"diffuse","color":[.23,.36,.19]},
                 "sea":{"type":"mirror","color":[.27,.61,.70]},
                 "foam":{"type":"diffuse","color":[.61,.83,.78]},
                 "ivy":{"type":"diffuse","color":[.10,.34,.13],"opacity_map":"../assets/leaf-opacity.ppm"},
                 "copper":{"type":"diffuse","color":[.19,.105,.055]},
                 "enamel":{"type":"diffuse","color":[.38,.049,.021]},
                 "linen":{"type":"diffuse","color":[.93,.76,.52]},
                 "glass":{"type":"glass","ior":1.45},
                 "lamp":{"type":"emissive","color":[9,3.1,.65]},
                 "haze":{"type":"emissive","color":[.86,1.25,1.50]},
                 "sun":{"type":"emissive","color":[6,3.0,1.18]},
                 "outer":{"type":"diffuse","color":[.22,.40,.48]},
                 "far":{"type":"diffuse","color":[.48,.58,.62]}}
    for mesh, material in [("sky","haze"),("far-isles","far"),("outer-isles","outer"),
                           ("tide","sea"),("sea-cliff","rock"),("headland","turf")]:
        place(mesh,(0,0,0),material)
    place("sun",(-24.2,6.25,-26.48),"sun")

    # The beacon is taller than the curtain walls, with an uninterrupted line of sight to sea.
    x,z = -.68,-.31
    y = .66
    for mesh,height in [("beacon-base",1.31*1.42),("beacon-gallery",.46*1.46),
                        ("beacon-eaves",.13*1.25)]:
        place(mesh,(x,y,z))
        y += height
    place("lantern-glass",(x,y,z),"glass")
    place("signal-core",(x,y+.085,z),"lamp")
    for i in range(8):
        theta = i*math.tau/8
        place("gallery-post",(x+.66*math.cos(theta),y,z+.66*math.sin(theta)),"copper")
    place("beacon-roof",(x,y+.67,z))
    place("pennant",(x,y+.67+.83*1.25,z))

    for x,z,windows in [(1.18,-.81,True),(-1.74,.66,False),(1.77,.83,False)]:
        y = .66
        place("keep-base",(x,y,z))
        y += 1.01*1.05
        if windows:
            place("keep-windows",(x,y,z))
            y += 1.01*.90
        place("keep-roof",(x,y,z))
    place("gate",(.32,.66,1.23))
    for x in [-.93,1.54]:
        place("wall-front",(x,.66,1.22))
    for x,z in [(-1.13,1.28),(-.69,1.28),(.72,1.63),(1.52,1.45)]:
        place("ivy",(x,.85,z),"ivy")
    for x in [-1.89,1.77]:
        for z in [-.38,.52]:
            place("wall-side",(x,.66,z))
    for x in [-1.3,0,1.32]:
        place("wall-front",(x,.66,-1.34))
    for x,z in [(-2.51,-.41),(-2.11,-1.08),(-1.40,-1.50),(.10,-1.72),(2.28,-.97)]:
        place("cypress",(x,.66,z))
    for x,z in [(-2.25,.98),(-.46,1.61),(2.53,.21),(2.25,-.71),(.15,-1.36)]:
        place("sapling",(x,.66,z))
    for x,z in [(-2.91,1.28),(-3.05,.2),(-2.69,-1.28),(2.92,.87),(2.72,-1.58),
                (-1.90,1.82),(1.67,1.97),(-3.51,.37),(3.51,-.27)]:
        place("reefs",(x,-.07,z))
    # The jetty leaves the island at the level of the gate. All deck segments share the same mesh.
    for i in range(17):
        place("pier-board",(.32,.64,1.43+i*.202),"boards")
    for i in range(5):
        for x in [-.18,.82]:
            pz = 1.58+i*.75
            place("mooring-post",(x,-.015,pz),"boards")
            place("column-cap",(x,.935,pz),"copper")
    boat = (2.43,.036,2.55)
    velocity = [.09,0,.48]
    for part,mat in [("cutter-hull","enamel"),("mast","copper"),("mainsail","linen"),("jib","enamel")]:
        place(part,boat,mat,velocity)
    place("wake",boat,"foam",velocity)

    return {"camera":{"position":[6.45,4.42,10.9],"target":[0,1.88,.25],"fov_degrees":39,
                      "lens_radius":.018,"focus_distance":12.5,"shutter_open":0,"shutter_close":.45},
            "materials":materials,"objects":objects,
            "lights":[{"type":"area","origin":[-6,8,3],"u":[3,0,0],"v":[0,0,3],"radiance":[22,15,8.5]},
                      {"type":"point","position":[2.4,5.2,-3.2],"intensity":[19,25,37]}]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--castle-zip", type=Path, help="rebuild the bundled CC0 model derivatives")
    args = parser.parse_args()
    ASSETS.mkdir(parents=True,exist_ok=True)
    if args.castle_zip:
        import_kit(args.castle_zip)
    terrain()
    details()
    scene = assemble()
    for obj in scene["objects"]:
        if not (SCENE.parent / obj["mesh"]).is_file():
            raise FileNotFoundError(obj["mesh"])
    # Keep each placement on one line so that adjacent pieces can be compared in code review.
    material_lines = [f"    {json.dumps(name)}: {json.dumps(value)}" for name,value in scene["materials"].items()]
    object_lines = ["    " + json.dumps(obj) for obj in scene["objects"]]
    light_lines = ["    " + json.dumps(light) for light in scene["lights"]]
    lines = ["{", '  "camera": ' + json.dumps(scene["camera"]) + ",", '  "materials": {',
             ",\n".join(material_lines), "  },", '  "objects": [', ",\n".join(object_lines),
             "  ],", '  "lights": [', ",\n".join(light_lines), "  ]", "}"]
    write_text(SCENE, "\n".join(lines))
    print(f"{SCENE.name}: {len(scene['objects'])} objects, {len(scene['materials'])} materials")


if __name__ == "__main__":
    main()
