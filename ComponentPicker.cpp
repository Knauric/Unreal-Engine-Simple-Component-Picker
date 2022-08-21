// Fill out your copyright notice in the Description page of Project Settings.

#include "ComponentPicker.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FComponentPicker::FComponentPicker( UActorComponent* pComponent )
    : m_pPickedComponent( pComponent )
{
    // Empty
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UActorComponent* FComponentPicker::GetComponent( ) const
{
    return m_pPickedComponent.IsValid( ) ? m_pPickedComponent.Get( ) : nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FComponentPicker::operator==( const FComponentPicker& rOther ) const
{
    return m_pPickedComponent == rOther.m_pPickedComponent;
}
