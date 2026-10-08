#include "NodeUpgradeTargeting.h"

#include "Buildables/FGBuildableGeneratorGeoThermal.h"
#include "Buildables/FGBuildableResourceExtractorBase.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "FGCharacterPlayer.h"
#include "NodeUpgradeGameAccess.h"
#include "Resources/FGResourceNode.h"
#include "Resources/FGResourceNodeBase.h"

AFGResourceNode* FNodeUpgradeTargeting::FindTargetNode(AFGCharacterPlayer* Character)
{
	if (!IsValid(Character))
	{
		return nullptr;
	}

	if (AFGResourceNode* Node = ResolveNode(Character->GetBestUsableActor()))
	{
		return Node;
	}

	UWorld* World = Character->GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	const FVector Start = Character->GetCameraComponentWorldLocation();
	const FVector End = Start + Character->GetCameraComponentForwardVector() * TraceDistance;
	FCollisionQueryParams Params(FName(TEXT("NodeUpgradeTrace")), /*bInTraceComplex*/ false, Character);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return ResolveNode(Hit.GetActor());
	}
	return nullptr;
}

AFGResourceNode* FNodeUpgradeTargeting::ResolveNode(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	if (AFGResourceNode* Node = Cast<AFGResourceNode>(Actor))
	{
		return IsSupportedNode(Node) ? Node : nullptr;
	}

	if (const AFGNodeMeshActor* MeshActor = Cast<AFGNodeMeshActor>(Actor))
	{
		AFGResourceNode* Node = Cast<AFGResourceNode>(MeshActor->mNodeActor.Get());
		return IsSupportedNode(Node) ? Node : nullptr;
	}

	if (const AFGBuildableResourceExtractorBase* Extractor = Cast<AFGBuildableResourceExtractorBase>(Actor))
	{
		AFGResourceNode* Node = Cast<AFGResourceNode>(Extractor->GetExtractableResource().GetObject());
		return IsSupportedNode(Node) ? Node : nullptr;
	}

	if (const AFGBuildableGeneratorGeoThermal* Generator = Cast<AFGBuildableGeneratorGeoThermal>(Actor))
	{
		AFGResourceNode* Node = Cast<AFGResourceNode>(FNodeUpgradeGameAccess::GetGeoThermalResource(Generator));
		return IsSupportedNode(Node) ? Node : nullptr;
	}

	return nullptr;
}

bool FNodeUpgradeTargeting::IsSupportedNode(const AFGResourceNode* Node)
{
	if (!IsValid(Node))
	{
		return false;
	}
	switch (Node->GetResourceNodeType())
	{
	case EResourceNodeType::Node:
	case EResourceNodeType::FrackingSatellite:
	case EResourceNodeType::Geyser:
		return true;
	default:
		return false;
	}
}
