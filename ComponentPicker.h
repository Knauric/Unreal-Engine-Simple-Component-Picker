// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ComponentPicker.generated.h"

class UActorComponent;

// UPROPERTY's that have this type will display a component picker in the editor, allowing users to select a component
// from an actor in the scene.
USTRUCT( )
struct FComponentPicker
{
    GENERATED_BODY( )

    // Default constructor
    FComponentPicker( ) = default;

    // Construct with default component selected
    FComponentPicker( UActorComponent* pComponent );

    // Get the component that was picked from the scene
    UActorComponent* GetComponent( ) const;

    // Comparison operator
    bool operator== ( const FComponentPicker& rOther ) const;

private:
    // The component that has been picked from the scene
    UPROPERTY( )
    TWeakObjectPtr<UActorComponent> m_pPickedComponent = nullptr;
};