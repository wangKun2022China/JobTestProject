#include "BodyController.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UObjectGlobals.h"


UAnimSequence* FMetaHumanBodyController::ResolveAnimation(const FString& NameOrPath) const
{
	if (NameOrPath.StartsWith(TEXT("/")))
	{
		return Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *NameOrPath));
	}
	return nullptr;
}


bool FMetaHumanBodyController::ApplyBodyState(const FBodyState& State, USkeletalMeshComponent* Body)
{
	if (!Body || State.AnimationAsset.IsEmpty()) { return true; }
	UAnimSequence* Sequence = ResolveAnimation(State.AnimationAsset);
	if (!Sequence)
	{
		UE_LOG(LogTemp, Warning, TEXT("MetaHumanBodyController: could not resolve animation '%s'"), *State.AnimationAsset);
		return false;
	}
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	if (!Body->GetAnimInstance()) { Body->InitAnim(true); }
	Body->PlayAnimation(Sequence, /*bLooping=*/false);
	if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(Body->GetAnimInstance()))
	{
		SingleNode->SetPosition(State.TimePosition, /*bFireNotifies=*/false);
		SingleNode->SetPlayRate(0.f);
		Body->RefreshBoneTransforms();  // 仅 SetPosition 不会刷新姿态；强制刷新，使还原的那一帧立即可见。
	}
	return true;
}


void FMetaHumanBodyController::PlayAnimation(USkeletalMeshComponent* Body, UAnimSequence* Sequence, bool bLooping)
{
	if (!Body || !Sequence)
	{
		UE_LOG(LogTemp, Warning, TEXT("MetaHumanBodyController::PlayAnimation: %s"),
			!Body ? TEXT("body mesh not found") : TEXT("null sequence"));
		return;
	}
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	if (!Body->GetAnimInstance()) { Body->InitAnim(true); }
	Body->PlayAnimation(Sequence, bLooping);
	Body->RefreshBoneTransforms();
}


void FMetaHumanBodyController::SetTimePosition(USkeletalMeshComponent* Body, float TimeSeconds)
{
	if (!Body) { return; }
	if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(Body->GetAnimInstance()))
	{
		SingleNode->SetPosition(TimeSeconds, false);
		Body->RefreshBoneTransforms();  // 强刷让视口立即更新
	}
}


void FMetaHumanBodyController::SetPlayRate(USkeletalMeshComponent* Body, float PlayRate)
{
	if (!Body) { return; }
	if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(Body->GetAnimInstance()))
	{
		SingleNode->SetPlayRate(PlayRate);
	}
}


void FMetaHumanBodyController::ResetToRefPose(USkeletalMeshComponent* Body)
{
	if (!Body) { return; }
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(Body->GetAnimInstance()))
	{
		SingleNode->SetAnimationAsset(nullptr);
	}
	Body->RefreshBoneTransforms();
}
