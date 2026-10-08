#include "JobTestProjectEditor.h"
#include "Panel.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Docking/TabManager.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"


#define LOCTEXT_NAMESPACE "JobTestProjectEditor"


IMPLEMENT_MODULE(FJobTestProjectEditorModule, JobTestProjectEditor)


const FName FJobTestProjectEditorModule::ToJsonTabId = FName("MetaHumanToJson");


void FJobTestProjectEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ToJsonTabId,
		FOnSpawnTab::CreateRaw(this, &FJobTestProjectEditorModule::SpawnToJsonTab))
		.SetDisplayName(LOCTEXT("ToJsonTabTitle", "MetaHumanToJson"))
		.SetAutoGenerateMenuEntry(false);

	RegisterMenu();
}


void FJobTestProjectEditorModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ToJsonTabId);
}


TSharedRef<SDockTab> FJobTestProjectEditorModule::SpawnToJsonTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);

	if (GEditor)
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			UMetaHumanPanel* Widget = CreateWidget<UMetaHumanPanel>(World, UMetaHumanPanel::StaticClass());
			if (Widget)
			{
				Tab->SetContent(Widget->TakeWidget());
			}
		}
	}

	return Tab;
}


void FJobTestProjectEditorModule::RegisterMenu()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
		if (!Section.FindEntry("MetaHumanToJson"))  // 避免模块热重载时出现重复项
		{
			Section.AddMenuEntry(
				"MetaHumanToJson",
				LOCTEXT("ToJsonMenuLabel", "MetaHumanToJson"),
				LOCTEXT("ToJsonMenuTip", "Open the MetaHuman-to-JSON details panel."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([]()
					{
						FGlobalTabmanager::Get()->TryInvokeTab(ToJsonTabId);
					})
				)
			);
		}
	}));
}


#undef LOCTEXT_NAMESPACE
