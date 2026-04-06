"""
Blender → ESP32 OLED Wireframe Sender
======================================
"""

import bpy
import struct
import math
import time
from bpy.props import (StringProperty, IntProperty, EnumProperty,
                       BoolProperty, FloatProperty, PointerProperty)
DEFAULT_PORT    = "COM3"           # Serial port
DEFAULT_BAUD    = 115200
DEFAULT_UDP_IP  = "192.168.1.100"  # ESP32 IP address (UDP mode only)
UDP_PORT        = 4210
MAX_VERTS       = 128              # OLED is tiny — keep this small
MAX_EDGES       = 200
SEND_FPS        = 20               # Target frames per second
# ─────────────────────────────────────────────

MAGIC      = b'\xAB\xCD'
_last_send = 0.0

def get_wireframe_data(obj, max_verts, max_edges):
    """
    Returns (verts, edges) from the active mesh object.
    verts = list of (x, y, z) in world space (float)
    edges = list of (a, b) index pairs (int)
    Decimates automatically if mesh exceeds limits.
    """
    if obj is None or obj.type != 'MESH':
        return [], []

    depsgraph = bpy.context.evaluated_depsgraph_get()
    obj_eval  = obj.evaluated_get(depsgraph)
    mesh      = obj_eval.data
    mat       = obj.matrix_world

    # Transform all verts to world space
    all_verts = [mat @ v.co for v in mesh.vertices]
    all_edges = [(e.vertices[0], e.vertices[1]) for e in mesh.edges]

    # Decimate verts by stride if over limit
    if len(all_verts) > max_verts:
        step     = math.ceil(len(all_verts) / max_verts)
        keep     = set(range(0, len(all_verts), step))
        remap    = {}
        new_v    = []
        for i, v in enumerate(all_verts):
            if i in keep:
                remap[i] = len(new_v)
                new_v.append(v)
        all_verts = new_v
        all_edges = [
            (remap[a], remap[b])
            for (a, b) in all_edges
            if a in remap and b in remap
        ]

    # Decimate edges by stride if still over limit
    if len(all_edges) > max_edges:
        step      = math.ceil(len(all_edges) / max_edges)
        all_edges = all_edges[::step]

    verts_out = [(v.x, v.y, v.z) for v in all_verts]
    return verts_out, all_edges


def build_packet(verts, edges):
    """Pack verts + edges into a compact binary frame."""
    buf  = bytearray(MAGIC)
    buf += struct.pack('<HH', len(verts), len(edges))
    for (x, y, z) in verts:
        buf += struct.pack('<fff', x, y, z)
    for (a, b) in edges:
        buf += struct.pack('<HH', a, b)
    return bytes(buf)



def open_serial(port, baud):
    try:
        import serial
        s = serial.Serial(port, baud, timeout=0.1)
        time.sleep(1.5)  # Give ESP32 time to reset after DTR toggle
        print(f"[WireframeSender] Serial opened: {port} @ {baud} baud")
        return s
    except ImportError:
        print("[WireframeSender] ERROR: pyserial not installed. Run: pip install pyserial")
    except Exception as e:
        print(f"[WireframeSender] Serial open failed: {e}")
    return None


def open_udp(ip, port):
    import socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(False)
    print(f"[WireframeSender] UDP socket ready → {ip}:{port}")
    return (sock, ip, port)


def send_packet(transport, mode, data):
    try:
        if mode == 'SERIAL':
            transport.write(data)
        else:
            sock, ip, port = transport
            sock.sendto(data, (ip, port))
    except Exception as e:
        print(f"[WireframeSender] Send error: {e}")


def close_transport(transport, mode):
    try:
        if mode == 'SERIAL':
            transport.close()
        else:
            transport[0].close()
    except Exception:
        pass



