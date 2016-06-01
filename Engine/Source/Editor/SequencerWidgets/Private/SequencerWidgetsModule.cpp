// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerWidgetsPrivatePCH.h"
#include "ISequencerWidgetsModule.h"
#include "ModuleManager.h"
#include "SSequencerTimeSlider.h"
#include "STimeRange.h"
#include "STimeRangeSlider.h"


/**
 * The public interface of SequencerModule
 */
class FSequencerWidgetsModule
	: public ISequencerWidgetsModule
{
public:

	// ISequencerWidgetsModule interface

	TSharedRef<ITimeSlider> CreateTimeSlider(const TSharedRef<ITimeSliderController>& InController, bool bMirrorLabels) override
	{
		return SNew(SSequencerTimeSlider, InController)
			.MirrorLabels(bMirrorLabels);
	}

	TSharedRef<ITimeSlider> CreateTimeSlider(const TSharedRef<ITimeSliderController>& InController, const TAttribute<EVisibility>& VisibilityDelegate, bool bMirrorLabels) override
	{
		return SNew(SSequencerTimeSlider, InController)
			.Visibility(VisibilityDelegate)
			.MirrorLabels(bMirrorLabels);
	}

	TSharedRef<SWidget> CreateTimeRangeSlider( const TSharedRef<class ITimeSliderController>& InController, const TAttribute<float>& InTimeSnapIntervalDelegate ) override
	{
		return SNew( STimeRangeSlider, InController )
			.TimeSnapInterval( InTimeSnapIntervalDelegate );
	}

	TSharedRef<ITimeSlider> CreateTimeRange(const FTimeRangeArgs& InArgs, const TSharedRef<SWidget>& Content) override
	{
		return SNew( STimeRange, InArgs.Controller, InArgs.NumericTypeInterface)
		.Visibility(InArgs.VisibilityDelegate)
		.ShowFrameNumbers(InArgs.ShowFrameNumbersDelegate)
		.ShowWorkingRange(!!(InArgs.ShowRanges & EShowRange::WorkingRange))
		.ShowViewRange(!!(InArgs.ShowRanges & EShowRange::ViewRange))
		.ShowPlaybackRange(!!(InArgs.ShowRanges & EShowRange::PlaybackRange))
		[
			Content
		];
	}

public:

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FSequencerWidgetsModule, SequencerWidgets);
