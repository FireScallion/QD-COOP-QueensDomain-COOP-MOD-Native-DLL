// Stock UMG bindings for the verified Steam Win64 build. Game thread only.
namespace TitleUi {
using Address=std::uintptr_t;
constexpr Address object_array=0x4a64260,name_pool=0x4a27f00,weak_assign=0x11bb360;

enum Symbol {
    title_type,button_type,vertical_box_type,text_type,border_type,
    widget_library,create_widget,gameplay,spawn_object,text_library,make_text,set_text,
    in_viewport,get_parent,child_count,child_at,add_child,add_vertical,remove_parent,
    text_style,button_style,brush_color,horizontal,vertical,click,focus,
    panel_type,options_type,image_type,size_type,get_visibility,set_visibility,height,
    title_options,title_load,title_back,load_slot,options_close,options_graph,button_graph,title_graph,
    level_name,open_level,edit_type,edit_get,edit_set,text_to_string,
    culture_library,get_language,local_pawn,kismet_library,saved_directory,
    make_soft_path,soft_object,load_asset,texture_type,set_texture,scale_type,set_stretch,set_enabled,
    slot_padding,slot_size,slot_horizontal,slot_vertical,canvas_type,add_canvas,canvas_position,canvas_get_position,
    canvas_anchors,canvas_alignment,canvas_size,horizontal_box_type,size_width,size_horizontal,size_vertical,
    title_load_saves,title_refresh_slots,canvas_layout,canvas_set_layout,
    horizontal_slot_padding,horizontal_slot_size,horizontal_slot_vertical,scale_slot_horizontal,stretch_direction,
    steam_friends_default,steam_get_friends,steam_friend_avatar,
    notice_justify,notice_zorder,
    symbol_count
};

struct Spec { const char* path;const char* leaf; };
constexpr std::array<Spec,symbol_count> specs{{
    {"/Game/UI/Title/TitleScreenUI.TitleScreenUI_C","TitleScreenUI_C"},
    {"/Game/UI/UI_Style/FS_Button.FS_Button_C","FS_Button_C"},
    {"/Script/UMG.VerticalBox","VerticalBox"},
    {"/Script/CommonUI.CommonTextBlock","CommonTextBlock"},
    {"/Script/UMG.Border","Border"},
    {"/Script/UMG.Default__WidgetBlueprintLibrary","Default__WidgetBlueprintLibrary"},
    {"/Script/UMG.WidgetBlueprintLibrary.Create","Create"},
    {"/Script/Engine.Default__GameplayStatics","Default__GameplayStatics"},
    {"/Script/Engine.GameplayStatics.SpawnObject","SpawnObject"},
    {"/Script/Engine.Default__KismetTextLibrary","Default__KismetTextLibrary"},
    {"/Script/Engine.KismetTextLibrary.Conv_StringToText","Conv_StringToText"},
    {"/Script/UMG.TextBlock.SetText","SetText"},
    {"/Script/UMG.UserWidget.IsInViewport","IsInViewport"},
    {"/Script/UMG.Widget.GetParent","GetParent"},
    {"/Script/UMG.PanelWidget.GetChildrenCount","GetChildrenCount"},
    {"/Script/UMG.PanelWidget.GetChildAt","GetChildAt"},
    {"/Script/UMG.PanelWidget.AddChild","AddChild"},
    {"/Script/UMG.VerticalBox.AddChildToVerticalBox","AddChildToVerticalBox"},
    {"/Script/UMG.Widget.RemoveFromParent","RemoveFromParent"},
    {"/Script/CommonUI.CommonTextBlock.SetStyle","SetStyle"},
    {"/Script/CommonUI.CommonButtonBase.SetStyle","SetStyle"},
    {"/Script/UMG.Border.SetBrushColor","SetBrushColor"},
    {"/Script/UMG.Border.SetHorizontalAlignment","SetHorizontalAlignment"},
    {"/Script/UMG.Border.SetVerticalAlignment","SetVerticalAlignment"},
    {"/Game/UI/UI_Style/FS_Button.FS_Button_C.BP_OnClicked","BP_OnClicked"},
    {"/Script/UMG.Widget.SetFocus","SetFocus"},
    {"/Script/UMG.PanelWidget","PanelWidget"},
    {"/Game/UI/Menus/OptionsMenu.OptionsMenu_C","OptionsMenu_C"},
    {"/Script/UMG.Image","Image"},
    {"/Script/UMG.SizeBox","SizeBox"},
    {"/Script/UMG.Widget.GetVisibility","GetVisibility"},
    {"/Script/UMG.Widget.SetVisibility","SetVisibility"},
    {"/Script/UMG.SizeBox.SetHeightOverride","SetHeightOverride"},
    {"/Game/UI/Title/TitleScreenUI.TitleScreenUI_C.BndEvt__TitleScreenUI_Options_K2Node_ComponentBoundEvent_4_CommonButtonBaseClicked__DelegateSignature",
     "BndEvt__TitleScreenUI_Options_K2Node_ComponentBoundEvent_4_CommonButtonBaseClicked__DelegateSignature"},
    {"/Game/UI/Title/TitleScreenUI.TitleScreenUI_C.BndEvt__TitleScreenUI_LoadGameButton_K2Node_ComponentBoundEvent_0_CommonButtonBaseClicked__DelegateSignature",
     "BndEvt__TitleScreenUI_LoadGameButton_K2Node_ComponentBoundEvent_0_CommonButtonBaseClicked__DelegateSignature"},
    {"/Game/UI/Title/TitleScreenUI.TitleScreenUI_C.BackToMainMenu","BackToMainMenu"},
    {"/Game/UI/Title/TitleScreenUI.TitleScreenUI_C.Load Slot","Load Slot"},
    {"/Game/UI/Menus/OptionsMenu.OptionsMenu_C.CloseMenu","CloseMenu"},
    {"/Game/UI/Menus/OptionsMenu.OptionsMenu_C.ExecuteUbergraph_OptionsMenu","ExecuteUbergraph_OptionsMenu"},
    {"/Game/UI/UI_Style/FS_Button.FS_Button_C.ExecuteUbergraph_FS_Button","ExecuteUbergraph_FS_Button"},
    {"/Game/UI/Title/TitleScreenUI.TitleScreenUI_C.ExecuteUbergraph_TitleScreenUI","ExecuteUbergraph_TitleScreenUI"},
    {"/Script/Engine.GameplayStatics.GetCurrentLevelName","GetCurrentLevelName"},
    {"/Script/Engine.GameplayStatics.OpenLevel","OpenLevel"},
    {"/Script/UMG.EditableTextBox","EditableTextBox"},
    {"/Script/UMG.EditableTextBox.GetText","GetText"},
    {"/Script/UMG.EditableTextBox.SetText","SetText"},
    {"/Script/Engine.KismetTextLibrary.Conv_TextToString","Conv_TextToString"},
    {"/Script/Engine.Default__KismetInternationalizationLibrary","Default__KismetInternationalizationLibrary"},
    {"/Script/Engine.KismetInternationalizationLibrary.GetCurrentLanguage","GetCurrentLanguage"},
    {"/Script/Engine.GameplayStatics.GetPlayerPawn","GetPlayerPawn"},
    {"/Script/Engine.Default__KismetSystemLibrary","Default__KismetSystemLibrary"},
    {"/Script/Engine.KismetSystemLibrary.GetProjectSavedDirectory","GetProjectSavedDirectory"},
    {"/Script/Engine.KismetSystemLibrary.MakeSoftObjectPath","MakeSoftObjectPath"},
    {"/Script/Engine.KismetSystemLibrary.Conv_SoftObjPathToSoftObjRef","Conv_SoftObjPathToSoftObjRef"},
    {"/Script/Engine.KismetSystemLibrary.LoadAsset_Blocking","LoadAsset_Blocking"},
    {"/Script/Engine.Texture2D","Texture2D"},
    {"/Script/UMG.Image.SetBrushFromTexture","SetBrushFromTexture"},
    {"/Script/UMG.ScaleBox","ScaleBox"},
    {"/Script/UMG.ScaleBox.SetStretch","SetStretch"},
    {"/Script/UMG.Widget.SetIsEnabled","SetIsEnabled"},
    {"/Script/UMG.VerticalBoxSlot.SetPadding","SetPadding"},
    {"/Script/UMG.VerticalBoxSlot.SetSize","SetSize"},
    {"/Script/UMG.VerticalBoxSlot.SetHorizontalAlignment","SetHorizontalAlignment"},
    {"/Script/UMG.VerticalBoxSlot.SetVerticalAlignment","SetVerticalAlignment"},
    {"/Script/UMG.CanvasPanel","CanvasPanel"},
    {"/Script/UMG.CanvasPanel.AddChildToCanvas","AddChildToCanvas"},
    {"/Script/UMG.CanvasPanelSlot.SetPosition","SetPosition"},
    {"/Script/UMG.CanvasPanelSlot.GetPosition","GetPosition"},
    {"/Script/UMG.CanvasPanelSlot.SetAnchors","SetAnchors"},
    {"/Script/UMG.CanvasPanelSlot.SetAlignment","SetAlignment"},
    {"/Script/UMG.CanvasPanelSlot.SetSize","SetSize"},
    {"/Script/UMG.HorizontalBox","HorizontalBox"},
    {"/Script/UMG.SizeBox.SetWidthOverride","SetWidthOverride"},
    {"/Script/UMG.SizeBoxSlot.SetHorizontalAlignment","SetHorizontalAlignment"},
    {"/Script/UMG.SizeBoxSlot.SetVerticalAlignment","SetVerticalAlignment"},
    {"/Game/UI/Title/TitleScreenUI.TitleScreenUI_C.LoadSaves","LoadSaves"},
    {"/Game/UI/Title/TitleScreenUI.TitleScreenUI_C.RefreshSlots","RefreshSlots"},
    {"/Script/UMG.CanvasPanelSlot.GetLayout","GetLayout"},
    {"/Script/UMG.CanvasPanelSlot.SetLayout","SetLayout"},
    {"/Script/UMG.HorizontalBoxSlot.SetPadding","SetPadding"},
    {"/Script/UMG.HorizontalBoxSlot.SetSize","SetSize"},
    {"/Script/UMG.HorizontalBoxSlot.SetVerticalAlignment","SetVerticalAlignment"},
    {"/Script/UMG.ScaleBoxSlot.SetHorizontalAlignment","SetHorizontalAlignment"},
    {"/Script/UMG.ScaleBox.SetStretchDirection","SetStretchDirection"},
    {"/Script/SteamBridge.Default__SteamFriends","Default__SteamFriends"},
    {"/Script/SteamBridge.SteamFriends.GetSteamFriends","GetSteamFriends"},
    {"/Script/SteamBridge.SteamFriends.GetFriendAvatar","GetFriendAvatar"},
    {"/Script/UMG.TextLayoutWidget.SetJustification","SetJustification"},
    {"/Script/UMG.CanvasPanelSlot.SetZOrder","SetZOrder"}
}};

template<class T>T at(Address base,unsigned offset){
    return UE::field<T>(reinterpret_cast<const void*>(base),offset);
}

void event(Address target,Address fn,void* parameters){
    using Event=void(*)(void*,void*,void*);
    const auto table=at<Address>(target,0);
    at<Event>(table,0x220)(reinterpret_cast<void*>(target),reinterpret_cast<void*>(fn),parameters);
}

struct Param { const char* name;const char* kind;unsigned offset,size; };
using ValueLifetime=void(*)(const void*,void*);
struct String {const wchar_t* data{};int count{},capacity{};};
using Text=std::array<unsigned char,24>;
using Margin=std::array<float,4>;
struct CanvasLayout {Margin offsets,anchors;std::array<float,2> alignment;};
struct ChildSize {float value;unsigned char rule;unsigned char padding[3]{};};
static_assert(sizeof(CanvasLayout)==40&&sizeof(ChildSize)==8);
struct ValueBinding {
    Address property{};
    ValueLifetime initialize{},destroy{};
    void init(void* value) const {initialize(reinterpret_cast<const void*>(property),value);}
    void release(void* value) const {destroy(reinterpret_cast<const void*>(property),value);}
};
struct Bindings {
    Address image{};
    std::array<UE::WeakKey,symbol_count> symbols{};
    UE::WeakKey active_title{};
    unsigned cursor{},title_cursor{},title_size{};
    Address text_result{},level_result{},edit_result{},converted_result{},language_result{},saved_result{};
    ValueBinding path_value{},soft_value{};
    bool ready{};

