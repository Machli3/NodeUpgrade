#pragma once

#include "CoreMinimal.h"

class AActor;
class AFGCharacterPlayer;
class AFGResourceNode;

/** Finds the resource node the player is aiming at. Runs only when called (key press, chat command): never ticks. */
class NODEUPGRADE_API FNodeUpgradeTargeting
{
public:
	/** Max distance of the fallback camera trace, in cm. */
	static constexpr float TraceDistance = 2000.0f;

	/** The game's own "best usable actor" first, then a single camera line trace. */
	static AFGResourceNode* FindTargetNode(AFGCharacterPlayer* Character);

	/**
	 * Maps an actor to the node it stands for: the node itself, its rock mesh (AFGNodeMeshActor),
	 * an extractor built on it (miner, oil extractor, fracking extractor) or a geothermal generator.
	 * Returns null for unsupported node types (deposits, fracking cores).
	 */
	static AFGResourceNode* ResolveNode(AActor* Actor);

	/** Normal nodes, fracking satellites and geysers. */
	static bool IsSupportedNode(const AFGResourceNode* Node);
};
