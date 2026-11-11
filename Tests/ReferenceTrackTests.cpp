#include "TestSignals.h"
#include "dsp/LoudnessMeter.h"
#include "ref/ReferenceLoader.h"

using namespace lucky;

class LoopRegionTests final : public juce::UnitTest
{
public:
    LoopRegionTests() : juce::UnitTest ("Loop region", "Lucky Reference") {}

    void runTest() override
    {
        constexpr double rate = 1000.0;

        beginTest ("Unset loop covers the whole track");
        expect (LoopRegion {}.resolve (10000, rate) == juce::Range<juce::int64> (0, 10000));

        beginTest ("A loop inside the track is kept");
        expect ((LoopRegion { 2.0, 6.5 }.resolve (10000, rate)) == juce::Range<juce::int64> (2000, 6500));

        beginTest ("A short loop grows to the minimum length");
        expect ((LoopRegion { 3.0, 3.5 }.resolve (10000, rate)) == juce::Range<juce::int64> (3000, 5000));

        beginTest ("A short loop at the end grows backwards");
        expect ((LoopRegion { 9.5, 9.9 }.resolve (10000, rate)) == juce::Range<juce::int64> (8000, 10000));

        beginTest ("A loop past the end is clamped");
        expect ((LoopRegion { 4.0, 30.0 }.resolve (10000, rate)) == juce::Range<juce::int64> (4000, 10000));

        beginTest ("A track shorter than the minimum loops whole");
        expect ((LoopRegion { 0.2, 0.4 }.resolve (1500, rate)) == juce::Range<juce::int64> (0, 1500));
    }
};

class ReferenceTrackTests final : public juce::UnitTest
{
public:
    ReferenceTrackTests() : juce::UnitTest ("Reference track", "Lucky Reference") {}

    void runTest() override
    {
        formats.registerBasicFormats();
        const auto folder = test::scratchFolder ("ReferenceTrack");

        beginTest ("Mono files play on both channels");
        {
            const auto file = folder.getChildFile ("mono.wav");
            test::writeWav (file, test::sine (-12.0f, 1.0, 48000.0, 1), 48000.0);
            const auto track = load (file, 48000.0);
            expect (track != nullptr);
            expectEquals (track->getAudio().getNumChannels(), 2);
            expectEquals (track->getLength(), juce::int64 { 48000 });
            expectEquals (track->getAudio().getSample (1, 1234), track->getAudio().getSample (0, 1234));
        }

        beginTest ("Files with more than two channels use the first two");
        {
            juce::AudioBuffer<float> quad (4, 4800);
            for (int ch = 0; ch < 4; ++ch)
                juce::FloatVectorOperations::fill (quad.getWritePointer (ch), 0.1f * static_cast<float> (ch + 1), 4800);
            const auto file = folder.getChildFile ("quad.wav");
            test::writeWav (file, quad, 48000.0);
            const auto track = load (file, 48000.0);
            expect (track != nullptr);
            expectWithinAbsoluteError (track->getAudio().getSample (0, 100), 0.1f, 1.0e-4f);
            expectWithinAbsoluteError (track->getAudio().getSample (1, 100), 0.2f, 1.0e-4f);
        }

        beginTest ("Files are resampled to the host rate without shifting in time or changing loudness");
        {
            auto click = test::sine (-20.0f, 10.0, 44100.0);
            click.setSample (0, 44100, 1.0f);
            click.setSample (1, 44100, 1.0f);
            const auto file = folder.getChildFile ("44k.wav");
            test::writeWav (file, click, 44100.0);
            const auto track = load (file, 48000.0);
            expect (track != nullptr);
            expect (std::abs (track->getLength() - 480000) <= 1, "length " + juce::String (track->getLength()));

            int peak = 47000;
            for (int i = 47000; i < 49000; ++i)
                if (track->getAudio().getSample (0, i) > track->getAudio().getSample (0, peak))
                    peak = i;
            expect (std::abs (peak - 48000) <= 2, "click moved to " + juce::String (peak));
            expectWithinAbsoluteError (track->getLoudness ({}), -20.0f, 0.15f);
        }

        beginTest ("Long files are capped");
        {
            const auto file = folder.getChildFile ("long.wav");
            test::writeWav (file, test::sine (-20.0f, 3.0, 48000.0), 48000.0);
            juce::String error;
            const auto track = ReferenceTrack::load (file, 48000.0, formats, error, 1.0);
            expect (track != nullptr);
            expectEquals (track->getLength(), juce::int64 { 48000 });
        }

        beginTest ("Missing and unreadable files report an error");
        {
            juce::String error;
            expect (ReferenceTrack::load (folder.getChildFile ("nothing.wav"), 48000.0, formats, error) == nullptr);
            expectEquals (error, juce::String ("File not found"));

            const auto junk = folder.getChildFile ("junk.wav");
            junk.replaceWithText ("not audio");
            expect (ReferenceTrack::load (junk, 48000.0, formats, error) == nullptr);
            expectEquals (error, juce::String ("Can't read this file"));
        }

        beginTest ("AAC files decode");
        {
            const auto wav = folder.getChildFile ("source.wav");
            const auto m4a = folder.getChildFile ("encoded.m4a");
            test::writeWav (wav, test::sine (-18.0f, 4.0, 44100.0), 44100.0);
            juce::ChildProcess encoder;
            const auto started = encoder.start (juce::StringArray { "/usr/bin/afconvert", "-f", "m4af", "-d", "aac",
                                                                     wav.getFullPathName(), m4a.getFullPathName() });
            expect (started && encoder.waitForProcessToFinish (30000) && m4a.existsAsFile(), "afconvert failed");
            const auto track = load (m4a, 48000.0);
            expect (track != nullptr);
            if (track != nullptr)
            {
                expect (std::abs (track->getLengthSeconds() - 4.0) < 0.1, "length " + juce::String (track->getLengthSeconds()));
                expectWithinAbsoluteError (track->getLoudness ({}), -18.0f, 0.3f);
            }
        }

        beginTest ("Loop loudness measures only the loop");
        {
            const auto quietThenLoud = test::concatenate (test::sine (-30.0f, 10.0, 48000.0), test::sine (-15.0f, 10.0, 48000.0));
            const ReferenceTrack track ({}, quietThenLoud, 48000.0);
            expectWithinAbsoluteError (track.getLoudness ({ 10.0, 20.0 }), -15.0f, 0.1f);
            expectWithinAbsoluteError (track.getLoudness ({ 0.0, 10.0 }), -30.0f, 0.1f);
            expectWithinAbsoluteError (track.getLoudness ({}), LoudnessMeter::measure (quietThenLoud, 0, quietThenLoud.getNumSamples(), 48000.0), 0.05f);
        }

        beginTest ("The overview follows the waveform");
        {
            const auto quietThenLoud = test::concatenate (test::sine (-40.0f, 1.0, 48000.0), test::sine (-6.0f, 1.0, 48000.0));
            const ReferenceTrack track ({}, quietThenLoud, 48000.0);
            const auto& overview = track.getOverview();
            expectEquals (static_cast<int> (overview.size()), ReferenceTrack::overviewBuckets);
            expect (overview.front() < 0.02f);
            expectWithinAbsoluteError (overview.back(), 0.5f, 0.01f);
        }
    }

private:
    ReferenceTrackPtr load (const juce::File& file, double rate)
    {
        juce::String error;
        auto track = ReferenceTrack::load (file, rate, formats, error);
        expect (track != nullptr, error);
        return track;
    }

