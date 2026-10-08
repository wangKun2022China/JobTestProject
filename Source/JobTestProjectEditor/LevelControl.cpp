#include "LevelControl.h"
#include "StateController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CineCameraActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "LevelSequence.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "UObject/SavePackage.h"


namespace MetaHumanLevelControl
{
	struct FCameraDef
	{
		FName Name;
		float ForwardFactor;
		float RightFactor;
	};

	const FCameraDef CameraDefs[] =
	{
		{ TEXT("Camera_Front"),         1.f,  0.f },
		{ TEXT("Camera_Left_45"),       1.f, -1.f },
		{ TEXT("Camera_Left_Profile"),  0.f, -1.f },
		{ TEXT("Camera_Right_45"),      1.f,  1.f },
		{ TEXT("Camera_Right_Profile"), 0.f,  1.f },
	};

	const float CameraDistance = 250.f; // 机位到角色枢轴的水平距离
	const float LookHeight = 160.f;     // 面部（视线目标）高度，世界 Z

	USkeletalMeshComponent* FindComponentByName(AActor* Owner, const FName& Name)
	{
		if (!Owner)
		{
			return nullptr;
		}

		TArray<USkeletalMeshComponent*> Meshes;
		Owner->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh->GetFName() == Name)
			{
				return Mesh;
			}
		}

		return nullptr;
	}

	USkeletalMeshComponent* GetBodyMesh(AActor* Owner)
	{
		return FindComponentByName(Owner, TEXT("Body"));
	}

	USkeletalMeshComponent* GetFaceMesh(AActor* Owner)
	{
		return FindComponentByName(Owner, TEXT("Face"));
	}

	AActor* FindActorWithFaceMesh(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			TArray<USkeletalMeshComponent*> Meshes;
			It->GetComponents<USkeletalMeshComponent>(Meshes);
			for (USkeletalMeshComponent* Mesh : Meshes)
			{
				if (Mesh && Mesh->GetFName() == TEXT("Face"))
				{
					return *It;
				}
			}
		}
		return nullptr;
	}

	UMetaHumanStateController* FindController(UObject* WorldContextObject)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (!World)
		{
			return nullptr;
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (UMetaHumanStateController* Controller = It->FindComponentByClass<UMetaHumanStateController>())
			{
				return Controller;
			}
		}
		return nullptr;
	}

	UMetaHumanStateController* FindOrAddController(UWorld* World)
	{
		if (UMetaHumanStateController* Existing = FindController(World))
		{
			return Existing;
		}

		AActor* MetaHuman = FindActorWithFaceMesh(World);
		if (!MetaHuman)
		{
			return nullptr;
		}

		return Cast<UMetaHumanStateController>(
			MetaHuman->AddComponentByClass(UMetaHumanStateController::StaticClass(), false, FTransform::Identity, false));
	}

	USceneComponent* GetMetaHumanOrientation(AActor* Actor)
	{
		if (USkeletalMeshComponent* Body = GetBodyMesh(Actor))
		{
			return Body;
		}
		if (USkeletalMeshComponent* Face = GetFaceMesh(Actor))
		{
			return Face;
		}
		return Actor ? Actor->GetRootComponent() : nullptr;
	}

	ACineCameraActor* FindCameraByName(UWorld* World, const FString& CameraName)
	{
		if (!World)
		{
			return nullptr;
		}

		const FName ActorName = FName(*CameraName);
		for (TActorIterator<ACineCameraActor> It(World); It; ++It)
		{
			if (It->GetFName() == ActorName)
			{
				return *It;
			}
		}
		return nullptr;
	}

	bool SetupScene(UWorld* World, const FString& PackageDirectory)
	{
		// 检查关卡
		if (!World) { return false; }
		// 1. 检查/补齐五个机位相机，并相对 MetaHuman 的实际位置与朝向摆放。
		AActor* MetaHuman = FindActorWithFaceMesh(World);
		USceneComponent* Orientation = GetMetaHumanOrientation(MetaHuman);
		const FVector Origin = MetaHuman ? MetaHuman->GetActorLocation() : FVector::ZeroVector;
		FVector Forward = Orientation ? Orientation->GetRightVector() : FVector(0.f, 1.f, 0.f);
		FVector Right = Orientation ? Orientation->GetForwardVector() : FVector(1.f, 0.f, 0.f);
		Forward.Z = 0.f;
		Forward = Forward.GetSafeNormal();
		Right.Z = 0.f;
		Right = Right.GetSafeNormal();
		const FVector LookTarget = Origin + FVector(0.f, 0.f, LookHeight);
		UE_LOG(LogTemp, Log, TEXT("SetupScene: MetaHuman orientation origin=%s forward=%s right=%s (actor=%s)"),
			*Origin.ToString(), *Forward.ToString(), *Right.ToString(),
			MetaHuman ? *MetaHuman->GetName() : TEXT("none"));

		for (const FCameraDef& Def : CameraDefs)
		{
			const FVector CameraLocation = Origin
				+ (Forward * Def.ForwardFactor + Right * Def.RightFactor) * CameraDistance
				+ FVector(0.f, 0.f, LookHeight);

			ACineCameraActor* Camera = FindCameraByName(World, Def.Name.ToString());
			if (!Camera)
			{
				const FString CameraName = Def.Name.ToString();
				FActorSpawnParameters SpawnParams;
				SpawnParams.Name = FName(*CameraName);
				SpawnParams.InitialActorLabel = CameraName;
				SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Camera = World->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), FTransform::Identity, SpawnParams);
				if (Camera)
				{
					const FName ActualName = Camera->GetFName();
					if (ActualName != SpawnParams.Name)
					{
						Camera->SetActorLabel(ActualName.ToString());
						UE_LOG(LogTemp, Warning, TEXT("SetupScene: camera name '%s' was taken, spawned as '%s' (label synced)"),
							*CameraName, *ActualName.ToString());
					}
					UE_LOG(LogTemp, Log, TEXT("SetupScene: spawned camera name='%s' label='%s'"),
						*ActualName.ToString(), *Camera->GetActorLabel());
				}
			}

			if (Camera)
			{
				Camera->SetActorLocation(CameraLocation);
				Camera->SetActorRotation((LookTarget - CameraLocation).Rotation());
			}
		}

		// 2. 把UMetaHumanStateController挂到 MetaHuman 角色上（通过其 "Face" 网格定位）
		if (UMetaHumanStateController* Controller = FindOrAddController(World))
		{
			UE_LOG(LogTemp, Log, TEXT("SetupScene: pose controller ready on '%s'"), *Controller->GetOwner()->GetName());
		}

		// 3. 生成五个 camera-cut 关卡序列, 确保任何生成的 actor 随关卡一起持久化。
		const bool bSequencesGenerated = GenerateShotSequences(World, PackageDirectory);
		if (World->GetOutermost())
		{
			World->GetOutermost()->MarkPackageDirty();
		}

		return bSequencesGenerated;
	}

	bool GenerateShotSequences(UWorld* World, const FString& PackageDirectory)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("GenerateShotSequences: world not found."));
			return false;
		}

		bool bAllSaved = true;
		for (const FCameraDef& Def : CameraDefs)
		{
			const FString CameraName = Def.Name.ToString();
			ACineCameraActor* Camera = FindCameraByName(World, CameraName);
			if (!Camera)
			{
				UE_LOG(LogTemp, Warning, TEXT("GenerateShotSequences: camera '%s' not resolved."), *CameraName);
				bAllSaved = false;
				continue;
			}

			const FString AssetName = FString::Printf(TEXT("Shot_%s"), *CameraName);
			const FString PackagePath = PackageDirectory / AssetName;

			if (FPackageName::DoesPackageExist(PackagePath))
			{
				UE_LOG(LogTemp, Log, TEXT("GenerateShotSequences: '%s' already exists, skipping."), *PackagePath);
				continue;
			}

			UPackage* Package = CreatePackage(*PackagePath);
			Package->SetFlags(RF_Public | RF_Standalone | RF_Transactional);

			ULevelSequence* Sequence = NewObject<ULevelSequence>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			// UE 5.6：构造函数会让 MovieScene 保持为 null；Initialize() 会分配它。
			if (Sequence)
			{
				Sequence->Initialize();
			}
			UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
			if (!MovieScene)
			{
				UE_LOG(LogTemp, Warning, TEXT("GenerateShotSequences: no movie scene for '%s'."), *PackagePath);
				bAllSaved = false;
				continue;
			}

			// 单个被捕获的帧。
			MovieScene->SetPlaybackRange(FFrameNumber(0), 1);

			const FGuid BindingGuid = MovieScene->AddPossessable(CameraName, ACineCameraActor::StaticClass());
			Sequence->BindPossessableObject(BindingGuid, *Camera, World);

			if (UMovieSceneCameraCutTrack* CutTrack = Cast<UMovieSceneCameraCutTrack>(MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass())))
			{
				if (UMovieSceneCameraCutSection* CutSection = Cast<UMovieSceneCameraCutSection>(CutTrack->CreateNewSection()))
				{
					CutSection->SetCameraGuid(BindingGuid);
					// MRQ 的 shot-list visitor 会对开放（无界）的 section 范围断言，因此
					// camera cut 必须是覆盖播放窗口的有限（FINITE）范围。
					CutSection->SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(1)));
					CutTrack->AddSection(*CutSection);
				}
			}

			FAssetRegistryModule::AssetCreated(Sequence);
			Sequence->MarkPackageDirty();

			const FString FileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			const bool bSaved = UPackage::SavePackage(Package, Sequence, *FileName, SaveArgs);
			UE_LOG(LogTemp, Log, TEXT("GenerateShotSequences: save '%s' -> %s (file=%s)"),
				*PackagePath, bSaved ? TEXT("ok") : TEXT("FAIL"), *FileName);
			if (!bSaved)
			{
				bAllSaved = false;
			}
		}

		return bAllSaved;
	}
}
