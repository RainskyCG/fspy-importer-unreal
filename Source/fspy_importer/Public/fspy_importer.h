#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class Ffspy_importerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void ImportFSpyFromDialog();
	bool ImportFSpyFile(const FString& FilePath) const;

	FDelegateHandle ToolMenusStartupHandle;
};
