#pragma once

#include "engine/component.h"
#include "engine/system.h"
#include "engine/touchable.h"

#include <memory>

namespace luabind {
class scope;
}

namespace OgreOggSound {
class OgreOggISound;
}

namespace thrive {

/**
* @brief A component for a camera
*
*/
class SoundSourceComponent : public Component {
    COMPONENT(SoundSource)

public:

    enum PlayState {
        Play,
        Pause,
        Stop
    };

    /**
    * @brief Properties
    */
    struct Properties : public Touchable {
        PlayState playState = PlayState::Stop;
        float startTime = 0.0f;
        float volume = 1.0f;
        float maxVolume = 1.0f;
        float minVolume = 0.0f;
        float insideAngle = 360.0f;
        float outsideAngle = 360.0f;
        float outerConeVolume = 0.0f;
        float maxDistance = -1.0f;
        float rolloffFactor = -1.0f;
        float referenceDistance = 100.0f;
        float pitch = 1.0f;
        bool relativeToListener = false;
        uint8_t priority = 0;
    };

    static luabind::scope
    luaBindings();

    SoundSourceComponent(
        std::string name,
        std::string filename,
        bool stream,
        bool loop,
        bool prebuffer
    );

    SoundSourceComponent();

    bool
    doesPrebuffer() const;

    std::string
    filename() const;

    bool
    isLoop() const;

    bool
    isStream() const;

    void
    load(
        const StorageContainer& storage
    ) override;

    std::string
    name() const;

    void 
    pause();

    void 
    play();

    void 
    stop();

    StorageContainer
    storage() const override;

    /**
    * @brief Pointer to internal sound
    */
    OgreOggSound::OgreOggISound* m_sound = nullptr;

    /**
    * @brief Properties
    */
    Properties
    m_properties;

private:

    std::string m_filename;

    bool m_loop = false;

    std::string m_name;

    bool m_prebuffer = false;

    bool m_stream = false;

};


/**
* @brief Creates, updates and removes sounds
*/
class SoundSourceSystem : public System {
    
public:

    /**
    * @brief Constructor
    */
    SoundSourceSystem();

    /**
    * @brief Destructor
    */
    ~SoundSourceSystem();

    /**
    * @brief Initializes the system
    *
    * @param engine
    *   Must be an OgreEngine
    */
    void init(Engine* engine) override;

    /**
    * @brief Shuts the system down
    */
    void shutdown() override;

    /**
    * @brief Updates the system
    */
    void update(int) override;

private:

    struct Implementation;
    std::unique_ptr<Implementation> m_impl;
};

}


