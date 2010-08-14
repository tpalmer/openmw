#ifndef GAME_SOUND_SOUNDMANAGER_H
#define GAME_SOUND_SOUNDMANAGER_H

#include <string>
#include <map>

#include "../mwworld/ptr.hpp"

namespace Ogre
{
    class Root;
    class Camera;
}

namespace MWSound
{
    class SoundManager
    {
            // Hide implementation details - engine.cpp is compiling
            // enough as it is.
            struct SoundImpl;
            SoundImpl *mData;

        public:
            SoundManager(Ogre::Root*, Ogre::Camera*);
            ~SoundManager();

            void say (MWWorld::Ptr reference, const std::string& filename);
            ///< Make an actor say some text.
            /// \param filename name of a sound file in "Sound/Vo/" in the data directory.

            bool sayDone (MWWorld::Ptr reference) const;
            ///< Is actor not speaking?

            void streamMusic (const std::string& filename);
            ///< Play a soundifle
            /// \param filename name of a sound file in "Music/" in the data directory.

            void playSound (const std::string& soundId, float volume, float pitch);
            ///< Play a sound, independently of 3D-position

            void playSound3D (MWWorld::Ptr reference, const std::string& soundId,
                float volume, float pitch, bool loop);
            ///< Play a sound from an object

            void stopSound3D (MWWorld::Ptr reference, const std::string& soundId = "");
            ///< Stop the given object from playing the given sound, If no soundId is given,
            /// all sounds for this reference will stop.

            bool getSoundPlaying (MWWorld::Ptr reference, const std::string& soundId) const;
            ///< Is the given sound currently playing on the given object?
    };
}

#endif
