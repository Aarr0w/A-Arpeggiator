/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

class LegacyAudioParameter : public juce::AudioProcessorParameter
{
public:
    LegacyAudioParameter(juce::AudioProcessor& audioProcessorToUse, int audioParameterIndex)
    {
        Aprocessor = &audioProcessorToUse;

        paramIndex = audioParameterIndex;
        jassert(paramIndex < Aprocessor->getNumParameters());
    }

    //==============================================================================
    float getValue() const override { return Aprocessor->getParameter(paramIndex); }
    void setValue(float newValue) override { Aprocessor->setParameter(paramIndex, newValue); }
    float getDefaultValue() const override { return Aprocessor->getParameterDefaultValue(paramIndex); }
    juce::String getName(int maxLen) const override { return Aprocessor->getParameterName(paramIndex, maxLen); }
    juce::String getLabel() const override { return Aprocessor->getParameterLabel(paramIndex); }
    int getNumSteps() const override { return Aprocessor->getParameterNumSteps(paramIndex); }
    bool isDiscrete() const override { return Aprocessor->isParameterDiscrete(paramIndex); }
    bool isBoolean() const override { return false; }
    bool isOrientationInverted() const override { return Aprocessor->isParameterOrientationInverted(paramIndex); }
    bool isAutomatable() const override { return Aprocessor->isParameterAutomatable(paramIndex); }
    bool isMetaParameter() const override { return Aprocessor->isMetaParameter(paramIndex); }
    Category getCategory() const override { return Aprocessor->getParameterCategory(paramIndex); }
    juce::String getCurrentValueAsText() const override { return Aprocessor->getParameterText(paramIndex); }
    juce::String getParamID() const { return Aprocessor->getParameterID(paramIndex); }

    //==============================================================================
    float getValueForText(const juce::String&) const override
    {
        // legacy parameters do not support this method
        jassertfalse;
        return 0.0f;
    }

    juce::String getText(float, int) const override
    {
        // legacy parameters do not support this method
        jassertfalse;
        return {};
    }

//==============================================================================
    static bool isLegacy(AudioProcessorParameter* param) noexcept
    {
        return (dynamic_cast<LegacyAudioParameter*> (param) != nullptr);
    }

    static int getParamIndex(juce::AudioProcessor& processor, AudioProcessorParameter* param) noexcept
    {
        if (auto* legacy = dynamic_cast<LegacyAudioParameter*> (param))
        {
            return legacy->paramIndex;
        }
        else
        {
            auto n = processor.getNumParameters();
            jassert(n == processor.getParameters().size());

            for (int i = 0; i < n; ++i)
            {
                if (processor.getParameters()[i] == param)
                    return i;
            }
        }

        return -1;
    }

    static juce::String getParamID(AudioProcessorParameter* param, bool forceLegacyParamIDs) noexcept
    {
        if (auto* legacy = dynamic_cast<LegacyAudioParameter*> (param))
        {
            return forceLegacyParamIDs ? juce::String(legacy->paramIndex) : legacy->getParamID();
        }
        else if (auto* paramWithID = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
        {
            if (!forceLegacyParamIDs)
                return paramWithID->paramID;
        }

        return juce::String(param->getParameterIndex());
    }
    juce::AudioProcessor* Aprocessor;
    int paramIndex;
};

//==============================================================================
class LegacyAudioParametersWrapper
{
public:
    void update(juce::AudioProcessor& audioProcessor, bool forceLegacyParamIDs)
    {
        clear();

        legacyParamIDs = forceLegacyParamIDs;

        auto numParameters = audioProcessor.getNumParameters();
        usingManagedParameters = audioProcessor.getParameters().size() == numParameters;

        for (int i = 0; i < numParameters; ++i)
        {
            juce::AudioProcessorParameter* param = usingManagedParameters ? audioProcessor.getParameters()[i]
                : (legacy.add(new LegacyAudioParameter(audioProcessor, i)));
            params.add(param);
        }
    }

