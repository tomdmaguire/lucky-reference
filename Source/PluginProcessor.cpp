#include "PluginProcessor.h"
#include "PluginEditor.h"

LuckyReferenceProcessor::LuckyReferenceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "LuckyReference", createLayout())
{
    referenceParameter = parameters.getRawParameterValue (referenceId);
    slotParameter = parameters.getRawParameterValue (slotId);
    matchParameter = parameters.getRawParameterValue (matchId);
    engine.prepare (48000.0, 512);
    loader.addChangeListener (this);
    startTimerHz (2);
}

LuckyReferenceProcessor::~LuckyReferenceProcessor()
{
    loader.removeChangeListener (this);
}

juce::AudioProcessorValueTreeState::ParameterLayout LuckyReferenceProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { referenceId, 1 }, "Reference", false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool on, int) { return on ? "Ref" : "Mix"; })));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { slotId, 1 }, "Slot", juce::StringArray { "1", "2", "3" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { matchId, 1 }, "Match", true));
    return layout;
}

void LuckyReferenceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto rateChanged = std::abs (sampleRate - loadingRate()) > 0.5;
    preparedRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);

    if (rateChanged)
        requestAll();
}

void LuckyReferenceProcessor::releaseResources() {}

bool LuckyReferenceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void LuckyReferenceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (isNonRealtime())
        return;

    lucky::ReferenceEngine::Transport transport;
    if (auto* head = getPlayHead())
        if (const auto position = head->getPosition())
        {
            transport.isPlaying = position->getIsPlaying();
            if (const auto time = position->getTimeInSamples())
                transport.timeInSamples = *time;
            if (const auto ppq = position->getPpqPosition())
                transport.ppqPosition = *ppq;
            if (const auto bpm = position->getBpm())
                transport.bpm = *bpm;
            if (position->getIsLooping())
                if (const auto loop = position->getLoopPoints())
                    transport.loopPpq = std::pair (loop->ppqStart, loop->ppqEnd);
        }

    const lucky::ReferenceEngine::Controls controls { referenceParameter->load() > 0.5f, juce::roundToInt (slotParameter->load()), matchParameter->load() > 0.5f };
    engine.process (buffer, controls, transport);
}

juce::AudioProcessorEditor* LuckyReferenceProcessor::createEditor()
{
    return new LuckyReferenceEditor (*this);
}

juce::ValueTree LuckyReferenceProcessor::slotState (int index)
{
    auto& state = parameters.state;
    for (auto child : state)
        if (child.hasType (slotType) && static_cast<int> (child[indexProperty]) == index)
            return child;

    juce::ValueTree child (slotType);
    child.setProperty (indexProperty, index, nullptr);
    state.appendChild (child, nullptr);
    return child;
}

juce::ValueTree LuckyReferenceProcessor::slotState (int index) const
{
    for (auto child : parameters.state)
        if (child.hasType (slotType) && static_cast<int> (child[indexProperty]) == index)
            return child;
    return {};
}

juce::File LuckyReferenceProcessor::getFile (int index) const
{
    const auto path = slotState (index)[pathProperty].toString();
    return path.isNotEmpty() ? juce::File (path) : juce::File();
}

lucky::LoopRegion LuckyReferenceProcessor::getLoop (int index) const
{
    const auto state = slotState (index);
    return { static_cast<double> (state.getProperty (loopStartProperty, 0.0)), static_cast<double> (state.getProperty (loopEndProperty, 0.0)) };
}

void LuckyReferenceProcessor::loadFile (int index, const juce::File& file)
{
    auto state = slotState (index);
    state.setProperty (pathProperty, file.getFullPathName(), nullptr);
    state.setProperty (loopStartProperty, 0.0, nullptr);
    state.setProperty (loopEndProperty, 0.0, nullptr);
    tracks[static_cast<size_t> (index)] = nullptr;
    errors[static_cast<size_t> (index)] = {};
    autoLoopPending[static_cast<size_t> (index)] = file != juce::File();
    engine.setSlot (index, nullptr, {});
    loader.request (index, file, loadingRate());
    sendChangeMessage();
}

void LuckyReferenceProcessor::switchToMix()
{
    if (referenceParameter->load() > 0.5f)
        parameters.getParameter (referenceId)->setValueNotifyingHost (0.0f);
}

void LuckyReferenceProcessor::clearSlot (int index)
{
    loadFile (index, {});
    slotState (index).removeProperty (pathProperty, nullptr);
}

void LuckyReferenceProcessor::setLoop (int index, const lucky::LoopRegion& loop)
{
    auto state = slotState (index);
    state.setProperty (loopStartProperty, loop.startSeconds, nullptr);
    state.setProperty (loopEndProperty, loop.endSeconds, nullptr);
    engine.setSlot (index, tracks[static_cast<size_t> (index)], loop);
    sendChangeMessage();
}

void LuckyReferenceProcessor::requestAll()
{
    for (int i = 0; i < numSlots; ++i)
        loader.request (i, getFile (i), loadingRate());
}

void LuckyReferenceProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (int i = 0; i < numSlots; ++i)
        if (auto result = loader.takeResult (i))
        {
            const auto index = static_cast<size_t> (i);
            tracks[index] = result->track;
            errors[index] = result->error;

            if (result->track != nullptr && std::exchange (autoLoopPending[index], false))
            {
                const auto loop = result->track->findLoudestSection (autoLoopSeconds);
                auto state = slotState (i);
                state.setProperty (loopStartProperty, loop.startSeconds, nullptr);
                state.setProperty (loopEndProperty, loop.endSeconds, nullptr);
            }

            engine.setSlot (i, result->track, getLoop (i));
        }
    sendChangeMessage();
}

void LuckyReferenceProcessor::timerCallback()
{
    engine.collectGarbage();
}

void LuckyReferenceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LuckyReferenceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    parameters.replaceState (juce::ValueTree::fromXml (*xml));
    for (auto& track : tracks)
        track = nullptr;
    for (auto& error : errors)
        error = {};
    autoLoopPending = {};
    for (int i = 0; i < numSlots; ++i)
        engine.setSlot (i, nullptr, {});
    requestAll();
    sendChangeMessage();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LuckyReferenceProcessor();
}
