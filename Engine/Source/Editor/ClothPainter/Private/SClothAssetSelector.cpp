// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SClothAssetSelector.h"
#include "SExpandableArea.h"
#include "SBoxPanel.h"
#include "EditorStyleSet.h"
#include "DetailLayoutBuilder.h"
#include "SButton.h"
#include "SImage.h"
#include "ClothingAsset.h"
#include "MultiBoxBuilder.h"
#include "SCheckBox.h"
#include "SInlineEditableTextBlock.h"
#include "Engine/SkeletalMesh.h"
#include "ApexClothingUtils.h"
#include "UObjectIterator.h"
#include "Components/SkeletalMeshComponent.h"
#include "ComponentReregisterContext.h"
#include "MessageDialog.h"
#include "ClothingSystemEditorInterfaceModule.h"
#include "ModuleManager.h"
#include "ClothingAssetFactoryInterface.h"

#define LOCTEXT_NAMESPACE "ClothAssetSelector"

FClothParameterMask_PhysMesh* FClothingMaskListItem::GetMask()
{
	if(UClothingAsset* Asset = ClothingAsset.Get())
	{
		if(Asset->IsValidLod(LodIndex))
		{
			FClothLODData& LodData = Asset->LodData[LodIndex];
			if(LodData.ParameterMasks.IsValidIndex(MaskIndex))
			{
				return &LodData.ParameterMasks[MaskIndex];
			}
		}
	}

	return nullptr;
}

class SAssetListRow : public SMultiColumnTableRow<TSharedPtr<FClothingAssetListItem>>
{
public:

	SLATE_BEGIN_ARGS(SAssetListRow)
	{}
		SLATE_EVENT(FSimpleDelegate, OnInvalidateList)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TSharedPtr<FClothingAssetListItem> InItem)
	{
		Item = InItem;
		OnInvalidateList = InArgs._OnInvalidateList;

		SMultiColumnTableRow<TSharedPtr<FClothingAssetListItem>>::Construct(FSuperRowType::FArguments(), InOwnerTable);
	}

	static FName Column_Name;

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
	{
		UClothingAsset* Asset = Item->ClothingAsset.Get();

		if(Asset && InColumnName == Column_Name)
		{
			return SNew(SBox)
			.Padding(2.0f)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
				.Text(FText::FromString(Asset->GetName()))
			];
		}

		return SNullWidget::NullWidget;
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if(Item.IsValid() && MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			FMenuBuilder Builder(true, nullptr);

			FUIAction DeleteAction(FExecuteAction::CreateSP(this, &SAssetListRow::DeleteAsset));
			FUIAction ReimportAction(FExecuteAction::CreateSP(this, &SAssetListRow::ReimportAsset), FCanExecuteAction::CreateSP(this, &SAssetListRow::CanReimportAsset));

			Builder.BeginSection(NAME_None, LOCTEXT("AssetActions_SectionName", "Actions"));
			{
				Builder.AddMenuEntry(LOCTEXT("AssetActions_Delete", "Delete"), LOCTEXT("AssetActions_Delete_Tooltip", "Delete this clothing asset, removing it from the mesh"), FSlateIcon(), DeleteAction);
				Builder.AddMenuEntry(LOCTEXT("AssetActions_Reimport", "Reimport"), LOCTEXT("AssetActions_Reimport_Tooltip", "Reimport this asset from the source file if it was originally imported from an APEX file."), FSlateIcon(), ReimportAction);
			}
			Builder.EndSection();

			FWidgetPath Path = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

			FSlateApplication::Get().PushMenu(AsShared(), Path, Builder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect::ContextMenu);

			return FReply::Handled();
		}

		return SMultiColumnTableRow<TSharedPtr<FClothingAssetListItem>>::OnMouseButtonUp(MyGeometry, MouseEvent);
	}

