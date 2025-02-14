// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DuneSurfer.h"
#include "DuneSurferCharacter.h"
#include "DuneSurferProjectile.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/InputSettings.h"
#include "Kismet/HeadMountedDisplayFunctionLibrary.h"
#include "MotionControllerComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// ADuneSurferCharacter

ADuneSurferCharacter::ADuneSurferCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->RelativeLocation = FVector(-39.56f, 1.75f, 64.f); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->RelativeRotation = FRotator(1.9f, -19.19f, 5.2f);
	Mesh1P->RelativeLocation = FVector(-0.5f, -4.4f, -155.7f);

	// Create a gun mesh component
	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(true);			// only the owning player will see this mesh
	FP_Gun->bCastDynamicShadow = false;
	FP_Gun->CastShadow = false;
	// FP_Gun->SetupAttachment(Mesh1P, TEXT("GripPoint"));
	FP_Gun->SetupAttachment(RootComponent);

	FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FP_MuzzleLocation->SetupAttachment(FP_Gun);
	FP_MuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 30.0f, 10.0f);

	// Note: The ProjectileClass and the skeletal mesh/anim blueprints for Mesh1P, FP_Gun, and VR_Gun 
	// are set in the derived blueprint asset named MyCharacter to avoid direct content references in C++.

	//this->ReceiveHit.AddDynamic(this, &ADuneSurferCharacter::OnReceiveHit);

	UCapsuleComponent* capsule = Cast<UCapsuleComponent>(RootComponent);
	if (capsule) { capsule->SetNotifyRigidBodyCollision(true); }
}

void ADuneSurferCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Attach gun mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("GripPoint"));

	Mesh1P->SetHiddenInGame(false, true);

	for (TActorIterator<AActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		FString ActorName = ActorItr->GetName();
		if (ActorName.Contains(TEXT("Landscape_")))
		{
			Landscape = *ActorItr;
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ADuneSurferCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ADuneSurferCharacter::JumpPressed);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ADuneSurferCharacter::JumpReleased);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ADuneSurferCharacter::OnFire);

	PlayerInputComponent->BindAxis("MoveForward", this, &ADuneSurferCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ADuneSurferCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ADuneSurferCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ADuneSurferCharacter::LookUpAtRate);
}

void ADuneSurferCharacter::OnFire()
{
	// try and fire a projectile
	if (ProjectileClass != NULL)
	{
		UWorld* const World = GetWorld();
		if (World != NULL)
		{
			const FRotator SpawnRotation = GetControlRotation();
			// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
			const FVector SpawnLocation = ((FP_MuzzleLocation != nullptr) ? FP_MuzzleLocation->GetComponentLocation() : GetActorLocation()) + SpawnRotation.RotateVector(GunOffset);

			// spawn the projectile at the muzzle
			World->SpawnActor<ADuneSurferProjectile>(ProjectileClass, SpawnLocation, SpawnRotation);
		}
	}

	// try and play the sound if specified
	if (FireSound != NULL)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation(), 0.1f); //Make it quieter. Too loud by default
	}

	// try and play a firing animation if specified
	if (FireAnimation != NULL)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
		if (AnimInstance != NULL)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}

void ADuneSurferCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		SetInclineSpeedModifier(Value);
		AddMovementInput(GetActorForwardVector(), Value * InclineSpeedModifier);
	}
}

void ADuneSurferCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void ADuneSurferCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ADuneSurferCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void ADuneSurferCharacter::JumpPressed()
{
	Super::Jump();
}

void ADuneSurferCharacter::JumpReleased()
{
	Super::StopJumping();
}

void ADuneSurferCharacter::NotifyHit(	class UPrimitiveComponent* MyComp,
										class AActor* Other,
										class UPrimitiveComponent* OtherComp,
										bool bSelfMoved,
										FVector HitLocation,
										FVector HitNormal,
										FVector NormalImpulse,
										const FHitResult& Hit)
{
	return;
}

void ADuneSurferCharacter::Tick(float DeltaSeconds)
{

}

void ADuneSurferCharacter::DetectTerrainSlope()
{
	FVector StartLocForGroundTrace = GetActorLocation();
	FVector EndLocForGroundTrace = GetActorLocation() + (FVector::UpVector * -1 * 10000.f);
	FHitResult hit;
	FCollisionQueryParams ignoreSelfParams;
	ignoreSelfParams.AddIgnoredActor(this);
	if (GetWorld()->LineTraceSingleByObjectType(hit, StartLocForGroundTrace, EndLocForGroundTrace, FCollisionObjectQueryParams::DefaultObjectQueryParam, ignoreSelfParams))
	{
		if (hit.Actor == Landscape)
		{
			FVector InclineTraceStart = GetActorLocation();
			FVector InclineTraceDirection = GetActorForwardVector().RotateAngleAxis(-60.f, FVector::CrossProduct(GetActorForwardVector(), FVector::UpVector));
			FVector InclineTraceEnd = GetActorLocation() + InclineTraceDirection * 10000.f;
			FHitResult inclineHit;
			if (GetWorld()->LineTraceSingleByObjectType(inclineHit, InclineTraceStart, InclineTraceEnd, FCollisionObjectQueryParams::DefaultObjectQueryParam, ignoreSelfParams))
			{
				if (inclineHit.Actor == Landscape)
				{
					FVector newForward = inclineHit.ImpactPoint - hit.ImpactPoint;
					ForwardTerrainDirection = newForward.GetSafeNormal();
					RightTerrainDirection = -FVector::CrossProduct(ForwardTerrainDirection, FVector::UpVector).GetSafeNormal();
					return;
				}
			}
		}
	}

	ForwardTerrainDirection = GetActorForwardVector();
	RightTerrainDirection = GetActorRightVector();
}

void ADuneSurferCharacter::SetInclineSpeedModifier(float Direction)
{
	if (ForwardTerrainDirection.Z == 0)
	{
		InclineSpeedModifier = 1.0f;
		return;
	}
	
	if (ForwardTerrainDirection.Z < 0)
	{
		if (Direction < 0)
		{
			InclineSpeedModifier = 1 - FMath::Abs(ForwardTerrainDirection.Z);
		}
		else
		{
			InclineSpeedModifier = 1 + FMath::Abs(ForwardTerrainDirection.Z);
		}
	}
	else
	{
		if (Direction < 0)
		{
			InclineSpeedModifier = 1 + ForwardTerrainDirection.Z;
		}
		else
		{
			InclineSpeedModifier = 1 - ForwardTerrainDirection.Z;
		}
	}
}