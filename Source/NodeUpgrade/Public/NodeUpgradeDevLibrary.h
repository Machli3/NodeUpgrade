#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NodeUpgradeDevLibrary.generated.h"

class UDataTable;
class UFont;
class UUserWidget;

/**
 * Developer tools, callable from editor Python (unreal.NodeUpgradeDevLibrary).
 * Used to read the exact reflection data (functions, parameters, variables, widget tree, fonts) of the game's own widgets
 * before reusing them in the menu, so that nothing is called with a guessed signature.
 */
UCLASS()
class NODEUPGRADE_API UNodeUpgradeDevLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Multi-line description of a class declared by the class itself (not its parents): functions with parameters, properties, delegate signatures and, for widget blueprints, the widget tree and named slots. */
	UFUNCTION(BlueprintCallable, Category = "NodeUpgrade|Dev")
	static FString DescribeClass(UClass* Class);

	/** Multi-line description of a font asset: cache type, typefaces of the default family (name, face asset, loading policy), fallback and sub-fonts (cultures, character ranges). */
	UFUNCTION(BlueprintCallable, Category = "NodeUpgrade|Dev")
	static FString DescribeFont(UFont* Font);

	/** One line per row of a rich text style table (FRichTextStyleRow): font, typeface, size and color of each named style. */
	UFUNCTION(BlueprintCallable, Category = "NodeUpgrade|Dev")
	static FString DescribeTextStyleTable(UDataTable* Table);

	/** One line per text block of a live widget, nested user widgets included: owner, font asset, typeface, size, spacing, outline and text. */
	static FString DescribeTextFonts(UUserWidget* Widget);
};
