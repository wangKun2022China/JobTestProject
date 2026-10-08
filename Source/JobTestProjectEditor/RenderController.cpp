#include "RenderController.h"
#include "StateController.h"
#include "LevelControl.h"
#include "State.h"
#include "StateSerial.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "LevelSequence.h"
#include "Misc/Paths.h"
#include "MoviePipelineAntiAliasingSetting.h"
#include "MoviePipelineCameraSetting.h"
#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelineExecutor.h"
#include "MoviePipelineImageSequenceOutput.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelinePIEExecutor.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueEngineSubsystem.h"
#include "UObject/SoftObjectPath.h"


// 渲染完成广播的静态成员定义（编辑器面板监听它弹出右下角通知）。
FOnMetaHumanRenderFinished UMetaHumanRenderController::OnRenderFinished;


namespace
{
	// 待 PIE executor 启动后、需在该 PIE 世界内重新应用的 JSON 状态路径。
	// PIE executor 会把 /Game/Main 复制成一个全新的 PIE 世界，而该复制
	// 不会携带运行时状态（面部 morph target、身体单节点动画），
	// 因此我们在 PIE 启动后从 JSON 重新应用它。
	FString ActiveStateJsonPath;
	FDelegateHandle StateApplyHandle;

	// 在 FEditorDelegates::PostPIEStarted 上运行：找到 PIE 世界，把姿态控制器
	// 重新挂到 MetaHuman 上（它是动态组件，不随关卡复制而存活），
	// 并在第一帧渲染前重新应用 JSON 状态。
	void ApplyStateToCurrentPIEWorld(bool /*bIsSimulating*/)
	{
		if (StateApplyHandle.IsValid())
		{
			FEditorDelegates::PostPIEStarted.Remove(StateApplyHandle);
			StateApplyHandle.Reset();
		}

		if (ActiveStateJsonPath.IsEmpty())
		{
			return;
		}

		UWorld* PIEWorld = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				PIEWorld = Context.World();
				break;
			}
		}

		if (!PIEWorld)
		{
			UE_LOG(LogTemp, Error, TEXT("MetaHumanRenderController: no PIE world found to apply state."));
			return;
		}

		FCharacterState State;
		if (!UMetaHumanStateSerial::ImportStateFromJson(ActiveStateJsonPath, State))
		{
			UE_LOG(LogTemp, Error, TEXT("MetaHumanRenderController: failed to load state '%s' in PIE world."), *ActiveStateJsonPath);
			return;
		}

		UMetaHumanStateController* Controller = MetaHumanLevelControl::FindOrAddController(PIEWorld);

		if (!Controller)
		{
			UE_LOG(LogTemp, Error, TEXT("MetaHumanRenderController: no MetaHuman pose controller in PIE world."));
			return;
		}

		Controller->ApplyState(State);
		UE_LOG(LogTemp, Log, TEXT("MetaHumanRenderController: applied state to PIE actor '%s'."), *Controller->GetOwner()->GetName());

		ActiveStateJsonPath.Empty();
	}
}

