#pragma once
#include "CoreMinimal.h"
#include "RenderController.generated.h"


class ULevelSequence;
class UMoviePipelineExecutorJob;
class UObject;


UCLASS()
class JOBTESTPROJECTEDITOR_API UMetaHumanRenderController : public UObject
{
	GENERATED_BODY()

public:
	// 进行配置
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Render")
	static void ConfigureRenderJob(
		UMoviePipelineExecutorJob* Job,
		const FString& OutputDirectory,
		const FString& OutputFilename,
		int32 WarmUpFrames);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Render")
	static bool RenderSequence(
		UObject* WorldContextObject,
		ULevelSequence* Sequence,
		const FString& OutputDirectory,
		const FString& OutputFilename,
		int32 WarmUpFrames);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Render")
	static bool RenderStateFromJson(
		UObject* WorldContextObject,
		const FString& JsonPath,
		const FString& OutputDirectory,
		int32 WarmUpFrames);

	// MRQ结束回调
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Render")
	static bool QuitEditorOnRenderFinished();

	// 主入口
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Render")
	static bool RenderHeadless(
		UObject* WorldContextObject,
		const FString& JsonPath,
		const FString& OutputDirectory,
		int32 WarmUpFrames);
};
