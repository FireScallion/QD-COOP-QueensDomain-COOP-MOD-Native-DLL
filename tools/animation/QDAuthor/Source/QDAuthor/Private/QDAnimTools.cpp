#include "QDAnimTools.h"
#include "Modules/ModuleManager.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_LocalRefPose.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_Slot.h"
#include "AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Animation/AnimCurveCompressionSettings.h"
#include "AnimGraphNode_ApplyMeshSpaceAdditive.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
IMPLEMENT_MODULE(FDefaultModuleImpl,QDAuthor)
namespace {
template<class T,class F> T* Make(UEdGraph* G,F Init) {FGraphNodeCreator<T> C(*G);T* N=C.CreateNode();Init(N);C.Finalize();return N;}
template<class T> T* Make(UEdGraph* G) {return Make<T>(G,[](T*){});}
UEdGraphPin* Out(UEdGraphNode* N) {for(auto* P:N->Pins)if(P->Direction==EGPD_Output)return P;return nullptr;}
void Wire(UEdGraphNode* A,UEdGraphNode* B,const TCHAR* Input) {
 auto* P=B->FindPin(FName(Input));checkf(P,TEXT("Missing pin %s on %s"),Input,*B->GetClass()->GetName());
 checkf(B->GetGraph()->GetSchema()->TryCreateConnection(Out(A),P),TEXT("Cannot link %s to %s.%s"),*A->GetClass()->GetName(),*B->GetClass()->GetName(),Input);
}
void Expose(UAnimGraphNode_Base* N,const TCHAR* Name) {for(auto& P:N->ShowPinForProperties)if(P.PropertyName==Name)P.bShowPin=true;N->ReconstructNode();}
void Var(UAnimBlueprint* B,UEdGraph* G,UEdGraphNode* N,const TCHAR* Pin,const TCHAR* Name,bool Rotation=false,const TCHAR* Default=TEXT("0")) {
 FEdGraphPinType T;T.PinCategory=Rotation?UEdGraphSchema_K2::PC_Struct:UEdGraphSchema_K2::PC_Float;if(Rotation)T.PinSubCategoryObject=TBaseStructure<FRotator>::Get();
 FBlueprintEditorUtils::AddMemberVariable(B,Name,T,Rotation?FString():FString(Default));
 auto* V=Make<UK2Node_VariableGet>(G,[&](auto* X){X->VariableReference.SetSelfMember(Name);});Wire(V,N,Pin);
}
UAnimSequence* Anim(const TCHAR* N) {auto* A=LoadObject<UAnimSequence>(nullptr,*(FString(TEXT("/Game/QDCoopNative/Animations/"))+N));check(A);return A;}
void Save(UObject* O) {auto* P=O->GetOutermost();P->MarkPackageDirty();FAssetRegistryModule::AssetCreated(O);FString File=FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension());check(UPackage::SavePackage(P,O,RF_Public|RF_Standalone,*File,GError,nullptr,false,true,SAVE_NoError));}
void Montage(USkeleton* S,const TCHAR* Name,bool Charge) {
 FString Package=FString(TEXT("/Game/QDCoopNative/Montages/"))+Name;auto* P=CreatePackage(*Package);
 auto* M=NewObject<UAnimMontage>(P,Name,RF_Public|RF_Standalone);M->SetSkeleton(S);auto* A=Anim(Name);
 FSlotAnimationTrack Slot;Slot.SlotName=TEXT("QDUpper");FAnimSegment Seg;Seg.AnimReference=A;Seg.AnimStartTime=0;Seg.AnimEndTime=A->SequenceLength;Seg.AnimPlayRate=1;Seg.LoopingCount=1;Slot.AnimTrack.AnimSegments.Add(Seg);M->SlotAnimTracks.Add(Slot);
 M->SequenceLength=M->CalculateSequenceLength();M->BlendIn.SetBlendTime(.01f);M->BlendOut.SetBlendTime(Charge?0.f:.25f);M->bEnableAutoBlendOut=!Charge;M->AddAnimCompositeSection(TEXT("Default"),0);
 if(Charge){M->AddAnimCompositeSection(TEXT("Loop"),.85f);M->CompositeSections[0].NextSectionName=TEXT("Loop");M->CompositeSections[1].NextSectionName=TEXT("Loop");}
 Save(M);
}
}
FString UQDAnimTools::BuildSpear() {
 auto* S=LoadObject<USkeleton>(nullptr,TEXT("/Game/Player/Assets/FPModel/Player2026/Player_FullChar_2026_Skeleton"));if(!S)return TEXT("missing original-index skeleton");S->RegisterSlotNode(TEXT("QDUpper"));Save(S);
 auto* P=CreatePackage(TEXT("/Game/QDCoopNative/ABP_QDSpear"));auto* Factory=NewObject<UAnimBlueprintFactory>();Factory->TargetSkeleton=S;Factory->ParentClass=UAnimInstance::StaticClass();
 auto* B=Cast<UAnimBlueprint>(Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(),P,TEXT("ABP_QDSpear"),RF_Public|RF_Standalone,nullptr,GWarn));check(B);
 UEdGraph* G=nullptr;UAnimGraphNode_Root* Root=nullptr;for(auto* Graph:B->FunctionGraphs)for(auto* N:Graph->Nodes)if(auto* R=Cast<UAnimGraphNode_Root>(N)){G=Graph;Root=R;}check(G&&Root);
 auto* Ref=Make<UAnimGraphNode_LocalRefPose>(G);
 auto Player=[&](const TCHAR* Clip){return Make<UAnimGraphNode_SequencePlayer>(G,[&](auto* N){N->Node.Sequence=Anim(Clip);N->Node.bLoopAnimation=true;});};
 auto* Walk=Player(TEXT("Move_walk"));auto* Run=Player(TEXT("Move_run"));Walk->Node.GroupName=TEXT("QDMove");Run->Node.GroupName=TEXT("QDMove");
 Expose(Walk,TEXT("PlayRate"));Expose(Run,TEXT("PlayRate"));Var(B,G,Walk,TEXT("PlayRate"),TEXT("WalkRate"),false,TEXT("1"));Var(B,G,Run,TEXT("PlayRate"),TEXT("RunRate"),false,TEXT("1"));
 auto Blend=[&](UEdGraphNode* A,UEdGraphNode* Z,const TCHAR* V){auto* N=Make<UAnimGraphNode_TwoWayBlend>(G);Wire(A,N,TEXT("A"));Wire(Z,N,TEXT("B"));Var(B,G,N,TEXT("Alpha"),V);return N;};
 auto* Move=Blend(Ref,Blend(Walk,Run,TEXT("RunBlend")),TEXT("MoveWeight"));
 auto* Jump=Make<UAnimGraphNode_SequenceEvaluator>(G,[&](auto* N){N->Node.Sequence=Anim(TEXT("Move_jump"));N->Node.bTeleportToExplicitTime=true;});Var(B,G,Jump,TEXT("ExplicitTime"),TEXT("JumpTime"));
 auto* Ground=Blend(Move,Jump,TEXT("JumpBlend"));
 auto* Cache=Make<UAnimGraphNode_SaveCachedPose>(G,[](auto* N){N->CacheName=TEXT("QDLocomotion");});Wire(Ground,Cache,TEXT("Pose"));
 auto Use=[&](){return Make<UAnimGraphNode_UseCachedPose>(G,[&](auto* N){N->SaveCachedPoseNode=Cache;});};
 auto* ToCS=Make<UAnimGraphNode_LocalToComponentSpace>(G);Wire(Use(),ToCS,TEXT("LocalPose"));
 auto* Turn=Make<UAnimGraphNode_ModifyBone>(G,[](auto* N){N->Node.BoneToModify.BoneName=TEXT("root_x");N->Node.RotationMode=BMM_Additive;N->Node.RotationSpace=BCS_ComponentSpace;});Wire(ToCS,Turn,TEXT("ComponentPose"));Var(B,G,Turn,TEXT("Rotation"),TEXT("LowerTurn"),true);
 auto* FromCS=Make<UAnimGraphNode_ComponentToLocalSpace>(G);Wire(Turn,FromCS,TEXT("ComponentPose"));
 auto Layer=[&](UEdGraphNode* Base,UEdGraphNode* Overlay,TArray<FName> Bones){auto* N=Make<UAnimGraphNode_LayeredBoneBlend>(G,[&](auto* X){X->Node.bMeshSpaceRotationBlend=true;X->Node.BlendWeights[0]=1;for(FName Bone:Bones){FBranchFilter F;F.BoneName=Bone;F.BlendDepth=1;X->Node.LayerSetup[0].BranchFilters.Add(F);}});Wire(Base,N,TEXT("BasePose"));Wire(Overlay,N,TEXT("BlendPoses_0"));return N;};
 auto* Lower=Layer(FromCS,Use(),{TEXT("spine_01_x")});
 auto* Idle=Player(TEXT("Spear_Idle"));auto* Slot=Make<UAnimGraphNode_Slot>(G,[](auto* N){N->Node.SlotName=TEXT("QDUpper");});Wire(Idle,Slot,TEXT("Source"));
 auto* Body=Layer(Lower,Slot,{TEXT("arm_stretch_l"),TEXT("arm_stretch_r")});
 auto* HeadCS=Make<UAnimGraphNode_LocalToComponentSpace>(G);Wire(Body,HeadCS,TEXT("LocalPose"));
 auto* Head=Make<UAnimGraphNode_ModifyBone>(G,[](auto* N){N->Node.BoneToModify.BoneName=TEXT("head_x");N->Node.RotationMode=BMM_Replace;N->Node.RotationSpace=BCS_ComponentSpace;});Wire(HeadCS,Head,TEXT("ComponentPose"));Var(B,G,Head,TEXT("Rotation"),TEXT("HeadRotation"),true);
 auto* HeadLocal=Make<UAnimGraphNode_ComponentToLocalSpace>(G);Wire(Head,HeadLocal,TEXT("ComponentPose"));Wire(HeadLocal,Root,TEXT("Result"));
 FKismetEditorUtilities::CompileBlueprint(B);if(B->Status==BS_Error)return TEXT("animation graph compile failed");Save(B);
 Montage(S,TEXT("Spear_Charge"),true);Montage(S,TEXT("Spear_Stab_1"),false);Montage(S,TEXT("Spear_Stab_2"),false);Montage(S,TEXT("Spear_Stab_3"),false);
 return FString::Printf(TEXT("OK %d nodes; standard engine runtime only"),G->Nodes.Num());
}

