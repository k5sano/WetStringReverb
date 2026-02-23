#include "PluginEditor.h"

WetStringReverbEditor::WetStringReverbEditor (WetStringReverbProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Dark colour scheme matching the navy background
    auto scheme = juce::LookAndFeel_V4::getDarkColourScheme();
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::windowBackground,  juce::Colour (0xff1a1a2e));
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::widgetBackground,  juce::Colour (0xff22223a));
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::menuBackground,    juce::Colour (0xff22223a));
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::outline,           juce::Colour (0xff333355));
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::defaultText,       juce::Colour (0xffddddee));
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::defaultFill,       juce::Colour (0xff7777bb));
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::highlightedText,   juce::Colours::white);
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::highlightedFill,   juce::Colour (0xff7777bb));
    darkLookAndFeel.setColourScheme (scheme);
    setLookAndFeel (&darkLookAndFeel);

    // Section accent colours
    const juce::Colour mainColour (0xff4fc3f7);   // light blue
    const juce::Colour reverbColour (0xff7777bb);  // purple
    const juce::Colour satColour (0xffef5350);     // red
    const juce::Colour modColour (0xff66bb6a);     // green

    // ---- MAIN knobs ----
    setupKnob (dryWetKnob,      Parameters::DRY_WET,        "Dry/Wet",     mainColour);
    setupKnob (preDelayKnob,    Parameters::PRE_DELAY_MS,   "Pre-Delay",   mainColour);
    setupKnob (earlyLevelKnob,  Parameters::EARLY_LEVEL_DB, "Early Lvl",   mainColour);
    setupKnob (lateLevelKnob,   Parameters::LATE_LEVEL_DB,  "Late Lvl",    mainColour);
    setupKnob (roomSizeKnob,    Parameters::ROOM_SIZE,      "Room Size",   mainColour);
    setupKnob (widthKnob,       Parameters::STEREO_WIDTH,   "Width",       mainColour);

    // ---- REVERB knobs ----
    setupKnob (lowRT60Knob,     Parameters::LOW_RT60_S,     "Low RT60",    reverbColour);
    setupKnob (highRT60Knob,    Parameters::HIGH_RT60_S,    "High RT60",   reverbColour);
    setupKnob (hfDampKnob,      Parameters::HF_DAMPING,     "HF Damp",     reverbColour);
    setupKnob (diffusionKnob,   Parameters::DIFFUSION,      "Diffusion",   reverbColour);
    setupKnob (decayShapeKnob,  Parameters::DECAY_SHAPE,    "Decay Shape", reverbColour);

    // ---- SATURATION knobs ----
    setupKnob (satAmountKnob,    Parameters::SAT_AMOUNT,    "Amount",      satColour);
    setupKnob (satDriveKnob,     Parameters::SAT_DRIVE_DB,  "Drive",       satColour);
    setupKnob (satToneKnob,      Parameters::SAT_TONE,      "Tone",        satColour);
    setupKnob (satAsymmetryKnob, Parameters::SAT_ASYMMETRY, "Asymmetry",   satColour);

    // ---- MODULATION knobs ----
    setupKnob (modDepthKnob, Parameters::MOD_DEPTH,   "Depth", modColour);
    setupKnob (modRateKnob,  Parameters::MOD_RATE_HZ, "Rate",  modColour);

    // ---- Combo boxes ----
    setupChoice (oversamplingChoice, Parameters::OVERSAMPLING, "OS");
    setupChoice (satTypeChoice,      Parameters::SAT_TYPE,     "Type");

    // ---- Bypass toggles ----
    setupToggle (bypassEarly,       Parameters::BYPASS_EARLY,        "Early");
    setupToggle (bypassFDN,         Parameters::BYPASS_FDN,          "FDN");
    setupToggle (bypassDVN,         Parameters::BYPASS_DVN,          "DVN");
    setupToggle (bypassSaturation,  Parameters::BYPASS_SATURATION,   "Saturation");
    setupToggle (bypassToneFilter,  Parameters::BYPASS_TONE_FILTER,  "Tone Filt");
    setupToggle (bypassAttenFilter, Parameters::BYPASS_ATTEN_FILTER, "Atten Filt");
    setupToggle (bypassModulation,  Parameters::BYPASS_MODULATION,   "Modulation");

    // ---- Buttons ----
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (imageButton);

    saveButton.onClick  = [this] { savePreset(); };
    loadButton.onClick  = [this] { loadPreset(); };
    imageButton.onClick = [this] { loadBackgroundImage(); };

    // Restore background image from saved state
    restoreBackgroundImage();

    setSize (900, 600);
}