    void clear()
    {
        legacy.clear();
        params.clear();
    }

    juce::AudioProcessorParameter* getParamForIndex(int index) const
    {
        if (juce::isPositiveAndBelow(index, params.size()))
            return params[index];

        return nullptr;
    }

    juce::String getParamID(juce::AudioProcessor& processor, int idx) const noexcept
    {
        if (usingManagedParameters && !legacyParamIDs)
            return processor.getParameterID(idx);

        return juce::String(idx);
    }

    bool isUsingManagedParameters() const noexcept { return usingManagedParameters; }
    int getNumParameters() const noexcept { return params.size(); }

    juce::Array<juce::AudioProcessorParameter*> params;

private:
    juce::OwnedArray<LegacyAudioParameter> legacy;
    bool legacyParamIDs = false, usingManagedParameters = false;
};

//=======================================================================================
class ParameterListener : private juce::AudioProcessorParameter::Listener,
        private juce::AudioProcessorListener,
        private juce::Timer
    {
    public:
        ParameterListener(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param)
            : processor(proc), parameter(param), isLegacyParam(LegacyAudioParameter::isLegacy(&param))
        {
           if (isLegacyParam)
                processor.addListener(this);
           else
               parameter.addListener(this);

           startTimer(100);
        }

        ~ParameterListener() override
        {
            if (isLegacyParam)
                processor.removeListener(this);
            else
                parameter.removeListener(this);
        }

        juce::AudioProcessorParameter& getParameter() const noexcept
        {
            return parameter;
        }

        virtual void handleNewParameterValue() = 0;

    private:
    //==============================================================================
        void parameterValueChanged(int, float) override
        {
            parameterValueHasChanged = 1;
        }

        void parameterGestureChanged(int, bool) override {}

    //==============================================================================
        void audioProcessorParameterChanged(juce::AudioProcessor*, int index, float) override
        {
            if (index == parameter.getParameterIndex())
                parameterValueHasChanged = 1;
        }

        void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override {}

        //==============================================================================
        void timerCallback() override
        {
            if (parameterValueHasChanged.compareAndSetBool(0, 1))
            {
                handleNewParameterValue();
                startTimerHz(50);
            }
            else
            {
                startTimer(juce::jmin(250, getTimerInterval() + 10));
            }
        }

        juce::AudioProcessor& processor;
        juce::AudioProcessorParameter& parameter;
        juce::Atomic<int> parameterValueHasChanged{ 0 };
        const bool isLegacyParam;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterListener)
   };

