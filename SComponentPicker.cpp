// Fill out your copyright notice in the Description page of Project Settings.

#include "SComponentPicker.h"

#include "Editor/SceneOutliner/Public/SceneOutlinerModule.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ActorTreeItem.h"
#include "ComponentTreeItem.h"

#define LOCTEXT_NAMESPACE "SComponentPicker"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SComponentPicker::Construct( const FArguments& rInArgs )
{
    m_pInitialComponent = rInArgs._pInitialComponent;
    m_bAllowClear = rInArgs._bAllowClear;
    m_oActorFilter = rInArgs._oActorFilter;
    m_oComponentFilter = rInArgs._oComponentFilter;
    m_oOnSet = rInArgs._oOnSet;
    m_oOnClose = rInArgs._oOnClose;

    FMenuBuilder MenuBuilder( true, NULL );

    MenuBuilder.BeginSection( NAME_None, LOCTEXT( "CurrentComponentOperationsHeader", "Current Component" ) );
    {
        if( m_pInitialComponent )
        {
            MenuBuilder.AddMenuEntry(
                LOCTEXT( "EditComponent", "Edit" ),
                LOCTEXT( "EditComponent_Tooltip", "Edit this component" ),
                FSlateIcon( ),
                FUIAction( FExecuteAction::CreateSP( this, &SComponentPicker::OnEdit ) ) );
        }

        MenuBuilder.AddMenuEntry(
            LOCTEXT( "CopyComponent", "Copy" ),
            LOCTEXT( "CopyComponent_Tooltip", "Copies the component to the clipboard" ),
            FSlateIcon( ),
            FUIAction( FExecuteAction::CreateSP( this, &SComponentPicker::OnCopy ) )
        );

        MenuBuilder.AddMenuEntry(
            LOCTEXT( "PasteComponent", "Paste" ),
            LOCTEXT( "PasteComponent_Tooltip", "Pastes an component from the clipboard to this field" ),
            FSlateIcon( ),
            FUIAction(
                FExecuteAction::CreateSP( this, &SComponentPicker::OnPaste ),
                FCanExecuteAction::CreateSP( this, &SComponentPicker::CanPaste ) )
        );

        if( m_bAllowClear )
        {
            MenuBuilder.AddMenuEntry(
                LOCTEXT( "ClearComponent", "Clear" ),
                LOCTEXT( "ClearComponent_ToolTip", "Clears the component set on this field" ),
                FSlateIcon( ),
                FUIAction( FExecuteAction::CreateSP( this, &SComponentPicker::OnClear ) )
            );
        }
    }
    MenuBuilder.EndSection( );

    MenuBuilder.BeginSection( NAME_None, LOCTEXT( "BrowseHeader", "Browse" ) );
    {
        TSharedPtr<SWidget> MenuContent;

        FSceneOutlinerModule& SceneOutlinerModule =
            FModuleManager::Get( ).LoadModuleChecked<FSceneOutlinerModule>( TEXT( "SceneOutliner" ) );

        FSceneOutlinerInitializationOptions InitOptions;
        InitOptions.bFocusSearchBoxWhenOpened = true;

        struct FPickerFilter : public FSceneOutlinerFilter
        {
            FPickerFilter( const FOnShouldFilterActor& InActorFilter, const FOnShouldFilterComponent& InComponentFilter )
                : FSceneOutlinerFilter( FSceneOutlinerFilter::EDefaultBehaviour::Fail )
                , ActorFilter( InActorFilter )
                , ComponentFilter( InComponentFilter )
            {
                // Empty
            }

            virtual bool PassesFilter( const ISceneOutlinerTreeItem& InItem ) const override
            {
                if( const FActorTreeItem* ActorItem = InItem.CastTo<FActorTreeItem>( ) )
                {
                    return ActorItem->IsValid( ) && ActorFilter.Execute( ActorItem->Actor.Get( ) );
                }
                else if( const FComponentTreeItem* ComponentItem = InItem.CastTo<FComponentTreeItem>( ) )
                {
                    return ComponentItem->IsValid( ) && ComponentFilter.Execute( ComponentItem->Component.Get( ) );
                }

                return DefaultBehaviour == FSceneOutlinerFilter::EDefaultBehaviour::Pass;
            }

            virtual bool GetInteractiveState( const ISceneOutlinerTreeItem& InItem ) const override
            {
                // All components which pass the filter are interactive
                if( const FComponentTreeItem* ComponentItem = InItem.CastTo<FComponentTreeItem>( ) )
                {
                    return true;
                }

                return DefaultBehaviour == FSceneOutlinerFilter::EDefaultBehaviour::Pass;
            }

            FOnShouldFilterActor ActorFilter;
            FOnShouldFilterComponent ComponentFilter;
        };

        TSharedRef<FSceneOutlinerFilter> Filter = MakeShared<FPickerFilter>( m_oActorFilter, m_oComponentFilter );
        InitOptions.Filters->Add( Filter );

        InitOptions.ColumnMap.Add( FSceneOutlinerBuiltInColumnTypes::Label( ),
                                   FSceneOutlinerColumnInfo( ESceneOutlinerColumnVisibility::Visible, 0 ) );

        // NOTE: Copied these constant width and height overrides from
        // Engine\Source\Editor\PropertyEditor\Private\UserInterface\PropertyEditor\PropertyEditorAssetConstants.h
        MenuContent = SNew( SBox )
            .WidthOverride( 300.0f )
            .HeightOverride( 300.0f )
            [
                SNew( SBorder )
                .BorderImage( FEditorStyle::GetBrush( "Menu.Background" ) )
            [
                SceneOutlinerModule.CreateComponentPicker(
                    InitOptions, FOnComponentPicked::CreateSP( this, &SComponentPicker::OnItemSelected ) )
            ]
            ];

        MenuBuilder.AddWidget( MenuContent.ToSharedRef( ), FText::GetEmpty( ), true );

    }
    MenuBuilder.EndSection( );

    ChildSlot
        [
            MenuBuilder.MakeWidget( )
        ];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SComponentPicker::OnEdit( )
{
    if( m_pInitialComponent )
    {
        GEditor->EditObject( m_pInitialComponent );
    }

    m_oOnClose.ExecuteIfBound( );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SComponentPicker::OnCopy( )
{
    if( m_pInitialComponent )
    {
        FPlatformApplicationMisc::ClipboardCopy( *FString::Printf( TEXT( "%s %s" ),
                                                                   *m_pInitialComponent->GetClass( )->GetPathName( ),
                                                                   *m_pInitialComponent->GetPathName( ) ) );
    }

    m_oOnClose.ExecuteIfBound( );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SComponentPicker::OnPaste( )
{
    FString ClipboardText;
    FPlatformApplicationMisc::ClipboardPaste( ClipboardText );

    bool bFound = false;

    int32 SpaceIndex = INDEX_NONE;

    if( ClipboardText.FindChar( TEXT( ' ' ), SpaceIndex ) )
    {
        FString ClassPath = ClipboardText.Left( SpaceIndex );
        FString PossibleObjectPath = ClipboardText.Mid( SpaceIndex );

        if( UClass* ClassPtr = LoadClass<UActorComponent>( nullptr, *ClassPath ) )
        {
            UActorComponent* Component = FindObject<UActorComponent>( nullptr, *PossibleObjectPath );

            if( Component &&
                Component->IsA( ClassPtr ) &&
                Component->GetOwner( ) &&
                ( !m_oComponentFilter.IsBound( ) || m_oComponentFilter.Execute( Component ) ) )
            {
                if( !m_oActorFilter.IsBound( ) || m_oActorFilter.Execute( Component->GetOwner( ) ) )
                {
                    SetValue( Component );
                    bFound = true;
                }
            }
        }
    }

    if( !bFound )
    {
        SetValue( nullptr );
    }

    m_oOnClose.ExecuteIfBound( );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SComponentPicker::CanPaste( )
{
    FString ClipboardText;
    FPlatformApplicationMisc::ClipboardPaste( ClipboardText );

    bool bCanPaste = false;

    int32 SpaceIndex = INDEX_NONE;

    if( ClipboardText.FindChar( TEXT( ' ' ), SpaceIndex ) )
    {
        FString Class = ClipboardText.Left( SpaceIndex );
        FString PossibleObjectPath = ClipboardText.Mid( SpaceIndex );

        bCanPaste = !Class.IsEmpty( ) && !PossibleObjectPath.IsEmpty( );

        if( bCanPaste )
        {
            bCanPaste = LoadClass<UActorComponent>( nullptr, *Class ) != nullptr;
        }

        if( bCanPaste )
        {
            bCanPaste = FindObject<UActorComponent>( nullptr, *PossibleObjectPath ) != nullptr;
        }
    }

    return bCanPaste;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SComponentPicker::OnClear( )
{
    SetValue( nullptr );
    m_oOnClose.ExecuteIfBound( );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SComponentPicker::OnItemSelected( UActorComponent* pComponent )
{
    SetValue( pComponent );
    m_oOnClose.ExecuteIfBound( );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SComponentPicker::SetValue( UActorComponent* pInComponent )
{
    m_oOnSet.ExecuteIfBound( pInComponent );
}

#undef LOCTEXT_NAMESPACE
