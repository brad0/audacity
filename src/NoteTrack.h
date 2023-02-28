/**********************************************************************

  Audacity: A Digital Audio Editor

  NoteTrack.h

  Dominic Mazzoni

**********************************************************************/

#ifndef __AUDACITY_NOTETRACK__
#define __AUDACITY_NOTETRACK__

#include <utility>
#include "AudioIOSequences.h"
#include "Prefs.h"
#include "PlayableTrack.h"

#if defined(USE_MIDI)

// define this switch to play MIDI during redisplay to sonify run times
// Note that if SONIFY is defined, the default MIDI device will be opened
// and may block normal MIDI playback.
//#define SONIFY 1

#ifdef SONIFY

#define SONFNS(name) \
   void Begin ## name(); \
   void End ## name();

SONFNS(NoteBackground)
SONFNS(NoteForeground)
SONFNS(Measures)
SONFNS(Serialize)
SONFNS(Unserialize)
SONFNS(ModifyState)
SONFNS(AutoSave)

#undef SONFNS

#endif

class wxDC;
class wxRect;

class Alg_seq;   // from "allegro.h"

using NoteTrackBase =
#ifdef EXPERIMENTAL_MIDI_OUT
   PlayableTrack
#else
   AudioTrack
#endif
   ;

using QuantizedTimeAndBeat = std::pair< double, double >;

class NoteTrack;
class StretchHandle;
class TimeWarper;

using NoteTrackAttachment = ClientData::Cloneable<>;

using NoteTrackAttachments = ClientData::Site<
   NoteTrack,
   NoteTrackAttachment,
   ClientData::DeepCopying
>;

