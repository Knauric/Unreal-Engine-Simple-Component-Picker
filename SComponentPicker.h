// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PropertyCustomizationHelpers.h"

class UActorComponent;

DECLARE_DELEGATE_OneParam( FOnComponentPicked, UActorComponent* );

namespace SceneOutliner
{
    struct ISceneOutlinerTreeItem;
}

// Essentially a duplicate of SPropertyMenuComponentPicker. A widget that allows picking components from the scene.
class SComponentPicker : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS( SComponentPicker )
        : _pInitialComponent( nullptr )
        , _bAllowClear( true )
        , _oActorFilter( )
    {
    }

    SLATE_ARGUMENT( UActorComponent*, pInitialComponent )
    SLATE_ARGUMENT( bool, bAllowClear )
    SLATE_ARGUMENT( FOnShouldFilterActor, oActorFilter )
    SLATE_ARGUMENT( FOnShouldFilterComponent, oComponentFilter )
    SLATE_EVENT( FOnComponentPicked, oOnSet )
    SLATE_EVENT( FSimpleDelegate, oOnClose )
    SLATE_END_ARGS( )

    // Construct the widget.
    void Construct( const FArguments& rInArgs );

private:
    // Edit the object referenced by this widget.
    void OnEdit( );

    
    // Delegate handling ctrl+c.
    void OnCopy( );

    // Delegate handling ctrl+v.
    void OnPaste( );

    // Returns true if the current clipboard contents can be pasted.
    bool CanPaste( );

    // Clear the referenced object.
    void OnClear( );

    // Delegate for handling selection by the actor picker.
    void OnItemSelected( UActorComponent* pComponent );

    // Set the value of the asset referenced by this property editor. Will set the underlying property handle if there
    // is one.
    void SetValue( UActorComponent* pInComponent );

private:
    UActorComponent* m_pInitialComponent;

    // Whether the asset can be 'None' in this case.
    bool m_bAllowClear;

    // Delegates used to test whether a item should be displayed or not.
    FOnShouldFilterActor m_oActorFilter;
    FOnShouldFilterComponent m_oComponentFilter;

    // Delegate to call when our object value should be set.
    FOnComponentPicked m_oOnSet;

    // Delegate to call when closing the containing menu.
    FSimpleDelegate m_oOnClose;
};
