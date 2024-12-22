// Fill out your copyright notice in the Description page of Project Settings.

#include "STUGameModeBase.h"

#include "Player/STUBaseCharacter.h"
#include "Player/STUPlayerController.h"
#include "Player/STUPlayerState.h"
#include "Components/STURespawnComponent.h"
#include "Components/STUWeaponComponent.h"
#include "UI/STUGameHUD.h"
#include "AIController.h"

#include "EngineUtils.h"

#include "Sound/SoundCue.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameModeBase, All, All)

ASTUGameModeBase::ASTUGameModeBase()
{
    DefaultPawnClass = ASTUBaseCharacter::StaticClass();
    PlayerControllerClass = ASTUPlayerController::StaticClass();
    HUDClass = ASTUGameHUD::StaticClass();
    PlayerStateClass = ASTUPlayerState::StaticClass();
}

void ASTUGameModeBase::StartPlay()
{
    Super::StartPlay();

    SpawnBots();
    CreateTeams();
    
    StartRound();
    SetMatchState(ESTUMatchState::InProgress);
}

bool ASTUGameModeBase::SetPause(APlayerController* PC, FCanUnpause CanUnpauseDelegate /*= FCanUnpause()*/)
{
    bool bPauseSet = Super::SetPause(PC, CanUnpauseDelegate);

    if (bPauseSet)
    {
        StopAllFire();
        SetMatchState(ESTUMatchState::Pause);
    }

    return bPauseSet;
}

bool ASTUGameModeBase::ClearPause()
{
    bool bPauseCleared = Super::ClearPause();

    if (bPauseCleared)
    {
        SetMatchState(ESTUMatchState::InProgress);
    }

    return bPauseCleared;
}

int32 ASTUGameModeBase::GetCurrentRoundNum() const
{
    return CurrentRound;
}

int32 ASTUGameModeBase::GetTotalRoundsNum() const
{
    return GameData.RoundsNum;
}

int32 ASTUGameModeBase::GetRoundTimeRemaining() const
{
    return RoundCountDown;
}

void ASTUGameModeBase::RespawnRequest(AController* Controller)
{
    ResetPlayer(Controller);

    if (Controller->IsPlayerController())
        UGameplayStatics::PlaySound2D(GetWorld(), PlayerRespawnSound);
}

UClass* ASTUGameModeBase::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    if (InController && InController->IsA<AAIController>())
    {
        return AIPawnClass;
    }

    return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ASTUGameModeBase::Killed(AController* Killer, AController* Victim)
{
    if (Killer)
    {
        const auto PlayerState = Cast<ASTUPlayerState>(Killer->PlayerState);
        if (PlayerState) PlayerState->AddKill();
    }

    if (Victim)
    {
        const auto PlayerState = Cast<ASTUPlayerState>(Victim->PlayerState);
        if (PlayerState) PlayerState->AddDeath();
    }

    RespawnPlayer(Victim);

    if (Victim->IsPlayerController())
        UGameplayStatics::PlaySound2D(GetWorld(), PlayerDeathSound);
}

void ASTUGameModeBase::LogInfo()
{
    for (auto Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
    {
        const auto Controller = Iterator->Get();

        if (Controller)
        {
            const auto PlayerState = Cast<ASTUPlayerState>(Controller->PlayerState);

            if (PlayerState)
            {
                PlayerState->LogInfo();
            }
        }
    }
}

void ASTUGameModeBase::SpawnBots()
{
    for (int32 Index = 0; Index < GameData.PlayersNum - 1; ++Index)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        const auto AIController = GetWorld()->SpawnActor<AAIController>(AIControllerClass, SpawnParams);
        RestartPlayer(AIController);
    }
}

