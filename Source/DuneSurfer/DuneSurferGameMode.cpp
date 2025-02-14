// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DuneSurfer.h"
#include "DuneSurferGameMode.h"
#include "DuneSurferHUD.h"
#include "DuneSurferCharacter.h"

ADuneSurferGameMode::ADuneSurferGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ADuneSurferHUD::StaticClass();
}