//============================================================================================================
class SliderParameterComponent final : public juce::Component,
    private ParameterListener
{
public:
    SliderParameterComponent(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param)
        : ParameterListener(proc, param)
    {
        //link = NULL;

        if (getParameter().getNumSteps() != juce::AudioProcessor::getDefaultNumParameterSteps())
            slider.setRange(0.0, 1.0, 1.0 / (getParameter().getNumSteps() - 1.0));
        else
            slider.setRange(0.0, 1.0);


        slider.setRange(0.0, 1.0);
        slider.setValue((int)getParameter().getDefaultValue());
        slider.setScrollWheelEnabled(false);
        addAndMakeVisible(slider);

        valueLabel.setColour(juce::Label::outlineColourId, slider.findColour(juce::Slider::textBoxOutlineColourId));
        valueLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
        valueLabel.setBorderSize({ 1, 1, 1, 1 });
        valueLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(valueLabel);

        // Set the initial value.
        handleNewParameterValue();

        slider.onValueChange = [this] { sliderValueChanged(); };
        slider.onDragStart = [this] { sliderStartedDragging(); };
        slider.onDragEnd = [this] { sliderStoppedDragging(); };
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds().reduced(0, 10);

        valueLabel.setBounds(area.removeFromRight(80));

        area.removeFromLeft(6);
        slider.setBounds(area);
    }

    void setLink(juce::Component& l)
    {
        link = &l;
    }

    void linkAction()
    {
        if (slider.getSkewFactor() == 1.0f)
        {
            slider.setRange(0.9f, .94f);
            slider.setSkewFactor(0.5f);
            slider.setValue(0.92f);
            bpm = true;        
        }
        else 
        {
            slider.setRange(0.0f,1.0f);
            slider.setSkewFactor(1.0f);
            slider.setValue(0.5f);
            bpm = false;
        }

        sliderValueChanged();
        handleNewParameterValue();
        updateTextDisplay();
    }


private:
    void updateTextDisplay()
    {
        if (!bpm)
        {
            valueLabel.setText(getParameter().getCurrentValueAsText(), juce::dontSendNotification);
        }
        else
        {
            f = ceil(getParameter().getValue() * 100);

            switch (f)
            {
                case 90: valueLabel.setText("1", juce::dontSendNotification); break;
                case 91: valueLabel.setText("1/2", juce::dontSendNotification); break;
                case 92: valueLabel.setText("1/4", juce::dontSendNotification); break;
                case 93: valueLabel.setText("1/8", juce::dontSendNotification); break;
                case 94: valueLabel.setText("1/16", juce::dontSendNotification); break;
                default:  valueLabel.setText(juce::String(f), juce::dontSendNotification); break;
            }
           
        }
    }

    void handleNewParameterValue() override
    {
        if (!isDragging)
        {
            slider.setValue(getParameter().getValue(), juce::dontSendNotification);
            updateTextDisplay();
        }
    }

    void sliderValueChanged()
    {
        auto newVal = (float)slider.getValue();

        if (getParameter().getValue() != newVal)
        {
            if (!isDragging)
                getParameter().beginChangeGesture();

            getParameter().setValueNotifyingHost((float)slider.getValue());
            updateTextDisplay();

            if (!isDragging)
                getParameter().endChangeGesture();
        }
    }

    void sliderStartedDragging()
    {
        isDragging = true;
        getParameter().beginChangeGesture();
    }

    void sliderStoppedDragging()
    {
        isDragging = false;
        getParameter().endChangeGesture();
    }

    juce::Slider slider{ juce::Slider::LinearHorizontal, juce::Slider::TextEntryBoxPosition::NoTextBox };
    juce::Component* link;
    juce::Label valueLabel;
    bool isDragging = false;
    bool bpm = false;
    int f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SliderParameterComponent)
};

//================================================================================================================

