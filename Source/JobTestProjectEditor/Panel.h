#pragma once
#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "State.h"
#include "Panel.generated.h"


class SWidget;
class UAnimSequence;
class UButton;
class UControlRigPoseAsset;
class UDetailsView;
class UHorizontalBox;
class UMetaHumanStateController;
class UTextBlock;
struct FPropertyChangedEvent;


USTRUCT(BlueprintType)
struct JOBTESTPROJECTEDITOR_API FFacialExpressionEntry
{
	GENERATED_BODY()

	// 面部表情资产
	UPROPERTY(EditAnywhere, Category = "Facial", meta = (DisplayName = "Expression"))
	UControlRigPoseAsset* PoseAsset = nullptr;

	// 表情强度
	UPROPERTY(EditAnywhere, Category = "Facial", meta = (DisplayName = "Intensity", UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Intensity = 1.f;
};


// 渲染机位选项：五个固定机位，枚举值名即相机 Actor 名（Camera_<名>）。
UENUM(BlueprintType)
enum class EMetaHumanCamera : uint8
{
	Camera_Front         UMETA(DisplayName = "Front (正视图)"),
	Camera_Left_Profile  UMETA(DisplayName = "Left Profile (左正侧 90°)"),
	Camera_Right_Profile UMETA(DisplayName = "Right Profile (右正侧 90°)"),
	Camera_Left_45       UMETA(DisplayName = "Left 45° (左侧 45°)"),
	Camera_Right_45      UMETA(DisplayName = "Right 45° (右侧 45°)"),
};


UCLASS()
class JOBTESTPROJECTEDITOR_API UMetaHumanPanel : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

public:
	// 渲染机位、输出文件名、预热帧数
	UPROPERTY(EditAnywhere, Category = "渲染设置", meta = (DisplayName = "渲染镜头"))
	EMetaHumanCamera SelectedCamera = EMetaHumanCamera::Camera_Front;

	UPROPERTY(EditAnywhere, Category = "渲染设置", meta = (DisplayName = "输出名"))
	FString OutPutName = TEXT("Output");

	UPROPERTY(EditAnywhere, Category = "渲染设置", meta = (DisplayName = "预热帧数"))
	int32 WarmUpFrames = 64;

	// 身体动画文件、动画进度（ 0-1 ）、表情列表
	UPROPERTY(EditAnywhere, Category = "Body", meta = (DisplayName = "Animation"))
	UAnimSequence* BodyAnimation = nullptr;

	UPROPERTY(EditAnywhere, Category = "Body", meta = (DisplayName = "Progress", UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float BodyProgress = 0.f;

	UPROPERTY(EditAnywhere, Category = "Facial", meta = (DisplayName = "Expressions"))
	TArray<FFacialExpressionEntry> FacialExpressions;

	// 三个按钮回调函数
	UFUNCTION()
	void ImportJson();

	UFUNCTION()
	void ExportJson();

	UFUNCTION()
	void Render();

	// 导出json文件存放路径、渲染结果输出路径
	static constexpr const TCHAR* SnapshotFilePath = TEXT("Saved/StateSnapshots/exported_state.json");
	static constexpr const TCHAR* RenderOutputDirectory = TEXT("Saved/MovieRenders");
	static constexpr const TCHAR* RenderStateFilePath = TEXT("Saved/StateSnapshots/render_state.json");

protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual void PostEditUndo() override;

private:
	// 按当前 Body/Facial 字段组装状态（不含渲染设置），ExportJson / Render 共用。
	FCharacterState BuildState() const;

	// 按当前 BodyAnimation / BodyProgress / FacialExpressions 全量重同步身体姿态与面部表情。
	void RefreshBodyAndFacial();

	// 在按钮行里创建一个与编辑器属性面板样式一致的按钮（返回按钮以便绑定 OnClicked）。
	UButton* CreateActionButton(UHorizontalBox* Row, const FString& Name, const FString& Label);

	// 把机位枚举映射为相机名（"Front"/"Left_Profile"/"Left_45"/...）。
	static FString GetCameraName(EMetaHumanCamera Camera);

	// 反向：把相机名映射回机位枚举（未知名回退到 Front）。
	static EMetaHumanCamera GetCameraEnum(const FString& Name);

	UPROPERTY()
	UDetailsView* DetailsView;
};
