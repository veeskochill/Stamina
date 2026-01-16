// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StaminaMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class STAMINA_API UStaminaMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:

	virtual float GetMaxSpeed() const override;
	
};
