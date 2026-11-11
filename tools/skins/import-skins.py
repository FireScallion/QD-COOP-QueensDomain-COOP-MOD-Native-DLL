import unreal,json,hashlib,os
from pathlib import Path
R=Path(os.environ['QDCOOP_SKIN_WORK']).resolve()
lib=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
skpath='/Game/Player/Assets/FPModel/Player2026/Player_FullChar_2026_Skeleton'
sk=lib.load_asset(skpath);assert sk
skfile=R/'editor-project/Content/Player/Assets/FPModel/Player2026/Player_FullChar_2026_Skeleton.uasset'
before=hashlib.sha256(skfile.read_bytes()).hexdigest()
def placeholder(name):
 p='/Game/NPCs/Knight/'+name
 if not lib.does_asset_exist(p):tools.create_asset(name,'/Game/NPCs/Knight',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
 return lib.load_asset(p)
mats=[placeholder(n)for n in ['Knight_Body_00_MI','Knight_head_02_MI','Knight_Helmet_00_MI']]
report={'engine':unreal.SystemLibrary.get_engine_version(),'meshes':[],'portraits':[]}
for i in range(2):
 name='SK_Knight%02d'%i;o=unreal.FbxImportUI()
 for k,v in dict(automated_import_should_detect_type=False,import_as_skeletal=True,import_materials=False,import_textures=False,create_physics_asset=False,mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH,import_mesh=True,import_animations=False,skeleton=sk).items():o.set_editor_property(k,v)
 sd=o.get_editor_property('skeletal_mesh_import_data')
 for k,v in dict(convert_scene=True,convert_scene_unit=True,force_front_x_axis=False,update_skeleton_reference_pose=False,use_t0_as_ref_pose=False).items():sd.set_editor_property(k,v)
 t=unreal.AssetImportTask()
 for k,v in dict(filename=str(R/'fbx'/(name+'.fbx')),destination_path='/Game/QDCoopSkins',destination_name=name,automated=True,replace_existing=True,save=True,options=o).items():t.set_editor_property(k,v)
 tools.import_asset_tasks([t]);mesh=lib.load_asset('/Game/QDCoopSkins/'+name);assert isinstance(mesh,unreal.SkeletalMesh)
 assert mesh.get_editor_property('skeleton')==sk
 slots=mesh.get_editor_property('materials');assert len(slots)==3,[str(s.material_slot_name)for s in slots]
 for j,m in enumerate(slots):
  slot=str(m.material_slot_name);index=0 if slot=='QD_Body'else(1 if slot=='QD_Face'else 2)
  m.set_editor_property('material_interface',mats[index])
  slots[j]=m
 mesh.set_editor_property('materials',slots);mesh.set_editor_property('physics_asset',None)
 # FBX changes local bone axes even with an identical hierarchy. The vertices
 # are already warped to the prince's component-space bind shape; canonicalize
 # their bind matrices by bone name to play the existing animation data exactly.
 before_bind=json.loads(unreal.QDSkinTools.match_bind_pose(mesh,True))
 after_bind=json.loads(unreal.QDSkinTools.match_bind_pose(mesh,False))
 assert all(r['positionError']<.001 and r['rotationErrorDegrees']<.06 and r['scaleError']<.001 for r in after_bind)
 lib.save_loaded_asset(mesh,only_if_is_dirty=False)
 report['meshes'].append({'name':name,'skeleton':sk.get_path_name(),'slots':[str(s.material_slot_name)for s in slots],'canonicalBind':True,'previousMaxRotationError':max(r['rotationErrorDegrees']for r in before_bind)})
for name in ['Prince','Arjaot','Ippibi','Ujji','Imba']:
 t=unreal.AssetImportTask()
 for k,v in dict(filename=str(R/'portraits'/(name+'.png')),destination_path='/Game/QDCoopSkins/Portraits',destination_name=name,automated=True,replace_existing=True,save=True).items():t.set_editor_property(k,v)
 tools.import_asset_tasks([t]);tex=lib.load_asset('/Game/QDCoopSkins/Portraits/'+name);assert isinstance(tex,unreal.Texture2D)
 tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON);tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI);tex.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS);tex.set_editor_property('srgb',True);lib.save_loaded_asset(tex,only_if_is_dirty=False);report['portraits'].append(tex.get_path_name())
lib.save_directory('/Game',only_if_is_dirty=True,recursive=True)
after=hashlib.sha256(skfile.read_bytes()).hexdigest();report['originalSkeletonUnchanged']=before==after
assert before==after,'Import modified shared skeleton'
(R/'import-result.json').write_text(json.dumps(report,indent=2))
print('SKINS_IMPORT_OK '+json.dumps(report))