class BooleanButtonParameterComponent final : public juce::Component,
    private ParameterListener
{
public:
    BooleanButtonParameterComponent(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param, juce::String buttonName)
        : ParameterListener(proc, param)
    {
        link = nullptr;
        // Set the initial value.
        button.setButtonText(buttonName);
        handleNewParameterValue();
        button.onClick = [this] { buttonClicked(); };
        button.setClickingTogglesState(true);
        addAndMakeVisible(button);
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromLeft(8);
        button.setBounds(area.reduced(0, 10));
    }

    void setLink(juce::Component& l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

private:
    void handleNewParameterValue() override
    {
        button.setToggleState(isParameterOn(), juce::dontSendNotification);
    }


    void buttonClicked()
    {
        if (isParameterOn() != button.getToggleState())
        {
            getParameter().beginChangeGesture();
            getParameter().setValueNotifyingHost(button.getToggleState() ? 1.0f : 0.0f);
            getParameter().endChangeGesture();
            dynamic_cast<SliderParameterComponent*>(link)->linkAction();
      
           
        }
    }

    bool isParameterOn() const { return getParameter().getValue() >= 0.5f; }
    juce::Component* link;
    juce::TextButton button;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BooleanButtonParameterComponent)
};
//==============================================================================
class BooleanParameterComponent final : public juce::Component,
    private ParameterListener
{
public:
    BooleanParameterComponent(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param, juce::String buttonName)
        : ParameterListener(proc, param)
    {
        link = NULL;

        // Set the initial value.
        button.setButtonText(buttonName.substring(1));
        handleNewParameterValue();
        button.onClick = [this] { buttonClicked(); };
        addAndMakeVisible(button);
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromLeft(8);
        button.setBounds(area.reduced(0, 10));
    }

    void setLink(juce::AudioProcessorParameter& l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

private:
    void handleNewParameterValue() override
    {
        button.setToggleState(isParameterOn(), juce::dontSendNotification);
    }

    void buttonClicked()
    {
        if (isParameterOn() != button.getToggleState())
        {
            getParameter().beginChangeGesture();
            getParameter().setValueNotifyingHost(button.getToggleState() ? 1.0f : 0.0f);
            getParameter().endChangeGesture();
        }

    }

    bool isParameterOn() const { return getParameter().getValue() >= 0.5f; }

    juce::AudioProcessorParameter* link;
    juce::ToggleButton button;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BooleanParameterComponent)
};
//==============================================================================
class SwitchParameterComponent final : public juce::Component,
    private ParameterListener
{
public:
    SwitchParameterComponent(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param)
        : ParameterListener(proc, param)
    {
        link = NULL;

        n = getParameter().getNumSteps();
        float x = 1.0 / (n-1);
        for (float i = 0.0; i < n; i+=1.0)
        {   // not really sure how the 'normalized value' iterator works but this does the trick for now
            auto* b = buttons.add(new juce::TextButton(getParameter().getText((0.0f+(x*i)), 16)));

            addAndMakeVisible(b);
            b->setRadioGroupId(42);
            b->setClickingTogglesState(true);

            if(i == 0)
                b->setConnectedEdges(juce::Button::ConnectedOnRight);
            else
                if(i == (n - 1))
                    b->setConnectedEdges(juce::Button::ConnectedOnLeft);
                else
                    b->setConnectedEdges(juce::Button::ConnectedOnRight + juce::Button::ConnectedOnLeft);  
            
        }
        //////////////////////////////////////////////////////////////////////////////////
        
        // Set the initial value.
        buttons[0]->setToggleState(true, juce::dontSendNotification);
        handleNewParameterValue();

        //buttons[1].onStateChange = [this] { rightButtonChanged(); };

        for (auto& button : buttons)
            addAndMakeVisible(button);
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds().reduced(0, 8);
        area.removeFromLeft(8);

        for (int i = 0; i < n; ++i)
        {
            buttons[i]->setBounds(area.removeFromLeft(60));
        }
        /*for (auto& button : buttons)
            button.setBounds(area.removeFromLeft(80));*/
    }

    void setLink(juce::AudioProcessorParameter& l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

private:
    void handleNewParameterValue() override
    {
        /*bool newState = isParameterOn();

        if (buttons[1]->getToggleState() != newState)
        {
            buttons[1].setToggleState(newState, juce::dontSendNotification);
            buttons[0].setToggleState(!newState, juce::dontSendNotification);
        }*/
    }

    void rightButtonChanged()
    {
        //auto buttonState = buttons[1].getToggleState();

        //if (isParameterOn() != buttonState)
        //{
        //    getParameter().beginChangeGesture();

        //    if (getParameter().getAllValueStrings().isEmpty())
        //    {
        //        getParameter().setValueNotifyingHost(buttonState ? 1.0f : 0.0f);
        //    }
        //    else
        //    {
        //        // When a parameter provides a list of strings we must set its
        //        // value using those strings, rather than a float, because VSTs can
        //        // have uneven spacing between the different allowed values and we
        //        // want the snapping behaviour to be consistent with what we do with
        //        // a combo box.
        //        auto selectedText = buttons[buttonState ? 1 : 0].getButtonText();
        //        getParameter().setValueNotifyingHost(getParameter().getValueForText(selectedText));
        //    }

        //    getParameter().endChangeGesture();
        //}
    }

    bool isParameterOn() const
    {
        if (getParameter().getAllValueStrings().isEmpty())
            return getParameter().getValue() > 0.5f;

        auto index = getParameter().getAllValueStrings()
            .indexOf(getParameter().getCurrentValueAsText());

        if (index < 0)
        {
            // The parameter is producing some unexpected text, so we'll do
            // some linear interpolation.
            index = juce::roundToInt(getParameter().getValue());
        }

        return index == 1;
    }
    
    //juce::TextButton buttons[3];
    float n;
    juce::AudioProcessorParameter* link;
    juce::OwnedArray<juce::TextButton> buttons;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SwitchParameterComponent)
};
//==============================================================================
class IncrementParameterComponent final : public juce::Component,
                                        private ParameterListener
{
public:
    IncrementParameterComponent(juce::AudioProcessor & proc, juce::AudioProcessorParameter & param)
        : ParameterListener(proc, param)
    { 
        link = NULL;

        if (getParameter().getNumSteps() != juce::AudioProcessor::getDefaultNumParameterSteps())
            box.setRange(0.0, 1.0, 1.0 / (getParameter().getNumSteps() - 1.0));
        else
            box.setRange(0.0, 1.0);

        box.setScrollWheelEnabled(false);
        addAndMakeVisible(box);

        valueLabel.setColour(juce::Label::outlineColourId, box.findColour(juce::Slider::textBoxOutlineColourId));
        valueLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
        valueLabel.setBorderSize({ 1, 1, 1, 1 });
        valueLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(valueLabel);

        // Set the initial value.
        handleNewParameterValue();

        box.onValueChange = [this] { sliderValueChanged(); };
        box.onDragStart = [this] { sliderStartedDragging(); };
        box.onDragEnd = [this] { sliderStoppedDragging(); };
    }

        void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds().reduced(0, 10);

        valueLabel.setBounds(area.removeFromRight(80));

        area.removeFromLeft(6);
        box.setBounds(area);
    }

    void setLink(juce::Component& l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

private:
    void updateTextDisplay()
    {
        valueLabel.setText(getParameter().getCurrentValueAsText(), juce::dontSendNotification);
    }

    void handleNewParameterValue() override
    {
        if (!isDragging)
        {
            box.setValue(getParameter().getValue(), juce::dontSendNotification);
            updateTextDisplay();
        }
    }

    void sliderValueChanged()
    {
        auto newVal = (float)box.getValue();

        if (getParameter().getValue() != newVal)
        {
            if (!isDragging)
                getParameter().beginChangeGesture();

            getParameter().setValueNotifyingHost((float)box.getValue());
            updateTextDisplay();

            if (!isDragging)
                getParameter().endChangeGesture();
        }
    }

    void sliderStartedDragging()
    {
        isDragging = true;
        getParameter().beginChangeGesture();
    }

    void sliderStoppedDragging()
    {
        isDragging = false;
        getParameter().endChangeGesture();
    }

    juce::Component* link;
    juce::Slider box{ juce::Slider::IncDecButtons, juce::Slider::TextEntryBoxPosition::NoTextBox };
    juce::Label valueLabel;
    bool isDragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IncrementParameterComponent)
};
//
//==============================================================================
class ChoiceParameterComponent final : public juce::Component,
    private ParameterListener
{
public:
    ChoiceParameterComponent(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param)
        : ParameterListener(proc, param),
        parameterValues(getParameter().getAllValueStrings())
    {
        link = NULL;

        box.addItemList(parameterValues, 1);
        // Set the initial value.
        handleNewParameterValue();

        box.onChange = [this] { boxChanged(); };
        addAndMakeVisible(box);
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromLeft(8);
        box.setBounds(area.reduced(0, 10));
    }

    void setLink(juce::Component &l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

private:
    void handleNewParameterValue() override
    {
        auto index = parameterValues.indexOf(getParameter().getCurrentValueAsText());

        if (index < 0)
        {
            // The parameter is producing some unexpected text, so we'll do
            // some linear interpolation.
            index = juce::roundToInt(getParameter().getValue() * (float)(parameterValues.size() - 1));
        }

        box.setSelectedItemIndex(index);
    }

    void boxChanged()
    {
        if (getParameter().getCurrentValueAsText() != box.getText())
        {
            getParameter().beginChangeGesture();

            // When a parameter provides a list of strings we must set its
            // value using those strings, rather than a float, because VSTs can
            // have uneven spacing between the different allowed values.
            getParameter().setValueNotifyingHost(getParameter().getValueForText(box.getText()));

            getParameter().endChangeGesture();
        }
    }

    juce::Component* link;
    juce::ComboBox box;
    const juce::StringArray parameterValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChoiceParameterComponent)
};

