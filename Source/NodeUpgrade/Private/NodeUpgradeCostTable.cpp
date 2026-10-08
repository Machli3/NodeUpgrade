#include "NodeUpgradeCostTable.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	constexpr int32 SupportedSchemaVersion = 1;
	constexpr int32 MaxAmountPerItem = 1000000;

	/** Reads one tier ("items" array). Duplicate item classes are merged. Returns false and fills OutError on invalid data. */
	bool ParseTier(const TSharedPtr<FJsonObject>& TierObject, TArray<FNodeUpgradeCostItem>& OutItems, FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* ItemValues = nullptr;
		if (!TierObject.IsValid() || !TierObject->TryGetArrayField(TEXT("items"), ItemValues) || ItemValues == nullptr || ItemValues->Num() == 0)
		{
			OutError = TEXT("'items' must be a non-empty array");
			return false;
		}

		for (int32 Index = 0; Index < ItemValues->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* ItemObject = nullptr;
			if (!(*ItemValues)[Index].IsValid() || !(*ItemValues)[Index]->TryGetObject(ItemObject) || ItemObject == nullptr)
			{
				OutError = FString::Printf(TEXT("items[%d] is not an object"), Index);
				return false;
			}

			FString ItemClassName;
			if (!(*ItemObject)->TryGetStringField(TEXT("itemClass"), ItemClassName) || ItemClassName.TrimStartAndEnd().IsEmpty())
			{
				OutError = FString::Printf(TEXT("items[%d].itemClass must be a non-empty string"), Index);
				return false;
			}
			ItemClassName.TrimStartAndEndInline();

			double AmountValue = 0.0;
			if (!(*ItemObject)->TryGetNumberField(TEXT("amount"), AmountValue)
				|| AmountValue != FMath::FloorToDouble(AmountValue)
				|| AmountValue < 1.0
				|| AmountValue > static_cast<double>(MaxAmountPerItem))
			{
				OutError = FString::Printf(TEXT("items[%d].amount must be an integer between 1 and %d"), Index, MaxAmountPerItem);
				return false;
			}
			const int32 Amount = static_cast<int32>(AmountValue);

			const FName ClassName(*ItemClassName);
			FNodeUpgradeCostItem* Existing = OutItems.FindByPredicate([&ClassName](const FNodeUpgradeCostItem& Item) { return Item.ItemClassName == ClassName; });
			if (Existing != nullptr)
			{
				Existing->Amount = FMath::Min(Existing->Amount + Amount, MaxAmountPerItem);
			}
			else
			{
				OutItems.Add(FNodeUpgradeCostItem{ ClassName, Amount });
			}
		}
		return true;
	}
}

