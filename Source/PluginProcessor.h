#pragma once

#include "dsp/ReferenceEngine.h"
#include "ref/ReferenceLoader.h"
#include <juce_audio_processors/juce_audio_processors.h>

class LuckyReferenceProcessor final : public juce::AudioProcessor,
                                      public juce::ChangeBroadcaster,
                                      private juce::ChangeListener,
                                      private juce::Timer
{
public:
    static constexpr int numSlots = lucky::ReferenceEngine::numSlots;
    static inline const juce::String referenceId { "reference" };
    static inline const juce::String slotId { "slot" };
    static inline const juce::String matchId { "match" };
    static inline const juce::Identifier slotType { "Slot" };
    static inline const juce::Identifier indexProperty { "index" };
    static inline const juce::Identifier pathProperty { "path" };
    static inline const juce::Identifier loopStartProperty { "loopStart" };
    static inline const juce::Identifier loopEndProperty { "loopEnd" };
    static constexpr double autoLoopSeconds = 16.0;

    LuckyReferenceProcessor();
    ~LuckyReferenceProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Loads a file into a slot (0-2), replacing what was there; once decoded, its loudest autoLoopSeconds become the loop. */
    void loadFile (int slot, const juce::File& file);

    /** Empties a slot. */
    void clearSlot (int slot);

    /** Sets a slot's loop; an unset loop plays the whole track. */
    void setLoop (int slot, const lucky::LoopRegion& loop);

    /** Moves a slot's playhead to a time in its track. */
    void seek (int slot, double seconds) { engine.seek (slot, seconds); }

    /** The file chosen for a slot, which may be missing or still loading. */
    juce::File getFile (int slot) const;

    /** The slot's loop. */
    lucky::LoopRegion getLoop (int slot) const;

    /** The decoded track in a slot, or nullptr while empty, loading or failed. */
    lucky::ReferenceTrackPtr getTrack (int slot) const { return tracks[static_cast<size_t> (slot)]; }

    /** Why the slot's file couldn't be loaded, or empty. */
    juce::String getError (int slot) const { return errors[static_cast<size_t> (slot)]; }

    /** True while a slot's file is being decoded. */
    bool isLoading (int slot) const { return loader.isLoading (slot); }

    const lucky::ReferenceEngine& getEngine() const noexcept { return engine; }
    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    juce::ValueTree slotState (int slot);
    juce::ValueTree slotState (int slot) const;
    void requestAll();
    double loadingRate() const noexcept { return preparedRate > 0.0 ? preparedRate : 48000.0; }

    juce::AudioProcessorValueTreeState parameters;
    lucky::ReferenceLoader loader;
    lucky::ReferenceEngine engine;
    std::array<lucky::ReferenceTrackPtr, numSlots> tracks;
    std::array<juce::String, numSlots> errors;
    std::array<bool, numSlots> autoLoopPending {};
    std::atomic<float>* referenceParameter = nullptr;
    std::atomic<float>* slotParameter = nullptr;
    std::atomic<float>* matchParameter = nullptr;
    double preparedRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckyReferenceProcessor)
};
