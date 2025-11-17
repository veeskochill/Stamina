// Fill out your copyright notice in the Description page of Project Settings.


#include "StaminaComponent.h"
#include "Misc/DateTime.h"
#include "Blueprint/UserWidget.h"
#include "Net/UnrealNetwork.h"

AActor* UStaminaComponent::GetStaminaOwner() {
	AActor* Owner = GetOwner();
#if !UE_BUILD_SHIPPING
	if (!Owner)
	{
		UE_LOG(LogTemp, Warning, TEXT("UStaminaComponent - Owner is null"));
		return nullptr;
	}
#endif
	return Owner;
}

UWorld* UStaminaComponent::GetStaminaWorld() {
	UWorld* World = GetWorld();
#if !UE_BUILD_SHIPPING
	if (!World) 
	{
		UE_LOG(LogTemp, Warning, TEXT("UStaminaComponent - World is null"));
		return nullptr;
	}
	#endif
	return World;
}


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

	AActor* Owner = GetStaminaOwner();
	if (!Owner)
	{
		return;
	}

	if (Owner->HasAuthority()) {
		CurrentStamina = MaxStamina;
	}

	//Widget needs to be created if we are using a dedi, or if we are standalone
	if (Owner->GetLocalRole() == ROLE_AutonomousProxy || GetNetMode() == NM_Standalone) 
	{
		UWorld* World = GetWorld();
		if (!World || !UIWidgetClass || UIWidgetInstance) {
			return;
		}
		UIWidgetInstance = CreateWidget<UUserWidget>(World, UIWidgetClass);
		if (UIWidgetInstance) {
			UIWidgetInstance->AddToViewport();
		}
	}
}


//Clean up stamina widget
void UStaminaComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (!UIWidgetInstance) 
	{
		return;
	}

	AActor* Owner = GetStaminaOwner();
	if (!Owner)
	{
		return;
	}
	
	if (Owner->GetLocalRole() == ROLE_AutonomousProxy || GetNetMode() == NM_Standalone)
	{
		UIWidgetInstance->RemoveFromViewport();
		UIWidgetInstance = nullptr;
	}
}

//Only tick component on the server (if possible), tick only when we are burning or Regenerating stamina
void UStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetStaminaOwner();
	if (!Owner)
	{
		return;
	}

	if (!Owner->HasAuthority())
	{
		return;
	}


	if (IsBurningStamina )
	{
		if (CurrentStamina > 0.0f) 
		{
			const float OldStamina = CurrentStamina;
			const float NewStamina = FMath::Clamp(CurrentStamina - StaminaBurnRate * DeltaTime, 0.0f, MaxStamina);
			CurrentStamina = NewStamina;
			if (!FMath::IsNearlyEqual(OldStamina, CurrentStamina, BroadcastEpsilon))
			{
				OnStaminaChanged.Broadcast(CurrentStamina); // Server side events
			}
		}
		// Use IsNearlyZero check to avoid exact float comparisons
		if (FMath::IsNearlyZero(CurrentStamina, KINDA_SMALL_NUMBER))
		{
			IsBurningStamina = false;
			SetComponentTickEnabled(false);
		}
	}

	if (IsRegenStamina)
	{
		const float OldStamina = CurrentStamina;
		const float NewStamina = FMath::Clamp(OldStamina + RegenRate * DeltaTime, 0.0f, MaxStamina);
		CurrentStamina = NewStamina;

		if (!FMath::IsNearlyEqual(OldStamina, CurrentStamina, BroadcastEpsilon))
		{
			OnStaminaChanged.Broadcast(CurrentStamina); // Server side events
		}
		if (CurrentStamina >= MaxStamina - KINDA_SMALL_NUMBER)
		{
			SetComponentTickEnabled(false);
			IsRegenStamina = false;
		}
	}
}


//Wait time after burning stamina is done, we can now start regeneration
void UStaminaComponent::RecoveryComplete() {
	IsRegenStamina = true;
	SetComponentTickEnabled(true);
}

void UStaminaComponent::Server_TryUseStamina_Implementation(float staminaAsk, const FGuid& EventId)
{
	AActor* Owner = GetStaminaOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (CurrentStamina >= staminaAsk) {
		Client_StaminaUseResult(true, EventId);
		CurrentStamina -= staminaAsk;

		OnStaminaChanged.Broadcast(CurrentStamina); //Server side events
		UWorld* World = GetWorld();
		if (World)
		{
			World->GetTimerManager().SetTimer(FRecoveryTimerHandle, this, &UStaminaComponent::RecoveryComplete, RegenDelay, false);
		}
	}
	else
	{
		Client_StaminaUseResult(false, EventId);
	}
}

void UStaminaComponent::Server_TryStartUseStamina_Implementation(float staminaBurnRate, const FGuid& EventId) {

	if (CurrentStamina <= 0.0f) {
		Client_StaminaUseResult(false, EventId);
		return;

	}
	StaminaBurnRate = staminaBurnRate;
	IsBurningStamina = true;
	SetComponentTickEnabled(true);
	Client_StaminaUseResult(true, EventId);

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(FRecoveryTimerHandle);
	}
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
void UStaminaComponent::Server_StopUseStamina_Implementation() {
	StaminaBurnRate = 0.0f;
	IsBurningStamina = false;
	SetComponentTickEnabled(false);
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(FRecoveryTimerHandle, this, &UStaminaComponent::RecoveryComplete, RegenDelay, false);
	}
}

float UStaminaComponent::GetStaminaRemaining() 
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