private:

	void DeleteAsset()
	{
		if(UClothingAsset* Asset = Item->ClothingAsset.Get())
		{
			if(USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Asset->GetOuter()))
			{
				int32 AssetIndex;
				if(SkelMesh->MeshClothingAssets.Find(Asset, AssetIndex))
				{
					TArray<UActorComponent*> ComponentsToReregister;
					for(TObjectIterator<USkeletalMeshComponent> It; It; ++It)
					{
						if((*It)->SkeletalMesh == SkelMesh)
						{
							ComponentsToReregister.Add(*It);
						}
					}

					FMultiComponentReregisterContext ReregisterContext(ComponentsToReregister);

					Asset->UnbindFromSkeletalMesh(SkelMesh);
					SkelMesh->MeshClothingAssets.RemoveAt(AssetIndex);

					// Need to fix up asset indices on sections.
					if(FSkeletalMeshResource* MeshResource = SkelMesh->GetImportedResource())
					{
						for(FStaticLODModel& LodModel : MeshResource->LODModels)
						{
							for(FSkelMeshSection& Section : LodModel.Sections)
							{
								if(Section.CorrespondClothAssetIndex > AssetIndex)
								{
									--Section.CorrespondClothAssetIndex;
								}
							}
						}
					}

					OnInvalidateList.ExecuteIfBound();
				}
			}
		}
	}

	void ReimportAsset()
	{
		if(UClothingAsset* Asset = Item->ClothingAsset.Get())
		{
			if(USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Asset->GetOuter()))
			{
				FString ReimportPath = Asset->ImportedFilePath;

				if(ReimportPath.IsEmpty())
				{
					const FText MessageText = LOCTEXT("Warning_NoReimportPath", "There is no reimport path available for this asset, it was likely created in the Editor. Would you like to select a file and overwrite this asset?");
					EAppReturnType::Type MessageReturn = FMessageDialog::Open(EAppMsgType::YesNo, MessageText);

					if(MessageReturn == EAppReturnType::Yes)
					{
						ReimportPath = ApexClothingUtils::PromptForClothingFile();
					}
				}

				if(ReimportPath.IsEmpty())
				{
					return;
				}

				// Retry if the file isn't there
				if(!FPaths::FileExists(ReimportPath))
				{
					const FText MessageText = LOCTEXT("Warning_NoFileFound", "Could not find an asset to reimport, select a new file on disk?");
					EAppReturnType::Type MessageReturn = FMessageDialog::Open(EAppMsgType::YesNo, MessageText);

					if(MessageReturn == EAppReturnType::Yes)
					{
						ReimportPath = ApexClothingUtils::PromptForClothingFile();
					}
				}

				FClothingSystemEditorInterfaceModule& ClothingEditorInterface = FModuleManager::Get().LoadModuleChecked<FClothingSystemEditorInterfaceModule>("ClothingSystemEditorInterface");
				UClothingAssetFactoryBase* Factory = ClothingEditorInterface.GetClothingAssetFactory();

				if(Factory && Factory->CanImport(ReimportPath))
				{
					Factory->Reimport(ReimportPath, SkelMesh, Asset);

					OnInvalidateList.ExecuteIfBound();
				}
			}
		}
	}

	bool CanReimportAsset() const
	{
		return Item.IsValid() && !Item->ClothingAsset->ImportedFilePath.IsEmpty();
	}

	TSharedPtr<FClothingAssetListItem> Item;
	FSimpleDelegate OnInvalidateList;

};

FName SAssetListRow::Column_Name(TEXT("Name"));

class SMaskListRow : public SMultiColumnTableRow<TSharedPtr<FClothingMaskListItem>>
{
public:

	SLATE_BEGIN_ARGS(SMaskListRow)
	{}

