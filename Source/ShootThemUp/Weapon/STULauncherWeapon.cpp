// Fill out your copyright notice in the Description page of Project Settings.

#include "STULauncherWeapon.h"
#include "STUProjectile.h"

#include "Sound/SoundCue.h"
#include "Kismet/GameplayStatics.h"

ASTULauncherWeapon::ASTULauncherWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASTULauncherWeapon::StartFire()
{
	MakeShot();
}

void ASTULauncherWeapon::StopFire()
{
	// Possible Override
}

void ASTULauncherWeapon::BeginPlay()
{
	Super::BeginPlay();
}

void ASTULauncherWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASTULauncherWeapon::MakeShot()
{
	FVector TraceStart, TraceEnd;

	if (IsClipEmpty() || !GetTraceData(TraceStart, TraceEnd))
	{
		UGameplayStatics::SpawnSoundAttached(NoAmmoSound, WeaponMesh, MuzzleSocket);

		if (!IsAmmoEmpty())
			OnEmptyClip.Broadcast(this);

		StopFire();
		return;
	}

	FHitResult HitResult;
	MakeHit(HitResult, TraceStart, TraceEnd);

	FVector EndPoint = HitResult.bBlockingHit ? HitResult.ImpactPoint : TraceEnd;
	FVector Direction = (EndPoint - GetMuzzleWorldLocation()).GetSafeNormal();

	FTransform SpawnTransform(FRotator::ZeroRotator, GetMuzzleWorldLocation());
	ASTUProjectile* Projectile = GetWorld()->SpawnActorDeferred<ASTUProjectile>(ProjectileClass, SpawnTransform);

	if (Projectile)
	{
		Projectile->SetOwner(GetOwner());
		Projectile->SetShotDirection(Direction);
		Projectile->FinishSpawning(SpawnTransform);
	}

	if (HitResult.bBlockingHit)
	{
		//DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 10.f, 24, FColor::Red, false, 3.0f);
	}

	//DrawDebugLine(GetWorld(), GetMuzzleWorldLocation(), EndPoint, FColor::Blue, false, 1.0f, 0.f, 3.f);
	
	SpawnMuzzleFX();
	
	UGameplayStatics::SpawnSoundAttached(FireSound, WeaponMesh, MuzzleSocket);

	DecreaseAmmo();	
}
