// Fill out your copyright notice in the Description page of Project Settings.

#include "ComponentPickerCustomization.h"
#include "ComponentPicker.h"
#include "SComponentPicker.h"

#include "DetailLayoutBuilder.h"
#include "Engine/LevelScriptActor.h"
#include "IDetailChildrenBuilder.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

static const FName NAME_AllowAnyActor = "AllowAnyActor";
static const FName NAME_AllowedClasses = "AllowedClasses";
static const FName NAME_DisallowedClasses = "DisallowedClasses";

#define LOCTEXT_NAMESPACE "ComponentPickerCustomization"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TSharedRef<IPropertyTypeCustomization> FComponentPickerCustomization::MakeInstance( )
{
    return MakeShareable( new FComponentPickerCustomization );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FComponentPickerCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> pInPropertyHandle,
                                                     FDetailWidgetRow& rHeaderRow,
                                                     IPropertyTypeCustomizationUtils& rCustomizationUtils )
{
    m_pPropertyHandle = pInPropertyHandle;

    m_pCachedComponent.Reset( );
    m_pCachedFirstOuterActor.Reset( );
    m_eCachedPropertyAccess = FPropertyAccess::Fail;

    FProperty* pProperty = pInPropertyHandle->GetProperty( );

    check( CastField<FStructProperty>( pProperty ) &&
           FComponentPicker::StaticStruct( ) == CastFieldChecked<const FStructProperty>( pProperty )->Struct );

    m_bAllowClear = !( pInPropertyHandle->GetMetaDataProperty( )->PropertyFlags & CPF_NoClear );
    m_bAllowAnyActor = pInPropertyHandle->HasMetaData( NAME_AllowAnyActor );

    BuildClassFilters( );
    BuildComboBox( );

    pInPropertyHandle->SetOnPropertyValueChanged(
        FSimpleDelegate::CreateSP( this, &FComponentPickerCustomization::OnPropertyValueChanged ) );

    // set cached values
    {
        m_pCachedComponent.Reset( );
        m_pCachedFirstOuterActor = GetFirstOuterActor( );

        FComponentPicker TmpComponentReference;
        m_eCachedPropertyAccess = GetValue( TmpComponentReference );

        if( m_eCachedPropertyAccess == FPropertyAccess::Success )
        {
            m_pCachedComponent = TmpComponentReference.GetComponent( );

            if( !IsComponentPickerValid( TmpComponentReference ) )
            {
                m_pCachedComponent.Reset( );
            }
        }
    }

    rHeaderRow.NameContent( )
        [
            pInPropertyHandle->CreatePropertyNameWidget( )
        ]
    .ValueContent( )
        [
            m_pComponentComboButton.ToSharedRef( )
        ]
    .IsEnabled( MakeAttributeSP( this, &FComponentPickerCustomization::CanEdit ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FComponentPickerCustomization::CustomizeChildren( TSharedRef<IPropertyHandle> pInPropertyHandle,
                                                       IDetailChildrenBuilder& rStructBuilder,
                                                       IPropertyTypeCustomizationUtils& rCustomizationUtils )
{
    uint32 unNumberOfChild;

    if( pInPropertyHandle->GetNumChildren( unNumberOfChild ) == FPropertyAccess::Success )
    {
        for( uint32 unIndex = 0; unIndex < unNumberOfChild; ++unIndex )
        {
            TSharedRef<IPropertyHandle> pChildPropertyHandle =
                pInPropertyHandle->GetChildHandle( unIndex ).ToSharedRef( );

            pChildPropertyHandle->SetOnPropertyValueChanged(
                FSimpleDelegate::CreateSP( this, &FComponentPickerCustomization::OnPropertyValueChanged ) );

            rStructBuilder.AddProperty( pChildPropertyHandle )
                .ShowPropertyButtons( true )
                .IsEnabled( MakeAttributeSP( this, &FComponentPickerCustomization::CanEditChildren ) );
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FComponentPickerCustomization::BuildClassFilters( )
{
    auto oAddToClassFilters = [this]( const UClass* Class,
                                      TArray<const UClass*>& ActorList,
                                      TArray<const UClass*>& ComponentList )
    {
        if( m_bAllowAnyActor && Class->IsChildOf( AActor::StaticClass( ) ) )
        {
            ActorList.Add( Class );
        }
        else if( Class->IsChildOf( UActorComponent::StaticClass( ) ) )
        {
            ComponentList.Add( Class );
        }
    };

    auto oParseClassFilters = [this, oAddToClassFilters]( const FString& MetaDataString,
                                                          TArray<const UClass*>& ActorList,
                                                          TArray<const UClass*>& ComponentList )
    {
        if( !MetaDataString.IsEmpty( ) )
        {
            TArray<FString> ClassFilterNames;
            MetaDataString.ParseIntoArrayWS( ClassFilterNames, TEXT( "," ), true );

            for( const FString& ClassName : ClassFilterNames )
            {
                UClass* Class = FindObject<UClass>( ANY_PACKAGE, *ClassName );

                if( !Class )
                {
                    Class = LoadObject<UClass>( nullptr, *ClassName );
                }

                if( Class )
                {
                    // If the class is an interface, expand it to be all classes in memory that implement the class.
                    if( Class->HasAnyClassFlags( CLASS_Interface ) )
                    {
                        for( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
                        {
                            UClass* const ClassWithInterface = ( *ClassIt );

                            if( ClassWithInterface->ImplementsInterface( Class ) )
                            {
                                oAddToClassFilters( ClassWithInterface, ActorList, ComponentList );
                            }
                        }
                    }
                    else
                    {
                        oAddToClassFilters( Class, ActorList, ComponentList );
                    }
                }
            }
        }
    };

    // Account for the allowed classes specified in the property metadata
    const FString& strAllowedClassesFilterString = m_pPropertyHandle->GetMetaData( NAME_AllowedClasses );
    oParseClassFilters( strAllowedClassesFilterString,
                        m_oAllowedActorClassFilters,
                        m_oAllowedComponentClassFilters );

    const FString& strDisallowedClassesFilterString = m_pPropertyHandle->GetMetaData( NAME_DisallowedClasses );
    oParseClassFilters( strDisallowedClassesFilterString,
                        m_oDisallowedActorClassFilters,
                        m_oDisallowedComponentClassFilters );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FComponentPickerCustomization::BuildComboBox( )
{
    TAttribute<bool> bIsEnabledAttribute( this, &FComponentPickerCustomization::CanEdit );
    TAttribute<FText> strTooltipAttribute;

    if( m_pPropertyHandle->GetMetaDataProperty( )->HasAnyPropertyFlags( CPF_EditConst | CPF_DisableEditOnTemplate ) )
    {
        TArray<UObject*> oObjectList;
        m_pPropertyHandle->GetOuterObjects( oObjectList );

        // If there is no objects, that means we must have a struct asset managing this property
        if( oObjectList.Num( ) == 0 )
        {
            bIsEnabledAttribute.Set( false );
            strTooltipAttribute.Set( LOCTEXT( "VariableHasDisableEditOnTemplate",
                                              "Editing this value in structure's defaults is not allowed" ) );
        }
        else
        {
            // Go through all the found objects and see if any are a CDO, we can't set an actor in a CDO default.
            for( UObject* pObj : oObjectList )
            {
                if( pObj->IsTemplate( ) && !pObj->IsA<ALevelScriptActor>( ) )
                {
                    bIsEnabledAttribute.Set( false );
                    strTooltipAttribute.Set( LOCTEXT( "VariableHasDisableEditOnTemplateTooltip",
                                                      "Editing this value in a Class Default Object is not allowed" ) );
                    break;
                }
            }
        }
    }

    TSharedRef<SVerticalBox> pObjectContent = SNew( SVerticalBox );

    if( m_bAllowAnyActor )
    {
        pObjectContent->AddSlot( )
            [
                SNew( SHorizontalBox )
                + SHorizontalBox::Slot( )
            .AutoWidth( )
            .HAlign( HAlign_Left )
            .VAlign( VAlign_Center )
            [
                SNew( SImage )
                .Image( this, &FComponentPickerCustomization::GetActorIcon )
            ]
        + SHorizontalBox::Slot( )
            .FillWidth( 1 )
            .VAlign( VAlign_Center )
            [
                // Show the name of the asset or actor
                SNew( STextBlock )
                .Font( IDetailLayoutBuilder::GetDetailFont( ) )
            .Text( this, &FComponentPickerCustomization::OnGetActorName )
            ]
            ];
    }

    pObjectContent->AddSlot( )
        [
            SNew( SHorizontalBox )
            + SHorizontalBox::Slot( )
        .AutoWidth( )
        .HAlign( HAlign_Left )
        .VAlign( VAlign_Center )
        [
            SNew( SImage )
            .Image( this, &FComponentPickerCustomization::GetComponentIcon )
        ]
        + SHorizontalBox::Slot( )
        .FillWidth( 1 )
        .VAlign( VAlign_Center )
        [
            // Show the name of the asset or actor
            SNew( STextBlock )
                .Font( IDetailLayoutBuilder::GetDetailFont( ) )
                .Text( this, &FComponentPickerCustomization::OnGetComponentName )
        ]
        ];

    TSharedRef<SHorizontalBox> pComboButtonContent = SNew( SHorizontalBox )
        + SHorizontalBox::Slot( )
        .AutoWidth( )
        .HAlign( HAlign_Left )
        .VAlign( VAlign_Center )
        [
            SNew( SImage )
            .Image( this, &FComponentPickerCustomization::GetStatusIcon )
        ]
    + SHorizontalBox::Slot( )
        .FillWidth( 1 )
        .VAlign( VAlign_Center )
        [
            pObjectContent
        ];

    m_pComponentComboButton = SNew( SComboButton )
        .ToolTipText( strTooltipAttribute )
        .ButtonStyle( FEditorStyle::Get( ), "PropertyEditor.AssetComboStyle" )
        .ForegroundColor( FEditorStyle::GetColor( "PropertyEditor.AssetName.ColorAndOpacity" ) )
        .OnGetMenuContent( this, &FComponentPickerCustomization::OnGetMenuContent )
        .OnMenuOpenChanged( this, &FComponentPickerCustomization::OnMenuOpenChanged )
        .IsEnabled( bIsEnabledAttribute )
        .ContentPadding( 2.0f )
        .ButtonContent( )
        [
            SNew( SWidgetSwitcher )
                .WidgetIndex( this, &FComponentPickerCustomization::OnGetComboContentWidgetIndex )
                + SWidgetSwitcher::Slot( )
                [
                    SNew( STextBlock )
                        .Text( LOCTEXT( "MultipleValuesText", "<multiple values>" ) )
                        .Font( IDetailLayoutBuilder::GetDetailFont( ) )
                ]
            + SWidgetSwitcher::Slot( )
            [
                pComboButtonContent
            ]
        ];
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
AActor* FComponentPickerCustomization::GetFirstOuterActor( ) const
{
    TArray<UObject*> oObjectList;
    m_pPropertyHandle->GetOuterObjects( oObjectList );

    for( UObject* pObj : oObjectList )
    {
        while( pObj )
        {
            if( AActor* pActor = Cast<AActor>( pObj ) )
            {
                return pActor;
            }

            if( UActorComponent* pComponent = Cast<UActorComponent>( pObj ) )
            {
                if( pComponent->GetOwner( ) )
                {
                    return pComponent->GetOwner( );
                }
            }

            pObj = pObj->GetOuter( );
        }
    }

    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FComponentPickerCustomization::SetValue( const FComponentPicker& rValue )
{
    m_pComponentComboButton->SetIsOpen( false );

    const bool bIsEmpty = rValue.GetComponent( ) == nullptr;
    const bool bAllowedToSetBasedOnFilter = IsComponentPickerValid( rValue );

    if( bIsEmpty || bAllowedToSetBasedOnFilter )
    {
        FString strTextValue;
        CastFieldChecked<const FStructProperty>( m_pPropertyHandle->GetProperty( ) )->Struct->ExportText(
            strTextValue,
            &rValue,
            &rValue,
            nullptr,
            EPropertyPortFlags::PPF_None,
            nullptr );
        ensure( m_pPropertyHandle->SetValueFromFormattedString( strTextValue ) == FPropertyAccess::Result::Success );
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FPropertyAccess::Result FComponentPickerCustomization::GetValue( FComponentPicker& rOutValue ) const
{
    // Potentially accessing the value while garbage collecting or saving the package could trigger a crash.
    // so we fail to get the value when that is occurring.
    if( GIsSavingPackage || IsGarbageCollecting( ) )
    {
        return FPropertyAccess::Fail;
    }

    FPropertyAccess::Result eResult = FPropertyAccess::Fail;

    if( m_pPropertyHandle.IsValid( ) && m_pPropertyHandle->IsValidHandle( ) )
    {
        TArray<void*> oRawData;
        m_pPropertyHandle->AccessRawData( oRawData );
        const UActorComponent* pCurrentComponent = nullptr;

        for( const void* pRawPtr : oRawData )
        {
            if( pRawPtr )
            {
                const FComponentPicker& rThisReference = *reinterpret_cast<const FComponentPicker*>( pRawPtr );

                if( eResult == FPropertyAccess::Success )
                {
                    if( rThisReference.GetComponent( ) != pCurrentComponent )
                    {
                        eResult = FPropertyAccess::MultipleValues;
                        break;
                    }
                }
                else
                {
                    rOutValue = rThisReference;
                    pCurrentComponent = rOutValue.GetComponent( );
                    eResult = FPropertyAccess::Success;
                }
            }
            else if( eResult == FPropertyAccess::Success )
            {
                eResult = FPropertyAccess::MultipleValues;
                break;
            }
        }
    }

    return eResult;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FComponentPickerCustomization::IsComponentPickerValid( const FComponentPicker& rValue ) const
{
    if( !m_bAllowAnyActor &&
        rValue.GetComponent( ) != nullptr &&
        ( rValue.GetComponent( )->GetOwner( ) != m_pCachedFirstOuterActor.Get( ) ) )
    {
        return false;
    }

    AActor* pCachedActor = m_pCachedFirstOuterActor.Get( );

    if( const UActorComponent* pNewComponent = rValue.GetComponent( ) )
    {
        if( !IsFilteredComponent( pNewComponent ) )
        {
            return false;
        }

        if( m_bAllowAnyActor )
        {
            if( pNewComponent->GetOwner( ) == nullptr )
            {
                return false;
            }


            TArray<UObject*> oObjectList;
            m_pPropertyHandle->GetOuterObjects( oObjectList );

            // Is the Outer object in the same world/level
            for( UObject* pObj : oObjectList )
            {
                AActor* pActor = Cast<AActor>( pObj );

                if( pActor == nullptr )
                {
                    if( UActorComponent* pActorComponent = Cast<UActorComponent>( pObj ) )
                    {
                        pActor = pActorComponent->GetOwner( );
                    }
                }

                if( pActor )
                {
                    if( pNewComponent->GetOwner( )->GetLevel( ) != pActor->GetLevel( ) )
                    {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FComponentPickerCustomization::OnPropertyValueChanged( )
{
    m_pCachedComponent.Reset( );
    m_pCachedFirstOuterActor = GetFirstOuterActor( );

    FComponentPicker oTmpComponentReference;
    m_eCachedPropertyAccess = GetValue( oTmpComponentReference );

    if( m_eCachedPropertyAccess == FPropertyAccess::Success )
    {
        m_pCachedComponent = oTmpComponentReference.GetComponent( );

        if( !IsComponentPickerValid( oTmpComponentReference ) )
        {
            m_pCachedComponent.Reset( );

            if( !( oTmpComponentReference == FComponentPicker( ) ) )
            {
                SetValue( FComponentPicker( ) );
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int32 FComponentPickerCustomization::OnGetComboContentWidgetIndex( ) const
{
    switch( m_eCachedPropertyAccess )
    {
        case FPropertyAccess::MultipleValues:
        {
            return 0;
        }
        case FPropertyAccess::Success:
        default:
        {
            return 1;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FComponentPickerCustomization::CanEdit( ) const
{
    return m_pPropertyHandle.IsValid( ) ? !m_pPropertyHandle->IsEditConst( ) : true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FComponentPickerCustomization::CanEditChildren( ) const
{
    return CanEdit( ) && !m_pCachedFirstOuterActor.IsValid( );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const FSlateBrush* FComponentPickerCustomization::GetActorIcon( ) const
{
    if( const UActorComponent* pComponent = m_pCachedComponent.Get( ) )
    {
        if( AActor* pOwner = pComponent->GetOwner( ) )
        {
            return FSlateIconFinder::FindIconBrushForClass( pOwner->GetClass( ) );
        }
    }

    return FSlateIconFinder::FindIconBrushForClass( AActor::StaticClass( ) );;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FText FComponentPickerCustomization::OnGetActorName( ) const
{
    if( const UActorComponent* pComponent = m_pCachedComponent.Get( ) )
    {
        if( AActor* pOwner = pComponent->GetOwner( ) )
        {
            return FText::AsCultureInvariant( pOwner->GetActorLabel( ) );
        }
    }

    return LOCTEXT( "NoActor", "None" );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const FSlateBrush* FComponentPickerCustomization::GetComponentIcon( ) const
{
    if( const UActorComponent* pActorComponent = m_pCachedComponent.Get( ) )
    {
        return FSlateIconFinder::FindIconBrushForClass( pActorComponent->GetClass( ) );
    }

    return FSlateIconFinder::FindIconBrushForClass( UActorComponent::StaticClass( ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FText FComponentPickerCustomization::OnGetComponentName( ) const
{
    if( m_eCachedPropertyAccess == FPropertyAccess::Success )
    {
        if( const UActorComponent* pActorComponent = m_pCachedComponent.Get( ) )
        {
            const FName strComponentName =
                FComponentEditorUtils::FindVariableNameGivenComponentInstance( pActorComponent );

            const bool bIsArrayVariable = !strComponentName.IsNone( ) &&
                pActorComponent->GetOwner( ) != nullptr &&
                FindFProperty<FArrayProperty>( pActorComponent->GetOwner( )->GetClass( ), strComponentName );

            if( !strComponentName.IsNone( ) && !bIsArrayVariable )
            {
                return FText::FromName( strComponentName );
            }

            return FText::AsCultureInvariant( pActorComponent->GetName( ) );
        }
    }
    else if( m_eCachedPropertyAccess == FPropertyAccess::MultipleValues )
    {
        return LOCTEXT( "MultipleValues", "Multiple Values" );
    }

    return LOCTEXT( "NoComponent", "None" );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const FSlateBrush* FComponentPickerCustomization::GetStatusIcon( ) const
{
    static FSlateNoResource EmptyBrush = FSlateNoResource( );

    if( m_eCachedPropertyAccess == FPropertyAccess::Fail )
    {
        return FEditorStyle::GetBrush( "Icons.Error" );
    }

    return &EmptyBrush;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TSharedRef<SWidget> FComponentPickerCustomization::OnGetMenuContent( )
{
    UActorComponent* pInitialComponent = m_pCachedComponent.Get( );

    return SNew( SComponentPicker )
        .pInitialComponent( pInitialComponent )
        .bAllowClear( m_bAllowClear )
        .oActorFilter( FOnShouldFilterActor::CreateSP( this, &FComponentPickerCustomization::IsAllowedActor ) )
        .oComponentFilter( FOnShouldFilterComponent::CreateSP( this, &FComponentPickerCustomization::IsFilteredComponent ) )
        .oOnSet( FOnComponentPicked::CreateSP( this, &FComponentPickerCustomization::OnComponentSelected ) )
        .oOnClose( FSimpleDelegate::CreateSP( this, &FComponentPickerCustomization::CloseComboButton ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FComponentPickerCustomization::OnMenuOpenChanged( bool bOpen )
{
    if( !bOpen )
    {
        m_pComponentComboButton->SetMenuContent( SNullWidget::NullWidget );
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FComponentPickerCustomization::IsAllowedActor( const AActor* const pActor ) const
{
    return m_bAllowAnyActor || pActor == m_pCachedFirstOuterActor.Get( );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FComponentPickerCustomization::IsFilteredComponent( const UActorComponent* const pComponent ) const
{
    const USceneComponent* pSceneComp = Cast<USceneComponent>( pComponent );
    const AActor* pOuterActor = m_pCachedFirstOuterActor.Get( );

    return pComponent->GetOwner( ) &&
        ( IsAllowedActor( pComponent->GetOwner( ) ) ) &&
        ( !m_bAllowAnyActor || ( pOuterActor != nullptr &&
                                 pComponent->GetOwner( )->GetLevel( ) == pOuterActor->GetLevel( ) ) ) &&
        FComponentEditorUtils::CanEditComponentInstance( pComponent, pSceneComp, false ) &&
        IsFilteredObject( pComponent, m_oAllowedComponentClassFilters, m_oDisallowedComponentClassFilters ) &&
        IsFilteredObject( pComponent->GetOwner( ), m_oAllowedActorClassFilters, m_oDisallowedActorClassFilters );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FComponentPickerCustomization::IsFilteredObject( const UObject* const pObject,
                                                      const TArray<const UClass*>& rAllowedFilters,
                                                      const TArray<const UClass*>& rDisallowedFilters )
{
    bool bAllowedToSetBasedOnFilter = true;

    const UClass* pObjectClass = pObject->GetClass( );

    if( rAllowedFilters.Num( ) > 0 )
    {
        bAllowedToSetBasedOnFilter = false;

        for( const UClass* pAllowedClass : rAllowedFilters )
        {
            const bool bAllowedClassIsInterface = pAllowedClass->HasAnyClassFlags( CLASS_Interface );

            if( pObjectClass->IsChildOf( pAllowedClass ) ||
                ( bAllowedClassIsInterface && pObjectClass->ImplementsInterface( pAllowedClass ) ) )
            {
                bAllowedToSetBasedOnFilter = true;
                break;
            }
        }
    }

    if( rDisallowedFilters.Num( ) > 0 && bAllowedToSetBasedOnFilter )
    {
        for( const UClass* pDisallowedClass : rDisallowedFilters )
        {
            const bool bDisallowedClassIsInterface = pDisallowedClass->HasAnyClassFlags( CLASS_Interface );

            if( pObjectClass->IsChildOf( pDisallowedClass ) ||
                ( bDisallowedClassIsInterface && pObjectClass->ImplementsInterface( pDisallowedClass ) ) )
            {
                bAllowedToSetBasedOnFilter = false;
                break;
            }
        }
    }

    return bAllowedToSetBasedOnFilter;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FComponentPickerCustomization::OnComponentSelected( UActorComponent* pInComponent )
{
    m_pComponentComboButton->SetIsOpen( false );
    FComponentPicker oComponentReference( pInComponent );
    SetValue( oComponentReference );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FComponentPickerCustomization::CloseComboButton( )
{
    m_pComponentComboButton->SetIsOpen( false );
}

#undef LOCTEXT_NAMESPACE