    Address get(UE::WeakKey key) const {return UE::resolve_weak(image+object_array,key,read_local);}
    Address get(Symbol id) const {return get(symbols[id]);}
    void remember(UE::WeakKey& key,Address object) const {
        reinterpret_cast<void(*)(UE::WeakKey*,const void*)>(image+weak_assign)(&key,reinterpret_cast<const void*>(object));
    }
    bool is(Address object,Address type) const {
        return UE::is_a(reinterpret_cast<void*>(object),reinterpret_cast<void*>(type));
    }
    bool kind(Address prop,std::string_view expected) const {
        const auto type=at<Address>(prop,8);
        std::string leaf;
        return type&&UE::name_string(image+name_pool,at<UE::NameKey>(type,0),read_local,leaf)&&leaf==expected;
    }
    Address member(Address object,const char* name,const char* property_kind,unsigned width) const {
        if(!object)return 0;
        const auto type=at<Address>(object,16);
        const auto field=UE::find_inherited_property(type,image+name_pool,name,read_local);
        if(!field||at<int>(field,56)!=1||at<unsigned>(field,60)!=width||!kind(field,property_kind))return 0;
        const auto offset=at<unsigned>(field,76),size=at<unsigned>(type,88);
        return width<=size&&offset<=size-width?field:0;
    }
    Address object(Address owner,const char* name,const char* property_kind="ObjectProperty") const {
        const auto property=member(owner,name,property_kind,8);
        return property?at<Address>(owner,at<unsigned>(property,76)):0;
    }
    void copy_value(Address property,Address destination,Address source) const {
        const auto copy=at<Address>(at<Address>(property,0),23*8);
        if(copy!=image+0x118e4b0)return;
        using Copy=void(*)(const void*,void*,const void*,int);
        reinterpret_cast<Copy>(copy)(reinterpret_cast<const void*>(property),reinterpret_cast<void*>(destination),
                                     reinterpret_cast<const void*>(source),1);
    }
    void copy_struct(Address destination,Address source,const char* field,unsigned size) const {
        const auto property=member(source,field,"StructProperty",size);
        if(!property||member(destination,field,"StructProperty",size)!=property)return;
        const auto offset=at<unsigned>(property,76);
        copy_value(property,destination+offset,source+offset);
    }
    bool method(Symbol id,unsigned size,std::initializer_list<Param> params) const {
        return method(get(id),size,params);
    }
    bool method(Address fn,unsigned size,std::initializer_list<Param> params) const {
        if(!fn||at<std::uint16_t>(fn,182)!=size||at<unsigned char>(fn,180)!=params.size())return false;
        for(const auto& p:params){
            const auto field=UE::find_property(fn,image+name_pool,p.name,read_local);
            if(!field||at<unsigned>(field,76)!=p.offset||at<unsigned>(field,60)!=p.size||
               at<int>(field,56)!=1||!(at<std::uint64_t>(field,64)&0x80u)||!kind(field,p.kind))return false;
        }
        return true;
    }
    bool shipping_code(Address address) const {
        MEMORY_BASIC_INFORMATION region{};
        return address&&VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region))&&region.State==MEM_COMMIT&&
            reinterpret_cast<Address>(region.AllocationBase)==image&&(region.Protect&0xf0u)!=0&&!(region.Protect&PAGE_GUARD);
    }
    bool value_binding(Symbol id,ValueBinding& value) const {
        const auto property=UE::find_property(get(id),image+name_pool,"ReturnValue",read_local);
        if(!property)return false;
        const auto table=at<Address>(property,0);
        if(!table)return false;
        const auto init=at<Address>(table,31*8),destroy=at<Address>(table,30*8);
        if(!shipping_code(init)||!shipping_code(destroy))return false;
        value={property,reinterpret_cast<ValueLifetime>(init),reinterpret_cast<ValueLifetime>(destroy)};
        return true;
    }
    bool bind() {
        if(!method(create_widget,32,{{"WorldContextObject","ObjectProperty",0,8},{"WidgetType","ClassProperty",8,8},
                                     {"OwningPlayer","ObjectProperty",16,8},{"ReturnValue","ObjectProperty",24,8}})||
           !method(spawn_object,24,{{"ObjectClass","ClassProperty",0,8},{"Outer","ObjectProperty",8,8},{"ReturnValue","ObjectProperty",16,8}})||
           !method(make_text,40,{{"inString","StrProperty",0,16},{"ReturnValue","TextProperty",16,24}})||
           !method(set_text,24,{{"InText","TextProperty",0,24}})||
           !method(in_viewport,1,{{"ReturnValue","BoolProperty",0,1}})||
           !method(get_parent,8,{{"ReturnValue","ObjectProperty",0,8}})||
           !method(child_count,4,{{"ReturnValue","IntProperty",0,4}})||
           !method(child_at,16,{{"Index","IntProperty",0,4},{"ReturnValue","ObjectProperty",8,8}})||
           !method(add_child,16,{{"Content","ObjectProperty",0,8},{"ReturnValue","ObjectProperty",8,8}})||
           !method(add_vertical,16,{{"Content","ObjectProperty",0,8},{"ReturnValue","ObjectProperty",8,8}})||
           !method(remove_parent,0,{})||!method(focus,0,{})||!method(click,0,{})||
           !method(text_style,8,{{"InStyle","ClassProperty",0,8}})||
           !method(button_style,8,{{"InStyle","ClassProperty",0,8}})||
           !method(brush_color,16,{{"InBrushColor","StructProperty",0,16}})||
           !method(horizontal,1,{{"InHorizontalAlignment","ByteProperty",0,1}})||
           !method(vertical,1,{{"InVerticalAlignment","ByteProperty",0,1}})||
           !method(get_visibility,1,{{"ReturnValue","EnumProperty",0,1}})||
           !method(set_visibility,1,{{"InVisibility","EnumProperty",0,1}})||
           !method(height,4,{{"InHeightOverride","FloatProperty",0,4}})||
           !method(title_options,8,{{"Button","ObjectProperty",0,8}})||
           !method(title_load,8,{{"Button","ObjectProperty",0,8}})||
           !method(title_back,0,{})||!method(options_close,0,{})||
           !method(load_slot,12,{{"Slot","IntProperty",0,4},{"Backup","BoolProperty",4,1},{"BackupSlot","IntProperty",8,4}})||
           !method(level_name,32,{{"WorldContextObject","ObjectProperty",0,8},{"bRemovePrefixString","BoolProperty",8,1},
                                 {"ReturnValue","StrProperty",16,16}})||
           !method(open_level,40,{{"WorldContextObject","ObjectProperty",0,8},{"LevelName","NameProperty",8,8},
                                 {"bAbsolute","BoolProperty",16,1},{"Options","StrProperty",24,16}})||
           !method(edit_get,24,{{"ReturnValue","TextProperty",0,24}})||
           !method(edit_set,24,{{"InText","TextProperty",0,24}})||
           !method(text_to_string,40,{{"InText","TextProperty",0,24},{"ReturnValue","StrProperty",24,16}})||
           !method(get_language,16,{{"ReturnValue","StrProperty",0,16}})||
           !method(local_pawn,24,{{"WorldContextObject","ObjectProperty",0,8},{"PlayerIndex","IntProperty",8,4},
                                  {"ReturnValue","ObjectProperty",16,8}})||
           !method(saved_directory,16,{{"ReturnValue","StrProperty",0,16}})||
           !method(make_soft_path,40,{{"PathString","StrProperty",0,16},{"ReturnValue","StructProperty",16,24}})||
           !method(soft_object,64,{{"SoftObjectPath","StructProperty",0,24},{"ReturnValue","SoftObjectProperty",24,40}})||
           !method(load_asset,48,{{"Asset","SoftObjectProperty",0,40},{"ReturnValue","ObjectProperty",40,8}})||
           !method(set_texture,9,{{"Texture","ObjectProperty",0,8},{"bMatchSize","BoolProperty",8,1}})||
           !method(set_stretch,1,{{"InStretch","ByteProperty",0,1}})||
           !method(canvas_layout,40,{{"ReturnValue","StructProperty",0,40}})||
           !method(canvas_set_layout,40,{{"InLayoutData","StructProperty",0,40}})||
           !method(slot_padding,16,{{"InPadding","StructProperty",0,16}})||
           !method(slot_size,8,{{"InSize","StructProperty",0,8}})||
           !method(size_width,4,{{"InWidthOverride","FloatProperty",0,4}})||
           !method(size_horizontal,1,{{"InHorizontalAlignment","ByteProperty",0,1}})||
           !method(size_vertical,1,{{"InVerticalAlignment","ByteProperty",0,1}})||
           !method(horizontal_slot_padding,16,{{"InPadding","StructProperty",0,16}})||
           !method(horizontal_slot_size,8,{{"InSize","StructProperty",0,8}})||
           !method(horizontal_slot_vertical,1,{{"InVerticalAlignment","ByteProperty",0,1}})||
           !method(scale_slot_horizontal,1,{{"InHorizontalAlignment","ByteProperty",0,1}})||
           !method(stretch_direction,1,{{"InStretchDirection","ByteProperty",0,1}}))return false;
        text_result=UE::find_property(get(make_text),image+name_pool,"ReturnValue",read_local);
        const auto text_setter=UE::find_property(get(set_text),image+name_pool,"InText",read_local);
        level_result=UE::find_property(get(level_name),image+name_pool,"ReturnValue",read_local);
        edit_result=UE::find_property(get(edit_get),image+name_pool,"ReturnValue",read_local);
        converted_result=UE::find_property(get(text_to_string),image+name_pool,"ReturnValue",read_local);
        language_result=UE::find_property(get(get_language),image+name_pool,"ReturnValue",read_local);
        saved_result=UE::find_property(get(saved_directory),image+name_pool,"ReturnValue",read_local);
        const auto table=at<Address>(text_result,0);
        return value_binding(make_soft_path,path_value)&&value_binding(soft_object,soft_value)&&
            table&&table==at<Address>(text_setter,0)&&table==at<Address>(edit_result,0)&&at<Address>(table,31*8)==image+0x11a4420&&
            at<Address>(table,30*8)==image+0x119e940&&
            at<Address>(at<Address>(level_result,0),30*8)==image+0x1170390&&
            at<Address>(at<Address>(converted_result,0),30*8)==image+0x1170390&&
            at<Address>(at<Address>(language_result,0),30*8)==image+0x1170390&&
            at<Address>(at<Address>(saved_result,0),30*8)==image+0x1170390;
    }
    bool shown(Address context) const {
        unsigned char result{};
        event(context,get(in_viewport),&result);
        return result!=0;
    }
    bool title_level(Address context,std::wstring* observed=nullptr,bool* known=nullptr) const {
        struct Args {Address context;bool strip;unsigned char padding[7];String result;};
        Args args{context,true,{},{}};
        event(get(gameplay),get(level_name),&args);
        const auto& text=args.result;
        const bool valid=text.data&&text.count>1&&text.count<=260&&text.data[text.count-1]==0;
        if(known)*known=valid;
        if(observed&&valid)
            observed->assign(text.data,static_cast<unsigned>(text.count-1));
        const bool title=valid&&text.count==13&&_wcsicmp(text.data,L"title_screen")==0;
        reinterpret_cast<ValueLifetime>(image+0x1170390)(reinterpret_cast<const void*>(level_result),&args.result);
        return title;
    }
    bool chinese() const {
        String result{};
        event(get(culture_library),get(get_language),&result);
        const bool zh=result.data&&result.count>=3&&
            (result.data[0]==L'z'||result.data[0]==L'Z')&&(result.data[1]==L'h'||result.data[1]==L'H');
        reinterpret_cast<ValueLifetime>(image+0x1170390)(reinterpret_cast<const void*>(language_result),&result);
        return zh;
    }
    std::wstring project_saved_directory() const {
        String result{};
        event(get(kismet_library),get(saved_directory),&result);
        std::wstring path;
        if(result.data&&result.count>0&&result.count<=32767&&result.data[result.count-1]==0)
            path.assign(result.data,static_cast<unsigned>(result.count-1));
        reinterpret_cast<ValueLifetime>(image+0x1170390)(reinterpret_cast<const void*>(saved_result),&result);
        return path;
    }
    Address load_asset_path(std::string_view expected) const {
        const std::wstring wide(expected.begin(),expected.end());
        const auto size=static_cast<int>(wide.size()+1);
        struct alignas(8) Path {String input;std::array<unsigned char,24> result{};};
        struct alignas(8) Ref {std::array<unsigned char,24> input{};std::array<unsigned char,40> result{};};
        struct alignas(8) Load {std::array<unsigned char,40> input{};Address result{};};
        Path path{{wide.c_str(),size,size},{}};
        Ref ref;
        Load load;
        const auto library=get(kismet_library);
        path_value.init(path.result.data());
        event(library,get(make_soft_path),&path);
        ref.input=path.result;
        soft_value.init(ref.result.data());
        event(library,get(soft_object),&ref);
        load.input=ref.result;
        event(library,get(load_asset),&load);
        soft_value.release(ref.result.data());
        path_value.release(path.result.data());
        std::string actual;
        return load.result&&UE::object_path(load.result,image+name_pool,read_local,actual)&&actual==expected?
            load.result:0;
    }
    Address portrait(std::string_view expected) const {
        const auto asset=load_asset_path(expected);
        return is(asset,get(texture_type))?asset:0;
    }
    Address friend_avatar(std::uint64_t friend_id,int handle) const {
        const auto cdo=get(steam_friends_default);
        if(!cdo||!method(steam_get_friends,8,{{"ReturnValue","ObjectProperty",0,8}})||
           !method(steam_friend_avatar,24,{{"SteamIDFriend","StructProperty",0,8},{"AvatarSize","EnumProperty",8,1},
                                          {"ImageHandle","IntProperty",12,4},{"ReturnValue","ObjectProperty",16,8}}))return 0;
        Address instance{};
        event(cdo,get(steam_get_friends),&instance);
        if(!is(instance,at<Address>(cdo,16)))return 0;
        struct Args {std::uint64_t friend_id;unsigned char size,padding[3];int handle;Address texture;};
        Args args{friend_id,0,{},0,0}; // ESteamAvatarSize::Small.
        event(instance,get(steam_friend_avatar),&args);
        return args.handle==handle&&is(args.texture,get(texture_type))?args.texture:0;
    }
    void step(unsigned budget=4096) {
        if(!image)image=reinterpret_cast<Address>(GetModuleHandleW(nullptr));
        const auto array=at<UE::ObjectArray>(image+object_array,0);
        if(!UE::object_array_shape(array))return;
        const auto size=static_cast<unsigned>(array.num_elements);
        std::string leaf,path;
        for(unsigned scanned=0;scanned<budget&&cursor<size;++scanned,++cursor){
            const auto chunk=at<Address>(array.chunks,(cursor/UE::objects_per_chunk)*8);
            if(!chunk)continue;
            const auto item=at<UE::ObjectItem>(chunk,(cursor%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
            if(!item.object||(item.flags&0x30000000u))continue;
            leaf.clear();
            path.clear();
            if(!UE::name_string(image+name_pool,at<UE::NameKey>(item.object,24),read_local,leaf))continue;
            for(unsigned i=0;i<specs.size();++i){
                if(leaf!=specs[i].leaf)continue;
                if(path.empty()&&!UE::object_path(item.object,image+name_pool,read_local,path))break;
                if(path==specs[i].path){remember(symbols[i],item.object);break;}
            }
        }
        if(cursor>=size)cursor=0;
        if(std::all_of(symbols.begin(),symbols.begin()+steam_friends_default,[&](UE::WeakKey key){return get(key)!=0;}))
            ready=bind();
    }
    Address find_title() {
        const auto array=at<UE::ObjectArray>(image+object_array,0);
        const auto type=get(title_type);
        if(!type||!UE::object_array_shape(array))return 0;
        const auto size=static_cast<unsigned>(array.num_elements);
        if(!title_cursor||size!=title_size)title_cursor=size;
        title_size=size;
        for(unsigned scanned=0;scanned<8192&&title_cursor;++scanned){
            const auto index=--title_cursor;
            const auto chunk=at<Address>(array.chunks,(index/UE::objects_per_chunk)*8);
            if(!chunk)continue;
            const auto item=at<UE::ObjectItem>(chunk,(index%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
            if(item.object&&!(item.flags&0x30000000u)&&at<Address>(item.object,16)==type&&
               !(at<unsigned>(item.object,8)&0x10u)&&shown(item.object)){
                remember(active_title,item.object);
                return item.object;
            }
        }
        return 0;
    }
    Address parent(Address widget) const {
        Address result{};
        event(widget,get(get_parent),&result);
        return result;
    }
    Address child(Address panel,int index) const {
        struct {int index;int padding;Address result;} args{index,0,0};
        event(panel,get(child_at),&args);
        return args.result;
    }
    int count(Address panel) const {
        int result{};
        event(panel,get(child_count),&result);
        return result;
    }
    Address add(Address panel,Address content,Symbol method=add_child) const {
        Address args[]{content,0};
        event(panel,get(method),args);
        return args[1];
    }
    bool insert_after(Address box,Address anchor,Address widget) const {
        if(!is(box,get(vertical_box_type))||!widget||
           !method(slot_padding,16,{{"InPadding","StructProperty",0,16}})||
           !method(slot_size,8,{{"InSize","StructProperty",0,8}})||
           !method(slot_horizontal,1,{{"InHorizontalAlignment","ByteProperty",0,1}})||
           !method(slot_vertical,1,{{"InVerticalAlignment","ByteProperty",0,1}}))return false;
        struct Layout {
            Address child{};
            std::array<unsigned char,16> padding{};
            std::array<unsigned char,8> size{};
            unsigned char h{},v{};
        };
        std::array<Layout,24> tail{};
        const auto total=count(box);
        if(total<1||static_cast<unsigned>(total)>tail.size())return false;
        unsigned used{};
        for(int i=0;i<total;++i){
            const auto item=child(box,i);
            if(!used&&item!=anchor)continue;
            const auto slot=object(item,"Slot");
            const auto padding=member(slot,"Padding","StructProperty",16);
            const auto size=member(slot,"Size","StructProperty",8);
            const auto h=member(slot,"HorizontalAlignment","ByteProperty",1);
            const auto v=member(slot,"VerticalAlignment","ByteProperty",1);
            if(!padding||!size||!h||!v)return false;
            auto& layout=tail[used++];
            layout.child=item;
            layout.padding=at<decltype(layout.padding)>(slot,at<unsigned>(padding,76));
            layout.size=at<decltype(layout.size)>(slot,at<unsigned>(size,76));
            layout.h=at<unsigned char>(slot,at<unsigned>(h,76));
            layout.v=at<unsigned char>(slot,at<unsigned>(v,76));
        }
        if(!used)return false;
        for(unsigned i=1;i<used;++i)event(tail[i].child,get(remove_parent),nullptr);
        const auto place=[&](Address item,Layout& layout){
            const auto slot=add(box,item,add_vertical);
            if(!slot)return false;
            event(slot,get(slot_padding),layout.padding.data());
            event(slot,get(slot_size),layout.size.data());
            event(slot,get(slot_horizontal),&layout.h);
            event(slot,get(slot_vertical),&layout.v);
            return true;
        };
        const bool inserted=place(widget,tail[0]);
        for(unsigned i=1;i<used;++i)place(tail[i].child,tail[i]);
        return inserted;
    }
    Address create(Address context,Address type) const {
        Address args[]{context,type,0,0};
        event(get(widget_library),get(create_widget),args);
        return args[3];
    }
    Address spawn(Address outer,Address type) const {
        Address args[]{type,outer,0};
        event(get(gameplay),get(spawn_object),args);
        return args[2];
    }
    void edit_style(Address edit,Address sample) const {
        const auto property=UE::find_inherited_property(at<Address>(edit,16),image+name_pool,"WidgetStyle",read_local);
        if(!property||!kind(property,"StructProperty"))return;
        const auto bytes=at<unsigned>(property,60);
        if(!member(edit,"WidgetStyle","StructProperty",bytes))return;
        const auto style=at<Address>(property,120);
        const auto font=UE::find_property(style,image+name_pool,"Font",read_local);
        const auto source=member(sample,"Font","StructProperty",104);
        const auto padding=UE::find_property(style,image+name_pool,"Padding",read_local);
        const auto data=edit+at<unsigned>(property,76);
        if(font&&source&&kind(font,"StructProperty")&&at<Address>(font,120)==at<Address>(source,120)&&
           bytes>=104&&at<unsigned>(font,76)<=bytes-104)
            copy_value(font,data+at<unsigned>(font,76),sample+at<unsigned>(source,76));
        if(padding&&kind(padding,"StructProperty")&&at<unsigned>(padding,60)==sizeof(Margin)&&
           bytes>=sizeof(Margin)&&at<unsigned>(padding,76)<=bytes-sizeof(Margin)){
            auto margin=at<Margin>(data,at<unsigned>(padding,76));
            margin[0]=margin[2]=16.f;
            std::memcpy(reinterpret_cast<void*>(data+at<unsigned>(padding,76)),margin.data(),sizeof(margin));
        }
    }
    Address first_text(Address widget,unsigned depth=0) const {
        if(is(widget,get(text_type)))return widget;
        if(depth==6||!is(widget,get(panel_type)))return 0;
        const auto children=count(widget);
        for(int i=0;i<children&&i<16;++i)
            if(const auto text=first_text(child(widget,i),depth+1))return text;
        return 0;
    }
    void label(Address widget,const wchar_t* text,Symbol setter=set_text) const {
        struct alignas(8) Args {String string;Text text{};};
        const auto size=static_cast<int>(std::char_traits<wchar_t>::length(text)+1);
        Args args{{text,size,size},{}};
        reinterpret_cast<ValueLifetime>(image+0x11a4420)(reinterpret_cast<const void*>(text_result),args.text.data());
        event(get(text_library),get(make_text),&args);
        event(widget,get(setter),args.text.data());
        reinterpret_cast<ValueLifetime>(image+0x119e940)(reinterpret_cast<const void*>(text_result),args.text.data());
    }
    std::wstring caption(Address widget) const {
        const auto property=member(widget,"Text","TextProperty",24);
        if(!property)return {};
        struct alignas(8) Args {Text text{};String string;};
        Args args{at<Text>(widget,at<unsigned>(property,76)),{}};
        event(get(text_library),get(text_to_string),&args);
        const auto& string=args.string;
        std::wstring value;
        if(string.data&&string.count>0&&string.count<=128&&string.data[string.count-1]==0)
            value.assign(string.data,static_cast<unsigned>(string.count-1));
        reinterpret_cast<ValueLifetime>(image+0x1170390)(reinterpret_cast<const void*>(converted_result),&args.string);
        return value;
    }
    std::wstring read_field(Address widget,unsigned max) const {
        struct alignas(8) Args {Text text{};String string;};
        Args args;
        reinterpret_cast<ValueLifetime>(image+0x11a4420)(reinterpret_cast<const void*>(edit_result),args.text.data());
        event(widget,get(edit_get),args.text.data());
        event(get(text_library),get(text_to_string),&args);
        std::wstring value;
        const auto& string=args.string;
        if(string.data&&string.count>0&&string.count<=static_cast<int>(max+1)&&string.data[string.count-1]==0)
            value.assign(string.data,static_cast<unsigned>(string.count-1));
        reinterpret_cast<ValueLifetime>(image+0x1170390)(reinterpret_cast<const void*>(converted_result),&args.string);
        reinterpret_cast<ValueLifetime>(image+0x119e940)(reinterpret_cast<const void*>(edit_result),args.text.data());
        return value;
    }
    Address button(Address context,Address source,const wchar_t* text) const {
        if(!is(source,get(button_type)))return 0;
        const auto original_body=object(source,"Subcontent");
        const auto original_text=first_text(original_body);
        if(!original_text)return 0;
        const auto original_border=parent(original_text);
        const auto button=create(context,get(button_type));
        const auto body=object(button,"Subcontent"),tree=object(button,"WidgetTree");
        if(!is(body,get(panel_type))||!tree)return 0;
        const auto border=spawn(tree,get(border_type)),caption=spawn(tree,get(text_type));
        if(!border||!caption)return 0;
        auto style=object(source,"Style","ClassProperty");
        if(style)event(button,get(button_style),&style);
        style=object(original_text,"Style","ClassProperty");
        if(style)event(caption,get(text_style),&style);
        copy_struct(caption,original_text,"Font",104);
        copy_struct(border,original_border,"Padding",16);
        const auto sound=member(source,"SpecialSounds","BoolProperty",1);
        if(sound){
            auto property=reinterpret_cast<void*>(sound);
            UE::set_bool(property,reinterpret_cast<void*>(button),
                         UE::get_bool(property,reinterpret_cast<const void*>(source),0),0);
        }
        float clear[]{1.f,1.f,1.f,0.f};
        unsigned char centered=2;
        event(border,get(brush_color),clear);
        event(border,get(horizontal),&centered);
        event(border,get(vertical),&centered);
        label(caption,text);
        return add(border,caption)&&add(body,border)?button:0;
    }
    unsigned char visibility(Address widget) const {
        unsigned char value{};
        event(widget,get(get_visibility),&value);
        return value;
    }
    void visibility(Address widget,unsigned char value) const {event(widget,get(set_visibility),&value);}
};
}
