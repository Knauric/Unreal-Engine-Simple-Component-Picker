// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

class SComboButton;
class SWidget;
struct FSlateBrush;
struct FComponentPicker;

class FComponentPickerCustomization : public IPropertyTypeCustomization
{
public:
    // Makes a new instance of this customization for a specific detail view requesting it.
    static TSharedRef<IPropertyTypeCustomization> MakeInstance( );

    // START IPropertyTypeCustomization interface.
    virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> pInPropertyHandle,
                                  class FDetailWidgetRow& rHeaderRow,
                                  IPropertyTypeCustomizationUtils& rCustomizationUtils ) override;

    virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> pInPropertyHandle,
                                    class IDetailChildrenBuilder& rStructBuilder,
                                    IPropertyTypeCustomizationUtils& rCustomizationUtils ) override;
    // END IPropertyTypeCustomization interface.

private:
    // From the property metadata, build the list of allowed and disallowed classes.
    void BuildClassFilters( );

    // Build the combobox widget.
    void BuildComboBox( );

    // From the Detail panel outer hierarchy, find the first actor or component owner we find.
    AActor* GetFirstOuterActor( ) const;

    // Set the value of the asset referenced by this property editor.
    // Will set the underlying property handle if there is one.
    void SetValue( const FComponentPicker& Value );

    // Get the value referenced by this widget.
    FPropertyAccess::Result GetValue( FComponentPicker& rOutValue ) const;

    // Is the Value valid.
    bool IsComponentPickerValid( const FComponentPicker& rValue ) const;

    // Callback when the property value changed.
    void OnPropertyValueChanged( );

private:
    // Return 0 if we have multiple values to edit.
    // Return 1 if we display the widget normally.
    int32 OnGetComboContentWidgetIndex( ) const;

    bool CanEdit( ) const;
    bool CanEditChildren( ) const;

    const FSlateBrush* GetActorIcon( ) const;
    FText OnGetActorName( ) const;
    const FSlateBrush* GetComponentIcon( ) const;
    FText OnGetComponentName( ) const;
    const FSlateBrush* GetStatusIcon( ) const;

    // Get the content to be displayed in the asset/actor picker menu 
    TSharedRef<SWidget> OnGetMenuContent( );

    // Called when the asset menu is closed, we handle this to force the destruction of the asset menu to
    // ensure any settings the user set are saved.
    void OnMenuOpenChanged( bool bOpen );

    // Returns whether the actor/component should be filtered out from selection.
    bool IsAllowedActor( const AActor* const pActor ) const;
    bool IsFilteredComponent( const UActorComponent* const pComponent ) const;
    static bool IsFilteredObject( const UObject* const pObject,
                                  const TArray<const UClass*>& rAllowedFilters,
                                  const TArray<const UClass*>& rDisallowedFilters );

    // Delegate for handling selection in the scene outliner.
    void OnComponentSelected( UActorComponent* pInComponent );

    // Closes the combo button.
    void CloseComboButton( );

private:
    // The property handle we are customizing
    TSharedPtr<IPropertyHandle> m_pPropertyHandle;

    // Main combo button
    TSharedPtr<SComboButton> m_pComponentComboButton;

    // Classes that can be used with this property
    TArray<const UClass*> m_oAllowedActorClassFilters;
    TArray<const UClass*> m_oAllowedComponentClassFilters;

    // Classes that can NOT be used with this property
    TArray<const UClass*> m_oDisallowedActorClassFilters;
    TArray<const UClass*> m_oDisallowedComponentClassFilters;

    // Whether the asset can be 'None' in this case
    bool m_bAllowClear;

    // Can the actor be different/selected.
    bool m_bAllowAnyActor;

    // Cached values
    TWeakObjectPtr<AActor> m_pCachedFirstOuterActor;
    TWeakObjectPtr<UActorComponent> m_pCachedComponent;
    FPropertyAccess::Result m_eCachedPropertyAccess;
};
