"""Offline skin binding to the already-shipped prince skeleton; no runtime retargeter.
Inputs: locally extracted game meshes, target reference transforms and semantic mapping.
Outputs: two helmet meshes, neutral portraits, and an inspectable Blender scene.
"""
import bpy,json,math,struct,os
from pathlib import Path
from mathutils import Matrix,Vector,Quaternion
R=Path(os.environ['QDCOOP_SKIN_WORK']).resolve();R.mkdir(parents=True,exist_ok=True)
INPUT=Path(os.environ['QDCOOP_SKIN_INPUTS']).resolve()
KN=INPUT/'Knight'
PR=INPUT/'Player/Player2026'
DATA=json.loads((INPUT/'target-reference.json').read_text())
mapping=json.loads((Path(__file__).resolve().parent/'bone-map.json').read_text())['entries']
bpy.ops.wm.read_factory_settings(use_empty=True)
S=bpy.context.scene;S.unit_settings.system='METRIC';S.unit_settings.scale_length=.01
C=Matrix.Diagonal((1,-1,1,1))
def matrix(t):
 x,y,z,w=t['q'];return C@Matrix.LocRotScale(Vector(t['p']),Quaternion((w,x,y,z)),Vector(t['s']))@C
ad=bpy.data.armatures.new('QD_SkinRig');arm=bpy.data.objects.new('Armature',ad);S.collection.objects.link(arm)
bpy.context.view_layer.objects.active=arm;arm.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
rest={}
for b in DATA['bones']:
 local=matrix(b);p=DATA['bones'][b['parent']]['name']if b['parent']>=0 else None
 absolute=rest[p]@local if p else local;rest[b['name']]=absolute
 eb=ad.edit_bones.new(b['name']);eb.matrix=absolute;eb.length=1
 if p:eb.parent=ad.edit_bones[p]
bpy.ops.object.mode_set(mode='OBJECT');arm.select_set(False)
def import_mesh(file):
 before=set(S.objects);bpy.ops.import_scene.gltf(filepath=str(file));objs=set(S.objects)-before
 return next(o for o in objs if o.type=='MESH' and o.name.startswith(file.stem)),next((o for o in objs if o.type=='ARMATURE'),None),objs
body,old,objects=import_mesh(KN/'Knight_mesh.glb')
src={b.name:(old.matrix_world@b.head_local) for b in old.data.bones}
dst={n:t.translation.copy()for n,t in rest.items()}
# Do not collapse three torso joints onto two. Preserve the armour's shape along
# the torso while assigning its weights to the corresponding existing spine.
anchors={n:dst[t].copy()for n,t in mapping.items()}
for n in ['Spine_000','Spine_001','Spine_002']:
 a=(src[n].z-src['Pelvis'].z)/(src['Neck'].z-src['Pelvis'].z)
 anchors[n]=dst['root_x'].lerp(dst['neck_x'],a)
anchors['Helmet']=anchors['Head']+(src['Helmet']-src['Head'])
ends={'Root':'Pelvis','Pelvis':'Spine_000','Spine_000':'Spine_001','Spine_001':'Spine_002','Spine_002':'Neck','Neck':'Head','Head':'Helmet'}
for s in ['L','R']:
 for a,b in [('Clavicle','UpperArm'),('UpperArm','LowerArm'),('LowerArm','Wrist'),('Wrist','Hand'),('Hand','Finger_Middle_000'),('Thigh','Calf'),('Calf','Foot'),('Foot','Toe')]:ends[a+'_'+s]=b+'_'+s
 for f in ['Pinky','Ring','Middle','Index','Thumb']:
  for i in range(2):ends[f'Finger_{f}_{i:03}_{s}']=f'Finger_{f}_{i+1:03}_{s}'
warp={}
for n in mapping:
 if n in ends:
  d=ends[n];v=src[d]-src[n];w=anchors[d]-anchors[n]
  q=v.rotation_difference(w)if min(v.length,w.length)>.001 else Quaternion()
 else:
  parent=old.data.bones.get(n).parent;pn=parent.name if parent else None;q=warp[pn][1]if pn in warp else Quaternion()
 warp[n]=(anchors[n],q)
