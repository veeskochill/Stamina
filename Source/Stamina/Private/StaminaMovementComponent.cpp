// Fill out your copyright notice in the Description page of Project Settings.

#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "CharacterAttributeSet.h"
#include "StaminaMovementComponent.h"

float UStaminaMovementComponent::GetMaxSpeed() const
{
    const ACharacter* Char = CharacterOwner;
    if (!Char)
    {
        return Super::GetMaxSpeed();
    }

    const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Char);
    if (!ASI)
    {
        return Super::GetMaxSpeed();
    }

    const UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
    if (!ASC)
    {
        return Super::GetMaxSpeed();
    }

    const float MoveSpeed = ASC->GetNumericAttribute(UCharacterAttributeSet::GetSpeedAttribute());
    return MoveSpeed > 0.f ? MoveSpeed : Super::GetMaxSpeed();
}