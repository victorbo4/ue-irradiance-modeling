/*=============================================================================
	PyranoEditor.h: Pyrano plugin editor module.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FPyranoEditorModule : public IModuleInterface
{

public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OpenPyranoPlanner();
};