void UMetaHumanRenderController::ConfigureRenderJob(
	UMoviePipelineExecutorJob* Job,
	const FString& OutputDirectory,
	const FString& OutputFilename,
	int32 WarmUpFrames)
{
	if (!Job) { return; }

	UMoviePipelinePrimaryConfig* Config = Job->GetConfiguration();
	if (!Config) { return; }

	UMoviePipelineOutputSetting* Output = Cast<UMoviePipelineOutputSetting>(Config->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass()));
	Output->OutputDirectory.Path = OutputDirectory;
	Output->FileNameFormat = FPaths::GetBaseFilename(OutputFilename);
	Output->bOverrideExistingOutput = true;
	Output->ZeroPadFrameNumbers = 4;

	// 预热帧让 Groom 头发和 Cloth 在首帧捕获前稳定下来，
	// 避免冷启动时 Groom 出现"爆炸" / 被裁剪。
	UMoviePipelineAntiAliasingSetting* AntiAliasing = Cast<UMoviePipelineAntiAliasingSetting>(Config->FindOrAddSettingByClass(UMoviePipelineAntiAliasingSetting::StaticClass()));
	AntiAliasing->EngineWarmUpCount = WarmUpFrames;
	AntiAliasing->RenderWarmUpCount = WarmUpFrames;

	// 只渲染序列 camera-cut 所指向的那台相机。
	UMoviePipelineCameraSetting* CameraSetting = Cast<UMoviePipelineCameraSetting>(Config->FindOrAddSettingByClass(UMoviePipelineCameraSetting::StaticClass()));
	CameraSetting->bRenderAllCameras = false;

	// Deferred（beauty）通道 + 无损 PNG 图像序列输出（PNG 避免了会引入
	// Imath/OpenEXR 私有第三方头文件的、易出问题的 EXR 头）。
	// 关闭"multisample 效果"（TAA / DoF / MotionBlur / Bloom / 色差）：MetaHuman 的
	// Groom 头发在单帧 PIE 渲染下与 TAA 时序历史冲突，侧面机位会出现竖向切割 /
	// 半透明噪块 / 像素化撕裂。MRQ 界面里的 "Disable Multisample Effects" 即为此开关，
	// 这里直接设为默认，免去每次手动勾选。
	UMoviePipelineDeferredPassBase* DeferredPass = Cast<UMoviePipelineDeferredPassBase>(
		Config->FindOrAddSettingByClass(UMoviePipelineDeferredPassBase::StaticClass()));
	if (DeferredPass)
	{
		DeferredPass->bDisableMultisampleEffects = true;
	}
	Config->FindOrAddSettingByClass(UMoviePipelineImageSequenceOutput_PNG::StaticClass());
}


bool UMetaHumanRenderController::RenderSequence(
	UObject* WorldContextObject,
	ULevelSequence* Sequence,
	const FString& OutputDirectory,
	const FString& OutputFilename,
	int32 WarmUpFrames)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World || !Sequence)
	{
		return false;
	}
	UMoviePipelineQueueEngineSubsystem* QueueSubsystem = GEngine->GetEngineSubsystem<UMoviePipelineQueueEngineSubsystem>();
	if (!QueueSubsystem || QueueSubsystem->IsRendering())
	{
		return false;
	}
	UMoviePipelineExecutorJob* Job = QueueSubsystem->AllocateJob(Sequence);
	if (!Job)
	{
		return false;
	}
	// AllocateJob 会从子系统自身的 GetWorld()（引擎子系统时为 null）解析地图，
	// 因此这里显式把它固定到当前关卡。PIE executor 会把该地图作为一个全新的 PIE 世界启动。
	Job->Map = FSoftObjectPath(World);

	ConfigureRenderJob(Job, OutputDirectory, OutputFilename, WarmUpFrames);

	// 在 PIE executor 复制地图之后，于 PIE 世界内重新应用 JSON 状态，
	// 因为运行时的 morph/身体状态不会在复制中保留。
	StateApplyHandle.Reset();
	StateApplyHandle =
		FEditorDelegates::PostPIEStarted.AddStatic(&ApplyStateToCurrentPIEWorld);

	// 通过 PIE executor（编辑器原生的 MRQ 路径）渲染。in-process executor 在纯编辑器
	// 会话中不可用：当 bUseCurrentLevel=true 时它需要一个 Game/PIE 世界
	// （MoviePipeline::FindCurrentWorld() 在编辑器中返回 null），而其地图重载分支
	// 在这里是无操作，因为 OpenLevel(null) 解析不到任何世界。
	UMoviePipelinePIEExecutor* PIEExecutor = NewObject<UMoviePipelinePIEExecutor>(QueueSubsystem);
	PIEExecutor->SetAllowUsingUnsavedLevels(true);
	QueueSubsystem->RenderQueueWithExecutorInstance(PIEExecutor);

	// 渲染完成时广播，供编辑器 UI 弹右下角通知。无头路径的 QuitEditorOnRenderFinished
	// 会另钩住同一 executor 的 OnExecutorFinished 并退出，两者互不影响。
	PIEExecutor->OnExecutorFinished().AddLambda([](UMoviePipelineExecutorBase*, bool bSuccess)
	{
		UMetaHumanRenderController::OnRenderFinished.Broadcast(bSuccess);
	});

	return true;
}