bool FNodeUpgradeCostTable::LoadFromFile(const FString& FilePath, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	if (FilePath.IsEmpty())
	{
		OutErrors.Add(TEXT("Cost file path is empty (plugin NodeUpgrade not found by the plugin manager)"));
		return false;
	}

	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *FilePath))
	{
		OutErrors.Add(FString::Printf(TEXT("Cannot read cost file '%s'"), *FilePath));
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("Cost file '%s' is not valid JSON: %s"), *FilePath, *Reader->GetErrorMessage()));
		return false;
	}

	double SchemaVersion = 0.0;
	if (!Root->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion) || static_cast<int32>(SchemaVersion) != SupportedSchemaVersion)
	{
		OutErrors.Add(FString::Printf(TEXT("Cost file '%s': unsupported or missing schemaVersion (expected %d)"), *FilePath, SupportedSchemaVersion));
		return false;
	}

	// Refund ratio: never more than what was paid, so it is clamped to [0, 1].
	float NewRefundRatio = 0.5f;
	const TSharedPtr<FJsonObject>* RefundObject = nullptr;
	if (Root->TryGetObjectField(TEXT("refund"), RefundObject) && RefundObject != nullptr)
	{
		double Ratio = 0.5;
		if ((*RefundObject)->TryGetNumberField(TEXT("ratio"), Ratio))
		{
			if (Ratio < 0.0 || Ratio > 1.0)
			{
				OutWarnings.Add(FString::Printf(TEXT("refund.ratio %f is outside [0, 1], clamped"), Ratio));
			}
			NewRefundRatio = static_cast<float>(FMath::Clamp(Ratio, 0.0, 1.0));
		}
		else
		{
			OutWarnings.Add(TEXT("refund.ratio missing, using 0.5"));
		}

		FString Rounding;
		if ((*RefundObject)->TryGetStringField(TEXT("rounding"), Rounding) && !Rounding.Equals(TEXT("floor"), ESearchCase::IgnoreCase))
		{
			OutWarnings.Add(FString::Printf(TEXT("refund.rounding '%s' is not supported, refunds are always rounded down"), *Rounding));
		}
	}
	else
	{
		OutWarnings.Add(TEXT("'refund' object missing, using ratio 0.5"));
	}

	const TArray<TSharedPtr<FJsonValue>>* ResourceValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("resources"), ResourceValues) || ResourceValues == nullptr)
	{
		OutErrors.Add(FString::Printf(TEXT("Cost file '%s': 'resources' array missing"), *FilePath));
		return false;
	}

	TArray<FNodeUpgradeCostEntry> NewEntries;
	for (int32 ResourceIndex = 0; ResourceIndex < ResourceValues->Num(); ++ResourceIndex)
	{
		const TSharedPtr<FJsonObject>* ResourceObject = nullptr;
		if (!(*ResourceValues)[ResourceIndex].IsValid() || !(*ResourceValues)[ResourceIndex]->TryGetObject(ResourceObject) || ResourceObject == nullptr)
		{
			OutErrors.Add(FString::Printf(TEXT("resources[%d] is not an object, skipped"), ResourceIndex));
			continue;
		}

		FNodeUpgradeCostEntry Entry;
		if (!(*ResourceObject)->TryGetStringField(TEXT("id"), Entry.Id) || Entry.Id.IsEmpty())
		{
			Entry.Id = FString::Printf(TEXT("resources[%d]"), ResourceIndex);
		}

		FString KindString;
		(*ResourceObject)->TryGetStringField(TEXT("nodeKind"), KindString);
		Entry.Kind = ParseNodeKind(KindString);
		if (Entry.Kind == ENodeUpgradeNodeKind::Unknown)
		{
			OutErrors.Add(FString::Printf(TEXT("'%s': unknown nodeKind '%s', skipped"), *Entry.Id, *KindString));
			continue;
		}

		const TSharedPtr<FJsonValue> ResourceClassValue = (*ResourceObject)->TryGetField(TEXT("resourceClass"));
		if (ResourceClassValue.IsValid() && ResourceClassValue->Type == EJson::String && !ResourceClassValue->AsString().TrimStartAndEnd().IsEmpty())
		{
			Entry.ResourceClassName = FName(*ResourceClassValue->AsString().TrimStartAndEnd());
		}
		else if (!ResourceClassValue.IsValid() || ResourceClassValue->Type == EJson::Null)
		{
			// Wildcard. Only geysers are expected to use it: other kinds need an explicit resource.
			if (Entry.Kind != ENodeUpgradeNodeKind::Geyser)
			{
				OutErrors.Add(FString::Printf(TEXT("'%s': resourceClass is required for nodeKind '%s', skipped"), *Entry.Id, *KindString));
				continue;
			}
			Entry.ResourceClassName = NAME_None;
		}
		else
		{
			OutErrors.Add(FString::Printf(TEXT("'%s': resourceClass must be a string or null, skipped"), *Entry.Id));
			continue;
		}

		const TSharedPtr<FJsonObject>* TiersObject = nullptr;
		if (!(*ResourceObject)->TryGetObjectField(TEXT("tiers"), TiersObject) || TiersObject == nullptr)
		{
			OutErrors.Add(FString::Printf(TEXT("'%s': 'tiers' object missing, skipped"), *Entry.Id));
			continue;
		}

		bool bTiersValid = true;
		for (int32 Tier = 0; Tier < NodeUpgradeTier::Count && bTiersValid; ++Tier)
		{
			const TSharedPtr<FJsonObject>* TierObject = nullptr;
			FString TierError;
			if (!(*TiersObject)->TryGetObjectField(TierKey(Tier), TierObject) || TierObject == nullptr)
			{
				TierError = TEXT("tier missing");
				bTiersValid = false;
			}
			else if (!ParseTier(*TierObject, Entry.Tiers[Tier], TierError))
			{
				bTiersValid = false;
			}

			if (!bTiersValid)
			{
				OutErrors.Add(FString::Printf(TEXT("'%s' tier '%s': %s, entry skipped"), *Entry.Id, TierKey(Tier), *TierError));
			}
		}
		if (!bTiersValid)
		{
			continue;
		}

		const bool bDuplicate = NewEntries.ContainsByPredicate([&Entry](const FNodeUpgradeCostEntry& Other)
		{
			return Other.Kind == Entry.Kind && Other.ResourceClassName == Entry.ResourceClassName;
		});
		if (bDuplicate)
		{
			OutWarnings.Add(FString::Printf(TEXT("'%s': another entry already covers this nodeKind/resourceClass, ignored"), *Entry.Id));
			continue;
		}

		NewEntries.Add(MoveTemp(Entry));
	}

	Entries = MoveTemp(NewEntries);
	RefundRatio = NewRefundRatio;
	SourcePath = FilePath;
	bLoaded = true;
	return true;
}