WetStringReverbEditor::~WetStringReverbEditor()
{
    setLookAndFeel (nullptr);
}

// ==============================================================================
void WetStringReverbEditor::setupKnob (KnobWithLabel& k, const juce::String& paramId,
                                        const juce::String& labelText,
                                        juce::Colour knobFill)
{
    addAndMakeVisible (k.slider);
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, knobFill);
    k.slider.setColour (juce::Slider::thumbColourId, knobFill.brighter (0.3f));

    addAndMakeVisible (k.label);
    k.label.setText (labelText, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, juce::Colour (0xffff9933));  // orange
    k.label.setFont (juce::FontOptions (18.0f));

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, paramId, k.slider);
}

void WetStringReverbEditor::setupChoice (ChoiceWithLabel& c, const juce::String& paramId,
                                          const juce::String& labelText)
{
    addAndMakeVisible (c.combo);

    addAndMakeVisible (c.label);
    c.label.setText (labelText, juce::dontSendNotification);
    c.label.setJustificationType (juce::Justification::centred);
    c.label.setColour (juce::Label::textColourId, juce::Colour (0xffff9933));  // orange
    c.label.setFont (juce::FontOptions (18.0f));

    c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.apvts, paramId, c.combo);
}

void WetStringReverbEditor::setupToggle (ToggleWithLabel& t, const juce::String& paramId,
                                          const juce::String& labelText)
{
    addAndMakeVisible (t.toggle);
    t.toggle.setButtonText (labelText);

    t.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, paramId, t.toggle);
}

// ==============================================================================
void WetStringReverbEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));

    // Background image at 25% opacity
    if (backgroundImage.isValid())
    {
        g.setOpacity (0.25f);
        g.drawImage (backgroundImage, getLocalBounds().toFloat(),
                     juce::RectanglePlacement::stretchToFit);
        g.setOpacity (1.0f);
    }

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (22.0f);
    g.drawText ("WetStringReverb", 20, 8, 300, 30, juce::Justification::centredLeft);

    g.setFont (11.0f);
    g.setColour (juce::Colour (0xff666688));
    g.drawText ("3-Layer Hybrid Reverb", 20, 28, 200, 16, juce::Justification::centredLeft);

    // Section backgrounds & labels
    auto drawSection = [&] (int y, int h, const juce::String& title)
    {
        g.setColour (juce::Colour (0x10ffffff));
        g.fillRoundedRectangle (8.0f, (float) y, (float) (getWidth() - 16), (float) h, 6.0f);
        g.setColour (juce::Colour (0xff7777bb));
        g.setFont (12.0f);
        g.drawText (title, 16, y + 2, 120, 16, juce::Justification::centredLeft);
    };

    drawSection (48,  114, "MAIN");
    drawSection (166, 114, "REVERB");
    drawSection (284, 114, "SATURATION");
    drawSection (402, 114, "MOD / BYPASS");
}

