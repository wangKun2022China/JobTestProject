#include "FaceController.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "ControlRigAnimInstance.h"
#include "ControlRigComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Rigs/RigHierarchy.h"
#include "Tools/ControlRigPose.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "AnimationDataSource.h"


namespace
{
	// 单个 CTRL_* 控制在多姿势混合时、相对中性的增量累加结果。
	struct FControlDelta
	{
		ERigControlType Type = ERigControlType::Float;
		float FloatDelta = 0.f;                              // Float / ScaleFloat
		FVector3f Vector2DDelta = FVector3f::ZeroVector;     // Vector2D
		FVector TranslationDelta = FVector::ZeroVector;      // 变换类：位移增量
		FRotator RotatorDelta = FRotator::ZeroRotator;       // 变换类：旋转增量
		FVector ScaleDelta = FVector::ZeroVector;            // 变换类：缩放增量
	};

	// 与引擎 FControlRigControlPose::BlendWithInitialPoses 一致：这些控制都按变换处理。
	bool IsTransformLike(ERigControlType Type)
	{
		return Type == ERigControlType::Transform ||
			Type == ERigControlType::EulerTransform ||
			Type == ERigControlType::TransformNoScale ||
			Type == ERigControlType::Position ||
			Type == ERigControlType::Rotator ||
			Type == ERigControlType::Scale;
	}

}


TSubclassOf<UControlRig> FMetaHumanFaceController::LoadRigClass(USkeletalMeshComponent* Face) const
{
	if (!Face) {  return nullptr; }
	
	TSoftObjectPtr<UObject> RigRef = Face->GetDefaultAnimatingRig();
	UE_LOG(LogTemp, Log, TEXT("MetaHumanFaceController::LoadRigClass: DefaultAnimatingRig='%s' isNull=%d mesh=%s"),
		*RigRef.ToString(), RigRef.IsNull(),
		Face->GetSkeletalMeshAsset() ? *Face->GetSkeletalMeshAsset()->GetPathName() : TEXT("none"));
	if (RigRef.IsNull()) { return nullptr; }
	
	UBlueprint* RigBP = Cast<UBlueprint>(RigRef.LoadSynchronous());
	if (!RigBP || !RigBP->GeneratedClass || !RigBP->GeneratedClass->IsChildOf(UControlRig::StaticClass()))
	{	 return nullptr;  }

	UClass* GeneratedClass = RigBP->GeneratedClass;
	return TSubclassOf<UControlRig>(GeneratedClass);
}


