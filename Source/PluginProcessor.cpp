#include "PluginProcessor.h"
#include "PluginEditor.h"

LuckyReferenceProcessor::LuckyReferenceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void LuckyReferenceProcessor::prepareToPlay (double, int) {}

void LuckyReferenceProcessor::releaseResources() {}

bool LuckyReferenceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void LuckyReferenceProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) {}

juce::AudioProcessorEditor* LuckyReferenceProcessor::createEditor()
{
    return new LuckyReferenceEditor (*this);
}

void LuckyReferenceProcessor::getStateInformation (juce::MemoryBlock&) {}

void LuckyReferenceProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LuckyReferenceProcessor();
}