def deform(p,n):a,q=warp[n];return a+q@(p-src[n])
print('BIND_MESH',body.name,len(body.vertex_groups),[[(g.group,g.weight)for g in v.groups]for v in list(body.data.vertices)[:3]]);weights=[];matrix_body=body.matrix_world.copy()
for v in body.data.vertices:
 p=(matrix_body@v.co);ws=[(body.vertex_groups[g.group].name,g.weight)for g in v.groups if g.weight>0]
 assert ws and all(n in mapping for n,w in ws),(v.index,ws,list(mapping))
 total=sum(w for _,w in ws);v.co=sum((deform(p,n)*(w/total)for n,w in ws),Vector())
 out={}
 for n,w in ws:out[mapping[n]]=out.get(mapping[n],0)+w/total
 weights.append(out)
body.parent=None;body.matrix_world=Matrix.Identity(4);body.modifiers.clear();body.vertex_groups.clear()
groups={n:body.vertex_groups.new(name=n)for n in set(mapping.values())}
for v,ws in zip(body.data.vertices,weights):
 for n,w in ws.items():groups[n].add([v.index],w,'REPLACE')
body.modifiers.new('Armature','ARMATURE').object=arm;body.parent=arm
for o in objects:
 if o!=body:bpy.data.objects.remove(o,do_unlink=True)
def material(name,texture):
 m=bpy.data.materials.new(name);m.use_nodes=True;n=m.node_tree.nodes;p=n.get('Principled BSDF');p.inputs['Roughness'].default_value=.75;p.inputs['Specular IOR Level'].default_value=.2
 t=n.new('ShaderNodeTexImage');t.image=bpy.data.images.load(str(texture),check_existing=True);t.interpolation='Closest';m.node_tree.links.new(t.outputs['Color'],p.inputs['Base Color']);return m
body.data.materials[0]=material('QD_Body',KN/'Knight_Body_00_TX.png');face=material('QD_Face',KN/'Knight_Face_02_TX.png');body.data.materials[1]=face
variants=[]
for i in range(2):
 hat,_,added=import_mesh(KN/f'Helmet_{i:02}_SM.glb');added_names=[o.name for o in added];mat=hat.matrix_world.copy()
 for v in hat.data.vertices:v.co=deform((mat@v.co),'Head')
 hat.parent=None;hat.matrix_world=Matrix.Identity(4);hat.modifiers.clear();hat.vertex_groups.clear();hat.vertex_groups.new(name='head_x').add(list(range(len(hat.data.vertices))),1,'REPLACE')
 hat.data.materials.clear();hat.data.materials.append(material('QD_Helmet',KN/'Knight_Helmet_00_TX.png'))
 copy=body.copy();copy.data=body.data.copy();S.collection.objects.link(copy)
 bpy.ops.object.select_all(action='DESELECT');copy.select_set(True);hat.select_set(True);bpy.context.view_layer.objects.active=copy;bpy.ops.object.join()
 copy.name=f'SK_Knight{i:02}';variants.append(copy)
 for name in added_names:
  if name in S.objects:bpy.data.objects.remove(S.objects[name],do_unlink=True)