		SLATE_EVENT(FSimpleDelegate, OnInvalidateList)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TSharedPtr<FClothingMaskListItem> InItem)
	{
		OnInvalidateList = InArgs._OnInvalidateList;
		Item = InItem;

		SMultiColumnTableRow<TSharedPtr<FClothingMaskListItem>>::Construct(FSuperRowType::FArguments(), InOwnerTable);
	}

	static FName Column_Enabled;
	static FName Column_MaskName;
	static FName Column_CurrentTarget;

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
	{
		if(InColumnName == Column_Enabled)
		{
			return SNew(SCheckBox)
				.IsEnabled(this, &SMaskListRow::IsMaskCheckboxEnabled, Item)
				.IsChecked(this, &SMaskListRow::IsMaskEnabledChecked, Item)
				.OnCheckStateChanged(this, &SMaskListRow::OnMaskEnabledCheckboxChanged, Item)
				.Padding(2.0f)
				.ToolTipText(LOCTEXT("MaskEnableCheckBox_ToolTip", "Sets whether this mask is enabled and can affect final parameters for its target parameter."));
		}

		if(InColumnName == Column_MaskName)
		{
			return SAssignNew(InlineText, SInlineEditableTextBlock)
				.Text(this, &SMaskListRow::GetMaskName)
				.OnTextCommitted(this, &SMaskListRow::OnCommitMaskName)
				.IsSelected(this, &SMultiColumnTableRow<TSharedPtr<FClothingMaskListItem>>::IsSelectedExclusively);
		}

		if(InColumnName == Column_CurrentTarget)
		{
			FClothParameterMask_PhysMesh* Mask = Item->GetMask();
			UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT(PREPROCESSOR_TO_STRING(MaskTarget_PhysMesh)), true);
			if(Enum && Mask)
			{
				return SNew(STextBlock).Text(Enum->GetDisplayNameTextByIndex((int32)Mask->CurrentTarget));
			}
		}

		return SNullWidget::NullWidget;
	}

	FText GetMaskName() const
	{
		if(Item.IsValid())
		{
			if(FClothParameterMask_PhysMesh* Mask = Item->GetMask())
			{
				return FText::FromName(Mask->MaskName);
			}
		}

		return LOCTEXT("MaskName_Invalid", "Invalid Mask");
	}

	void OnCommitMaskName(const FText& InText, ETextCommit::Type CommitInfo)
	{
		if(Item.IsValid())
		{
			if(FClothParameterMask_PhysMesh* Mask = Item->GetMask())
			{
				FText TrimText = FText::TrimPrecedingAndTrailing(InText);
				Mask->MaskName = FName(*TrimText.ToString());
			}
		}
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		// Spawn menu
		if(MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && Item.IsValid())
		{
			if(FClothParameterMask_PhysMesh* Mask = Item->GetMask())
			{
				FMenuBuilder Builder(true, nullptr);

				FUIAction MoveUpAction(FExecuteAction::CreateSP(this, &SMaskListRow::OnMoveUp), FCanExecuteAction::CreateSP(this, &SMaskListRow::CanMoveUp));
				FUIAction MoveDownAction(FExecuteAction::CreateSP(this, &SMaskListRow::OnMoveDown), FCanExecuteAction::CreateSP(this, &SMaskListRow::CanMoveDown));
				FUIAction DeleteAction(FExecuteAction::CreateSP(this, &SMaskListRow::OnDeleteMask));
				FUIAction ApplyAction(FExecuteAction::CreateSP(this, &SMaskListRow::OnApply));

				Builder.BeginSection(NAME_None, LOCTEXT("MaskActions_SectionName", "Actions"));
				{
					Builder.AddSubMenu(LOCTEXT("MaskActions_SetTarget", "Set Target"), LOCTEXT("MaskActions_SetTarget_Tooltip", "Choose the target for this mask"), FNewMenuDelegate::CreateSP(this, &SMaskListRow::BuildTargetSubmenu));
					Builder.AddMenuEntry(LOCTEXT("MaskActions_MoveUp", "Move Up"), LOCTEXT("MaskActions_MoveUp_Tooltip", "Move the mask up in the list"), FSlateIcon(), MoveUpAction);
					Builder.AddMenuEntry(LOCTEXT("MaskActions_MoveDown", "Move Down"), LOCTEXT("MaskActions_MoveDownp_Tooltip", "Move the mask down in the list"), FSlateIcon(), MoveDownAction);
					Builder.AddMenuEntry(LOCTEXT("MaskActions_Delete", "Delete"), LOCTEXT("MaskActions_Delete_Tooltip", "Delete the mask"), FSlateIcon(), DeleteAction);
					Builder.AddMenuEntry(LOCTEXT("MaskActions_Apply", "Apply"), LOCTEXT("MaskActions_Apply_Tooltip", "Apply the mask to the physical mesh"), FSlateIcon(), ApplyAction);
				}
				Builder.EndSection();

				FWidgetPath Path = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

				FSlateApplication::Get().PushMenu(AsShared(), Path, Builder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect::ContextMenu);

				return FReply::Handled();
			}
		}

		return SMultiColumnTableRow<TSharedPtr<FClothingMaskListItem>>::OnMouseButtonUp(MyGeometry, MouseEvent);
	}

	void EditName()
	{
		if(InlineText.IsValid())
		{
			InlineText->EnterEditingMode();
		}
	}

