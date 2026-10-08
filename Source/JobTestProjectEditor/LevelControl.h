#pragma once
#include "CoreMinimal.h"


class AActor;
class ACineCameraActor;
class UMetaHumanStateController;
class USceneComponent;
class USkeletalMeshComponent;
class UWorld;


/**
 * 读取和修改Level
 */
namespace MetaHumanLevelControl
{
	// 设置Level。检查/生成五个机位相机、创建shot文件，挂stateController
	bool SetupScene(UWorld* World, const FString& PackageDirectory = TEXT("/Game/Render"));

	// 按组件名在 actor 的骨骼网格组件中查找（找不到返回 null）
	USkeletalMeshComponent* FindComponentByName(AActor* Owner, const FName& Name);

	// 找Body网格
	USkeletalMeshComponent* GetBodyMesh(AActor* Owner);

	// 找Face网格
	USkeletalMeshComponent* GetFaceMesh(AActor* Owner);

	// 找带 "Face" 骨骼网格的 actor，用于定位MetaHuman
	AActor* FindActorWithFaceMesh(UWorld* World);

	// 获取metahuman挂的stateController
	UMetaHumanStateController* FindController(UObject* WorldContextObject);

	// 获取或创建metahuman挂的stateController
	UMetaHumanStateController* FindOrAddController(UWorld* World);

	// 按相机名找对应actor
	ACineCameraActor* FindCameraByName(UWorld* World, const FString& CameraName);

	// 获取组件用于确定角色朝向
	USceneComponent* GetMetaHumanOrientation(AActor* Actor);

	// 生成五个 camera-cut 关卡序列（已存在的跳过）
	bool GenerateShotSequences(UWorld* World, const FString& PackageDirectory = TEXT("/Game/Render"));
}
