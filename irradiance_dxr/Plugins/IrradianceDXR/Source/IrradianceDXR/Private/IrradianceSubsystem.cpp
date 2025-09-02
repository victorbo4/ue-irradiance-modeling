
#include "IrradianceSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/OutputDevice.h"
#include "Engine/EngineTypes.h"
#include "Engine/Console.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameViewportClient.h"
#include "EngineGlobals.h"
#include "SceneViewExtension.h"


void UIrradianceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ViewExt = FSceneViewExtensions::NewExtension<FIrradianceViewExtension>();
	UE_LOG(LogTemp, Log, TEXT("SceneViewExtensionTemplate: Subsystem initialized & SceneViewExtension created"));

	IConsoleManager& ICM = IConsoleManager::Get();

	// Comando Pyrano.RunOnce -> ejecutar shader
	CmdRunOnce = ICM.RegisterConsoleCommand(
		TEXT("Pyrano.RunOnce"),
		TEXT("Ejecuta el pass de irradiancia en el siguiente PrePostProcess."),
		FConsoleCommandDelegate::CreateLambda([this]()
			{
				if (ViewExt.IsValid()) ViewExt->RequestOneShot();
			}),
		ECVF_Default);

	//Comando Pyrano.Read -> leer resultado
	CmdRead = ICM.RegisterConsoleCommand(
		TEXT("Pyrano.Read"),
		TEXT("Intenta leer el resultado del readback."),
		FConsoleCommandDelegate::CreateLambda([this]()
			{
				if (!ViewExt.IsValid()) { UE_LOG(LogTemp, Warning, TEXT("ViewExt invalido")); return; }
				FLinearColor Result;
				if (ViewExt->TryConsumeReadback(Result)) {
					UE_LOG(LogTemp, Display, TEXT("[Pyrano.Read] = %s"), *Result.ToString());
				}
				else {
					UE_LOG(LogTemp, Display, TEXT("[Pyrano.Read] Aún no listo."));
				}
			}),
		ECVF_Default);

	/*CmdAuto = ICM.RegisterConsoleCommand(
		TEXT("Pyrano.Auto"),
		TEXT("Activa/desactiva ejecución cada frame: Pyrano.Auto 0/1"),
		FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
			{
				if (!ViewExt.IsValid()) return;
				const bool bEnable = (Args.Num() > 0) ? (FCString::Atoi(*Args[0]) != 0) : true;
				ViewExt->SetAutoMode(bEnable);
				UE_LOG(LogTemp, Display, TEXT("[Pyrano.Auto] %s"), bEnable ? TEXT("ON") : TEXT("OFF"));
			}),
		ECVF_Default);*/


}


void UIrradianceSubsystem::Deinitialize()
{
	IConsoleManager& ICM = IConsoleManager::Get();
	if (CmdRunOnce) { ICM.UnregisterConsoleObject(CmdRunOnce); CmdRunOnce = nullptr; }
	if (CmdRead) { ICM.UnregisterConsoleObject(CmdRead);    CmdRead = nullptr; }
	//if (CmdAuto) { ICM.UnregisterConsoleObject(CmdAuto);    CmdAuto = nullptr; }

	ViewExt.Reset();
	Super::Deinitialize();
}