const FNodeUpgradeCostEntry* FNodeUpgradeCostTable::FindEntry(ENodeUpgradeNodeKind Kind, FName ResourceClassName) const
{
	const FNodeUpgradeCostEntry* Wildcard = nullptr;
	for (const FNodeUpgradeCostEntry& Entry : Entries)
	{
		if (Entry.Kind != Kind)
		{
			continue;
		}
		if (Entry.ResourceClassName.IsNone())
		{
			if (Wildcard == nullptr)
			{
				Wildcard = &Entry;
			}
		}
		else if (Entry.ResourceClassName == ResourceClassName)
		{
			return &Entry;
		}
	}
	return Wildcard;
}

FString FNodeUpgradeCostTable::GetDefaultFilePath()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("NodeUpgrade"));
	if (!Plugin.IsValid())
	{
		return FString();
	}
	return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("costs.json"));
}

ENodeUpgradeNodeKind FNodeUpgradeCostTable::ParseNodeKind(const FString& Value)
{
	if (Value.Equals(TEXT("solid"), ESearchCase::IgnoreCase))
	{
		return ENodeUpgradeNodeKind::Solid;
	}
	if (Value.Equals(TEXT("fluid_node"), ESearchCase::IgnoreCase))
	{
		return ENodeUpgradeNodeKind::FluidNode;
	}
	if (Value.Equals(TEXT("fracking_satellite"), ESearchCase::IgnoreCase))
	{
		return ENodeUpgradeNodeKind::FrackingSatellite;
	}
	if (Value.Equals(TEXT("geyser"), ESearchCase::IgnoreCase))
	{
		return ENodeUpgradeNodeKind::Geyser;
	}
	return ENodeUpgradeNodeKind::Unknown;
}

const TCHAR* FNodeUpgradeCostTable::NodeKindToString(ENodeUpgradeNodeKind Kind)
{
	switch (Kind)
	{
	case ENodeUpgradeNodeKind::Solid: return TEXT("solid");
	case ENodeUpgradeNodeKind::FluidNode: return TEXT("fluid_node");
	case ENodeUpgradeNodeKind::FrackingSatellite: return TEXT("fracking_satellite");
	case ENodeUpgradeNodeKind::Geyser: return TEXT("geyser");
	default: return TEXT("unknown");
	}
}

const TCHAR* FNodeUpgradeCostTable::TierKey(int32 Tier)
{
	return Tier == 0 ? TEXT("Impure->Normal") : TEXT("Normal->Pure");
}
