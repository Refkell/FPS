// Fill out your copyright notice in the Description page of Project Settings.

#include "STUHealthComponent.h"
#include "TimerManager.h"

#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Perception/AISense_Damage.h"

#include "../Dev/STUFireDamageType.h"
#include "../Dev/STUIceDamageType.h"

#include "../STUGameModeBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogHealthComponent, All, All)

USTUHealthComponent::USTUHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float USTUHealthComponent::GetHealth() const
{
	return Health;
}

float USTUHealthComponent::GetHealthPercent() const
{
	return Health / MaxHealth;
}

float USTUHealthComponent::GetMaxHealth() const
{
	return MaxHealth;
}

bool USTUHealthComponent::IsDead() const
{
	return Health <= 0.f && bCanBeKilled;
}

bool USTUHealthComponent::TryToAddHealth(float HealthAmount)
{
	if (!IsDead() && Health < MaxHealth)
	{
		SetHealth(Health + HealthAmount);
		return true;
	}

	return false;
}

void USTUHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	SetHealth(MaxHealth);

	GetOwner()->OnTakeAnyDamage.AddDynamic(this, &USTUHealthComponent::OnTakeAnyDamage);
	GetOwner()->OnTakePointDamage.AddDynamic(this, &USTUHealthComponent::OnTakePointDamage);
	GetOwner()->OnTakeRadialDamage.AddDynamic(this, &USTUHealthComponent::OnTakeRadialDamage);
}

void USTUHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USTUHealthComponent::OnTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser)
{
	// Possible Override
}

void USTUHealthComponent::OnTakePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation, class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser)
{
	ApplyDamage(Damage * GetDamageModifier(DamagedActor, BoneName), InstigatedBy);
}

void USTUHealthComponent::OnTakeRadialDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, FVector Origin, const FHitResult& HitInfo, class AController* InstigatedBy, AActor* DamageCauser)
{
	ApplyDamage(Damage, InstigatedBy);
}

void USTUHealthComponent::SetHealth(float NewHealth)
{
	const float NextHealth = FMath::Clamp(NewHealth, 0.f, MaxHealth);
	const float HealthDelta = NextHealth - Health;

	Health = NextHealth;
	OnHealthChanged.Broadcast(Health, HealthDelta, MaxHealth);
}

void USTUHealthComponent::HealUpdate()
{
	if (!bAutoHeal || Health == MaxHealth || IsDead())
	{
		GetWorld()->GetTimerManager().ClearTimer(HealTimerHandle);
	}
	else
	{
		SetHealth(Health + HealModifier);
	}
}

float USTUHealthComponent::GetDamageModifier(AActor* DamagedActor, FName BoneName) const
{
	const auto Character = Cast<ACharacter>(GetOwner());

	if (Character && Character->GetMesh() && Character->GetMesh()->GetBodyInstance(BoneName))
	{
		const auto PhysMaterial = Character->GetMesh()->GetBodyInstance(BoneName)->GetSimplePhysicalMaterial();

		if (DamageModifiers.Contains(PhysMaterial))
		{
			return DamageModifiers[PhysMaterial];
		}
	}

	return 1.0f;
}

void USTUHealthComponent::ReportDamageEvent(float Damage, AController* InvistigatedBy)
{
	if (GetOwner() && InvistigatedBy && InvistigatedBy->GetPawn())
	{
		UAISense_Damage::ReportDamageEvent(GetWorld(), GetOwner(), InvistigatedBy->GetPawn(), Damage, InvistigatedBy->GetPawn()->GetActorLocation(), GetOwner()->GetActorLocation());
	}
}

void USTUHealthComponent::ApplyDamage(float Damage, AController* InstigatedBy)
{
	if (Damage <= 0.f || IsDead())
		return;

	SetHealth(Health - Damage);

	if (IsDead())
	{
		Killed(InstigatedBy);
		OnDeath.Broadcast();
	}
	else if (bAutoHeal)
	{
		GetWorld()->GetTimerManager().ClearTimer(HealTimerHandle);
		GetWorld()->GetTimerManager().SetTimer(HealTimerHandle, this, &USTUHealthComponent::HealUpdate, HealUpdateTime, true, HealDelayTime);
	}

	PlayShake();
	ReportDamageEvent(Damage, InstigatedBy);
}

void USTUHealthComponent::PlayShake()
{
	if (!IsDead())
	{
		const auto Player = Cast<APawn>(GetOwner());

		if (Player)
		{
			const auto Controller = Player->GetController<APlayerController>();

			if (Controller && Controller->PlayerCameraManager)
			{
				Controller->PlayerCameraManager->StartCameraShake(CameraShake);
			}
		}
	}
}

void USTUHealthComponent::Killed(AController* Killer)
{
	const auto GameMode = Cast<ASTUGameModeBase>(GetWorld()->GetAuthGameMode());

	if (GameMode)
	{
		const auto Player = Cast<APawn>(GetOwner());
		const auto Controller = Player ? Player->Controller : nullptr;

		GameMode->Killed(Killer, Controller);
	}
}