namespace {
TSharedPtr<FJsonObject> ReadManifest(const FString& File) {
 FString Text;check(FFileHelper::LoadFileToString(Text,*File));TSharedPtr<FJsonObject> J;check(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J));return J;
}
UAnimSequence* AllAnim(const FString& Name) {return LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/QDCoopBody/Animations/")+Name));}
void ObjectVar(UAnimBlueprint* B,UEdGraph* G,UAnimGraphNode_Base* N,const TCHAR* Pin,const TCHAR* Name,UClass* Class,UObject* Default) {
 Expose(N,Pin);FEdGraphPinType T;T.PinCategory=UEdGraphSchema_K2::PC_Object;T.PinSubCategoryObject=Class;
 FBlueprintEditorUtils::AddMemberVariable(B,Name,T,Default?Default->GetPathName():FString());
 auto* V=Make<UK2Node_VariableGet>(G,[&](auto* X){X->VariableReference.SetSelfMember(Name);});Wire(V,N,Pin);
}
}

FString UQDAnimTools::ImportAll(const FString& Manifest) {
 auto J=ReadManifest(Manifest);auto* S=LoadObject<USkeleton>(nullptr,TEXT("/Game/Player/Assets/FPModel/Player2026/Player_FullChar_2026_Skeleton"));check(S);
 const auto& Names=J->GetArrayField(TEXT("bones"));check(Names.Num()==S->GetReferenceSkeleton().GetNum());int32 Count=0;
 // This private authoring placeholder has the original index order, but its FBX
 // bind transforms are not the game's reference pose. Additive compression must
 // subtract the actual game pose; the placeholder skeleton is never packaged.
 {
  TArray<uint8> Ref;check(FFileHelper::LoadFileToArray(Ref,*J->GetStringField(TEXT("referenceFile"))));check(Ref.Num()==Names.Num()*40);
  FReferenceSkeletonModifier Modifier(const_cast<FReferenceSkeleton&>(S->GetReferenceSkeleton()),S);
  for(int32 b=0;b<Names.Num();b++){float X[10];FMemory::Memcpy(X,Ref.GetData()+b*40,40);check(S->GetReferenceSkeleton().GetBoneName(b)==FName(*Names[b]->AsString()));Modifier.UpdateRefPoseTransform(b,FTransform(FQuat(X[3],X[4],X[5],X[6]).GetNormalized(),FVector(X[0],X[1],X[2]),FVector(X[7],X[8],X[9])));}
 }
 auto* BC=LoadObject<UAnimBoneCompressionSettings>(nullptr,TEXT("/Game/QDCoopNative/Compression/QD_bone"));auto* CC=LoadObject<UAnimCurveCompressionSettings>(nullptr,TEXT("/Game/QDCoopNative/Compression/QD_curve"));check(BC&&CC);
 for(auto V:J->GetArrayField(TEXT("clips"))) {
  auto C=V->AsObject();FString Name=C->GetStringField(TEXT("name"));int32 Frames=C->GetIntegerField(TEXT("frames"));TArray<uint8> Data;check(FFileHelper::LoadFileToArray(Data,*C->GetStringField(TEXT("file"))));check(Data.Num()==Frames*Names.Num()*40);
  auto* P=CreatePackage(*(TEXT("/Game/QDCoopBody/Animations/")+Name));auto* A=NewObject<UAnimSequence>(P,*Name,RF_Public|RF_Standalone);A->SetSkeleton(S);A->SequenceLength=C->GetNumberField(TEXT("duration"));A->SetRawNumberOfFrame(Frames);A->BoneCompressionSettings=BC;A->CurveCompressionSettings=CC;
  for(int32 b=0;b<Names.Num();b++) {
   FName Bone(*Names[b]->AsString());check(S->GetReferenceSkeleton().FindBoneIndex(Bone)==b);FRawAnimSequenceTrack T;
   T.PosKeys.Reserve(Frames);T.RotKeys.Reserve(Frames);T.ScaleKeys.Reserve(Frames);
   for(int32 f=0;f<Frames;f++){float X[10];FMemory::Memcpy(X,Data.GetData()+(f*Names.Num()+b)*40,40);T.PosKeys.Add(FVector(X[0],X[1],X[2]));T.RotKeys.Add(FQuat(X[3],X[4],X[5],X[6]).GetNormalized());T.ScaleKeys.Add(FVector(X[7],X[8],X[9]));}
   check(A->AddNewRawTrack(Bone,&T)==b);
  }
  if(C->GetBoolField(TEXT("additive"))){A->AdditiveAnimType=AAT_RotationOffsetMeshSpace;A->RefPoseType=ABPT_RefPose;}
  A->PostProcessSequence();A->RequestSyncAnimRecompression();Save(A);Count++;
 }
 for(auto V:J->GetArrayField(TEXT("clips"))) {
  auto C=V->AsObject();if(C->GetStringField(TEXT("type"))!=TEXT("AnimMontage"))continue;
  FString Name=C->GetStringField(TEXT("name"));auto* A=AllAnim(Name);check(A);auto* P=CreatePackage(*(TEXT("/Game/QDCoopBody/Montages/")+Name));auto* M=NewObject<UAnimMontage>(P,*Name,RF_Public|RF_Standalone);M->SetSkeleton(S);
  check(C->GetArrayField(TEXT("slots")).Num()==1);FName SlotName(*(TEXT("QD_")+C->GetArrayField(TEXT("slots"))[0]->AsString()));S->RegisterSlotNode(SlotName);
  FSlotAnimationTrack Slot;Slot.SlotName=SlotName;FAnimSegment Seg;Seg.AnimReference=A;Seg.AnimEndTime=A->SequenceLength;Seg.AnimPlayRate=1;Seg.LoopingCount=1;Slot.AnimTrack.AnimSegments.Add(Seg);M->SlotAnimTracks.Add(Slot);
  M->SequenceLength=M->CalculateSequenceLength();M->BlendIn.SetBlendTime(C->GetNumberField(TEXT("blendIn")));M->BlendOut.SetBlendTime(C->GetNumberField(TEXT("blendOut")));M->bEnableAutoBlendOut=C->GetBoolField(TEXT("autoBlendOut"));
  for(auto SV:C->GetArrayField(TEXT("sections"))){auto Section=SV->AsObject();int32 i=M->AddAnimCompositeSection(FName(*Section->GetStringField(TEXT("name"))),Section->GetNumberField(TEXT("time"))*A->SequenceLength/C->GetNumberField(TEXT("sourceDuration")));check(i!=INDEX_NONE);M->CompositeSections[i].NextSectionName=FName(*Section->GetStringField(TEXT("next")));}
  Save(M);
 }
 Save(S);return FString::Printf(TEXT("OK imported %d original-index sequences and all source montage sections; no gameplay notifies"),Count);
}