// ==============================================================================
void WetStringReverbEditor::resized()
{
    // ---- Title-bar buttons ----
    constexpr int btnW = 58, btnH = 26, btnY = 10;
    imageButton.setBounds (getWidth() - btnW - 12,           btnY, btnW, btnH);
    loadButton .setBounds (getWidth() - btnW * 2 - 18,       btnY, btnW, btnH);
    saveButton .setBounds (getWidth() - btnW * 3 - 24,       btnY, btnW, btnH);

    // ---- Layout helpers ----
    constexpr int pad = 10;
    const int usableW = getWidth() - pad * 2;

    auto placeKnob = [] (KnobWithLabel& k, int x, int y, int w, int h)
    {
        constexpr int labelH = 22;
        k.label.setBounds (x, y + h - labelH, w, labelH);
        k.slider.setBounds (juce::Rectangle<int> (x, y, w, h - labelH - 1).reduced (4, 0));
    };

    auto placeChoice = [] (ChoiceWithLabel& c, int x, int y, int w, int h)
    {
        constexpr int labelH = 22;
        constexpr int comboH = 24;
        c.label.setBounds (x, y + h - labelH, w, labelH);
        int comboY = y + (h - labelH - comboH) / 2;
        c.combo.setBounds (x + 8, comboY, w - 16, comboH);
    };

    // ---- Row 1: MAIN  (y=48..162, content starts at y=66) ----
    {
        constexpr int rowY = 66, rowH = 90;
        int n = 7;
        int cellW = usableW / n;
        int x0 = pad;
        placeKnob   (dryWetKnob,         x0 + cellW * 0, rowY, cellW, rowH);
        placeKnob   (preDelayKnob,       x0 + cellW * 1, rowY, cellW, rowH);
        placeKnob   (earlyLevelKnob,     x0 + cellW * 2, rowY, cellW, rowH);
        placeKnob   (lateLevelKnob,      x0 + cellW * 3, rowY, cellW, rowH);
        placeKnob   (roomSizeKnob,       x0 + cellW * 4, rowY, cellW, rowH);
        placeKnob   (widthKnob,          x0 + cellW * 5, rowY, cellW, rowH);
        placeChoice (oversamplingChoice,  x0 + cellW * 6, rowY, cellW, rowH);
    }

    // ---- Row 2: REVERB  (y=166..280, content at y=184) ----
    {
        constexpr int rowY = 184, rowH = 90;
        int n = 5;
        int cellW = usableW / n;
        int x0 = pad;
        placeKnob (lowRT60Knob,    x0 + cellW * 0, rowY, cellW, rowH);
        placeKnob (highRT60Knob,   x0 + cellW * 1, rowY, cellW, rowH);
        placeKnob (hfDampKnob,     x0 + cellW * 2, rowY, cellW, rowH);
        placeKnob (diffusionKnob,  x0 + cellW * 3, rowY, cellW, rowH);
        placeKnob (decayShapeKnob, x0 + cellW * 4, rowY, cellW, rowH);
    }

    // ---- Row 3: SATURATION  (y=284..398, content at y=302) ----
    {
        constexpr int rowY = 302, rowH = 90;
        int n = 5;
        int cellW = usableW / n;
        int x0 = pad;
        placeKnob   (satAmountKnob,    x0 + cellW * 0, rowY, cellW, rowH);
        placeKnob   (satDriveKnob,     x0 + cellW * 1, rowY, cellW, rowH);
        placeChoice (satTypeChoice,     x0 + cellW * 2, rowY, cellW, rowH);
        placeKnob   (satToneKnob,      x0 + cellW * 3, rowY, cellW, rowH);
        placeKnob   (satAsymmetryKnob, x0 + cellW * 4, rowY, cellW, rowH);
    }

    // ---- Row 4: MOD + BYPASS  (y=402..516, content at y=420) ----
    {
        constexpr int rowY = 420, rowH = 90;
        int modCellW = usableW / 5;
        int x0 = pad;

        // Mod knobs (left)
        placeKnob (modDepthKnob, x0,            rowY, modCellW, rowH);
        placeKnob (modRateKnob,  x0 + modCellW, rowY, modCellW, rowH);

        // Bypass toggles (right) — 2 rows: 4 top, 3 bottom
        int toggleX = x0 + modCellW * 2 + 16;
        int toggleAreaW = getWidth() - pad - toggleX;
        int halfH = rowH / 2;

        int topW = toggleAreaW / 4;
        int botW = toggleAreaW / 3;

        bypassEarly     .toggle.setBounds (toggleX + topW * 0, rowY,          topW, halfH);
        bypassFDN       .toggle.setBounds (toggleX + topW * 1, rowY,          topW, halfH);
        bypassDVN       .toggle.setBounds (toggleX + topW * 2, rowY,          topW, halfH);
        bypassSaturation.toggle.setBounds (toggleX + topW * 3, rowY,          topW, halfH);

        bypassToneFilter .toggle.setBounds (toggleX + botW * 0, rowY + halfH, botW, halfH);
        bypassAttenFilter.toggle.setBounds (toggleX + botW * 1, rowY + halfH, botW, halfH);
        bypassModulation .toggle.setBounds (toggleX + botW * 2, rowY + halfH, botW, halfH);
    }
}

