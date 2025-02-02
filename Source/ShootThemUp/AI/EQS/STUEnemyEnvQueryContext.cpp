// Fill out your copyright notice in the Description page of Project Settings.


#include "STUEnemyEnvQueryContext.h"

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"

void USTUEnemyEnvQueryContext::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
    const auto QueryOwner = Cast<AActor>(QueryInstance.Owner.Get());
    const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(QueryOwner);

    if (Blackboard)
    {
        const auto ContextActor = Cast<AActor>(Blackboard->GetValueAsObject(EnemyActorKeyName));
        UEnvQueryItemType_Actor::SetContextHelper(ContextData, ContextActor);
    }
}