FString UQDAnimTools::BuildAll(const FString& Manifest) {
 auto J=ReadManifest(Manifest);TMap<FString,FString> Paths;
 for(auto V:J->GetArrayField(TEXT("clips"))){auto C=V->AsObject();FString Path;if(C->TryGetStringField(TEXT("path"),Path))Paths.Add(Path,C->GetStringField(TEXT("name")));}
 auto Find=[&](const TCHAR* Suffix){for(auto& Pair:Paths)if(Pair.Key.EndsWith(Suffix))return AllAnim(Pair.Value);return (UAnimSequence*)nullptr;};
 auto* S=LoadObject<USkeleton>(nullptr,TEXT("/Game/Player/Assets/FPModel/Player2026/Player_FullChar_2026_Skeleton"));check(S);
 auto* P=CreatePackage(TEXT("/Game/QDCoopBody/ABP_QDBody"));auto* F=NewObject<UAnimBlueprintFactory>();F->TargetSkeleton=S;F->ParentClass=UAnimInstance::StaticClass();auto* B=Cast<UAnimBlueprint>(F->FactoryCreateNew(UAnimBlueprint::StaticClass(),P,TEXT("ABP_QDBody"),RF_Public|RF_Standalone,nullptr,GWarn));check(B);
 UEdGraph* G=nullptr;UAnimGraphNode_Root* Root=nullptr;for(auto* Graph:B->FunctionGraphs)for(auto* N:Graph->Nodes)if(auto* R=Cast<UAnimGraphNode_Root>(N)){G=Graph;Root=R;}check(G&&Root);
 auto Player=[&](UAnimSequence* A){check(A);return Make<UAnimGraphNode_SequencePlayer>(G,[&](auto* N){N->Node.Sequence=A;N->Node.bLoopAnimation=true;});};
 auto Blend=[&](UEdGraphNode* A,UEdGraphNode* Z,const TCHAR* V){auto* N=Make<UAnimGraphNode_TwoWayBlend>(G);Wire(A,N,TEXT("A"));Wire(Z,N,TEXT("B"));Var(B,G,N,TEXT("Alpha"),V);return N;};
 auto Layer=[&](UEdGraphNode* Base,UEdGraphNode* Overlay,TArray<FName> Bones){auto* N=Make<UAnimGraphNode_LayeredBoneBlend>(G,[&](auto* X){X->Node.bMeshSpaceRotationBlend=true;X->Node.BlendWeights[0]=1;for(FName Bone:Bones){FBranchFilter F;F.BoneName=Bone;F.BlendDepth=1;X->Node.LayerSetup[0].BranchFilters.Add(F);}});Wire(Base,N,TEXT("BasePose"));Wire(Overlay,N,TEXT("BlendPoses_0"));return N;};
 auto Slot=[&](UEdGraphNode* Base,const TCHAR* Name){S->RegisterSlotNode(Name);auto* N=Make<UAnimGraphNode_Slot>(G,[&](auto* X){X->Node.SlotName=Name;X->Node.bAlwaysUpdateSourcePose=true;});Wire(Base,N,TEXT("Source"));return N;};
 auto Add=[&](UEdGraphNode* Base,UEdGraphNode* Overlay,const TCHAR* Weight){auto* N=Make<UAnimGraphNode_ApplyMeshSpaceAdditive>(G);Wire(Base,N,TEXT("Base"));Wire(Overlay,N,TEXT("Additive"));Var(B,G,N,TEXT("Alpha"),Weight);return N;};
 auto* Ref=Make<UAnimGraphNode_LocalRefPose>(G);auto* Walk=Player(AllAnim(TEXT("Move_walk")));auto* Run=Player(AllAnim(TEXT("Move_run")));Walk->Node.GroupName=TEXT("QDMove");Run->Node.GroupName=TEXT("QDMove");Expose(Walk,TEXT("PlayRate"));Expose(Run,TEXT("PlayRate"));Var(B,G,Walk,TEXT("PlayRate"),TEXT("WalkRate"),false,TEXT("1"));Var(B,G,Run,TEXT("PlayRate"),TEXT("RunRate"),false,TEXT("1"));
 auto* Move=Blend(Ref,Blend(Walk,Run,TEXT("RunBlend")),TEXT("MoveWeight"));auto* Jump=Make<UAnimGraphNode_SequenceEvaluator>(G,[&](auto* N){N->Node.Sequence=AllAnim(TEXT("Move_jump"));N->Node.bTeleportToExplicitTime=true;});Var(B,G,Jump,TEXT("ExplicitTime"),TEXT("JumpTime"));
 auto* Ground=Blend(Move,Jump,TEXT("JumpBlend"));auto* Cache=Make<UAnimGraphNode_SaveCachedPose>(G,[](auto* N){N->CacheName=TEXT("QDLocomotion");});Wire(Ground,Cache,TEXT("Pose"));auto Use=[&](){return Make<UAnimGraphNode_UseCachedPose>(G,[&](auto* N){N->SaveCachedPoseNode=Cache;});};
 auto* ToCS=Make<UAnimGraphNode_LocalToComponentSpace>(G);Wire(Use(),ToCS,TEXT("LocalPose"));auto* Turn=Make<UAnimGraphNode_ModifyBone>(G,[](auto* N){N->Node.BoneToModify.BoneName=TEXT("root_x");N->Node.RotationMode=BMM_Additive;N->Node.RotationSpace=BCS_ComponentSpace;});Wire(ToCS,Turn,TEXT("ComponentPose"));Var(B,G,Turn,TEXT("Rotation"),TEXT("LowerTurn"),true);auto* FromCS=Make<UAnimGraphNode_ComponentToLocalSpace>(G);Wire(Turn,FromCS,TEXT("ComponentPose"));auto* Lower=Layer(FromCS,Use(),{TEXT("spine_01_x")});
 auto* RootAnim=Find(TEXT(".Player_FPArms_Base_RootPose"));check(RootAnim);auto* Right=Player(RootAnim);ObjectVar(B,G,Right,TEXT("Sequence"),TEXT("RootSequence"),UAnimSequence::StaticClass(),RootAnim);
 auto* Left=Player(RootAnim);ObjectVar(B,G,Left,TEXT("Sequence"),TEXT("LeftSequence"),UAnimSequence::StaticClass(),RootAnim);
 auto* RightIdle=Player(Find(TEXT(".Player_FPArms_Base_Idle")));auto* LeftIdle=Player(Find(TEXT(".Player_FPArms_LeftHand_Idle")));
 auto* RightBase=Add(Right,RightIdle,TEXT("IdleWeight"));auto* LeftBase=Add(Left,LeftIdle,TEXT("LeftIdleWeight"));
 auto* RightWalk=Player(Find(TEXT(".Player_FPArms_Base_Walk")));auto* LeftWalk=Player(Find(TEXT(".Player_FPArms_LeftHand_Walk")));
 auto* RightMoving=Add(RightBase,RightWalk,TEXT("ArmWalkWeight"));auto* LeftMoving=Add(LeftBase,LeftWalk,TEXT("LeftWalkWeight"));
 auto* RSlot=Slot(Slot(Slot(RightMoving,TEXT("QD_DefaultSlot")),TEXT("QD_RightArmDetailDefault")),TEXT("QD_RightArmDetailFromLeftArmControl"));
 auto* LSlot=Slot(Slot(Slot(LeftMoving,TEXT("QD_LeftArmDefault")),TEXT("QD_LeftArmDetailDefault")),TEXT("QD_LeftArmInAirAdditivesDefault"));
 auto* Upper=Layer(Layer(Ref,RSlot,{TEXT("arm_stretch_r")}),LSlot,{TEXT("arm_stretch_l")});auto* Both=Slot(Slot(Slot(Upper,TEXT("QD_BothArmsSlot")),TEXT("QD_Jump")),TEXT("QD_DeathSlot"));auto* Body=Layer(Lower,Both,{TEXT("arm_stretch_l"),TEXT("arm_stretch_r")});
 auto* HeadCS=Make<UAnimGraphNode_LocalToComponentSpace>(G);Wire(Body,HeadCS,TEXT("LocalPose"));auto* Head=Make<UAnimGraphNode_ModifyBone>(G,[](auto* N){N->Node.BoneToModify.BoneName=TEXT("head_x");N->Node.RotationMode=BMM_Replace;N->Node.RotationSpace=BCS_ComponentSpace;});Wire(HeadCS,Head,TEXT("ComponentPose"));Var(B,G,Head,TEXT("Rotation"),TEXT("HeadRotation"),true);auto* HeadLocal=Make<UAnimGraphNode_ComponentToLocalSpace>(G);Wire(Head,HeadLocal,TEXT("ComponentPose"));
 auto* Life=Make<UAnimGraphNode_SequenceEvaluator>(G,[&](auto* N){N->Node.Sequence=AllAnim(TEXT("QD_DownedEnter_Proof"));N->Node.bTeleportToExplicitTime=true;});ObjectVar(B,G,Life,TEXT("Sequence"),TEXT("LifeSequence"),UAnimSequence::StaticClass(),AllAnim(TEXT("QD_DownedEnter_Proof")));Var(B,G,Life,TEXT("ExplicitTime"),TEXT("LifeTime"));auto* Final=Blend(HeadLocal,Life,TEXT("LifeWeight"));Wire(Final,Root,TEXT("Result"));
 FKismetEditorUtilities::CompileBlueprint(B);if(B->Status==BS_Error)return TEXT("body animation graph compile failed");Save(B);Save(S);return FString::Printf(TEXT("OK %d standard engine nodes; independent montage slots and native lifecycle poses"),G->Nodes.Num());
}
