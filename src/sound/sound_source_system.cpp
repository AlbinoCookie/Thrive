#include "sound_source_system.h"

#include "engine/component_factory.h"
#include "engine/entity_filter.h"
#include "engine/engine.h"
#include "engine/serialization.h"
#include "ogre/scene_node_system.h"
#include "scripting/luabind.h"

#include <OgreOggISound.h>
#include <OgreOggSoundManager.h>

using namespace thrive;

////////////////////////////////////////////////////////////////////////////////
// SoundSourceComponent
////////////////////////////////////////////////////////////////////////////////

luabind::scope
SoundSourceComponent::luaBindings() {
    using namespace luabind;
    return class_<SoundSourceComponent, Component>("SoundSourceComponent")
        .enum_("ID") [
            value("TYPE_ID", SoundSourceComponent::TYPE_ID)
        ]
        .scope [
            def("TYPE_NAME", &SoundSourceComponent::TYPE_NAME),
            class_<Properties, Touchable>("Properties")
                .def_readwrite("playState", &Properties::playState)
                .def_readwrite("startTime", &Properties::startTime)
                .def_readwrite("volume", &Properties::volume)
                .def_readwrite("maxVolume", &Properties::maxVolume)
                .def_readwrite("minVolume", &Properties::minVolume)
                .def_readwrite("insideAngle", &Properties::insideAngle)
                .def_readwrite("outsideAngle", &Properties::outsideAngle)
                .def_readwrite("outerConeVolume", &Properties::outerConeVolume)
                .def_readwrite("maxDistance", &Properties::maxDistance)
                .def_readwrite("rolloffFactor", &Properties::rolloffFactor)
                .def_readwrite("referenceDistance", &Properties::referenceDistance)
                .def_readwrite("pitch", &Properties::pitch)
                .def_readwrite("relativeToListener", &Properties::relativeToListener)
                .def_readwrite("priority", &Properties::priority)
        ]
        .enum_("PlayState") [
            value("Play", PlayState::Play),
            value("Pause", PlayState::Pause),
            value("Stop", PlayState::Stop)
        ]
        .def(constructor<std::string, std::string, bool, bool, bool>())
        .def("pause", &SoundSourceComponent::pause)
        .def("play", &SoundSourceComponent::play)
        .def("stop", &SoundSourceComponent::stop)
    ;
}


SoundSourceComponent::SoundSourceComponent(
    std::string name,
    std::string filename,
    bool stream,
    bool loop,
    bool prebuffer
) : m_filename(filename),
    m_loop(loop),
    m_name(name),
    m_prebuffer(prebuffer),
    m_stream(stream)
{
}


SoundSourceComponent::SoundSourceComponent()
  : SoundSourceComponent("", "", false, false, false)
{
}


bool
SoundSourceComponent::doesPrebuffer() const {
    return m_prebuffer;
}


std::string
SoundSourceComponent::filename() const {
    return m_filename;
}


bool
SoundSourceComponent::isLoop() const {
    return m_loop;
}


bool
SoundSourceComponent::isStream() const {
    return m_stream;
}


void
SoundSourceComponent::load(
    const StorageContainer& storage
) {
    Component::load(storage);
    m_filename = storage.get<std::string>("filename");
    m_loop = storage.get<bool>("loop");
    m_name = storage.get<std::string>("name");
    m_prebuffer = storage.get<bool>("prebuffer");
    m_stream = storage.get<bool>("stream");
    m_properties.playState = static_cast<PlayState>(
        storage.get<int16_t>("playState", PlayState::Stop)
    );
    m_properties.startTime = storage.get<float>("startTime");
    m_properties.volume = storage.get<float>("volume");
    m_properties.maxVolume = storage.get<float>("maxVolume");
    m_properties.minVolume = storage.get<float>("minVolume");
    m_properties.insideAngle = storage.get<float>("insideAngle", 360.0f);
    m_properties.outsideAngle = storage.get<float>("outsideAngle", 360.0f);
    m_properties.outerConeVolume = storage.get<float>("outerConeVolume");
    m_properties.maxDistance = storage.get<float>("maxDistance", -1.0f);
    m_properties.rolloffFactor = storage.get<float>("rolloffFactor", -1.0f);
    m_properties.referenceDistance = storage.get<float>("referenceDistance", 100.0f);
    m_properties.pitch = storage.get<float>("pitch", 1.0f);
    m_properties.relativeToListener = storage.get<bool>("relativeToListener");
    m_properties.priority = storage.get<uint8_t>("priority");
}


std::string
SoundSourceComponent::name() const {
    return m_name;
}


void
SoundSourceComponent::play() {
    m_properties.playState = PlayState::Play;
    m_properties.touch();
}


void
SoundSourceComponent::pause() {
    m_properties.playState = PlayState::Pause;
    m_properties.touch();
}


void
SoundSourceComponent::stop() {
    m_properties.playState = PlayState::Stop;
    m_properties.touch();
}


StorageContainer
SoundSourceComponent::storage() const {
    StorageContainer storage = Component::storage();
    storage.set("filename", m_filename);
    storage.set("loop", m_loop);
    storage.set("name", m_name);
    storage.set("prebuffer", m_prebuffer);
    storage.set("stream", m_stream);
    storage.set<int16_t>("playState", m_properties.playState);
    storage.set("startTime", m_properties.startTime);
    storage.set("volume", m_properties.volume);
    storage.set("maxVolume", m_properties.maxVolume);
    storage.set("minVolume", m_properties.minVolume);
    storage.set("insideAngle", m_properties.insideAngle);
    storage.set("outsideAngle", m_properties.outsideAngle);
    storage.set("outerConeVolume", m_properties.outerConeVolume);
    storage.set("maxDistance", m_properties.maxDistance);
    storage.set("rolloffFactor", m_properties.rolloffFactor);
    storage.set("referenceDistance", m_properties.referenceDistance);
    storage.set("pitch", m_properties.pitch);
    storage.set("relativeToListener", m_properties.relativeToListener);
    storage.set("priority", m_properties.priority);
    return storage;
}

