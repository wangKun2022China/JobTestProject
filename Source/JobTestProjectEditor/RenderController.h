#pragma once
#include "CoreMinimal.h"
#include "RenderController.generated.h"


class ULevelSequence;
class UMoviePipelineExecutorJob;
class UObject;


// 渲染完成广播（参数：bSuccess）。编辑器面板监听它弹出右下角通知；
// 无头路径的 QuitEditorOnRenderFinished 会另行钩住 executor 并退出，两者互不影响。
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMetaHumanRenderFinished, bool /*bSuccess*/);


UCLASS()
class JOBTESTPROJECTEDITOR_API UMetaHumanRenderController : public UObject
{
	GENERATED_BODY()

public:
	// 渲染完成时广播（bSuccess）。编辑器面板监听此事件弹出右下角通知。
	static FOnMetaHumanRenderFinished OnRenderFinished;

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