UControlRigComponent* FMetaHumanFaceController::GetFaceControlRigComponent(USkeletalMeshComponent* Face)
{
	if (!Face || !Face->GetOwner()) { return nullptr; }

	TSubclassOf<UControlRig> RigClass = LoadRigClass(Face);
	if (!RigClass) { return nullptr; }

	AActor* Owner = Face->GetOwner();
	// 复用已经承载同一 rig 的组件（Sequencer 轨道也是复用同一个 ControlRigComponent）。
	TArray<UControlRigComponent*> Components;
	Owner->GetComponents(Components);
	for (UControlRigComponent* Comp : Components)
	{
		if (Comp && Comp->ControlRigClass == RigClass)
		{
			return Comp;
		}
	}

	// MetaHuman actor 默认没有面部 rig 组件，这里新建一个承载面部 rig 的组件。
	UControlRigComponent* Comp = NewObject<UControlRigComponent>(Owner, TEXT("MetaHumanFaceControlRig"));
	if (!Comp) { return nullptr; }
	Comp->RegisterComponent();
	Comp->AttachToComponent(Face, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Comp->SetControlRigClass(RigClass);
	Comp->AddMappedCompleteSkeletalMesh(Face);

	// RigLogic 经 DataSourceRegistry 的 "OwnerComponent" 找 SkeletalMeshComponent 读 DNA；
	// 默认注册的是组件自身，这里改注册为 Face 网格，否则 RigLogic 静默跳过、无 morph/下颌。
	if (UControlRig* Rig = Comp->GetControlRig())
	{
		Rig->GetDataSourceRegistry()->UnregisterDataSource(UControlRig::OwnerComponent);
		Rig->GetDataSourceRegistry()->RegisterDataSource(UControlRig::OwnerComponent, Face);
	}

	// 禁用自动 tick：否则渲染帧会以完整事件队列复位 CTRL_* GUI 控制 → 表情被冲掉。
	Comp->bUpdateRigOnTick = false;
	return Comp;
}


void FMetaHumanFaceController::ResetToNeutral(UControlRig* Rig) const
{
	// 复位到 Initial默认值
	if (URigHierarchy* Hierarchy = Rig->GetHierarchy())
	{
		Hierarchy->ResetPoseToInitial(ERigElementType::Control);
		Hierarchy->ResetCurveValues();
	}
	Rig->Execute(FRigUnit_BeginExecution::EventName);
}


void FMetaHumanFaceController::BlendPoseIntoControls(UControlRig* Rig, UControlRigPoseAsset* PoseAsset, float Weight)
{
	// 捕获 rig 默认姿态作为“中性表情”参考，再按 Weight 在 [中性, 姿势] 间混合。
	FControlRigControlPose NeutralPose;
	PoseAsset->GetCurrentPose(Rig, NeutralPose);
	PoseAsset->SelectControls(Rig, /*bDoMirror=*/false, /*bClearSelection=*/true);
	PoseAsset->BlendWithInitialPoses(NeutralPose, Rig, /*bDoKey=*/false, /*bDoMirror=*/false, Weight);
}


void FMetaHumanFaceController::ApplyFacialPoseAssets(
	const TArray<UControlRigPoseAsset*>& PoseAssets,
	const TArray<float>& Weights,
	USkeletalMeshComponent* Face)
{
	if (!Face || !Face->GetSkeletalMeshAsset())
	{
		UE_LOG(LogTemp, Warning, TEXT("MetaHumanFaceController::ApplyFacialPoseAssets: face mesh not found"));
		return;
	}

	UControlRigComponent* Comp = GetFaceControlRigComponent(Face);
	if (!Comp || !Comp->GetControlRig())
	{
		UE_LOG(LogTemp, Warning, TEXT("MetaHumanFaceController::ApplyFacialPoseAssets: could not create face Control Rig component for '%s'"),
			*Face->GetPathName());
		return;
	}

	UControlRig* Rig = Comp->GetControlRig();

	// 复位到中性，捕获中性 CTRL_* 控制值作为混合基准。最终表情 = 中性 + Σ(各姿势相对中性的增量 · 强度)
	ResetToNeutral(Rig);
	const FControlRigControlPose NeutralPose(Rig, /*bUseAll=*/true);

	TMap<FName, FControlDelta> Deltas;

	const int32 Count = FMath::Min(PoseAssets.Num(), Weights.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		UControlRigPoseAsset* PoseAsset = PoseAssets[i];
		if (!PoseAsset || Weights[i] <= 0.f) { continue; }

		// 套满权重姿势，读出该姿势在默认父空间下的控制值（SavePose 会补偿非默认父空间）。
		ResetToNeutral(Rig);
		BlendPoseIntoControls(Rig, PoseAsset, 1.f);
		const FControlRigControlPose FullPose(Rig, /*bUseAll=*/true);

		for (const FRigControlCopy& Copy : FullPose.CopyOfControls)
		{
			const int32* NeutralIndex = NeutralPose.CopyOfControlsNameToIndex.Find(Copy.Name);
			if (!NeutralIndex) { continue; }
			const FRigControlCopy& NeutralCopy = NeutralPose.CopyOfControls[*NeutralIndex];
			if (NeutralCopy.ControlType != Copy.ControlType) { continue; }
			FControlDelta& Delta = Deltas.FindOrAdd(Copy.Name);
			Delta.Type = Copy.ControlType;
			switch (Copy.ControlType)
			{
			case ERigControlType::Float:
			case ERigControlType::ScaleFloat:
				Delta.FloatDelta += (Copy.Value.Get<float>() - NeutralCopy.Value.Get<float>()) * Weights[i];
				break;
			case ERigControlType::Vector2D:
				Delta.Vector2DDelta += (Copy.Value.Get<FVector3f>() - NeutralCopy.Value.Get<FVector3f>()) * Weights[i];
				break;
			default:
				if (IsTransformLike(Copy.ControlType))
				{
					const FTransform N = NeutralCopy.LocalTransform;
					const FTransform P = Copy.LocalTransform;
					Delta.TranslationDelta += (P.GetLocation() - N.GetLocation()) * Weights[i];

					const FRotator RotDelta = P.GetRotation().Rotator() - N.GetRotation().Rotator();
					Delta.RotatorDelta.Pitch += RotDelta.Pitch * Weights[i];
					Delta.RotatorDelta.Yaw += RotDelta.Yaw * Weights[i];
					Delta.RotatorDelta.Roll += RotDelta.Roll * Weights[i];

					Delta.ScaleDelta += (P.GetScale3D() - N.GetScale3D()) * Weights[i];
				}
				break;
			}
		}
	}
	// 把「中性 + 增量」写回 rig 的 CTRL_* 控制
	for (const TPair<FName, FControlDelta>& Pair : Deltas)
	{
		const FControlDelta& Delta = Pair.Value;

		if (Delta.Type == ERigControlType::Float || Delta.Type == ERigControlType::ScaleFloat)
		{
			float Neutral = 0.f;
			if (const int32* Idx = NeutralPose.CopyOfControlsNameToIndex.Find(Pair.Key))
			{
				Neutral = NeutralPose.CopyOfControls[*Idx].Value.Get<float>();
			}
			Rig->SetControlValue<float>(Pair.Key, Neutral + Delta.FloatDelta);
		}
		else if (Delta.Type == ERigControlType::Vector2D)
		{
			FVector3f Neutral = FVector3f::ZeroVector;
			if (const int32* Idx = NeutralPose.CopyOfControlsNameToIndex.Find(Pair.Key))
			{
				Neutral = NeutralPose.CopyOfControls[*Idx].Value.Get<FVector3f>();
			}
			Rig->SetControlValue<FVector3f>(Pair.Key, Neutral + Delta.Vector2DDelta);
		}
		else if (IsTransformLike(Delta.Type))
		{
			FTransform Neutral = FTransform::Identity;
			if (const int32* Idx = NeutralPose.CopyOfControlsNameToIndex.Find(Pair.Key))
			{
				Neutral = NeutralPose.CopyOfControls[*Idx].LocalTransform;
			}
			const FRotator FinalRotation = Neutral.GetRotation().Rotator() + Delta.RotatorDelta;
			const FTransform Final(FQuat(FinalRotation), Neutral.GetLocation() + Delta.TranslationDelta, Neutral.GetScale3D() + Delta.ScaleDelta);
			Rig->SetControlLocalTransform(Pair.Key, Final);
		}
	}
	// 执行 Forwards Solve（GUI→raw）+ RigLogic，并经 OnExecuted→TransferOutputs 写回 Face 主 AnimInstance
	Rig->Execute(FRigUnit_BeginExecution::EventName);
	Face->RefreshBoneTransforms();
}


void FMetaHumanFaceController::SyncFaceToBody(USkeletalMeshComponent* Face, USkeletalMeshComponent* Body)
{
	// 先 UpdateAnimation 让后处理实例跑一遍 PreUpdate刷新源缓存，再 RefreshBoneTransforms 用新源求值，使头部当帧跟随身体
	if (!Face || !Body) { return; }
	if (Face->PostProcessAnimInstance)
	{
		Face->PostProcessAnimInstance->UpdateAnimation(0.f, /*bNeedsValidRootMotion=*/false);
	}
	Face->RefreshBoneTransforms();
}
 