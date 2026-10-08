#pragma once
#include "CoreMinimal.h"
#include "State.generated.h"


// 身体动画，资产路径 + 时间位置（秒）
USTRUCT(BlueprintType)
struct JOBTESTPROJECTEDITOR_API FBodyState
{
	GENERATED_BODY()

	// 动画路径
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FString AnimationAsset;

	// 动画时刻，秒
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	float TimePosition = 0.f;
};


// 表情 ccontrol rig pose文件路径： 强度值0-1
USTRUCT(BlueprintType)
struct JOBTESTPROJECTEDITOR_API FFacialCurves
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	TMap<FName, float> Values;
};


// 渲染设置（机位 + 输出文件名）
USTRUCT(BlueprintType)
struct JOBTESTPROJECTEDITOR_API FRenderSettings
{
	GENERATED_BODY()

	// 相机名：Front / Left_45 / Right_45 / Left_Profile / Right_Profile
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FString Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FString OutputFilename;
};


// 身体姿态 + 面部曲线 + 渲染设置
USTRUCT(BlueprintType)
struct JOBTESTPROJECTEDITOR_API FCharacterState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FBodyState Body;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FFacialCurves FacialCurves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FRenderSettings RenderSettings;
};