REGISTER_COMPONENT(SoundSourceComponent)


////////////////////////////////////////////////////////////////////////////////
// SoundSourceSystem
////////////////////////////////////////////////////////////////////////////////

struct SoundSourceSystem::Implementation {

    void
    removeAllSounds() {
        for (const auto& item : m_entities) {
            this->removeSound(item.first);
        }
    }

    void
    removeSound(
        EntityId entityId
    ) {
        auto& soundManager = OgreOggSound::OgreOggSoundManager::getSingleton();
        OgreOggSound::OgreOggISound* sound = m_sounds[entityId];
        if (sound) {
            Ogre::SceneNode* sceneNode = sound->getParentSceneNode();
            sceneNode->detachObject(sound);
            soundManager.destroySound(sound);
        }
        m_sounds.erase(entityId);
    }

    void
    restoreAllSounds() {
        for (const auto& item : m_entities) {
            EntityId entityId = item.first;
            OgreSceneNodeComponent* sceneNodeComponent = std::get<0>(item.second);
            SoundSourceComponent* soundSourceComponent = std::get<1>(item.second);
            this->restoreSound(entityId, sceneNodeComponent, soundSourceComponent);
        }
    }

    void
    restoreSound(
        EntityId entityId,
        OgreSceneNodeComponent* sceneNodeComponent,
        SoundSourceComponent* soundSourceComponent
    ) {
        if (not sceneNodeComponent->m_sceneNode) {
            return;
        }
        auto& soundManager = OgreOggSound::OgreOggSoundManager::getSingleton();
        OgreOggSound::OgreOggISound* sound = soundManager.createSound(
            soundSourceComponent->name(),
            soundSourceComponent->filename(),
            soundSourceComponent->isStream(),
            soundSourceComponent->isLoop(),
            soundSourceComponent->doesPrebuffer()
        );
        if (sound) {
            soundSourceComponent->m_sound = sound;
            m_sounds[entityId] = sound;
            sceneNodeComponent->m_sceneNode->attachObject(sound);
        }
        else {
            //TODO: Log error. Or does OgreOggSound do this already?
        }
    }
    
    EntityFilter<OgreSceneNodeComponent, SoundSourceComponent> m_entities = {true};

    std::unordered_map<EntityId, OgreOggSound::OgreOggISound*> m_sounds;

};


SoundSourceSystem::SoundSourceSystem()
  : m_impl(new Implementation())
{

}


SoundSourceSystem::~SoundSourceSystem() {}


void
SoundSourceSystem::activate() {
    System::activate();
    auto& soundManager = OgreOggSound::OgreOggSoundManager::getSingleton();
    soundManager.setSceneManager(this->gameState()->sceneManager());
    m_impl->restoreAllSounds();
}


void
SoundSourceSystem::deactivate() {
    System::deactivate();
    auto& soundManager = OgreOggSound::OgreOggSoundManager::getSingleton();
    m_impl->removeAllSounds();
    soundManager.setSceneManager(nullptr);
}


void
SoundSourceSystem::init(
    GameState* gameState
) {
    System::init(gameState);
    m_impl->m_entities.setEntityManager(&gameState->entityManager());
}


void
SoundSourceSystem::shutdown() {
    m_impl->m_entities.setEntityManager(nullptr);
    System::shutdown();
}


void
SoundSourceSystem::update(int) {
    for (EntityId entityId : m_impl->m_entities.removedEntities()) {
        m_impl->removeSound(entityId);
    }
    for (auto& value : m_impl->m_entities.addedEntities()) {
        EntityId entityId = value.first;
        OgreSceneNodeComponent* sceneNodeComponent = std::get<0>(value.second);
        SoundSourceComponent* soundSourceComponent = std::get<1>(value.second);
        m_impl->restoreSound(entityId, sceneNodeComponent, soundSourceComponent);
    }
    m_impl->m_entities.clearChanges();
    for (auto& value : m_impl->m_entities) {
        SoundSourceComponent* soundSourceComponent = std::get<1>(value.second);
        if (soundSourceComponent->m_properties.hasChanges()) {
            const auto& properties = soundSourceComponent->m_properties;
            OgreOggSound::OgreOggISound* sound = soundSourceComponent->m_sound;
            sound->setLoopOffset(properties.startTime);
            sound->setVolume(properties.volume);
            sound->setMaxVolume(properties.maxVolume);
            sound->setMinVolume(properties.minVolume);
            sound->setConeAngles(properties.insideAngle, properties.outsideAngle);
            sound->setOuterConeVolume(properties.outerConeVolume);
            sound->setMaxDistance(properties.maxDistance);
            sound->setRolloffFactor(properties.rolloffFactor);
            sound->setReferenceDistance(properties.referenceDistance);
            sound->setPitch(properties.pitch);
            sound->setRelativeToListener(properties.relativeToListener);
            sound->setPriority(properties.priority);
            switch(properties.playState) {
                case SoundSourceComponent::PlayState::Play:
                    sound->play();
                    break;
                case SoundSourceComponent::PlayState::Pause:
                    sound->pause();
                    break;
                case SoundSourceComponent::PlayState::Stop:
                    sound->stop();
                    break;
                default:
                    // Shut up GCC
                    break;
            }
            soundSourceComponent->m_properties.untouch();
        }
    }
}

