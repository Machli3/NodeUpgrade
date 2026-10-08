#pragma once

#include "CoreMinimal.h"

class AActor;
class AFGBuildableFrackingActivator;
class AFGBuildableGeneratorGeoThermal;
class AFGBuildableResourceExtractor;

/**
 * The only place that touches private / protected game members.
 * Access is granted by Config/AccessTransformers.ini (Friend=...FriendClass="FNodeUpgradeGameAccess").
 * Must stay in the global namespace: the friend declarations injected in the game headers are unqualified.
 */
class NODEUPGRADE_API FNodeUpgradeGameAccess
{
public:
	/** AFGBuildableResourceExtractor::CalculateProductionCycleTime (private): refreshes the cached cycle time from the node's current multiplier. */
	static void RecalculateExtractor(AFGBuildableResourceExtractor* Extractor);

	/** AFGBuildableFrackingActivator::CalculateDefaultPotentialExtractionPerMinute (private): refreshes the pressurizer's displayed total. */
	static void RecalculateFrackingActivator(AFGBuildableFrackingActivator* Activator);

	/** AFGBuildableGeneratorGeoThermal::OnExtractableResourceSet (private): recomputes the generator's power range from the geyser. */
	static void RecalculateGeoThermal(AFGBuildableGeneratorGeoThermal* Generator);

	/** AFGBuildableGeneratorGeoThermal::mExtractableResource (private): the geyser this generator is built on. */
	static AActor* GetGeoThermalResource(const AFGBuildableGeneratorGeoThermal* Generator);
};
