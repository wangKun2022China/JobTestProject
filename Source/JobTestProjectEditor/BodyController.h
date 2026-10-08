#pragma once
#include "CoreMinimal.h"
#include "State.h"


class UAnimSequence;
class USkeletalMeshComponent;

/**
 * 身体网格控制器：身体动画播放、定格、冻结与复位
 */
class FMetaHumanBodyController
{
public:
	// 解析动画并定格到指定时间
	bool ApplyBodyState(const FBodyState& State, USkeletalMeshComponent* Body);

	void PlayAnimation(USkeletalMeshComponent* Body, UAnimSequence* Sequence, bool bLooping);

	void SetTimePosition(USkeletalMeshComponent* Body, float TimeSeconds);

	void SetPlayRate(USkeletalMeshComponent* Body, float PlayRate);

	// 清空身体动画复位到T-Pose
	void ResetToRefPose(USkeletalMeshComponent* Body);

private:
	// 加载动画
	UAnimSequence* ResolveAnimation(const FString& NameOrPath) const;
};