//==============================================================================


//==============================================================================

class ParameterDisplayComponent : public juce::Component
{
public:
    ParameterDisplayComponent(juce::AudioProcessor& processor, juce::AudioProcessorParameter& param)
        : parameter(param)
    {
        link = NULL;

        const juce::Array<juce::AudioProcessorParameter*>& p = processor.getParameters();
        // substring removes first char indicator (for switch component, circular/horizontal slider etc)
        if(!parameter.isBoolean())
            parameterName.setText(parameter.getName(128).substring(1), juce::dontSendNotification);
            parameterName.setJustificationType(juce::Justification::centredRight);
            addAndMakeVisible(parameterName);
       
        parameterLabel.setText(parameter.getLabel(), juce::dontSendNotification);
        addAndMakeVisible(parameterLabel);

        //addAndMakeVisible(*(parameterComp = createParameterComp(processor)));
        parameterComp = createParameterComp(processor);
        //addAndMakeVisible(parameterComp.get()); 
        addChildAndSetID(parameterComp.get(),"ActualComponent");
        actualComp = parameterComp.get();

        setSize(400, 40);
    }

    void paint(juce::Graphics&) override {}

    void displayParameterName()
    {
        parameterName.setText(parameter.getName(128), juce::dontSendNotification);
        parameterName.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(parameterName);
    }
    void resized() override
    {
        auto area = getLocalBounds();

        parameterName.setBounds(area.removeFromLeft(100));
        parameterLabel.setBounds(area.removeFromRight(50));
        parameterComp->setBounds(area);
    }

