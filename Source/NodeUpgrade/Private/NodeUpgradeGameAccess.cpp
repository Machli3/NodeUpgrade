#include "NodeUpgradeGameAccess.h"

#include "Buildables/FGBuildableFrackingActivator.h"
#include "Buildables/FGBuildableGeneratorGeoThermal.h"
#include "Buildables/FGBuildableResourceExtractor.h"

void FNodeUpgradeGameAccess::RecalculateExtractor(AFGBuildableResourceExtractor* Extractor)
{
	if (IsValid(Extractor))
	{
		Extractor->CalculateProductionCycleTime();
	}
}

void FNodeUpgradeGameAccess::RecalculateFrackingActivator(AFGBuildableFrackingActivator* Activator)
{
	if (IsValid(Activator))
	{
		Activator->CalculateDefaultPotentialExtractionPerMinute();
	}
}

void FNodeUpgradeGameAccess::RecalculateGeoThermal(AFGBuildableGeneratorGeoThermal* Generator)
{
	if (IsValid(Generator) && Generator->mExtractableResource != nullptr)
	{
		Generator->OnExtractableResourceSet();
	}
}

AActor* FNodeUpgradeGameAccess::GetGeoThermalResource(const AFGBuildableGeneratorGeoThermal* Generator)
{
	return IsValid(Generator) ? Generator->mExtractableResource.Get() : nullptr;
}