private:

	FClothLODData* GetCurrentLod() const
	{
		if(Item.IsValid())
		{
			if(UClothingAsset* Asset = Item->ClothingAsset.Get())
			{
				if(Asset->LodData.IsValidIndex(Item->LodIndex))
				{
					FClothLODData& LodData = Asset->LodData[Item->LodIndex];

					return &LodData;
				}
			}
		}

		return nullptr;
	}

	void OnDeleteMask()
	{
		if(FClothLODData* LodData = GetCurrentLod())
		{
			if(LodData->ParameterMasks.IsValidIndex(Item->MaskIndex))
			{
				LodData->ParameterMasks.RemoveAt(Item->MaskIndex);
				OnInvalidateList.ExecuteIfBound();
			}
		}
	}

	void OnMoveUp()
	{
		if(Item->MaskIndex > 0)
		{
			if(FClothLODData* LodData = GetCurrentLod())
			{
				if(LodData->ParameterMasks.IsValidIndex(Item->MaskIndex))
				{
					LodData->ParameterMasks.Swap(Item->MaskIndex, Item->MaskIndex - 1);
					OnInvalidateList.ExecuteIfBound();
				}
			}
		}
	}

	bool CanMoveUp() const
	{
		return(Item.IsValid() && Item->MaskIndex > 0);
	}

	void OnMoveDown()
	{
		if(FClothLODData* LodData = GetCurrentLod())
		{
			if(LodData->ParameterMasks.IsValidIndex(Item->MaskIndex + 1))
			{
				LodData->ParameterMasks.Swap(Item->MaskIndex, Item->MaskIndex + 1);
				OnInvalidateList.ExecuteIfBound();
			}
		}
	}

	bool CanMoveDown() const
	{
		if(Item.IsValid())
		{
			if(FClothLODData* LodData = GetCurrentLod())
			{
				return LodData->ParameterMasks.IsValidIndex(Item->MaskIndex + 1);
			}
		}

		return false;
	}

	void OnApply()
	{
		if(FClothParameterMask_PhysMesh* Mask = Item->GetMask())
		{
			if(FClothLODData* LodData = GetCurrentLod())
			{
				Mask->Apply(LodData->PhysicalMeshData);
			}
		}
	}

	void OnSetTarget(int32 InTargetEntryIndex)
	{
		if(Item.IsValid())
		{
			if(FClothParameterMask_PhysMesh* Mask = Item->GetMask())
			{
				Mask->CurrentTarget = (MaskTarget_PhysMesh)InTargetEntryIndex;

				if(Mask->CurrentTarget == MaskTarget_PhysMesh::None)
				{
					// Make sure to disable this mask if it has no valid target
					Mask->bEnabled = false;
				}

				OnInvalidateList.ExecuteIfBound();
			}
		}
	}

	void BuildTargetSubmenu(FMenuBuilder& Builder)
	{
		Builder.BeginSection(NAME_None, LOCTEXT("MaskTargets_SectionName", "Targets"));
		{
			UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT(PREPROCESSOR_TO_STRING(MaskTarget_PhysMesh)), true);
			if(Enum)
			{
				const int32 NumEntries = Enum->NumEnums();

				// Iterate to -1 to skip the _MAX entry appended to the end of the enum
				for(int32 Index = 0; Index < NumEntries - 1; ++Index)
				{
					FUIAction EntryAction(FExecuteAction::CreateSP(this, &SMaskListRow::OnSetTarget, Index));

					FText EntryText = Enum->GetDisplayNameTextByIndex(Index);

					Builder.AddMenuEntry(EntryText, FText::GetEmpty(), FSlateIcon(), EntryAction);
				}
			}
		}
		Builder.EndSection();
	}

	// Mask enabled checkbox handling
	ECheckBoxState IsMaskEnabledChecked(TSharedPtr<FClothingMaskListItem> InItem) const
	{
		if(InItem.IsValid())
		{
			if(FClothParameterMask_PhysMesh* Mask = InItem->GetMask())
			{
				return Mask->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}
		}

		return ECheckBoxState::Unchecked;
	}

	bool IsMaskCheckboxEnabled(TSharedPtr<FClothingMaskListItem> InItem) const
	{
		if(InItem.IsValid())
		{
			if(FClothParameterMask_PhysMesh* Mask = InItem->GetMask())
			{
				return Mask->CurrentTarget != MaskTarget_PhysMesh::None;
			}
		}

		return false;
	}

	void OnMaskEnabledCheckboxChanged(ECheckBoxState InState, TSharedPtr<FClothingMaskListItem> InItem)
	{
		if(InItem.IsValid())
		{
			if(FClothParameterMask_PhysMesh* Mask = InItem->GetMask())
			{
				bool bNewEnableState = InState == ECheckBoxState::Checked;

				if(Mask->bEnabled != bNewEnableState)
				{
					if(bNewEnableState)
					{
						// Disable all other masks that affect this target
						if(UClothingAsset* Asset = InItem->ClothingAsset.Get())
						{
							if(Asset->LodData.IsValidIndex(InItem->LodIndex))
							{
								FClothLODData& LodData = Asset->LodData[InItem->LodIndex];

								TArray<FClothParameterMask_PhysMesh*> AllTargetMasks;
								LodData.GetParameterMasksForTarget(Mask->CurrentTarget, AllTargetMasks);

								for(FClothParameterMask_PhysMesh* TargetMask : AllTargetMasks)
								{
									if(TargetMask && TargetMask != Mask)
									{
										TargetMask->bEnabled = false;
									}
								}
							}
						}
					}

					// Set the flag
					Mask->bEnabled = bNewEnableState;
				}
			}
		}
	}

	FSimpleDelegate OnInvalidateList;
	TSharedPtr<FClothingMaskListItem> Item;
	TSharedPtr<SInlineEditableTextBlock> InlineText;
};