// ==============================================================================
void WetStringReverbEditor::savePreset()
{
    fileChooser = std::make_shared<juce::FileChooser> (
        "Save Preset", juce::File(), "*.xml");

    auto safeThis = juce::Component::SafePointer<WetStringReverbEditor> (this);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis] (const juce::FileChooser& fc)
        {
            if (safeThis == nullptr) return;

            auto file = fc.getResult();
            if (file == juce::File()) return;

            if (! file.hasFileExtension (".xml"))
                file = file.withFileExtension ("xml");

            auto state = safeThis->processorRef.apvts.copyState();
            std::unique_ptr<juce::XmlElement> xml (state.createXml());
            file.replaceWithText (xml->toString());
        });
}

void WetStringReverbEditor::loadPreset()
{
    fileChooser = std::make_shared<juce::FileChooser> (
        "Load Preset", juce::File(), "*.xml");

    auto safeThis = juce::Component::SafePointer<WetStringReverbEditor> (this);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis] (const juce::FileChooser& fc)
        {
            if (safeThis == nullptr) return;

            auto file = fc.getResult();
            if (file == juce::File() || ! file.existsAsFile()) return;

            juce::XmlDocument doc (file);
            auto xml = doc.getDocumentElement();

            if (xml != nullptr)
            {
                auto tree = juce::ValueTree::fromXml (*xml);
                if (tree.isValid())
                {
                    safeThis->processorRef.apvts.replaceState (tree);
                    safeThis->restoreBackgroundImage();
                    safeThis->repaint();
                }
            }
        });
}

void WetStringReverbEditor::loadBackgroundImage()
{
    fileChooser = std::make_shared<juce::FileChooser> (
        "Load Background Image", juce::File(), "*.png;*.jpg;*.jpeg;*.gif;*.bmp");

    auto safeThis = juce::Component::SafePointer<WetStringReverbEditor> (this);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis] (const juce::FileChooser& fc)
        {
            if (safeThis == nullptr) return;

            auto file = fc.getResult();
            if (file == juce::File() || ! file.existsAsFile()) return;

            safeThis->backgroundImage = juce::ImageCache::getFromFile (file);
            safeThis->processorRef.apvts.state.setProperty (
                "backgroundImagePath", file.getFullPathName(), nullptr);
            safeThis->repaint();
        });
}

void WetStringReverbEditor::restoreBackgroundImage()
{
    auto path = processorRef.apvts.state
                    .getProperty ("backgroundImagePath", "").toString();

    if (path.isNotEmpty())
    {
        juce::File imgFile (path);
        if (imgFile.existsAsFile())
            backgroundImage = juce::ImageCache::getFromFile (imgFile);
        else
            backgroundImage = {};
    }
    else
    {
        backgroundImage = {};
    }
}
