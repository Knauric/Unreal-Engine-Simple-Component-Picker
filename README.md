# Unreal-Engine-Simple-Component-Picker
A component picker that is simpler than the built-in FComponentReference. Full disclosure: This code appears to build, run, and serialize without problems on my machine, but I have not tested it in a full, packaged build. 

To use the component picker, add the source and header files in this repository to your project, then add the following lines to your game's module's StartupModule function:

    FPropertyEditorModule& rPropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

    rPropertyModule.RegisterCustomPropertyTypeLayout(
        "ComponentPicker",
        FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FComponentPickerCustomization::MakeInstance ) );

Then you can add it as a property to your classes like this:

    UPROPERTY( EditInstanceOnly )
    FComponentPicker m_oComponentPicker;
    
It supports the following meta tags (which function the same as they do on FComponentReference): AllowAnyActor, AllowedClasses, and DisallowedClasses