class AUDACITY_DLL_API NoteTrack final
   : public UniqueChannelTrack<NoteTrackBase>
   , public OtherPlayableSequence
   , public NoteTrackAttachments
{
public:
   using Attachments = NoteTrackAttachments;
   static EnumSetting<bool> AllegroStyleSetting;

   // Construct and also build all attachments
   static NoteTrack *New(AudacityProject &project);

   NoteTrack();
   //! Copy construction hasn't been necessary yet
   NoteTrack(const NoteTrack &orig) = delete;
   NoteTrack(const NoteTrack &orig, ProtectedCreationArg &&) = delete;
   virtual ~NoteTrack();

   using Holder = std::shared_ptr<NoteTrack>;

private:
   TrackListHolder Clone() const override;

public:
   void MoveTo(double origin) override { mOrigin = origin; }

   Alg_seq &GetSeq() const;

   void WarpAndTransposeNotes(double t0, double t1,
                              const TimeWarper &warper, double semitones);

   void SetSequence(std::unique_ptr<Alg_seq> &&seq);
   void PrintSequence();

   Alg_seq *MakeExportableSeq(std::unique_ptr<Alg_seq> &cleanup) const;
   bool ExportMIDI(const wxString &f) const;
   bool ExportAllegro(const wxString &f) const;

   // High-level editing
   TrackListHolder Cut(double t0, double t1) override;
   TrackListHolder Copy(double t0, double t1, bool forClipboard = true)
      const override;
   bool Trim (double t0, double t1) /* not override */;
   void Clear(double t0, double t1) override;
   void Paste(double t, const Track &src) override;
   void
   Silence(double t0, double t1, ProgressReporter reportProgress = {}) override;
   void InsertSilence(double t, double len) override;
   bool Shift(double t) /* not override */;

#ifdef EXPERIMENTAL_MIDI_OUT
   float GetVelocity() const {
      return mVelocity.load(std::memory_order_relaxed); }
   void SetVelocity(float velocity);
#endif

   QuantizedTimeAndBeat NearestBeatTime( double time ) const;
   bool StretchRegion
      ( QuantizedTimeAndBeat t0, QuantizedTimeAndBeat t1, double newDur );

   /// Gets the current bottom note (a pitch)
   int GetBottomNote() const { return mBottomNote; }
   /// Gets the current top note (a pitch)
   int GetTopNote() const { return mTopNote; }
   /// Sets the bottom note (a pitch), making sure that it is never greater than the top note.
   void SetBottomNote(int note);
   /// Sets the top note (a pitch), making sure that it is never less than the bottom note.
   void SetTopNote(int note);
   /// Sets the top and bottom note (both pitches) automatically, swapping them if needed.
   void SetNoteRange(int note1, int note2) const;

   /// Zooms so that all notes are visible
   void ZoomAllNotes();
   /// Zooms so that the entire track is visible
   void ZoomMaxExtent() { SetNoteRange(MinPitch, MaxPitch); }
   /// Shifts all notes vertically by the given pitch
   void ShiftNoteRange(int offset);

#if 0
   // Vertical scrolling is performed by dragging the keyboard at
   // left of track. Protocol is call StartVScroll, then update by
   // calling VScroll with original and final mouse position.
   // These functions are not used -- instead, zooming/dragging works like
   // audio track zooming/dragging. The vertical scrolling is nice however,
   // so I left these functions here for possible use in the future.
   void StartVScroll();
   void VScroll(int start, int end);
#endif

   bool HandleXMLTag(const std::string_view& tag, const AttributesList& attrs) override;
   XMLTagHandler *HandleXMLChild(const std::string_view& tag) override;
   void WriteXML(XMLWriter &xmlFile) const override;

   // channels are numbered as integers 0-15, visible channels
   // (mVisibleChannels) is a bit set. Channels are displayed as
   // integers 1-16.

   // Allegro's data structure does not restrict channels to 16.
   // Since there is not way to select more than 16 channels,
   // map all channel numbers mod 16. This will have no effect
   // on MIDI files, but it will allow users to at least select
   // all channels on non-MIDI event sequence data.
#define NUM_CHANNELS 16
   // Bitmask with all NUM_CHANNELS bits set
#define ALL_CHANNELS (1 << NUM_CHANNELS) - 1
#define CHANNEL_BIT(c) (1 << (c % NUM_CHANNELS))
   unsigned GetVisibleChannels() const {
      return mVisibleChannels.load(std::memory_order_relaxed);
   }
   void SetVisibleChannels(unsigned value) {
      mVisibleChannels.store(value, std::memory_order_relaxed);
   }
   bool IsVisibleChan(int c) const {
      return (GetVisibleChannels() & CHANNEL_BIT(c)) != 0;
   }
   void SetVisibleChan(int c) {
      mVisibleChannels.fetch_or(CHANNEL_BIT(c), std::memory_order_relaxed); }
   void ClearVisibleChan(int c) {
      mVisibleChannels.fetch_and(~CHANNEL_BIT(c), std::memory_order_relaxed); }
   void ToggleVisibleChan(int c) {
      mVisibleChannels.fetch_xor(CHANNEL_BIT(c), std::memory_order_relaxed); }
   // Solos the given channel.  If it's the only channel visible, all channels
   // are enabled; otherwise, it is set to the only visible channel.
   void SoloVisibleChan(int c) {
      auto visibleChannels = 0u;
      if (GetVisibleChannels() == CHANNEL_BIT(c))
         visibleChannels = ALL_CHANNELS;
      else
         visibleChannels = CHANNEL_BIT(c);
      mVisibleChannels.store(visibleChannels, std::memory_order_relaxed);
   }

   const TypeInfo &GetTypeInfo() const override;
   static const TypeInfo &ClassTypeInfo();

   Track::Holder PasteInto(AudacityProject &project, TrackList &list)
      const override;

   size_t NIntervals() const override;

   struct Interval : WideChannelGroupInterval {
      using WideChannelGroupInterval::WideChannelGroupInterval;
      ~Interval() override;
      std::shared_ptr<ChannelInterval> DoGetChannel(size_t iChannel) override;
   };

private:
   std::shared_ptr<WideChannelGroupInterval> DoGetInterval(size_t iInterval)
      override;

#ifdef EXPERIMENTAL_MIDI_OUT
   void DoSetVelocity(float velocity);
#endif

   void AddToDuration( double delta );
   void DoOnProjectTempoChange(
      const std::optional<double>& oldTempo, double newTempo) override;

   // These are mutable to allow NoteTrack to switch details of representation
   // in logically const methods
   // At most one of the two pointers is not null at any time.
   // Both are null in a newly constructed NoteTrack.
   mutable std::unique_ptr<Alg_seq> mSeq;
   mutable std::unique_ptr<char[]> mSerializationBuffer;
   mutable long mSerializationLength;

#ifdef EXPERIMENTAL_MIDI_OUT
   //! Atomic because it may be read by worker threads in playback
   std::atomic<float> mVelocity{ 0.0f }; // velocity offset
#endif

   mutable int mBottomNote, mTopNote;
#if 0
   // Also unused from vertical scrolling
   int mStartBottomNote;
#endif

   enum { MinPitch = 0, MaxPitch = 127 };

   //! A bit set; atomic because it may be read by worker threads in playback
   std::atomic<unsigned> mVisibleChannels{ ALL_CHANNELS };
   double mOrigin{ 0.0 };
};

extern AUDACITY_DLL_API StringSetting MIDIPlaybackDevice;
extern AUDACITY_DLL_API StringSetting MIDIRecordingDevice;
extern AUDACITY_DLL_API IntSetting MIDISynthLatency_ms;

ENUMERATE_TRACK_TYPE(NoteTrack);

#endif // USE_MIDI

#ifndef SONIFY
// no-ops:
#define SonifyBeginSonification()
#define SonifyEndSonification()
#define SonifyBeginNoteBackground()
#define SonifyEndNoteBackground()
#define SonifyBeginNoteForeground()
#define SonifyEndNoteForeground()
#define SonifyBeginMeasures()
#define SonifyEndMeasures()
#define SonifyBeginSerialize()
#define SonifyEndSerialize()
#define SonifyBeginUnserialize()
#define SonifyEndUnserialize()
#define SonifyBeginAutoSave()
#define SonifyEndAutoSave()
#define SonifyBeginModifyState()
#define SonifyEndModifyState()
#endif


AUDACITY_DLL_API wxString GetMIDIDeviceInfo();

#endif
