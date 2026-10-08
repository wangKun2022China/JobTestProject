#include "Panel.h"
#include "LevelControl.h"
#include "BodyController.h"
#include "StateController.h"
#include "RenderController.h"
#include "State.h"
#include "StateSerial.h"
#include "Animation/AnimSequence.h"
#include "Blueprint/WidgetTree.h"
#include "CineCameraActor.h"
#include "Components/Button.h"
#include "Components/DetailsView.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "Misc/Paths.h"
#include "Tools/ControlRigPose.h"
#include "UObject/UnrealType.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"


namespace
{
	// 右下角通知：成功显示绿勾、失败显示红叉，几秒后自动消失。
	void ShowEditorToast(const FString& Message, bool bSuccess = true, float ExpireSeconds = 3.f)
	{
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = ExpireSeconds;
		Info.bUseSuccessFailIcons = true;
		TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}
}


#pragma region SetupPanel
TSharedRef<SWidget> UMetaHumanPanel::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		// todo 此处不需要保证啥
		// MetaHumanLevelControl::SetupScene(World, TEXT("/Game/Render")); // 确保环境有初始化
		UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
		WidgetTree->RootWidget = Root;
		// 三个操作按钮
		UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonRow"));
		Root->AddChildToVerticalBox(ButtonRow);
		CreateActionButton(ButtonRow, TEXT("ImportBtn"), TEXT("导入Json"))->OnClicked.AddDynamic(this, &UMetaHumanPanel::ImportJson);
		CreateActionButton(ButtonRow, TEXT("ExportBtn"), TEXT("导出Json"))->OnClicked.AddDynamic(this, &UMetaHumanPanel::ExportJson);
		CreateActionButton(ButtonRow, TEXT("RenderBtn"), TEXT("渲染"))->OnClicked.AddDynamic(this, &UMetaHumanPanel::Render);
		// Details 面板承载分区：Body / Facial 属性 + 渲染设置
		DetailsView = WidgetTree->ConstructWidget<UDetailsView>(UDetailsView::StaticClass(), TEXT("Details"));
		DetailsView->CategoriesToShow = {
			TEXT("Body"),
			TEXT("Facial"),
			TEXT("渲染设置")
		};
		DetailsView->bShowScrollBar = false;
		DetailsView->SetObject(this);
		Root->AddChildToVerticalBox(DetailsView);

		// 渲染完成时弹右下角通知（广播由 RenderController 在 executor 结束时触发）。
		UMetaHumanRenderController::OnRenderFinished.AddUObject(this, &UMetaHumanPanel::OnRenderFinishedHandler);
	}
	return Super::RebuildWidget();
}


UButton* UMetaHumanPanel::CreateActionButton(UHorizontalBox* Row, const FString& Name, const FString& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*Name));
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*(Name + TEXT("_Label"))));
	LabelText->SetText(FText::FromString(Label));
	Button->SetContent(LabelText);
	if (UHorizontalBoxSlot* ButtonSlot = Row->AddChildToHorizontalBox(Button))
	{
		ButtonSlot->SetPadding(FMargin(2.f, 0.f));
	}
	return Button;
}
#pragma endregion


void UMetaHumanPanel::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshBodyAndFacial();
}


void UMetaHumanPanel::PostEditUndo()
{
	Super::PostEditUndo();
	// 延迟一帧执行一次
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis = TWeakObjectPtr<UMetaHumanPanel>(this)](float)
	{
		if (UMetaHumanPanel* Widget = WeakThis.Get())
		{
			Widget->RefreshBodyAndFacial();
		}
		return false;
	}));
}


FString UMetaHumanPanel::GetCameraName(EMetaHumanCamera Camera)
{
	// 枚举值名即相机 Actor 名（如 Camera_Front），反射直接取。
	return StaticEnum<EMetaHumanCamera>()->GetNameStringByValue(static_cast<int64>(Camera));
}


EMetaHumanCamera UMetaHumanPanel::GetCameraEnum(const FString& Name)
{
	const int64 Value = StaticEnum<EMetaHumanCamera>()->GetValueByNameString(Name);
	// 未知名回退到 Front，行为与原来一致。
	return (Value == INDEX_NONE) ? EMetaHumanCamera::Camera_Front : static_cast<EMetaHumanCamera>(Value);
}


void UMetaHumanPanel::RefreshBodyAndFacial()
{
	if (!GEditor) { return; }
	UMetaHumanStateController* Controller = MetaHumanLevelControl::FindOrAddController(GEditor->GetEditorWorldContext().World());
	if (!Controller) { return; }

	USkeletalMeshComponent* Body = MetaHumanLevelControl::GetBodyMesh(Controller->GetOwner());
	USkeletalMeshComponent* Face = MetaHumanLevelControl::GetFaceMesh(Controller->GetOwner());
	FMetaHumanBodyController& BodyController = Controller->GetBodyController();
	FMetaHumanFaceController& FaceController = Controller->GetFaceController();

	// 刷新动画
	if (BodyAnimation)   
	{
		BodyController.PlayAnimation(Body, BodyAnimation, /*bLooping=*/false);
		// FaceController.SyncFaceToBody(Face, Body);
		BodyController.SetPlayRate(Body, 0.f);
		BodyController.SetTimePosition(Body, BodyProgress * FMath::Max(BodyAnimation->GetPlayLength(), 0.01f));
		// FaceController.SyncFaceToBody(Face, Body);
	}
	else
	{
		BodyController.ResetToRefPose(Body);
	}
	FaceController.SyncFaceToBody(Face, Body);

	// 刷新表情
	TArray<UControlRigPoseAsset*> PoseAssets;
	TArray<float> Weights;
	PoseAssets.Reserve(FacialExpressions.Num());
	Weights.Reserve(FacialExpressions.Num());
	for (const FFacialExpressionEntry& Entry : FacialExpressions)
	{
		if (Entry.PoseAsset && Entry.Intensity > 0.f)
		{
			PoseAssets.Add(Entry.PoseAsset);
			Weights.Add(Entry.Intensity);
		}
	}
	FaceController.ApplyFacialPoseAssets(PoseAssets, Weights, Face);
}


