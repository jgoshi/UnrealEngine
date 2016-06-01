// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionAtmosphericFogColor.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionAtmosphericFogColor : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput WorldPosition;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};



