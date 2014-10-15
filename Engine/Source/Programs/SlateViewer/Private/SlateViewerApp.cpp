// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SlateViewerApp.h"
#include "RequiredProgramMainCPPInclude.h"
#include "STestSuite.h"
#include "ISourceCodeAccessModule.h"

IMPLEMENT_APPLICATION(SlateViewer, "SlateViewer");


namespace WorkspaceMenu
{
	TSharedRef<FWorkspaceItem> DeveloperMenu = FWorkspaceItem::NewGroup(NSLOCTEXT("SlateViewer", "DeveloperMenu", "Developer"));
}


void RunSlateViewer( const TCHAR* CommandLine )
{
	// start up the main loop
	GEngineLoop.PreInit(CommandLine);

	// crank up a normal Slate application using the platform's standalone renderer
	FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());

	// Load the source code access module
	ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>( FName( "SourceCodeAccess" ) );
	
	// Manually load in the source code access plugins, as standalone programs don't currently support plugins.
#if PLATFORM_MAC
	IModuleInterface& XCodeSourceCodeAccessModule = FModuleManager::LoadModuleChecked<IModuleInterface>( FName( "XCodeSourceCodeAccess" ) );
	SourceCodeAccessModule.SetAccessor(FName("XCodeSourceCodeAccess"));
#elif PLATFORM_WINDOWS
	IModuleInterface& VisualStudioSourceCodeAccessModule = FModuleManager::LoadModuleChecked<IModuleInterface>( FName( "VisualStudioSourceCodeAccess" ) );
	SourceCodeAccessModule.SetAccessor(FName("VisualStudioSourceCodeAccess"));
#endif

	// set the application name
	FGlobalTabmanager::Get()->SetApplicationTitle(NSLOCTEXT("SlateViewer", "AppTitle", "Slate Viewer"));
	FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector").RegisterTabSpawner(WorkspaceMenu::DeveloperMenu);

	// Bring up the test suite.
	RestoreSlateTestSuite();

#if WITH_SHARED_POINTER_TESTS
	SharedPointerTesting::TestSharedPointer<ESPMode::Fast>();
	SharedPointerTesting::TestSharedPointer<ESPMode::ThreadSafe>();
#endif

	// loop while the server does the rest
	while (!GIsRequestingExit)
	{
		FTicker::GetCoreTicker().Tick(0);
		FSlateApplication::Get().PumpMessages();
		FSlateApplication::Get().Tick();
		FPlatformProcess::Sleep(0);
	}

	FSlateApplication::Shutdown();
}
