#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "State.h"
#include "StateSerial.generated.h"


//FCharacterStat-Json序列化逻辑
UCLASS()
class JOBTESTPROJECTEDITOR_API UMetaHumanStateSerial : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|State")
	static bool ExportStateToJson(const FCharacterState& State, const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|State")
	static bool ImportStateFromJson(const FString& FilePath, FCharacterState& OutState);
};
