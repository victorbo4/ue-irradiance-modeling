
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "IrradianceViewExtension.h"
#include "Templates/SharedPointer.h"
#include "IrradianceSubsystem.generated.h"



UCLASS()
class UIrradianceSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	TSharedPtr<class FIrradianceViewExtension, ESPMode::ThreadSafe> ViewExt;
	IConsoleObject* CmdRunOnce = nullptr;
	IConsoleObject* CmdRead = nullptr;
	//IConsoleObject* CmdAuto = nullptr;

};