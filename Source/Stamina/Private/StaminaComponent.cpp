// Fill out your copyright notice in the Description page of Project Settings.


#include "StaminaComponent.h"
#include "Blueprint/UserWidget.h"
#include "Net/UnrealNetwork.h"

// Sets default values for this component's properties
UStaminaComponent::UStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickEnabled(false);
	SetIsReplicatedByDefault(true);
}

// Called when the game starts
void UStaminaComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (Owner->HasAuthority()) 
	{
		CurrentStamina = MaxStamina;
	}

	//Widget needs to be created if we are using a dedi, or if we are standalone
	if (Owner->GetLocalRole() == ROLE_AutonomousProxy || GetNetMode() == NM_Standalone) 
	{
		UWorld* World = GetWorld();
		check(World);

		if (!UIWidgetClass || UIWidgetInstance) 
		{
			return;
		}

		if (APawn* OwnerPawn = Cast<APawn>(Owner))
		{
			if (APlayerController* PlayerController = Cast<APlayerController>(OwnerPawn->GetController()))
			{
				UIWidgetInstance = CreateWidget<UUserWidget>(PlayerController, UIWidgetClass);
				if (UIWidgetInstance) 
				{
					UIWidgetInstance->AddToViewport();
				}
			}
		}
	}
}

//Clean up stamina widget
void UStaminaComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (UIWidgetInstance && IsValid(UIWidgetInstance))
	{
		UIWidgetInstance->RemoveFromParent();
		UIWidgetInstance = nullptr;
	}
}

//Only tick component on the server (if possible), tick only when we are burning or Regenerating stamina
void UStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!Owner->HasAuthority())
	{
		return;
	}


	if (bIsBurningStamina )
	{
		if (CurrentStamina > 0.0f) 
		{
			const float OldStamina = CurrentStamina;
			float NewStamina = CurrentStamina - StaminaBurnRate * DeltaTime;
			if(NewStamina < 0.0f){
				NewStamina = 0.0f;
				bIsBurningStamina = false;
				SetComponentTickEnabled(false);
			}

			CurrentStamina = NewStamina;
			if (!FMath::IsNearlyEqual(OldStamina, CurrentStamina, BroadcastEpsilon))
			{
				OnStaminaChanged.Broadcast(CurrentStamina); // Server side events
			}
		}
	}

	if (bIsRegenStamina)
	{
		const float OldStamina = CurrentStamina;
		const float NewStamina = FMath::Clamp(OldStamina + RegenRate * DeltaTime, 0.0f, MaxStamina);
		CurrentStamina = NewStamina;

		if (!FMath::IsNearlyEqual(OldStamina, CurrentStamina, BroadcastEpsilon))
		{
			OnStaminaChanged.Broadcast(CurrentStamina); // Server side events
		}
		if (CurrentStamina >= MaxStamina) 
		{
			CurrentStamina = MaxStamina;
			SetComponentTickEnabled(false);
			bIsRegenStamina = false;
		}
	}
}


//Wait time after burning stamina is done, we can now start regeneration
void UStaminaComponent::RecoveryComplete() 
{

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (!bIsBurningStamina)
	{
		bIsRegenStamina = true;
		SetComponentTickEnabled(true);
	}
}

void UStaminaComponent::Server_TryUseStamina_Implementation(float StaminaCost, const FGuid& EventId)
{
	if (CurrentStamina >= StaminaCost) 
	{
		Client_StaminaUseResult(true, EventId);
		CurrentStamina -= StaminaCost;

		OnStaminaChanged.Broadcast(CurrentStamina); //Server side events
		UWorld* World = GetWorld();
		check(World);
		World->GetTimerManager().SetTimer(FRecoveryTimerHandle, this, &UStaminaComponent::RecoveryComplete, RegenDelay, false);
	}
	else
	{
		Client_StaminaUseResult(false, EventId);
	}
}

void UStaminaComponent::Server_TryStartUseStamina_Implementation(float BurnRate, const FGuid& EventId) 
{
	if (CurrentStamina <= 0.0f) 
	{
		Client_StaminaUseResult(false, EventId);
		return;

	}
	StaminaBurnRate = BurnRate;
	bIsBurningStamina = true;
	SetComponentTickEnabled(true);
	Client_StaminaUseResult(true, EventId);

	bIsRegenStamina = false;
	UWorld* World = GetWorld();
	check(World);
	World->GetTimerManager().ClearTimer(FRecoveryTimerHandle);
}

//Notify the client if the action succeeds or fails with the appropriate handle (EventId)
void UStaminaComponent::Client_StaminaUseResult_Implementation(bool bSuccess, const FGuid& EventId){

	if (bSuccess)
	{
		OnStaminaSuccess.Broadcast(EventId);
	}
	else
	{
		OnStaminaFailed.Broadcast(EventId);
	}
}

//We are requiring that the user stop trying to use stamina before we allow the recovery timer to start
void UStaminaComponent::Server_StopUseStamina_Implementation() 
{
	StaminaBurnRate = 0.0f;
	bIsBurningStamina = false;
	SetComponentTickEnabled(false);
	UWorld* World = GetWorld();
	check(World);
	World->GetTimerManager().SetTimer(FRecoveryTimerHandle, this, &UStaminaComponent::RecoveryComplete, RegenDelay, false);
}

float UStaminaComponent::GetStaminaRemaining() const
{
	return CurrentStamina;
}

void UStaminaComponent::OnRep_Stamina()
{
	OnStaminaChanged.Broadcast(CurrentStamina); //Client side update
}

void UStaminaComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UStaminaComponent, CurrentStamina, COND_OwnerOnly);
}