    void setLink(juce::Component& l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

    template<typename T>
    T* getParameterComp()
    {
        if (dynamic_cast<T*>(parameterComp.get()) != nullptr)
        {
            return dynamic_cast<T*>(parameterComp.get());
        }
    }


private:
    juce::AudioProcessorParameter& parameter;
    juce::Label parameterName, parameterLabel;
    juce::Component* actualComp;
    juce::Component* link;
    std::unique_ptr<Component> parameterComp;

    std::unique_ptr<Component> createParameterComp(juce::AudioProcessor& processor) const
    {

        // The AU, AUv3 and VST (only via a .vstxml file) SDKs support
        // marking a parameter as boolean. If you want consistency across
        // all  formats then it might be best to use a
        // SwitchParameterComponent instead.
        if (parameter.isBoolean())
            if(parameter.getName(128).startsWithChar('b'))
                //indicates an on/off button switch, substring removes the 'B' button indicator in the parameterName
                return std::make_unique<BooleanButtonParameterComponent>(processor, parameter, parameter.getName(128).substring(1));
            else
                return std::make_unique<BooleanParameterComponent>(processor, parameter,parameter.getName(128));

        // Most hosts display any parameter with just two steps as a switch.
        if (parameter.getNumSteps() == 2 )
            return std::make_unique<SwitchParameterComponent>(processor, parameter);

        // If we have a list of strings to represent the different states a
        // parameter can be in then we should present a dropdown allowing a
        // user to pick one of them.
        if (!parameter.getAllValueStrings().isEmpty()
            && std::abs(parameter.getNumSteps() - parameter.getAllValueStrings().size()) <= 1)
            //return std::make_unique<ChoiceParameterComponent>(processor, parameter);
            return std::make_unique<SwitchParameterComponent>(processor, parameter);

        //for incrementing box
        if (parameter.getName(128).startsWithChar('i'))
            return std::make_unique<IncrementParameterComponent>(processor, parameter);

        // Everything else can be represented as a slider.
        return std::make_unique<SliderParameterComponent>(processor, parameter);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterDisplayComponent)
};

//==============================================================================
class ParametersPanel : public juce::Component
{
public:
    ParametersPanel(juce::AudioProcessor& processor, const juce::Array<juce::AudioProcessorParameter*>& parameters)
    {
        for (auto* param : parameters)
            if (param->isAutomatable())
                addChildAndSetID(paramComponents.add(new ParameterDisplayComponent(processor, *param)),param->getName(128)+"Comp");
        /*for (auto* param : parameters)
            if (param->isAutomatable())
                addAndMakeVisible(paramComponents.add(new ParameterDisplayComponent(processor, *param)));*/

        int maxWidth = 400;
        int height = 0;

        for (auto& comp : paramComponents)
        {
            maxWidth = juce::jmax(maxWidth, comp->getWidth());
            height += comp->getHeight();
        }

        setSize(maxWidth, juce::jmax(height, 125));
    }

