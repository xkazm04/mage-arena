#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CreateBudgetFxCommandlet.generated.h"

/**
 * D-G4 particle proxy (docs/research/NIAGARA-PROXY-2026-10.md 1.2, 5.2). Builds the six NS_Budget_ Niagara systems
 * under /Game/Budget from the engine's emitter templates, with the values of the BudgetStress::FxRows() table.
 * Idempotent: a system that already loads is reported and left alone; -force rebuilds it.
 * -dump builds the systems in memory and logs every rapid-iteration parameter (name, type, value), then saves nothing.
 * Run through apps/vr/tools/create-budget-fx.ps1 (UnrealEditor-Cmd <uproject> -run=CreateBudgetFx). Editor builds only.
 */
UCLASS()
class MAGEARENAVR_API UCreateBudgetFxCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCreateBudgetFxCommandlet();
	virtual int32 Main(const FString& Params) override;
};
