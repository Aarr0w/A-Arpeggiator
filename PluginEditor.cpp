/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


//=======================================================================================
class ParameterListener : private juce::AudioProcessorParameter::Listener,
        private juce::AudioProcessorListener,
        private juce::Timer
    {
    public:
        ParameterListener(juce::AudioProcessor& proc, juce::AudioProcessorParameter& param)
            : processor(proc), parameter(param)/*, isLegacyParam(LegacyAudioParameter::isLegacy(&param))*/
        {
          /* if (isLegacyParam)
                processor.addListener(this);
           else*/
               parameter.addListener(this);

           startTimer(100);
        }

        ~ParameterListener() override
        {
           /* if (isLegacyParam)
                processor.removeListener(this);
            else*/
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
        //const bool isLegacyParam;

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
        //slider.setValue((int)getParameter().getDefaultValue());
        getParameter().setValue(getParameter().getDefaultValue());
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
        getParameter().setValue(getParameter().getDefaultValue());
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
        button.setBounds(area.reduced(0, 8)); // (0,10)
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
            if(link)
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
        getParameter().setValue(getParameter().getDefaultValue());
        handleNewParameterValue();
        button.onClick = [this] { buttonClicked(); };
        addAndMakeVisible(button);
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds();
        //area.removeFromLeft(8);
        button.setBounds(area.reduced(0, 10));  //(0,10)
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

            b->setToggleState(false, juce::dontSendNotification);
            b->onClick = [this,i] { aButtonChanged(i); };

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
        aButtonChanged(0);
        //handleNewParameterValue();
 

        for (auto& button : buttons)
            addAndMakeVisible(button);
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds().reduced(0, 8); // (0,8)
        //area.removeFromLeft(8);

        for (int i = 0; i < n; ++i)
        {
            buttons[i]->setBounds(area.removeFromLeft( (getWidth()/n)  )); //60
        }
  
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
        int newState = getCurrentState();
        
        for (int i = 0; i < buttons.size(); i++)
        {
            //auto state = (i == newState) ? false : true;
            if (i == newState && (buttons[i]->getToggleState() == false))
            {
                buttons[i]->setToggleState(true, juce::dontSendNotification);
            }
            
        }

  
    }

    void rightButtonChanged()
    {
 
    }

    void aButtonChanged(int i)
    {
        auto buttonState = (*buttons[i]).getToggleState();

        if (getCurrentState() != i)
        {
            getParameter().beginChangeGesture();

            if (getParameter().getAllValueStrings().isEmpty())
            {
                getParameter().setValueNotifyingHost(float(i));
            }
            else
            {
                // When a parameter provides a list of strings we must set its
                // value using those strings, rather than a float, because VSTs can
                // have uneven spacing between the different allowed values and we
                // want the snapping behaviour to be consistent with what we do with
                // a combo box.
                auto selectedText = (*buttons[i]).getButtonText();
                getParameter().setValueNotifyingHost(getParameter().getValueForText(selectedText));
                auto jim = (int)(getParameter().getValueForText(selectedText));
            }

            getParameter().endChangeGesture();
        }
    }

    int getCurrentState()
    {
        return (int) (getParameter().getAllValueStrings()
            .indexOf(getParameter().getCurrentValueAsText()));
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

        getParameter().setValue(getParameter().getDefaultValue());

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

        valueLabel.setBounds(area.removeFromRight(80)); //80

        area.removeFromLeft(20);
        area.removeFromRight(20);
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
        box.setBounds(area.reduced(0, 0)); // (0,10)
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
    ParameterDisplayComponent(juce::AudioProcessor& processor, juce::AudioProcessorParameter& param, int wdth)
        : parameter(param), paramWidth(wdth)
    {
        link = NULL;

        const juce::Array<juce::AudioProcessorParameter*>& p = processor.getParameters();
        // substring removes first char indicator (for switch component, circular/horizontal slider etc)
        if(!parameter.isBoolean() && parameter.getAllValueStrings().size() <2 )
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

        //setSize(400, 40);
        setSize(paramWidth , 40);
        
    }

    ~ParameterDisplayComponent() override
    {}

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

        //parameterName.setBounds(area.removeFromLeft(100));
        parameterName.setBounds(area.removeFromLeft(getWidth()/4));
        //parameterLabel.setBounds(area.removeFromRight(50));
        if(paramWidth == 400) // basically... if parentpanel is horizontal
            parameterLabel.setBounds(area.removeFromRight(getWidth()/8));
        parameterComp->setBounds(area);
    }

    void setLink(juce::Component& l)
    {
        link = &l;
    }

    void linkAction()
    {
    }

    template<typename A>
    A* getParameterComp()
    {
        if (dynamic_cast<A*>(parameterComp.get()) != nullptr)
        {
            return dynamic_cast<A*>(parameterComp.get());
        }
    }


  

private:
    juce::AudioProcessorParameter& parameter;
    juce::Label parameterName, parameterLabel;
    juce::Component* actualComp;
    juce::Component* link;
    int paramWidth;
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
    ParametersPanel(juce::AudioProcessor& processor, const juce::Array<juce::AudioProcessorParameter*> parameters, bool hrzntl)
        : horizontal(hrzntl)
    {
        if(horizontal)
            paramWidth = 400 / parameters.size();
            paramHeight = 40;

        for (auto* param : parameters)
            if (param->isAutomatable())
                addChildAndSetID(paramComponents.add(new ParameterDisplayComponent(processor, *param, paramWidth)),param->getName(128)+"Comp");
        
        //allComponents.addArray(paramComponents);   this line causes exception error... idk why but it's not being deleted properly
        for (auto* param : parameters)    // have to do the loop again as a fix... still looking for a cleaner solution 
            if (param->isAutomatable())
                allComponents.add(new ParameterDisplayComponent(processor, *param, paramWidth));

        maxWidth = 400;
        height = 0;
        if (!horizontal)
        {
            for (auto& comp : paramComponents)
            {
                maxWidth = juce::jmax(maxWidth, comp->getWidth());
                height += comp->getHeight();
            }
        }
        else
        {
            height += 40;
        }
        setSize(maxWidth, juce::jmax(height, 40));
        
    }

    ~ParametersPanel() override
    {
        allComponents.clear();
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

    bool isHorizontal()
    { return horizontal; }

    void resized() override
    {
        auto area = getLocalBounds();

        if (horizontal)
        {
            auto row = area.removeFromTop(40);
            for (auto* comp : paramComponents)   // change to allComponents if you start stacking panels horizontally 
                comp->setBounds(row.removeFromLeft(paramWidth));
        }
        else
        {
            for (auto* comp : allComponents)
                comp->setBounds(area.removeFromTop(comp->getHeight()));
        }
        
        
    }

    void addPanel(ParametersPanel* p)
    {   
        allComponents.add(p);
        addAndMakeVisible(p);
        setSize(maxWidth, getHeight() + p->getHeight()); 
        //auto area = getLocalBounds();
        //p->setBounds(area.removeFromBottom(p->getHeight()));
       
    }

public :
    int height;
    int maxWidth;
    int paramWidth=400;
    int paramHeight=40;
    juce::OwnedArray<ParameterDisplayComponent> paramComponents;
    juce::OwnedArray<Component> allComponents;

private:  
    bool horizontal;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParametersPanel)
};