class WIREFRAME_OT_Start(bpy.types.Operator):
    bl_idname  = "wireframe.start"
    bl_label   = "Start Wireframe Sender"
    bl_options = {'REGISTER'}

    _timer     = None
    _transport = None
    _mode      = 'SERIAL'

    def modal(self, context, event):
        global _last_send
        props = context.scene.wf_props

        # Stop if user clicked Stop or closed panel
        if not props.is_running:
            self.cancel(context)
            return {'CANCELLED'}

        if event.type == 'TIMER':
            now     = time.time()
            min_gap = 1.0 / max(props.fps, 1)
            if now - _last_send < min_gap:
                return {'PASS_THROUGH'}
            _last_send = now

            obj = context.active_object
            verts, edges = get_wireframe_data(obj, props.max_verts, props.max_edges)

            if verts:
                packet = build_packet(verts, edges)
                send_packet(self._transport, self._mode, packet)
                props.status = f"OK  {len(verts)}v  {len(edges)}e  {len(packet)} bytes"
            else:
                props.status = "Select a mesh object"

        return {'PASS_THROUGH'}

    def execute(self, context):
        global _last_send
        props      = context.scene.wf_props
        self._mode = props.mode

        if self._mode == 'SERIAL':
            self._transport = open_serial(props.port, DEFAULT_BAUD)
        else:
            self._transport = open_udp(props.udp_ip, UDP_PORT)

        if self._transport is None:
            self.report({'ERROR'}, "Could not open connection — check port/IP in the panel.")
            props.is_running = False
            return {'CANCELLED'}

        props.is_running = True
        props.status     = "Running…"
        _last_send       = 0.0

        wm           = context.window_manager
        self._timer  = wm.event_timer_add(1.0 / 60, window=context.window)
        wm.modal_handler_add(self)
        return {'RUNNING_MODAL'}

    def cancel(self, context):
        wm = context.window_manager
        if self._timer:
            wm.event_timer_remove(self._timer)
        if self._transport:
            close_transport(self._transport, self._mode)
        props            = context.scene.wf_props
        props.is_running = False
        props.status     = "Stopped"
        print("[WireframeSender] Stopped.")


class WIREFRAME_OT_Stop(bpy.types.Operator):
    bl_idname = "wireframe.stop"
    bl_label  = "Stop Sender"

    def execute(self, context):
        context.scene.wf_props.is_running = False
        return {'FINISHED'}


class WireframeProps(bpy.types.PropertyGroup):
    mode: EnumProperty(
        name="Connection",
        items=[
            ('SERIAL', 'Serial / USB', 'Connect via USB cable'),
            ('UDP',    'UDP / WiFi',   'Connect via WiFi UDP'),
        ],
        default='SERIAL',
    )
    port:      StringProperty(name="Serial Port", default=DEFAULT_PORT)
    udp_ip:    StringProperty(name="ESP32 IP",    default=DEFAULT_UDP_IP)
    max_verts: IntProperty(name="Max Vertices", default=MAX_VERTS, min=4,  max=256)
    max_edges: IntProperty(name="Max Edges",    default=MAX_EDGES, min=4,  max=512)
    fps:       IntProperty(name="Send FPS",     default=SEND_FPS,  min=1,  max=60)
    is_running: BoolProperty(default=False)
    status:    StringProperty(default="Idle")


class WIREFRAME_PT_Panel(bpy.types.Panel):
    bl_label       = "Wireframe Sender"
    bl_idname      = "WIREFRAME_PT_Panel"
    bl_space_type  = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category    = "Wireframe Sender"

    def draw(self, context):
        layout = self.layout
        props  = context.scene.wf_props

        # Connection settings
        box = layout.box()
        box.label(text="Connection", icon='LINKED')
        box.prop(props, "mode", expand=True)
        if props.mode == 'SERIAL':
            box.prop(props, "port")
        else:
            box.prop(props, "udp_ip")

        # Stream settings
        box2 = layout.box()
        box2.label(text="Stream Settings", icon='MOD_WIREFRAME')
        row = box2.row()
        row.prop(props, "max_verts")
        row.prop(props, "max_edges")
        box2.prop(props, "fps")

        layout.separator()

        # Start / Stop
        if not props.is_running:
            layout.operator("wireframe.start", text="▶  Start Sending", icon='PLAY')
        else:
            layout.operator("wireframe.stop",  text="■  Stop",          icon='PAUSE')

        # Status
        box3 = layout.box()
        icon = 'CHECKMARK' if "OK" in props.status else 'INFO'
        box3.label(text=props.status, icon=icon)



_classes = [
    WireframeProps,
    WIREFRAME_OT_Start,
    WIREFRAME_OT_Stop,
    WIREFRAME_PT_Panel,
]

def register():
    for cls in _classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.wf_props = PointerProperty(type=WireframeProps)
    print("[WireframeSender] Registered. Open the N-panel in the 3D Viewport.")

def unregister():
    for cls in reversed(_classes):
        bpy.utils.unregister_class(cls)
    del bpy.types.Scene.wf_props

if __name__ == "__main__":
    # Allow re-running the script in Blender without "already registered" errors
    try:
        unregister()
    except Exception:
        pass
    register()
