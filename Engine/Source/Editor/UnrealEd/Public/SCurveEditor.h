// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#ifndef __SCurveEditor_h__
#define __SCurveEditor_h__

//////////////////////////////////////////////////////////////////////////
// FTrackScaleInfo

/** Utility struct for converting between curve space and local/absolute screen space.  The input domain is traditionally
 *  the time axis of the curve, and the output domain is traditionally the value axis.
 */
struct FTrackScaleInfo
{
	float ViewMinInput;
	float ViewMaxInput;
	float ViewInputRange;
	float PixelsPerInput;

	float ViewMinOutput;
	float ViewMaxOutput;
	float ViewOutputRange;
	float PixelsPerOutput;

	FVector2D WidgetSize;

	FTrackScaleInfo(float InViewMinInput, float InViewMaxInput, float InViewMinOutput, float InViewMaxOutput, const FVector2D InWidgetSize)
	{
		WidgetSize = InWidgetSize;

		ViewMinInput = InViewMinInput;
		ViewMaxInput = InViewMaxInput;
		ViewInputRange = ViewMaxInput - ViewMinInput;
		PixelsPerInput = ViewInputRange > 0 ? ( WidgetSize.X / ViewInputRange ) : 0;

		ViewMinOutput = InViewMinOutput;
		ViewMaxOutput = InViewMaxOutput;
		ViewOutputRange = InViewMaxOutput - InViewMinOutput;
		PixelsPerOutput = ViewOutputRange > 0 ? ( WidgetSize.Y / ViewOutputRange ) : 0;
	}

	/** Local Widget Space -> Curve Input domain. */
	float LocalXToInput(float ScreenX) const
	{
		return (ScreenX/PixelsPerInput) + ViewMinInput;
	}

	/** Curve Input domain -> local Widget Space */
	float InputToLocalX(float Input) const
	{
		return (Input - ViewMinInput) * PixelsPerInput;
	}

	/** Local Widget Space -> Curve Output domain. */
	float LocalYToOutput(float ScreenY) const
	{
		return (ViewOutputRange + ViewMinOutput) - (ScreenY/PixelsPerOutput);
	}

	/** Curve Output domain -> local Widget Space */
	float OutputToLocalY(float Output) const
	{
		return (ViewOutputRange - (Output - ViewMinOutput)) * PixelsPerOutput;
	}

	float GetTrackCenterY() const
	{
		return (0.5f * WidgetSize.Y);
	}


};

//////////////////////////////////////////////////////////////////////////
// SCurveEditor

DECLARE_DELEGATE_TwoParams( FOnSetInputViewRange, float, float )

