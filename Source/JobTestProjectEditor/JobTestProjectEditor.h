#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"


class SDockTab;
class FSpawnTabArgs;


class FJobTestProjectEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<SDockTab> SpawnToJsonTab(const FSpawnTabArgs& Args);
	void RegisterMenu();

	static const FName ToJsonTabId;
};
