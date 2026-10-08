#include "NodeUpgradeInteractionComponent.h"

#include "EnhancedInputComponent.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "InputAction.h"
#include "NodeUpgrade.h"
#include "NodeUpgradeMenuWidget.h"
#include "NodeUpgradeTargeting.h"
#include "NodeUpgradeText.h"
#include "PlayerMappableKeySettings.h"
#include "Resources/FGResourceNode.h"
#include "UObject/UnrealType.h"

namespace
{
	/**
	 * Asset created in the editor (see docs/SETUP.md). The key itself is set by MC_NodeUpgrade and can be rebound in the game's controls.
	 * "Inputs" is the folder scanned for FGChildInputMappingContext by the Game Feature Data created by Alpakit (DefaultAlpakit.ini).
	 */
	const TCHAR* const OpenMenuActionPath = TEXT("/NodeUpgrade/Inputs/IA_NodeUpgrade_OpenMenu.IA_NodeUpgrade_OpenMenu");
	const TCHAR* const MappingContextPath = TEXT("/NodeUpgrade/Inputs/MC_NodeUpgrade.MC_NodeUpgrade");

	bool SetTextProperty(UObject* Object, const TCHAR* PropertyName, const FText& Value)
	{
		FTextProperty* Property = Object != nullptr ? CastField<FTextProperty>(Object->GetClass()->FindPropertyByName(FName(PropertyName))) : nullptr;
		if (Property == nullptr)
		{
			return false;
		}
		Property->SetPropertyValue_InContainer(Object, Value);
		return true;
	}

	/**
	 * The texts shown in Options > Controls come from the input assets, which were created by script with untranslatable texts.
	 * They are replaced once, in memory, by the mod's own localized texts (same keys as localization/strings.json).
	 */
	void LocalizeInputAssets(UInputAction* Action)
	{
		static bool bDone = false;
		if (bDone || Action == nullptr)
		{
			return;
		}
		bDone = true;

		const bool bAction = SetTextProperty(Action, TEXT("ActionDescription"), NodeUpgradeText::InputOpenMenuDescription())
			& SetTextProperty(Action->GetPlayerMappableKeySettings().Get(), TEXT("DisplayName"), NodeUpgradeText::InputOpenMenu());
		const bool bContext = SetTextProperty(LoadObject<UObject>(nullptr, MappingContextPath), TEXT("mDisplayName"), NodeUpgradeText::InputCategory());
		if (!bAction || !bContext)
		{
			UE_LOG(LogNodeUpgrade, Warning, TEXT("Could not localize the controls menu texts (action %d, category %d)"), bAction, bContext);
		}
	}
}

UNodeUpgradeInteractionComponent::UNodeUpgradeInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(false);
}

void UNodeUpgradeInteractionComponent::BindToPlayer(AFGCharacterPlayer* Character, UInputComponent* InputComponent)
{
	if (!IsValid(Character) || InputComponent == nullptr)
	{
		return;
	}

	UNodeUpgradeInteractionComponent* Component = Character->FindComponentByClass<UNodeUpgradeInteractionComponent>();
	if (Component == nullptr)
	{
		Component = NewObject<UNodeUpgradeInteractionComponent>(Character);
		Character->AddInstanceComponent(Component);
		Component->RegisterComponent();
	}
	Component->BindInput(InputComponent);
}

void UNodeUpgradeInteractionComponent::BindInput(UInputComponent* InputComponent)
{
	if (mBoundInputComponent.Get() == InputComponent)
	{
		// Already bound on this component: binding twice would open and close the menu on the same press.
		return;
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInput == nullptr)
	{
		UE_LOG(LogNodeUpgrade, Warning, TEXT("Player input component is not an Enhanced Input component, the menu key cannot be bound"));
		return;
	}

	if (mOpenMenuAction == nullptr)
	{
		mOpenMenuAction = LoadObject<UInputAction>(nullptr, OpenMenuActionPath);
	}
	if (mOpenMenuAction == nullptr)
	{
		UE_LOG(LogNodeUpgrade, Error, TEXT("Input action '%s' not found: create it in the editor (docs/SETUP.md, NodeUpgrade assets)"), OpenMenuActionPath);
		return;
	}
	LocalizeInputAssets(mOpenMenuAction);

	// Started fires once per press whatever triggers the action asset uses.
	EnhancedInput->BindAction(mOpenMenuAction.Get(), ETriggerEvent::Started, this, &UNodeUpgradeInteractionComponent::HandleOpenMenuPressed);
	mBoundInputComponent = InputComponent;
	UE_LOG(LogNodeUpgrade, Display, TEXT("Menu key bound for %s"), *GetOwner()->GetName());
}

void UNodeUpgradeInteractionComponent::HandleOpenMenuPressed()
{
	if (mOpenMenu != nullptr && !mOpenMenu->IsClosing())
	{
		mOpenMenu->CloseMenu();
		return;
	}
	mOpenMenu = nullptr;
	OpenMenuForTarget();
}

void UNodeUpgradeInteractionComponent::OpenMenuForTarget()
{
	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(GetOwner());
	if (!IsValid(Character) || !Character->IsLocallyControlled())
	{
		return;
	}

	AFGResourceNode* Node = FNodeUpgradeTargeting::FindTargetNode(Character);
	if (Node == nullptr)
	{
		UE_LOG(LogNodeUpgrade, Verbose, TEXT("Menu key pressed but no supported resource node is targeted"));
		return;
	}

	AFGPlayerController* PlayerController = Character->GetFGPlayerController();
	UNodeUpgradeMenuWidget* Menu = UNodeUpgradeMenuWidget::Open(PlayerController, Character, Node, this);
	mOpenMenu = (Menu != nullptr && !Menu->IsClosing()) ? Menu : nullptr;
}

void UNodeUpgradeInteractionComponent::NotifyMenuClosed(UNodeUpgradeMenuWidget* Menu)
{
	if (mOpenMenu == Menu)
	{
		mOpenMenu = nullptr;
	}
}
