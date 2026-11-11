#include "QDSkinTools.h"
#include "Modules/ModuleManager.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Serialization/JsonSerializer.h"
IMPLEMENT_MODULE(FDefaultModuleImpl,QDSkinAuthor)
FString UQDSkinTools::MatchBindPose(USkeletalMesh* Mesh,bool Apply){
 if(!Mesh||!Mesh->GetSkeleton())return TEXT("missing mesh/skeleton");
 auto& Ref=Mesh->GetRefSkeleton();const auto& Target=Mesh->GetSkeleton()->GetReferenceSkeleton();
 TArray<FTransform> Poses;TArray<TSharedPtr<FJsonValue>> Rows;
 for(int32 I=0;I<Ref.GetRawBoneNum();++I){
  const FName Name=Ref.GetBoneName(I);int32 J=Target.FindBoneIndex(Name);
  if(J<0)return FString(TEXT("missing bone "))+Name.ToString();
  int32 A=Ref.GetParentIndex(I),B=Target.GetParentIndex(J);
  if((A<0)!=(B<0)||(A>=0&&Ref.GetBoneName(A)!=Target.GetBoneName(B)))return FString(TEXT("parent mismatch "))+Name.ToString();
  const auto& X=Ref.GetRawRefBonePose()[I];const auto& Y=Target.GetRawRefBonePose()[J];Poses.Add(Y);
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Name.ToString());
  Row->SetNumberField(TEXT("positionError"),(X.GetTranslation()-Y.GetTranslation()).Size());
  Row->SetNumberField(TEXT("rotationErrorDegrees"),FMath::RadiansToDegrees(X.GetRotation().AngularDistance(Y.GetRotation())));
  Row->SetNumberField(TEXT("scaleError"),(X.GetScale3D()-Y.GetScale3D()).Size());Rows.Add(MakeShared<FJsonValueObject>(Row));
 }
 if(Apply){
  Mesh->Modify();FReferenceSkeletonModifier Modifier(Ref,Mesh->GetSkeleton());
  for(int32 I=0;I<Poses.Num();++I)Modifier.UpdateRefPoseTransform(I,Poses[I]);
  Mesh->SetRetargetBasePose(Ref.GetRefBonePose());Mesh->GetRefBasesInvMatrix().Empty();Mesh->CalculateInvRefMatrices();Mesh->MarkPackageDirty();
 }
 FString Result;FJsonSerializer::Serialize(Rows,TJsonWriterFactory<>::Create(&Result));return Result;
}