//==============================================================================>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>


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
        /*auto* p = parent.getAudioProcessor();
         jassert(p != nullptr);
        legacyParameters.update(*p, false);*/

        owner.setOpaque(true);

        
        //paramComponents.add(new ParameterDisplayComponent(processor, *param))

        //you could make a loop for this...
        params.add(owner.audioProcessor.speed);
 


        ParametersPanel* myPanel = new ParametersPanel(owner.audioProcessor, params,false);

        params.clear();
        params.add(owner.audioProcessor.sync);
        params.add(owner.audioProcessor.dot);
        params.add(owner.audioProcessor.trip);
        ParametersPanel* Panel2 = new ParametersPanel(owner.audioProcessor, params, true);
        myPanel->addPanel(Panel2);

        params.clear();
        params.add(owner.audioProcessor.octaves);
        ParametersPanel* Panel3 = new ParametersPanel(owner.audioProcessor, params, false);
        myPanel->addPanel(Panel3);

        params.clear();
        params.add(owner.audioProcessor.direction);
        params.add(owner.audioProcessor.turn);
        ParametersPanel* Panel4 = new ParametersPanel(owner.audioProcessor, params, true);
        myPanel->addPanel(Panel4);

        params.clear();
        params.add(owner.audioProcessor.prob);
        ParametersPanel* Panel5 = new ParametersPanel(owner.audioProcessor, params, false);
        myPanel->addPanel(Panel5);

        for (auto* comp : myPanel->getChildren())
            auto pie = comp->getComponentID();
            //attach breakpoint if you need help checking componentID's

        ParameterDisplayComponent* SyncComp = dynamic_cast<ParameterDisplayComponent*>(Panel2->findChildWithID("bBPM LinkComp"));
        ParameterDisplayComponent* SpeedComp = dynamic_cast<ParameterDisplayComponent*>(myPanel->findChildWithID("-SpeedComp"));
        SyncComp->getParameterComp<BooleanButtonParameterComponent>()->setLink( *SpeedComp->findChildWithID("ActualComponent"));
        
        params.clear();
        view.setViewedComponent(myPanel);
        owner.addAndMakeVisible(view);

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
    juce::Array<juce::AudioProcessorParameter*> params;
    //LegacyAudioParametersWrapper legacyParameters;
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

    //AarrowLookAndFeel* Aalf = new AarrowLookAndFeel();
    setLookAndFeel(&Aalf);
    setSize(pimpl->view.getViewedComponent()->getWidth() + pimpl->view.getVerticalScrollBar().getWidth(),
        juce::jmin(pimpl->view.getViewedComponent()->getHeight(), 400));


    
}

AarrowAudioProcessorEditor::~AarrowAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
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