    ~ParametersPanel() override
    {
        paramComponents.clear();
    }

    void addComponent(ParameterDisplayComponent* comp, juce::String ID)
    {
        addChildAndSetID(paramComponents.add(comp),ID);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto area = getLocalBounds();

        for (auto* comp : paramComponents)
            comp->setBounds(area.removeFromTop(comp->getHeight()));

        //x button1.setBounds(r.removeFromLeft100));
        //x button2.setBounds(r.removeFromTop(50));
        
    }

private:
    juce::OwnedArray<ParameterDisplayComponent> paramComponents;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParametersPanel)
};

//==============================================================================>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
class AarrowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AarrowLookAndFeel()
    {
        setColour(juce::Label::textColourId, juce::Colours::cyan);
        setColour(juce::Slider::thumbColourId, juce::Colours::antiquewhite);
        setColour(juce::Slider::trackColourId, (juce::Colours::cyan).withBrightness(0.8)); // You should really change the saturation on this..        setColour(juce::Label::textColourId, juce::Colours::cyan);

        setColour(juce::TextButton::textColourOnId, juce::Colours::cyan);
        setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);

        
        setColour(juce::ToggleButton::tickColourId, juce::Colours::orange);
       

    }
    ~AarrowLookAndFeel()
    {}
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AarrowLookAndFeel)
};

//void ParameterDisplayComponent::findChildWithID()
//{
//    return;
//}
// 
//=============================================================================

