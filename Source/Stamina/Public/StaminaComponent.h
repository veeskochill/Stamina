// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "StaminaComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStaminaChanged, float, UpdatedStamina);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStaminaSuccess, FGuid, EventId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStaminaFailed, FGuid, EventId);


class UUserWidget;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class STAMINA_API UStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStaminaComponent();

	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_TryUseStamina(float staminaAsk, const FGuid& EventId);

	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_TryStartUseStamina(float staminaBurnRate, const FGuid& EventId);

	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_StopUseStamina();

	UFUNCTION(BlueprintCallable)
	float GetStaminaRemaining();

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> UIWidgetClass;

	UPROPERTY(BlueprintAssignable, Category = "Delegates")
	FOnStaminaChanged OnStaminaChanged;
	UPROPERTY(BlueprintAssignable, Category = "Delegates")
	FOnStaminaSuccess OnStaminaSuccess;
	UPROPERTY(BlueprintAssignable, Category = "Delegates")
	FOnStaminaFailed OnStaminaFailed;

protected:

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite,	Category = "Config")
	float RegenRate = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config")
	float RegenDelay = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config")
	float MaxStamina = 100.0f;

	// Small tolerance to prevent excessive tiny-network updates
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Networking")
	float BroadcastEpsilon = 0.01f;


	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	UFUNCTION(Client, Reliable)
	void Client_StaminaUseResult(bool bSuccess, const FGuid& EventId);
	UFUNCTION()
	void OnRep_Stamina();

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void RecoveryComplete();

	AActor* GetStaminaOwner();
	UWorld* GetStaminaWorld();


	UPROPERTY(ReplicatedUsing = OnRep_Stamina)
	float CurrentStamina = 0.0f;

	UUserWidget* UIWidgetInstance;

	bool IsBurningStamina = false;
	bool IsRegenStamina = false;
	float StaminaBurnRate = 0.0f;
	FTimerHandle FRecoveryTimerHandle;

};