bpy.data.objects.remove(body,do_unlink=True)
(R/'fbx').mkdir(exist_ok=True);(R/'portraits').mkdir(exist_ok=True)
for o in variants:
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);arm.select_set(True);bpy.context.view_layer.objects.active=arm
 bpy.ops.export_scene.fbx(filepath=str(R/'fbx'/(o.name+'.fbx')),use_selection=True,object_types={'ARMATURE','MESH'},global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',bake_space_transform=False,add_leaf_bones=False,use_armature_deform_only=False,armature_nodetype='NULL',primary_bone_axis='Y',secondary_bone_axis='X',bake_anim=False)
 o.hide_render=True
# Original prince geometry uses the exact same bind pose; preserve original slots.
prince,pa,objs=import_mesh(PR/'Player_FullChar_2026.glb');pm=prince.matrix_world.copy()
for v in prince.data.vertices:v.co=(pm@v.co)
prince.parent=None;prince.matrix_world=Matrix.Identity(4);prince.modifiers.clear();prince.modifiers.new('Armature','ARMATURE').object=arm;prince.parent=arm
for o in objs:
 if o!=prince:bpy.data.objects.remove(o,do_unlink=True)
for m in prince.data.materials:
 if m and m.use_nodes:
  p=m.node_tree.nodes.get('Principled BSDF')
  if p:p.inputs['Roughness'].default_value=.75
prince.hide_render=True
for i,(name,tex) in enumerate([('PrinceFace',PR/'PlayerFace_26_tx.png'),('PrinceHair',PR/'playerhair26_tx.png'),('PrinceArms',PR.parent/'PlayerArm_00.png'),('PrinceArmor',PR.parent/'PlayerArmor_00_TX.png')]):
 prince.data.materials[i]=material(name,tex)
# Hair_26_MI uses a two-sided opacity mask with a clip value of 0.3333.
hair=prince.data.materials[1].node_tree
hair_texture=next(node for node in hair.nodes if node.type=='TEX_IMAGE')
hair_mask=hair.nodes.new('ShaderNodeMath')
hair_mask.operation='GREATER_THAN'
hair_mask.inputs[1].default_value=.3333
hair.links.new(hair_texture.outputs['Alpha'],hair_mask.inputs[0])
hair.links.new(hair_mask.outputs[0],hair.nodes['Principled BSDF'].inputs['Alpha'])
# Both models use precisely the same relaxed arm pose, not their NPC poses.
for side in ['l','r']:
 n='arm_stretch_'+side;child='forearm_stretch_'+side
 # About 13.5 degrees of outward arm opening clears the armour and skirt.
 pb=arm.pose.bones[n];q=(rest[child].translation-rest[n].translation).rotation_difference(Vector((.24 if side=='l'else-.24,0,-1)))
 pb.rotation_mode='QUATERNION';pb.rotation_quaternion=pb.bone.matrix_local.to_quaternion().inverted()@q@pb.bone.matrix_local.to_quaternion()
 bpy.context.view_layer.update()
 print('NEUTRAL',n,'reference',list(rest[n].translation),'rest-end',list(rest[child].translation),'end',list(arm.pose.bones[child].matrix.translation),'desired',list(q@(rest[child].translation-rest[n].translation)))
bpy.context.view_layer.update()
def set_camera():
 camdata=bpy.data.cameras.new('Camera');cam=bpy.data.objects.new('Camera',camdata);S.collection.objects.link(cam);S.camera=cam
 cam.location=(165,-470,165);target=Vector((0,0,92));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();camdata.type='ORTHO';camdata.ortho_scale=210
set_camera();world=bpy.data.worlds.new('Studio');S.world=world;world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.15,.19,.23,1);world.node_tree.nodes['Background'].inputs[1].default_value=.55
for name,loc,energy,size in [('key',(200,-300,400),3500000,400),('fill',(-300,-100,200),1800000,300),('rim',(0,300,300),3000000,300)]:
 d=bpy.data.lights.new(name,'AREA');d.energy=energy;d.shape='DISK';d.size=size;o=bpy.data.objects.new(name,d);S.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector((0,0,100))-o.location).to_track_quat('-Z','Y').to_euler()
S.render.engine='CYCLES';S.cycles.samples=24;S.cycles.use_denoising=True;S.render.film_transparent=True;S.render.resolution_x=600;S.render.resolution_y=800;S.render.resolution_percentage=100;S.view_settings.view_transform='Standard';S.view_settings.look='None';S.render.image_settings.file_format='PNG';S.render.image_settings.color_mode='RGBA'
def render(name):S.render.filepath=str(R/'portraits'/(name+'.png'));bpy.ops.render.render(write_still=True)
prince.hide_render=False;render('Prince');prince.hide_render=True
for name,index,faceidx in [('Arjaot',0,2),('Ippibi',0,4),('Ujji',0,6),('Imba',1,5)]:
 o=variants[index];o.hide_render=False;next(n for n in face.node_tree.nodes if n.type=='TEX_IMAGE').image=bpy.data.images.load(str(KN/f'Knight_Face_{faceidx:02}_TX.png'),check_existing=True);render(name);o.hide_render=True
variants[0].hide_render=False
bpy.ops.wm.save_as_mainfile(filepath=str(R/'skins.blend'))
report={'sourceBones':58,'targetBones':len(ad.bones),'meshes':[{'name':o.name,'vertices':len(o.data.vertices),'materials':[m.name for m in o.data.materials]}for o in variants],'mappedVertices':len(weights),'allWeightsNormalized':all(abs(sum(w.values())-1)<1e-5 for w in weights),'menuPose':'same relaxed arms-down pose','engineValidationPending':True}
(R/'binding-report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))