struct AarrowAudioProcessorEditor::Pimpl
{
    Pimpl(AarrowAudioProcessorEditor& parent) : owner(parent)
    {
        auto* p = parent.getAudioProcessor();
         jassert(p != nullptr);
        legacyParameters.update(*p, false);

        // p => AudioProcessor not 'NewProjectAudioProcessor' and loses all our unique variables
        // this sucks
        // so <maybe> instead I can manually add parameters to <LegacyAudioParameters> and link them immediately after
        owner.setOpaque(true);

        
        //paramComponents.add(new ParameterDisplayComponent(processor, *param))

        //you could make a loop for this...
        params.add(owner.audioProcessor.speed);
        params.add(owner.audioProcessor.sync);
        params.add(owner.audioProcessor.velocity);
        params.add(owner.audioProcessor.rest);
        params.add(owner.audioProcessor.latch);
        params.add(owner.audioProcessor.octaves);
        params.add(owner.audioProcessor.direction);

        ParametersPanel* myPanel = new ParametersPanel(owner.audioProcessor, params);

        /*ParameterDisplayComponent* SyncComp = new ParameterDisplayComponent(owner.audioProcessor, *owner.audioProcessor.sync);
        ParameterDisplayComponent* SpeedComp = new ParameterDisplayComponent(owner.audioProcessor, *owner.audioProcessor.speed);
        ParameterDisplayComponent* VelocityComp = new ParameterDisplayComponent(owner.audioProcessor, *owner.audioProcessor.velocity);
        ParameterDisplayComponent* RestComp = new ParameterDisplayComponent(owner.audioProcessor, *owner.audioProcessor.rest);
        ParameterDisplayComponent* LatchComp = new ParameterDisplayComponent(owner.audioProcessor, *owner.audioProcessor.latch);
        ParameterDisplayComponent* OctavesComp = new ParameterDisplayComponent(owner.audioProcessor, *owner.audioProcessor.octaves);
        ParameterDisplayComponent* DirectionComp = new ParameterDisplayComponent(owner.audioProcessor, *owner.audioProcessor.direction);*/

        //SyncComp->getParameterComp();
        
        //myParametersPanel* myPanel = new myParametersPanel(owner.audioProcessor, ParameterDisplayComponentArray);
        
        //addAndMakeVisible(paramComponents.add(new ParameterDisplayComponent(owner.audioProcessor, *param)));
        for (auto* comp : myPanel->getChildren())
            auto pie = comp->getComponentID();
            //attach breakpoint if you need help checking componentID's
        //dynamic_cast<LegacyAudioParameter*> 
        ParameterDisplayComponent* SyncComp = dynamic_cast<ParameterDisplayComponent*>(myPanel->findChildWithID("bBPM LinkComp"));
        ParameterDisplayComponent* SpeedComp = dynamic_cast<ParameterDisplayComponent*>(myPanel->findChildWithID("-SpeedComp"));
      /*  for (auto* prm : params)
            myPanel->addComponent(new ParameterDisplayComponent(owner.audioProcessor, *prm), prm->getName(128) + "Comp");*/
        SyncComp->getParameterComp<BooleanButtonParameterComponent>()->setLink( *SpeedComp->findChildWithID("ActualComponent"));
      
        //std::unique_ptr<Component> a = myPanel->findChildWithID("bBPM LinkComp")->findChildWithID("ActualComponent");
        //view.setViewedComponent(new ParametersPanel(owner.audioProcessor, params));
        //view.setViewedComponent(newParametersPanel(*p, legacyParameters.params));

        view.setViewedComponent(myPanel);
        owner.addAndMakeVisible(view);

        // now link the parameters and edit freely (?)  //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
        view.setScrollBarsShown(true, false);
    }

    ~Pimpl()
    {
        view.setViewedComponent(nullptr, false);
    }

    void resize(juce::Rectangle<int> size)
    {
        view.setBounds(size);
        auto content = view.getViewedComponent();
        content->setSize(view.getMaximumVisibleWidth(), content->getHeight());
    }

    //==============================================================================
    AarrowAudioProcessorEditor& owner;
    juce::OwnedArray<ParameterDisplayComponent> paramComponents;
    juce::Array<juce::AudioProcessorParameter*> params;
    LegacyAudioParametersWrapper legacyParameters;
    juce::Viewport view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Pimpl)
};
//==============================================================================
AarrowAudioProcessorEditor::AarrowAudioProcessorEditor (NewProjectAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), pimpl(new Pimpl(*this))
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    //setSize (400, 300);

    //addAndMakeVisible(new ParameterDisplayComponent(processor,*audioProcessor.sync));//////>>>>>>>>>>>>>>>>>>>>>>>>>>>

    AarrowLookAndFeel* Aalf = new AarrowLookAndFeel();
    setLookAndFeel(Aalf);
    setSize(pimpl->view.getViewedComponent()->getWidth() + pimpl->view.getVerticalScrollBar().getWidth(),
        juce::jmin(pimpl->view.getViewedComponent()->getHeight(), 400));


    
}

AarrowAudioProcessorEditor::~AarrowAudioProcessorEditor()
{
}

//==============================================================================
void AarrowAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

   /* g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Please Work", getLocalBounds(), juce::Justification::centred, 1);*/
}

void AarrowAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    pimpl->resize(getLocalBounds());
}

//===================================================================================


