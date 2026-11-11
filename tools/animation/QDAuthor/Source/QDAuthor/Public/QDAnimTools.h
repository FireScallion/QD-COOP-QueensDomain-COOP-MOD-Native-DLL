#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QDAnimTools.generated.h"
UCLASS()
class QDAUTHOR_API UQDAnimTools: public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="QDAuthor") static FString BuildSpear();
 UFUNCTION(BlueprintCallable,Category="QDAuthor") static FString ImportAll(const FString& Manifest);
 UFUNCTION(BlueprintCallable,Category="QDAuthor") static FString BuildAll(const FString& Manifest);
};
