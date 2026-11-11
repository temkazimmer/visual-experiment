#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisexMenuLayout.h"
#include "VisexMenuLayoutEditor.generated.h"

class AVisexMenuDirector;
class SWidget;
class STextBlock;

/**
 * The start menu's layout editor: § on the start menu opens it, § or ESC closes it. A small
 * list in the screen's lower right edits the menu block live: place, perspective, size,
 * spacing and the logo. While it is open its keys go to it, not the menu.
 *
 *  UP DOWN        choose a setting
 *  LEFT RIGHT     change it (hold SHIFT for fine steps)
 *  ENTER          on LOGO: type a new logo (ENTER done, ESC cancel); on SAVE / RESET: do it
 *  R              this setting back to the saved value
 *  § / ESC        close (unsaved changes stay for this session only)
 */
UCLASS()
class VISEX_API AVisexMenuLayoutEditor : public AActor
{
	GENERATED_BODY()

public:
	AVisexMenuLayoutEditor();

	void Toggle(AVisexMenuDirector* InDirector);
	bool IsOpen() const { return bOpen; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void Open();
	void Close();
	void BindKeys();
	void Up() { Move(-1); }
	void Down() { Move(1); }
	void Move(int32 Delta);
	void Left() { Step(-1.f); }
	void Right() { Step(1.f); }
	void Step(float Dir);
	void Enter();
	void Escape();
	void ResetRow();
	void AnyKey(FKey Key);
	void Push();
	void ShowHud(bool bShow);
	void RefreshHud();
	FString RowValue(int32 Row) const;

	TWeakObjectPtr<AVisexMenuDirector> Director;
	FVisexMenuLayout Edit;
	FVisexMenuLayout Saved;
	int32 Row = 0;
	bool bOpen = false;
	bool bKeysBound = false;
	bool bTyping = false;
	FString Typed;
	FString Status;
	TSharedPtr<SWidget> Hud;
	TSharedPtr<STextBlock> Body;
};
