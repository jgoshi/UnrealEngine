// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraph.h"
#include "ControlRigGraph.generated.h"

class UControlRigBlueprint;
class UControlRigGraphSchema;

UCLASS()
class CONTROLRIGDEVELOPER_API UControlRigGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UControlRigGraph();

	/** Set up this graph */
	void Initialize(UControlRigBlueprint* InBlueprint);

	/** Get the skeleton graph schema */
	const UControlRigGraphSchema* GetControlRigGraphSchema();
};

