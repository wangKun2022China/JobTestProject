#include "StateController.h"
#include "FaceController.h"
#include "LevelControl.h"
#include "State.h"
#include "Components/SkeletalMeshComponent.h"
#include "Tools/ControlRigPose.h"
#include "UObject/UObjectGlobals.h"


bool UMetaHumanStateController::ApplyState(const FCharacterState& State)
{
	// 身体
	USkeletalMeshComponent* Body = MetaHumanLevelControl::GetBodyMesh(GetOwner());
	bool bOk = BodyController.ApplyBodyState(State.Body, Body);

	// 面部：facial_curves 的 key 是「姿势资产路径 -> 强度」。逐 morph 精确回放已不再需要，
	// 无法加载为姿势资产的 key（round-trip 捕获的 morph target 名）直接忽略。
	TArray<UControlRigPoseAsset*> PoseAssets;
	TArray<float> PoseWeights;
	for (const TPair<FName, float>& Pair : State.FacialCurves.Values)
	{
		if (UControlRigPoseAsset* PoseAsset = LoadObject<UControlRigPoseAsset>(nullptr, *Pair.Key.ToString()))
		{
			PoseAssets.Add(PoseAsset);
			PoseWeights.Add(Pair.Value);
		}
	}

	// 有姿势资产则按强度应用；没有则喂中性曲线回中性。
	FaceController.ApplyFacialPoseAssets(PoseAssets, PoseWeights, MetaHumanLevelControl::GetFaceMesh(GetOwner()));
	return bOk;
}