class SCurveEditor : 
	public SCompoundWidget,
	public FGCObject,
	public FEditorUndoClient
{
private:

	/** Represents the different states of a drag operation */
	enum class EDragState
	{
		/** The user has clicked a mouse button, but hasn't moved more then the drag threshold. */
		PreDrag,
		/** The user is dragging the selected keys. */
		DragKey,
		/** The user is dragging a selected tangent handle. */
		DragTangent,
		/** The user is performing a marquee selection of keys. */
		MarqueeSelect,
		/** The user is panning the curve view. */
		Pan,
		/** There is no active drag operation. */
		None
	};

public:
	SLATE_BEGIN_ARGS( SCurveEditor )
		: _ViewMinInput(0.0f)
		, _ViewMaxInput(10.0f)
		, _ViewMinOutput(0.0f)
		, _ViewMaxOutput(1.0f)
		, _InputSnap(0.1f)
		, _OutputSnap(0.05f)
		, _SnappingEnabled(false)
		, _TimelineLength(5.0f)
		, _DesiredSize(FVector2D::ZeroVector)
		, _DrawCurve(true)
		, _HideUI(true)
		, _AllowZoomOutput(true)
		, _AlwaysDisplayColorCurves(false)
		, _ZoomToFitVertical(true)
		, _ZoomToFitHorizontal(true)
		, _ShowZoomButtons(true)
		{}

		SLATE_ATTRIBUTE( float, ViewMinInput )
		SLATE_ATTRIBUTE( float, ViewMaxInput )
		SLATE_ATTRIBUTE( TOptional<float>, DataMinInput )
		SLATE_ATTRIBUTE( TOptional<float>, DataMaxInput )
		SLATE_ARGUMENT( float, ViewMinOutput )
		SLATE_ARGUMENT( float, ViewMaxOutput )
		SLATE_ATTRIBUTE( float, InputSnap )
		SLATE_ATTRIBUTE( float, OutputSnap )
		SLATE_ARGUMENT( bool, SnappingEnabled )
		SLATE_ATTRIBUTE( float, TimelineLength )
		SLATE_ATTRIBUTE( FVector2D, DesiredSize )
		SLATE_ARGUMENT( bool, DrawCurve )
		SLATE_ARGUMENT( bool, HideUI )
		SLATE_ARGUMENT( bool, AllowZoomOutput )
		SLATE_ARGUMENT( bool, AlwaysDisplayColorCurves )
		SLATE_ARGUMENT( bool, ZoomToFitVertical )
		SLATE_ARGUMENT( bool, ZoomToFitHorizontal )
		SLATE_ARGUMENT( bool, ShowZoomButtons )
		SLATE_EVENT( FOnSetInputViewRange, OnSetInputViewRange )
		SLATE_EVENT( FSimpleDelegate, OnCreateAsset )
	SLATE_END_ARGS()

	UNREALED_API void Construct(const FArguments& InArgs);

	/** Destructor */
	UNREALED_API ~SCurveEditor();

	/** Set the curve that is being edited by this track widget. Also provide an option to enable/disable editing */
	UNREALED_API void SetCurveOwner(FCurveOwnerInterface* InCurveOwner, bool bCanEdit = true);

	/** Set new zoom to fit **/
	UNREALED_API void SetZoomToFit(bool bNewZoomToFitVertical, bool bNewZoomToFitHorizontal);

	/** Get the currently edited curve */
	FCurveOwnerInterface* GetCurveOwner() const;

	/** Construct an object of type UCurveFactory and return it's reference*/
	UNREALED_API UCurveFactory* GetCurveFactory( );

	/**
	* Function to create a curve object and return its reference.
	 * 
	 * @param 1: Type of curve to create
	 * @param 2: Reference to the package in which the asset has to be created
	 * @param 3: Name of asset
	 *
	 * @return: Reference to the newly created curve object
	 */
	UNREALED_API UObject* CreateCurveObject( TSubclassOf<UCurveBase> CurveType, UObject* PackagePtr, FName& AssetName );

	/**
	 * Since we create a UFactory object, it needs to be serialized
	 *
	 * @param Ar The archive to serialize with
	 */
	UNREALED_API virtual void AddReferencedObjects( FReferenceCollector& Collector );

	/** Gets a list of the commands handled by this control */
	UNREALED_API TSharedPtr<FUICommandList> GetCommands();

private:
	/** Used to track a key and the curve that owns it */
	struct FSelectedCurveKey
	{
		FSelectedCurveKey(FRichCurve* InCurve, FKeyHandle InKeyHandle)
			: Curve(InCurve), KeyHandle(InKeyHandle)
		{}

		/** If this is a valid Curve/Key */
		bool			IsValid() const
		{
			return Curve != NULL && Curve->IsKeyHandleValid(KeyHandle);
		}
		/** Does the curve match? */ 
		bool			IsSameCurve(const FSelectedCurveKey& Key) const 
		{ 
			return Curve == Key.Curve;
		}

		/** Does the curve and the key match ?*/
		bool			operator == (const FSelectedCurveKey& Other) const 
		{
			return (Curve == Other.Curve) && (KeyHandle == Other.KeyHandle);
		}

		FRichCurve*		Curve;
		FKeyHandle		KeyHandle;
	};

	/** Used to track a key and tangent*/
	struct FSelectedTangent
	{
		FSelectedTangent(): Key(NULL,FKeyHandle()),bIsArrival(false)
		{
		}

		/** If this is a valid Curve/Key */
		bool			IsValid() const;

		/** The Key for the tangent */
		FSelectedCurveKey Key;

		/** Indicates if it is the arrival tangent, or the leave tangent */
		bool			  bIsArrival;
	};

private:
	/** Adds a new key to the curve. */
	void AddNewKey(FGeometry InMyGeometry, FVector2D ScreenPosition);

	/** Get the color associated with a given curve */
	FLinearColor	GetCurveColor(FRichCurve* Curve) const;

	/** Get the name for the given curve */
	FText			GetCurveName(FRichCurve* Curve) const;

	/** Test if the curve is exists, and if it being displayed on this widget */
	bool		IsValidCurve(FRichCurve* Curve) const;

	/** Util to get a curve by index */
	FRichCurve* GetCurve(int32 CurveIndex) const;

	/** Called when new value for a key is entered */
	void NewValueEntered( const FText& NewText, ETextCommit::Type CommitInfo );

	void NewHorizontalGridScaleEntered(const FString& NewText, bool bCommitFromEnter );
	void NewVerticalGridScaleEntered(const FString& NewText, bool bCommitFromEnter );

	/** Called by delete command */
	void DeleteSelectedKeys();

	/** Test a screen space location to find which key was clicked on */
	FSelectedCurveKey HitTestKeys(const FGeometry& InMyGeometry, const FVector2D& HitScreenPosition);

	/** Test a screen space location to find if any cubic tangents were clicked on */
	FSelectedTangent HitTestCubicTangents(const FGeometry& InMyGeometry, const FVector2D& HitScreenPosition);

	/** Get screen space tangent positions for a given key */
	void GetTangentPoints( FTrackScaleInfo &ScaleInfo,const FSelectedCurveKey &Key, FVector2D& Arrive, FVector2D& Leave) const;

	/** Get the set of keys within a rectangle in local space */
	TArray<FSelectedCurveKey> GetKeysWithinMarquee(const FGeometry& InMyGeometry, FVector2D MarqueeTopLeft, FVector2D MarqueeBottomRight) const;

	/** Empy key selecttion set */
	void EmptySelection();
	/** Add a key to the selection set */
	void AddToSelection(FSelectedCurveKey Key);
	/** Remove a key from the selection set */
	void RemoveFromSelection(FSelectedCurveKey Key);
	/** See if a key is currently selected */
	bool IsKeySelected(FSelectedCurveKey Key) const;
	/** See if any keys are selected */
	bool AreKeysSelected() const;

	/** Get the value of the desired key as text */
	TOptional<float> GetKeyValue(FSelectedCurveKey Key) const;
	/** Get the time of the desired key */
	TOptional<float> GetKeyTime(FSelectedCurveKey Key) const;

	/** Move the selected keys */
	void MoveSelectedKeys(FVector2D NewLocation);

	/** Function to check whether the current track is editable */
	bool IsEditingEnabled() const;

	/* Get the curves to will be used during a fit operation */
	TArray<FRichCurve*> GetCurvesToFit()const;

	void ZoomToFitHorizontal();
	void ZoomToFitVertical();

	FReply ZoomToFitHorizontalClicked();
	FReply ZoomToFitVerticalClicked();

	void ToggleSnapping();
	bool IsSnappingEnabled();

	TOptional<float> OnGetTime() const;
	void OnTimeComitted(float NewValue, ETextCommit::Type CommitType);
	void OnTimeChanged(float NewValue);

	TOptional<float> OnGetValue() const;
	void OnValueComitted(float NewValue, ETextCommit::Type CommitType);
	void OnValueChanged(float NewValue);

	void OnBeginSliderMovement(FText TransactionName);
	void OnEndSliderMovement(float NewValue);

	EVisibility GetCurveAreaVisibility() const;
	EVisibility GetControlVisibility() const;
	EVisibility GetEditVisibility() const;
	EVisibility GetColorGradientVisibility() const;
	EVisibility GetZoomButtonVisibility() const;

	bool GetInputEditEnabled() const;

	/** Function to create context menu on mouse right click*/
	void CreateContextMenu(const FGeometry& InMyGeometry, const FVector2D& ScreenPosition);

	/** Callback function called when item is select in the context menu */
	void OnCreateExternalCurveClicked();

	/** Called when "Show Curves" is selected from the context menu */
	void OnShowCurveToggled() { bAreCurvesVisible = !bAreCurvesVisible; }

	/** Called when "Show Gradient" is selected from the context menu */
	void OnShowGradientToggled() { bIsGradientEditorVisible = !bIsGradientEditorVisible; }

	/** Function pointer to execute callback function when user select 'Create external curve'*/
	FSimpleDelegate OnCreateAsset;

	// SWidget interface
	UNREALED_API virtual FVector2D ComputeDesiredSize() const override;

	/** Paint a curve */
	void PaintCurve(	FRichCurve* Curve, const FGeometry &AllottedGeometry, FTrackScaleInfo &ScaleInfo, FSlateWindowElementList &OutDrawElements, 
					int32 LayerId, const FSlateRect& MyClippingRect, ESlateDrawEffect::Type DrawEffects, const FWidgetStyle &InWidgetStyle) const;

	/** Paint the keys that make up a curve */
	int32 PaintKeys( FRichCurve* Curve, FTrackScaleInfo &ScaleInfo, FSlateWindowElementList &OutDrawElements, int32 LayerId, const FGeometry &AllottedGeometry, const FSlateRect& MyClippingRect, ESlateDrawEffect::Type DrawEffects, const FWidgetStyle &InWidgetStyle ) const;

	/** Paint the tangent for a key with cubic curves */
	void PaintTangent( FTrackScaleInfo &ScaleInfo, FRichCurve* Curve, FKeyHandle KeyHandle, FVector2D KeyLocation, FSlateWindowElementList &OutDrawElements, int32 LayerId, 
					   const FGeometry &AllottedGeometry, const FSlateRect& MyClippingRect, ESlateDrawEffect::Type DrawEffects, int32 LayerToUse, const FWidgetStyle &InWidgetStyle ) const;

	/** Paint Grid lines, these make it easier to visualize relative distance */
	void PaintGridLines( const FGeometry &AllottedGeometry, FTrackScaleInfo &ScaleInfo, FSlateWindowElementList &OutDrawElements, int32 LayerId, const FSlateRect& MyClippingRect, ESlateDrawEffect::Type DrawEffects )const;

	/** Paints the marquee for selection */
	void PaintMarquee(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	// SWidget interface
	UNREALED_API virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	UNREALED_API virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	UNREALED_API virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	UNREALED_API virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	UNREALED_API virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent ) override;
	UNREALED_API virtual void   OnMouseCaptureLost() override;

	/** Attempts to start a drag operation when the mouse moves. */
	void TryStartDrag(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Processes an ongoing drag operation when the mouse moves. */
	void ProcessDrag(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Completes an ongoing drag operation on mouse up. */
	void EndDrag(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Handles a mouse click operation on mouse up. */
	void ProcessClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	/* Generates the line(s) for rendering between KeyIndex and the following key. */
	void CreateLinesForSegment( FRichCurve* Curve, const FRichCurveKey& Key1, const FRichCurveKey& Key2, TArray<FVector2D>& Points, FTrackScaleInfo &ScaleInfo) const;
	
	/** Detect if user is clicking on a curve */
	bool HitTestCurves(  const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent );

	/** Add curve to selection */
	void SelectCurve(FRichCurve* Curve, bool bMultiple);

	/* user is moving the tangent for a key */
	void OnMoveTangent(FVector2D MouseCurvePosition);

	//Curve Selection interface 

	/** Construct widget that allows user to select which curve to edit if there are multiple */
	TSharedRef<SWidget> CreateCurveSelectionWidget() const;

	/** Get the state of a given curve's checkbox */
	ESlateCheckBoxState::Type	GetCheckState(FRichCurve* Curve) const;

	/** Called when the user clicks on the checkbox for a curve */
	void			OnUserSelectedCurve( ESlateCheckBoxState::Type State, FRichCurve* Curve );

	/** Get the tooltip for a curve selection checkbox */
	FText			GetCurveCheckBoxToolTip(FRichCurve* Curve) const;

	/* Get text representation of selected keys interpolation modes */
	FText			GetInterpolationModeText() const;

	/** Create ontext Menu for waring menu*/
	void	PushWarningMenu(FVector2D Position, const FText& Message);

	/** Create context Menu for key interpolation settings*/
	void	PushInterpolationMenu(FVector2D Position);

	/** Called when the user selects the interpolation mode */
	void	OnSelectInterpolationMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode);

	/** Begin a transaction for dragging a key or tangent */
	void	BeginDragTransaction();

	/** End a transaction for dragging a key or tangent */
	void	EndDragTransaction();

	/** Calculate the distance between grid lines: determines next lowest power of 2, works with fractional numbers */
	static float CalcGridLineStepDistancePow2(double RawValue);

	/** Perform undo */
	void	UndoAction();
	/** Perform redo*/
	void	RedoAction();

	// Begin FEditorUndoClient Interface
	UNREALED_API virtual void PostUndo(bool bSuccess) override;
	UNREALED_API virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
	// End of FEditorUndoClient

	bool AreCurvesVisible() const { return bAlwaysDisplayColorCurves || bAreCurvesVisible; }
	bool IsGradientEditorVisible() const { return bIsGradientEditorVisible; }
	bool IsLinearColorCurve() const;

	FVector2D SnapLocation(FVector2D InLocation);

protected:

	/** Set Default output values when range is too small **/
	UNREALED_API virtual void SetDefaultOutput(const float MinZoomRange);
	/** Get Time Step for vertical line drawing **/
	UNREALED_API virtual float GetTimeStep(FTrackScaleInfo &ScaleInfo) const;
	
	// SWidget interface
	UNREALED_API virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, 
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const ;

	/** Update view range */
	UNREALED_API void SetInputMinMax(float NewMin, float NewMax);

private:

	/** Curve selection*/
	TWeakPtr<SBox>		CurveSelectionWidget;

	/** Text block used to display warnings related to curves */
	TSharedPtr< SErrorText > WarningMessageText;

	/** Interface for curve supplier */
	FCurveOwnerInterface*		CurveOwner;
	/** Curves that we are editing */
	TArray<FRichCurveEditInfo>	Curves;
	/** Color for each curve */
	TArray<FLinearColor>		CurveColors;

	/** If we should draw the curve */
	bool				bDrawCurve;
	/**If we should hide the UI when the mouse leaves the control */
	bool				bHideUI;
	/**If we should allow zoom for output */
	bool				bAllowZoomOutput;
	/** If we always show the color curves or allow the user to toggle this */
	bool				bAlwaysDisplayColorCurves;
	/** Which curves are currently selected for editing */
	TArray<FRichCurve*>	SelectedCurves;

	/** Array of selected keys*/
	TArray<FSelectedCurveKey> SelectedKeys;

	/** Currently selected tangent */
	FSelectedTangent	SelectedTangent;

	/** Minimum input of data range  */
	TAttribute< TOptional<float> >	DataMinInput;
	/** Maximum input of data range  */
	TAttribute< TOptional<float> >	DataMaxInput;

	/** Editor Size **/
	TAttribute<FVector2D>	DesiredSize;

	/** Handler for adjust timeline panning viewing */
	FOnSetInputViewRange	SetInputViewRangeHandler;

	/** Index for the current transaction if any */
	int32					TransactionIndex;

	// Command stuff

	TSharedPtr< FUICommandList > Commands;

	/**Flag to enable/disable track editing*/
	bool		bCanEditTrack;

	/** True if the curves are being displayed */
	bool bAreCurvesVisible;
	
	/** True if the gradient editor is being displayed */
	bool bIsGradientEditorVisible;

	/** Reference to curve factor instance*/
	UCurveFactory* CurveFactory;

	/** Gradient editor */
	TSharedPtr<class SColorGradientEditor> GradientViewer;

protected:
	/** Minimum input of view range  */
	TAttribute<float>	ViewMinInput;
	/** Maximum input of view range  */
	TAttribute<float>	ViewMaxInput;
	/** How long the overall timeline is */
	TAttribute<float>		TimelineLength;

	/** Max output view range */
	float				ViewMinOutput;
	/** Min output view range */
	float				ViewMaxOutput;

	/** The snapping value for the input domain. */
	TAttribute<float> InputSnap;

	/** The snapping value for the output domain. */
	TAttribute<float> OutputSnap;

	/** Whether or not snapping is enabled. */
	bool bSnappingEnabled;

	/** True if you want the curve editor to fit to zoom **/
	bool				bZoomToFitVertical;

	/** True if you want the curve editor to fit to zoom **/
	bool				bZoomToFitHorizontal;

	/** True if the sliders are being used to adjust point values **/
	bool bIsUsingSlider;

	/** True if the internal zoom buttons should be visible. */
	bool bShowZoomButtons;

	/** The location of mouse during the last OnMouseButtonDown callback in widget local coordinates. */
	FVector2D MouseDownLocation;

	/** The location of the mouse during the last OnMouseMove callback in widget local coordinates. */
	FVector2D MouseMoveLocation;

	/** The state of the current drag operation happening with the widget, if any. */
	EDragState DragState;

	/** The number of pixels which the mouse must move before a drag operation starts. */
	float DragThreshold;

	/** A handle to the key which was clicked to start a key drag operation. */
	FKeyHandle DraggedKeyHandle;

	/** A map of selected key handle to their starting locations at the beginning of a drag operation. */
	TMap<FKeyHandle, FVector2D> PreDragKeyLocations;
};

#endif // __SCurveEditor_h__
