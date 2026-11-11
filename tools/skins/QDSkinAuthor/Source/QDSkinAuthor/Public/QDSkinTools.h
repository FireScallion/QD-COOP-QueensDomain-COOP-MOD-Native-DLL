#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QDSkinTools.generated.h"
class USkeletalMesh;
UCLASS()
class QDSKINAUTHOR_API UQDSkinTools:public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="QDSkinAuthor") static FString MatchBindPose(USkeletalMesh* Mesh,bool Apply);
};
