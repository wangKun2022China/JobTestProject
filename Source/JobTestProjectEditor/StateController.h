#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BodyController.h"
#include "FaceController.h"
#include "State.h"
#include "StateController.generated.h"


/**
 * 核心控制组件
 *
 * 身体姿态由 FMetaHumanBodyController 用 UAnimSingleNodeInstance 驱动（SetPosition + SetPlayRate(0)），
 * 从而无需任何 montage 播放即可冻结在精确的某一帧。
 *
 * 面部状态由 FMetaHumanFaceController 用 UControlRigComponent 驱动面部 rig（Face_ControlBoard_CtrlRig），
 * 其 Forwards Solve 产生的 CTRL_expressions_* 曲线经 Face 网格主实例流入后处理 AnimBP（ABP_Face_PostProcess），
 * 由引擎原生 RigLogic 节点还原成 morph target 与下颌关节（复刻 Sequencer 的 Control Rig 轨道做法）。
 */
UCLASS(ClassGroup = (MetaHuman), meta = (BlueprintSpawnableComponent))
class JOBTESTPROJECTEDITOR_API UMetaHumanStateController : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|State")
	bool ApplyState(const FCharacterState& State);

	FMetaHumanBodyController& GetBodyController() { return BodyController; }

	FMetaHumanFaceController& GetFaceController() { return FaceController; }

private:
	FMetaHumanBodyController BodyController;
	FMetaHumanFaceController FaceController;
};
