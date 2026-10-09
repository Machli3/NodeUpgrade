#include "NodeUpgradeGameInstanceModule.h"

#include "NodeUpgradeHintRowWidget.h"
#include "Patching/WidgetBlueprintHookManager.h"

namespace
{
	/** The game's look-at prompt ("Press E to start mining..."), shown by BP_GameUI (docs/FINDINGS.md, look-at text). */
	const TCHAR* const PromptWidgetClassPath = TEXT("/Game/FactoryGame/Interface/UI/InGame/Widget_PlayerInteraction.Widget_PlayerInteraction_C");
}

UNodeUpgradeGameInstanceModule::UNodeUpgradeGameInstanceModule()
{
	bRootModule = true;

	UWidgetBlueprintHookData* HintLineHook = CreateDefaultSubobject<UWidgetBlueprintHookData>(TEXT("HintLineHook"));
	HintLineHook->DeveloperComment = TEXT("NodeUpgrade: 'Press [Y] to upgrade this node' line under the game's look-at prompt.");
	HintLineHook->WidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(PromptWidgetClassPath));
	HintLineHook->NewWidgetClass = UNodeUpgradeHintRowWidget::StaticClass();
	HintLineHook->NewWidgetName = TEXT("NodeUpgradeHintLine");
	// The game's keyboard line (mInteractionTextContainer) is a variable; its parent, the prompt's vertical box, is not:
	// SML takes the parent panel of that variable.
	HintLineHook->ParentWidgetType = EWidgetBlueprintHookParentType::Indirect_Child;
	HintLineHook->ParentWidgetName = TEXT("mInteractionTextContainer");
	// Right after the game's keyboard line (slot 0), before its gamepad line.
	HintLineHook->ParentSlotIndex = 1;
	// No SlotConfiguration: the engine's default vertical box slot (automatic size, fill, no padding) is exactly the game line's,
	// and SML 3.12's UWidgetBlueprintHookSlot_Generic crashes on a vertical box slot (it calls SetSize on a null horizontal box slot).
	// The gap above the line is inside UNodeUpgradeHintRowWidget.
	WidgetBlueprintHooks.Add(HintLineHook);
}