FName SMaskListRow::Column_Enabled(TEXT("Enabled"));
FName SMaskListRow::Column_MaskName(TEXT("MaskName"));
FName SMaskListRow::Column_CurrentTarget(TEXT("CurrentTarget"));

void SClothAssetSelector::Construct(const FArguments& InArgs, USkeletalMesh* InMesh)
{
	Mesh = InMesh;
	OnSelectionChanged = InArgs._OnSelectionChanged;

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 2.0f)
		.AutoHeight()
		[
			SNew(SExpandableArea)
			.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f))
			.BodyBorderImage(FEditorStyle::GetBrush("DetailsView.CategoryMiddle"))
			.BodyBorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f))
			.HeaderContent()
			[
				SAssignNew(AssetHeaderBox, SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AssetExpander_Title", "Assets"))
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SAssignNew(ImportApexButton, SButton)
					.ButtonStyle(FEditorStyle::Get(), "RoundButton")
					.ForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
					.ContentPadding(FMargin(2, 0))
					.OnClicked(this, &SClothAssetSelector::OnImportApexFileClicked)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(0, 1))
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("Plus"))
						]

						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						.Padding(FMargin(2, 0, 0, 0))
						[
							SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFontBold())
							.Text(LOCTEXT("NewAssetButtonText", "Import APEX file"))
							.Visibility(this, &SClothAssetSelector::GetAssetHeaderButtonTextVisibility)
							.ShadowOffset(FVector2D(1, 1))
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SComboButton)
					.ButtonStyle(FEditorStyle::Get(), "RoundButton")
					.ForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
					.ContentPadding(FMargin(2, 0))
					.OnGetMenuContent(this, &SClothAssetSelector::OnGetLodMenu)
					.HasDownArrow(true)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFontBold())
						.Text(this, &SClothAssetSelector::GetLodButtonText)
						.ShadowOffset(FVector2D(1, 1))
					]
				]
			]
			.BodyContent()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(3.0f)
				.AutoHeight()
				[
					SNew(SBox)
					.MinDesiredHeight(100.0f)
					.MaxDesiredHeight(100.0f)
					[
						SAssignNew(AssetList, SAssetList)
						.ItemHeight(24)
						.ListItemsSource(&AssetListItems)
						.OnGenerateRow(this, &SClothAssetSelector::OnGenerateWidgetForClothingAssetItem)
						.OnSelectionChanged(this, &SClothAssetSelector::OnAssetListSelectionChanged)
						.ClearSelectionOnClick(false)
						.SelectionMode(ESelectionMode::Single)
						.HeaderRow
						(
							SNew(SHeaderRow)
							+ SHeaderRow::Column(TEXT("Name"))
							.DefaultLabel(LOCTEXT("AssetListHeader_Name", "Name"))
						)
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 2.0f)
		.AutoHeight()
		[
			SNew(SExpandableArea)
			.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f))
			.BodyBorderImage(FEditorStyle::GetBrush("DetailsView.CategoryMiddle"))
			.BodyBorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f))
			.HeaderContent()
			[
				SAssignNew(MaskHeaderBox, SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MaskExpander_Title", "Masks"))
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SAssignNew(NewMaskButton, SButton)
					.ButtonStyle(FEditorStyle::Get(), "RoundButton")
					.ForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
					.ContentPadding(FMargin(2, 0))
					.OnClicked(this, &SClothAssetSelector::AddNewMask)
					.IsEnabled(this, &SClothAssetSelector::CanAddNewMask)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(0, 1))
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("Plus"))
						]

						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						.Padding(FMargin(2, 0, 0, 0))
						[
							SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFontBold())
							.Text(LOCTEXT("NewMaskButtonText", "Mask"))
							.Visibility(this, &SClothAssetSelector::GetMaskHeaderButtonTextVisibility)
							.ShadowOffset(FVector2D(1, 1))
						]
					]
				]
			]
			.BodyContent()
			[
				SNew(SBox)
				.MinDesiredHeight(100.0f)
				.MaxDesiredHeight(200.0f)
				.Padding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
				[
					SAssignNew(MaskList, SMaskList)
					.ItemHeight(24)
					.ListItemsSource(&MaskListItems)
					.OnGenerateRow(this, &SClothAssetSelector::OnGenerateWidgetForMaskItem)
					.OnSelectionChanged(this, &SClothAssetSelector::OnMaskSelectionChanged)
					.ClearSelectionOnClick(false)
					.SelectionMode(ESelectionMode::Single)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(SMaskListRow::Column_Enabled)
						.FixedWidth(24)
						.DefaultLabel(LOCTEXT("MaskListHeader_Enabled", "Enabled"))
						.HeaderContent()
						[
							SNew(SBox)
						]
						+ SHeaderRow::Column(SMaskListRow::Column_MaskName)
						.FillWidth(0.5f)
						.DefaultLabel(LOCTEXT("MaskListHeader_Name", "Name"))
						+ SHeaderRow::Column(SMaskListRow::Column_CurrentTarget)
						.FillWidth(0.3f)
						.DefaultLabel(LOCTEXT("MaskListHeader_Target", "Target"))
					)
				]
			]
		]
	];

	RefreshAssetList();
	RefreshMaskList();
}