void ASTUGameModeBase::CreateTeams()
{
    int32 TeamID = -1;
    FLinearColor TeamColor = FLinearColor::Black;

    for (auto Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
    {
        const auto Controller = Iterator->Get();

        if (Controller)
        {
            const auto PlayerState = Cast<ASTUPlayerState>(Controller->PlayerState);

            if (PlayerState)
            {
                if (GameData.TeamsInfo.Num() > 0)
                {
                    TeamID = (TeamID + 1) % GameData.TeamsInfo.Num();
                    TeamColor = GameData.TeamsInfo[TeamID];
                }
                else
                {
                    TeamID = TeamID + 1;
                    TeamColor = FLinearColor::MakeRandomSeededColor(TeamID);
                }

                PlayerState->SetTeamID(TeamID);
                PlayerState->SetTeamColor(TeamColor);
                PlayerState->SetPlayerName(Controller->IsPlayerController() ? "Player" : "Bot");

                SetPlayerColor(Controller);
            }
        }
    }
}

void ASTUGameModeBase::SetPlayerColor(AController* InController)
{
    if (InController)
    {
        auto Character = Cast<ASTUBaseCharacter>(InController->GetPawn());
        auto PlayerState = Cast<ASTUPlayerState>(InController->PlayerState);

        if (Character && PlayerState)
        {
            Character->SetPlayerColor(PlayerState->GetTeamColor());
        }
    }
}

void ASTUGameModeBase::StartRound()
{
    RoundCountDown = GameData.RoundTime;

    GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &ASTUGameModeBase::UpdateRound, 1.0f, true);

    UGameplayStatics::PlaySound2D(GetWorld(), PlayerRespawnSound);

    UE_LOG(LogGameModeBase, Display, TEXT("--------- Round %i / %i ---------"), CurrentRound, GameData.RoundsNum);
}

void ASTUGameModeBase::UpdateRound()
{
    if (--RoundCountDown <= 0)
    {
        GetWorldTimerManager().ClearTimer(RoundTimerHandle);

        if (++CurrentRound <= GameData.RoundsNum)
        {
            StartRound();
            ResetPlayers();
        }
        else
        {
            GameOver();
        }
    }
}

void ASTUGameModeBase::ResetPlayer(AController* InController)
{
    if (InController && InController->GetPawn())
    {
        InController->GetPawn()->Reset();
    }

    RestartPlayer(InController);
    SetPlayerColor(InController);
}

void ASTUGameModeBase::ResetPlayers()
{
    for (auto Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
    {
        ResetPlayer(Iterator->Get());
    }
}

void ASTUGameModeBase::RespawnPlayer(AController* InController)
{
    if (GameData.bAllowRespawn)
    {
        const auto RespawnComponent = InController->FindComponentByClass<USTURespawnComponent>();

        if (RespawnComponent && RoundCountDown > GameData.RespawnTime)
            RespawnComponent->Respawn(GameData.RespawnTime);
    }
}

void ASTUGameModeBase::GameOver()
{
    UE_LOG(LogGameModeBase, Display, TEXT("--------- Stats ---------"));
    LogInfo();
    UE_LOG(LogGameModeBase, Display, TEXT("--------- GameOver ---------"));

    for (auto Pawn : TActorRange<APawn>(GetWorld()))
    {
        Pawn->TurnOff();
        Pawn->DisableInput(nullptr);
    }

    SetMatchState(ESTUMatchState::GameOver);

    UGameplayStatics::PlaySound2D(GetWorld(), PlayerDeathSound);
}

void ASTUGameModeBase::SetMatchState(ESTUMatchState State)
{
    if (MatchState != State)
    {
        MatchState = State;
        OnMatchStateChanged.Broadcast(MatchState);
    }
}

void ASTUGameModeBase::StopAllFire()
{
    for (auto Pawn : TActorRange<APawn>(GetWorld()))
    {
        const auto WeaponComponent = Pawn->FindComponentByClass<USTUWeaponComponent>();

        if (WeaponComponent)
        {
            WeaponComponent->StopFire();
        }
    }
}
