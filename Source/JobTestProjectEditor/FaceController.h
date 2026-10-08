#pragma once
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"


class UControlRig;
class UControlRigComponent;
class UControlRigPoseAsset;
class USkeletalMeshComponent;

/**
 * 参考Sequencer 的 Control Rig 轨道做法，用UControlRigComponent 驱动MetaHuman 面部 rig
 *
 * 组件把 Face 网格 AddMappedCompleteSkeletalMesh 映射进来后，其内部 rig 的 Forwards Solve
 * 会产生 CTRL_expressions_* 曲线；TransferOutputs 会把这些曲线（以及骨骼）写进 Face 网格的
 * 主 AnimInstance（UControlRigAnimInstance 的 StoredCurves）
 * 
 * 随后再经过Face 网格的后处理 AnimBP（ABP_Face_PostProcess）应用
 */


class FMetaHumanFaceController
{
public:
	// 应用一组表情姿势资产的加权混合；数组为空时回到默认
	void ApplyFacialPoseAssets(
		const TArray<UControlRigPoseAsset*>& PoseAssets,
		const TArray<float>& Weights,
		USkeletalMeshComponent* Face);
	
	// 身体姿态变化后同步头部
	void SyncFaceToBody(USkeletalMeshComponent* Face, USkeletalMeshComponent* Body);

private:
	// 在 Face 所属 actor 上获取或新建承载面部 rig 的 UControlRigComponent=
	UControlRigComponent* GetFaceControlRigComponent(USkeletalMeshComponent* Face);

	// 以 UBlueprint 加载面部 Control Rig（从 Face 网格的 DefaultAnimatingRig 取），返回其 GeneratedClass（TSubclassOf<UControlRig>）
	TSubclassOf<UControlRig> LoadRigClass(USkeletalMeshComponent* Face) const;

	// 把 rig 复位到默认
	void ResetToNeutral(UControlRig* Rig) const;

	// 按 Weight 把 Pose 资产混合到 rig 的控制上
	void BlendPoseIntoControls(UControlRig* Rig, UControlRigPoseAsset* PoseAsset, float Weight);
};