bool UMetaHumanRenderController::RenderStateFromJson(
	UObject* WorldContextObject,
	const FString& JsonPath,
	const FString& OutputDirectory,
	int32 WarmUpFrames)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return false;
	}

	// 确保相机 + 姿态控制器 + 关卡序列存在（幂等）。姿态控制器是动态组件，
	// 不随关卡重载而存活。
	MetaHumanLevelControl::SetupScene(World, TEXT("/Game/Render"));

	FCharacterState State;
	if (!UMetaHumanStateSerial::ImportStateFromJson(JsonPath, State))
	{
		UE_LOG(LogTemp, Error, TEXT("RenderStateFromJson: failed to load state from '%s'"), *JsonPath);
		return false;
	}

	// 在当前世界中对角色应用状态。
	if (UMetaHumanStateController* Controller = MetaHumanLevelControl::FindController(WorldContextObject))
	{
		Controller->ApplyState(State);
	}

	// 把机位解析到预生成的关卡序列。
	if (!MetaHumanLevelControl::FindCameraByName(World, State.RenderSettings.Camera))
	{
		UE_LOG(LogTemp, Error, TEXT("RenderStateFromJson: camera '%s' not found"), *State.RenderSettings.Camera);
		return false;
	}

	const FString ShotAssetName = FString::Printf(TEXT("Shot_%s"), *State.RenderSettings.Camera);
	const FString ShotPackagePath = FString::Printf(TEXT("/Game/Render/%s.%s"), *ShotAssetName, *ShotAssetName);
	ULevelSequence* Sequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *ShotPackagePath));
	if (!Sequence)
	{
		UE_LOG(LogTemp, Error, TEXT("RenderStateFromJson: shot sequence '%s' not found (run Scripts/setup_scene.py first)"), *ShotPackagePath);
		return false;
	}

	FString Filename = State.RenderSettings.OutputFilename;
	if (Filename.IsEmpty())
	{
		Filename = FString::Printf(TEXT("State_%s.png"), *State.RenderSettings.Camera);
	}

	// 把状态交给 PIE 钩子，使其在 executor 复制地图之后、于 PIE 世界内重新应用
	// （运行时的 morph/身体状态不会在复制中保留）。
	ActiveStateJsonPath = JsonPath;
	const bool bStarted = RenderSequence(WorldContextObject, Sequence, OutputDirectory, Filename, WarmUpFrames);
	if (!bStarted)
	{
		ActiveStateJsonPath.Empty();
	}
	return bStarted;
}

bool UMetaHumanRenderController::QuitEditorOnRenderFinished()
{
	UMoviePipelineQueueEngineSubsystem* QueueSubsystem = GEngine->GetEngineSubsystem<UMoviePipelineQueueEngineSubsystem>();
	if (!QueueSubsystem)
	{
		return false;
	}

	UMoviePipelineExecutorBase* Executor = QueueSubsystem->GetActiveExecutor();
	if (!Executor)
	{
		return false;
	}

	Executor->OnExecutorFinished().AddLambda([](UMoviePipelineExecutorBase*, bool bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("MetaHumanRender: render finished (success=%d), exiting editor."), bSuccess);
		// 不要在 executor 结束回调里同步 RequestExit：该回调仍在 Movie Pipeline / 队列子系统的
		// 收尾调用栈上，立即退出会在引擎关停阶段访问已析构对象（退出码 0xC0000005）。
		// 推迟到下一帧 tick，让渲染收尾栈先完全退干净再退出。
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			// 强制硬退出（Force=true）：跳过 C++ 静态析构阶段。无头 MRQ 渲染激活的惰性静态对象
			// 在析构时会非法 free（实测崩溃于 ntdll!RtlFreeHeap，退出码 0xC0000005、bash 139），
			// 优雅 RequestExit(false) 会走到这一步而崩溃。UE 官方语义：Windows 上 Force 分支就是
			// GLog->Flush() 之后 TerminateProcess(GetCurrentProcess(), 0)，稳定得到退出码 0。
			FPlatformMisc::RequestExit(true);
			return false; // 兜底：RequestExit(true) 内部 TerminateProcess，正常不会返回
		}));
	});

	return true;
}


bool UMetaHumanRenderController::RenderHeadless(
	UObject* WorldContextObject,
	const FString& JsonPath,
	const FString& OutputDirectory,
	int32 WarmUpFrames)
{
	if (!RenderStateFromJson(WorldContextObject, JsonPath, OutputDirectory, WarmUpFrames))
	{
		return false;
	}
	return QuitEditorOnRenderFinished();
}