TWeakObjectPtr<UClothingAsset> SClothAssetSelector::GetSelectedAsset() const
{
	return SelectedAsset;
	
}

int32 SClothAssetSelector::SClothAssetSelector::GetSelectedLod() const
{
	return SelectedLod;
}

int32 SClothAssetSelector::GetSelectedMask() const
{
	return SelectedMask;
}

FReply SClothAssetSelector::OnImportApexFileClicked()
{
	if(Mesh)
	{
		ApexClothingUtils::PromptAndImportClothing(Mesh);
		OnRefresh();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

EVisibility SClothAssetSelector::GetAssetHeaderButtonTextVisibility() const
{
	bool bShow = AssetHeaderBox.IsValid() && AssetHeaderBox->IsHovered();

	return bShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

EVisibility SClothAssetSelector::GetMaskHeaderButtonTextVisibility() const
{
	bool bShow = MaskHeaderBox.IsValid() && MaskHeaderBox->IsHovered();

	return bShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SClothAssetSelector::OnGetLodMenu()
{
	FMenuBuilder Builder(true, nullptr);

	int32 NumLods = 0;

	if(UClothingAsset* CurrAsset = SelectedAsset.Get())
	{
		NumLods = CurrAsset->LodData.Num();
	}

	if(NumLods == 0)
	{
		Builder.AddMenuEntry(LOCTEXT("LodMenu_NoLods", "Select an asset..."), FText::GetEmpty(), FSlateIcon(), FUIAction());
	}
	else
	{
		for(int32 LodIdx = 0; LodIdx < NumLods; ++LodIdx)
		{
			FText ItemText = FText::Format(LOCTEXT("LodMenuItem", "LOD{0}"), FText::AsNumber(SelectedLod));
			FText ToolTipText = FText::Format(LOCTEXT("LodMenuItemToolTip", "Select LOD{0}"), FText::AsNumber(SelectedLod));

			FUIAction Action;
			Action.ExecuteAction.BindSP(this, &SClothAssetSelector::OnClothingLodSelected, LodIdx);

			Builder.AddMenuEntry(ItemText, ToolTipText, FSlateIcon(), Action);
		}
	}

	return Builder.MakeWidget();
}

FText SClothAssetSelector::GetLodButtonText() const
{
	if(SelectedLod == INDEX_NONE)
	{
		return LOCTEXT("LodButtonGenTextEmpty", "LOD");
	}

	return FText::Format(LOCTEXT("LodButtonGenText", "LOD{0}"), FText::AsNumber(SelectedLod));
}

TSharedRef<ITableRow> SClothAssetSelector::OnGenerateWidgetForClothingAssetItem(TSharedPtr<FClothingAssetListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if(UClothingAsset* Asset = InItem->ClothingAsset.Get())
	{
		return SNew(SAssetListRow, OwnerTable, InItem)
			.OnInvalidateList(this, &SClothAssetSelector::OnRefresh);
	}

	return SNew(STableRow<TSharedPtr<FClothingAssetListItem>>, OwnerTable)
		.Content()
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("No Assets Available")))
		];
}

void SClothAssetSelector::OnAssetListSelectionChanged(TSharedPtr<FClothingAssetListItem> InSelectedItem, ESelectInfo::Type InSelectInfo)
{
	if(InSelectedItem.IsValid() && InSelectInfo != ESelectInfo::Direct)
	{
		SetSelectedAsset(InSelectedItem->ClothingAsset);
	}
}

TSharedRef<ITableRow> SClothAssetSelector::OnGenerateWidgetForMaskItem(TSharedPtr<FClothingMaskListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if(FClothParameterMask_PhysMesh* Mask = InItem->GetMask())
	{
		return SNew(SMaskListRow, OwnerTable, InItem)
			.OnInvalidateList(this, &SClothAssetSelector::OnRefresh);
	}

	return SNew(STableRow<TSharedPtr<FClothingMaskListItem>>, OwnerTable)
	[
		SNew(STextBlock).Text(LOCTEXT("MaskList_NoMasks", "No masks available"))
	];
}

void SClothAssetSelector::OnMaskSelectionChanged(TSharedPtr<FClothingMaskListItem> InSelectedItem, ESelectInfo::Type InSelectInfo)
{
	if(InSelectedItem.IsValid() 
		&& InSelectedItem->ClothingAsset.IsValid() 
		&& InSelectedItem->LodIndex != INDEX_NONE 
		&& InSelectedItem->MaskIndex != INDEX_NONE
		&& InSelectedItem->MaskIndex != SelectedMask
		&& InSelectInfo != ESelectInfo::Direct)
	{
		SetSelectedMask(InSelectedItem->MaskIndex);
	}
}

FReply SClothAssetSelector::AddNewMask()
{
	if(UClothingAsset* Asset = SelectedAsset.Get())
	{
		if(Asset->LodData.IsValidIndex(SelectedLod))
		{
			FClothLODData& LodData = Asset->LodData[SelectedLod];
			const int32 NumRequiredValues = LodData.PhysicalMeshData.Vertices.Num();

			LodData.ParameterMasks.AddDefaulted();

			FClothParameterMask_PhysMesh& NewMask = LodData.ParameterMasks.Last();

			NewMask.MaskName = TEXT("New Mask");
			NewMask.CurrentTarget = MaskTarget_PhysMesh::None;
			NewMask.MaxValue = 0.0f;
			NewMask.Values.AddZeroed(NumRequiredValues);

			OnRefresh();
		}
	}

	return FReply::Handled();
}

bool SClothAssetSelector::CanAddNewMask() const
{
	return SelectedAsset.Get() != nullptr;
}

void SClothAssetSelector::OnRefresh()
{
	RefreshAssetList();
	RefreshMaskList();
}

void SClothAssetSelector::RefreshAssetList()
{
	UClothingAsset* CurrSelectedAsset = nullptr;
	int32 SelectedItem = INDEX_NONE;

	if(AssetList.IsValid())
	{
		TArray<TSharedPtr<FClothingAssetListItem>> SelectedItems;
		AssetList->GetSelectedItems(SelectedItems);

		if(SelectedItems.Num() > 0)
		{
			CurrSelectedAsset = SelectedItems[0]->ClothingAsset.Get();
		}
	}

	AssetListItems.Empty();

	for(UClothingAssetBase* Asset : Mesh->MeshClothingAssets)
	{
		UClothingAsset* ConcreteAsset = Cast<UClothingAsset>(Asset);

		TSharedPtr<FClothingAssetListItem> Entry = MakeShareable(new FClothingAssetListItem);

		Entry->ClothingAsset = ConcreteAsset;

		AssetListItems.Add(Entry);

		if(ConcreteAsset == CurrSelectedAsset)
		{
			SelectedItem = AssetListItems.Num() - 1;
		}
	}

	if(AssetListItems.Num() == 0)
	{
		// Add an invalid entry so we can show a "none" line
		AssetListItems.Add(MakeShareable(new FClothingAssetListItem));
	}

	if(AssetList.IsValid())
	{
		AssetList->RequestListRefresh();

		if(SelectedItem != INDEX_NONE)
		{
			AssetList->SetSelection(AssetListItems[SelectedItem]);
		}
	}
}

void SClothAssetSelector::RefreshMaskList()
{
	int32 CurrSelectedLod = INDEX_NONE;
	int32 CurrSelectedMask = INDEX_NONE;
	int32 SelectedItem = INDEX_NONE;

	if(MaskList.IsValid())
	{
		TArray<TSharedPtr<FClothingMaskListItem>> SelectedItems;

		MaskList->GetSelectedItems(SelectedItems);

		if(SelectedItems.Num() > 0)
		{
			CurrSelectedLod = SelectedItems[0]->LodIndex;
			CurrSelectedMask = SelectedItems[0]->MaskIndex;
		}
	}

	MaskListItems.Empty();

	UClothingAsset* Asset = SelectedAsset.Get();
	if(Asset && Asset->IsValidLod(SelectedLod))
	{
		FClothLODData& LodData = Asset->LodData[SelectedLod];
		const int32 NumMasks = LodData.ParameterMasks.Num();

		for(int32 Index = 0; Index < NumMasks; ++Index)
		{
			TSharedPtr<FClothingMaskListItem> NewItem = MakeShareable(new FClothingMaskListItem);
			NewItem->ClothingAsset = SelectedAsset;
			NewItem->LodIndex = SelectedLod;
			NewItem->MaskIndex = Index;
			MaskListItems.Add(NewItem);

			if(NewItem->LodIndex == CurrSelectedLod &&
				NewItem->MaskIndex == CurrSelectedMask)
			{
				SelectedItem = MaskListItems.Num() - 1;
			}
		}
	}

	if(MaskListItems.Num() == 0)
	{
		// Add invalid entry so we can make a widget for "none"
		TSharedPtr<FClothingMaskListItem> NewItem = MakeShareable(new FClothingMaskListItem);
		MaskListItems.Add(NewItem);
	}

	if(MaskList.IsValid())
	{
		MaskList->RequestListRefresh();

		if(SelectedItem != INDEX_NONE)
		{
			MaskList->SetSelection(MaskListItems[SelectedItem]);
		}
	}
}

void SClothAssetSelector::OnClothingLodSelected(int32 InNewLod)
{
	if(InNewLod == INDEX_NONE)
	{
		SetSelectedLod(InNewLod);
		//ClothPainterSettings->OnAssetSelectionChanged.Broadcast(SelectedAsset.Get(), SelectedLod, SelectedMask);
	}

	if(SelectedAsset.IsValid())
	{
		SetSelectedLod(InNewLod);

		int32 NewMaskSelection = INDEX_NONE;
		if(SelectedAsset->LodData.IsValidIndex(SelectedLod))
		{
			FClothLODData& LodData = SelectedAsset->LodData[SelectedLod];

			if(LodData.ParameterMasks.Num() > 0)
			{
				NewMaskSelection = 0;
			}
		}

		SetSelectedMask(NewMaskSelection);
	}
}

void SClothAssetSelector::SetSelectedAsset(TWeakObjectPtr<UClothingAsset> InSelectedAsset)
{
	SelectedAsset = InSelectedAsset;

	RefreshMaskList();

	if(UClothingAsset* NewAsset = SelectedAsset.Get())
	{
		if(NewAsset->LodData.Num() > 0)
		{
			SetSelectedLod(0);

			FClothLODData& LodData = NewAsset->LodData[SelectedLod];
			if(LodData.ParameterMasks.Num() > 0)
			{
				SetSelectedMask(0);
			}
			else
			{
				SetSelectedMask(INDEX_NONE);
			}
		}
		else
		{
			SetSelectedLod(INDEX_NONE);
			SetSelectedMask(INDEX_NONE);
		}

		OnSelectionChanged.ExecuteIfBound(SelectedAsset, SelectedLod, SelectedMask);
	}
}

void SClothAssetSelector::SetSelectedLod(int32 InLodIndex, bool bRefreshMasks /*= true*/)
{
	if(InLodIndex != SelectedLod)
	{
		SelectedLod = InLodIndex;

		if(MaskList.IsValid() && bRefreshMasks)
		{
			// New LOD means new set of masks, refresh that list
			RefreshMaskList();
		}

		OnSelectionChanged.ExecuteIfBound(SelectedAsset, SelectedLod, SelectedMask);
	}
}

void SClothAssetSelector::SetSelectedMask(int32 InMaskIndex)
{
	SelectedMask = InMaskIndex;

	if(MaskList.IsValid())
	{
		TSharedPtr<FClothingMaskListItem>* FoundItem = nullptr;
		if(InMaskIndex != INDEX_NONE)
		{
			// Find the item so we can select it in the list
			FoundItem = MaskListItems.FindByPredicate([&](const TSharedPtr<FClothingMaskListItem>& InItem)
			{
				return InItem->MaskIndex == InMaskIndex;
			});
		}

		if(FoundItem)
		{
			MaskList->SetSelection(*FoundItem);
		}
		else
		{
			MaskList->ClearSelection();
		}
	}

	OnSelectionChanged.ExecuteIfBound(SelectedAsset, SelectedLod, SelectedMask);
}

#undef LOCTEXT_NAMESPACE