    juce::AudioFormatManager formats;
};

class ReferenceLoaderTests final : public juce::UnitTest
{
public:
    ReferenceLoaderTests() : juce::UnitTest ("Reference loader", "Lucky Reference") {}

    void runTest() override
    {
        const auto folder = test::scratchFolder ("ReferenceLoader");
        const auto first = folder.getChildFile ("first.wav"), second = folder.getChildFile ("second.wav");
        test::writeWav (first, test::sine (-20.0f, 2.0, 48000.0), 48000.0);
        test::writeWav (second, test::sine (-20.0f, 3.0, 48000.0), 48000.0);

        ReferenceLoader loader;

        beginTest ("A request loads on the background thread");
        {
            loader.request (1, first, 48000.0);
            const auto result = waitFor (loader, 1);
            expect (result && result->track != nullptr && result->file == first);
            if (result && result->track)
                expectEquals (result->track->getLength(), juce::int64 { 96000 });
            expect (! loader.isLoading (1));
            expect (! loader.takeResult (1).has_value());
        }

        beginTest ("Only the newest request for a slot is delivered");
        {
            loader.request (0, first, 48000.0);
            loader.request (0, second, 48000.0);
            const auto result = waitFor (loader, 0);
            expect (result && result->file == second);
            if (result && result->track)
                expectEquals (result->track->getLength(), juce::int64 { 144000 });
        }

        beginTest ("An empty file clears the slot, and failures carry an error");
        {
            loader.request (2, {}, 48000.0);
            const auto cleared = waitFor (loader, 2);
            expect (cleared && cleared->track == nullptr && cleared->error.isEmpty());

            loader.request (2, folder.getChildFile ("missing.wav"), 48000.0);
            const auto failed = waitFor (loader, 2);
            expect (failed && failed->track == nullptr && failed->error.isNotEmpty());
        }
    }

private:
    static std::optional<ReferenceLoader::Result> waitFor (ReferenceLoader& loader, int slot)
    {
        for (int attempt = 0; attempt < 500; ++attempt)
        {
            if (auto result = loader.takeResult (slot))
                return result;
            juce::Thread::sleep (10);
        }
        return std::nullopt;
    }
};

static LoopRegionTests loopRegionTests;
static ReferenceTrackTests referenceTrackTests;
static ReferenceLoaderTests referenceLoaderTests;