#pragma region Callbacks
void UMetaHumanPanel::ImportJson()
{
	const FString Path = FPaths::ProjectDir() / SnapshotFilePath;
	FCharacterState State;
	if (!UMetaHumanStateSerial::ImportStateFromJson(Path, State))
	{
		ShowEditorToast(FString::Printf(TEXT("导入Json失败：%s"), *Path), /*bSuccess=*/false);
		return;
	}
	// 身体
	BodyAnimation = State.Body.AnimationAsset.IsEmpty() ? nullptr : LoadObject<UAnimSequence>(nullptr, *State.Body.AnimationAsset);
	BodyProgress = 0.f;
	if (BodyAnimation)
	{
		BodyProgress = FMath::Clamp(State.Body.TimePosition / FMath::Max(BodyAnimation->GetPlayLength(), 0.01f), 0.f, 1.f);
	}
	// 表情
	FacialExpressions.Reset();
	for (const TPair<FName, float>& Pair : State.FacialCurves.Values)
	{
		if (UControlRigPoseAsset* PoseAsset = LoadObject<UControlRigPoseAsset>(nullptr, *Pair.Key.ToString()))
		{
			FFacialExpressionEntry& Entry = FacialExpressions.AddDefaulted_GetRef();
			Entry.PoseAsset = PoseAsset;
			Entry.Intensity = Pair.Value;
		}
	}
	// 渲染设置
	SelectedCamera = GetCameraEnum(State.RenderSettings.Camera);
	OutPutName = State.RenderSettings.OutputFilename;
	// 刷新
	RefreshBodyAndFacial();
	if (DetailsView) { DetailsView->SetObject(this); }
	ShowEditorToast(FString::Printf(TEXT("导入Json成功：%s"), *Path));
}


FCharacterState UMetaHumanPanel::BuildState() const
{
	FCharacterState State;
	if (BodyAnimation)
	{
		State.Body.AnimationAsset = BodyAnimation->GetPathName();
		State.Body.TimePosition = BodyProgress * FMath::Max(BodyAnimation->GetPlayLength(), 0.01f);
	}
	for (const FFacialExpressionEntry& Entry : FacialExpressions)
	{
		if (Entry.PoseAsset)
		{
			State.FacialCurves.Values.Add(FName(*Entry.PoseAsset->GetPathName()), Entry.Intensity);
		}
	}
	return State;
}


void UMetaHumanPanel::ExportJson()
{
	// 组织state
	FCharacterState State = BuildState();
	State.RenderSettings.Camera = GetCameraName(SelectedCamera);
	State.RenderSettings.OutputFilename = OutPutName;
	// 存盘为Json
	const FString Path = FPaths::ProjectDir() / SnapshotFilePath;
	const bool bOk = UMetaHumanStateSerial::ExportStateToJson(State, Path);
	ShowEditorToast(
		FString::Printf(TEXT("%s：%s"), bOk ? TEXT("导出Json成功") : TEXT("导出Json失败"), *Path),
		bOk);
}


void UMetaHumanPanel::Render()
{
	if (!GEditor) { return; }
	UWorld* World = GEditor->GetEditorWorldContext().World();
	UMetaHumanStateController* Controller = MetaHumanLevelControl::FindOrAddController(World);
	if (!Controller) { return; }
	// 组织state
	FCharacterState State = BuildState();
	const FString CameraName = GetCameraName(SelectedCamera);
	State.RenderSettings.Camera = CameraName;
	State.RenderSettings.OutputFilename = FString::Printf(TEXT("State_%s"), *CameraName);
	// 存盘为json再调用渲染
	const FString StateJsonPath = FPaths::ProjectDir() / RenderStateFilePath;
	if (!UMetaHumanStateSerial::ExportStateToJson(State, StateJsonPath))
	{
		ShowEditorToast(FString::Printf(TEXT("渲染状态写入失败：%s"), *StateJsonPath), /*bSuccess=*/false);
		return;
	}
	const FString OutputDirectory = FPaths::ProjectDir() / RenderOutputDirectory;
	const bool bStarted = UMetaHumanRenderController::RenderStateFromJson( World, StateJsonPath, OutputDirectory, WarmUpFrames);
	ShowEditorToast(bStarted ? TEXT("渲染开始") : TEXT("渲染启动失败"), bStarted);
}


void UMetaHumanPanel::OnRenderFinishedHandler(bool bSuccess)
{
	// 渲染完成/失败通知由 RenderController 的 OnRenderFinished 广播触发（executor 结束回调）。
	ShowEditorToast(bSuccess ? TEXT("渲染完成") : TEXT("渲染失败"), bSuccess);
}
#pragma endregion