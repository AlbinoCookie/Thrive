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
// Sound
////////////////////////////////////////////////////////////////////////////////

luabind::scope
Sound::luaBindings() {
    using namespace luabind;
    return class_<Sound>("Sound")
        .scope [
            class_<Properties, Touchable>("Properties")
                .def_readwrite("playState", &Properties::playState)
                .def_readwrite("loop", &Properties::loop)
                .def_readwrite("volume", &Properties::volume)
                .def_readwrite("maxDistance", &Properties::maxDistance)
                .def_readwrite("rolloffFactor", &Properties::rolloffFactor)
                .def_readwrite("referenceDistance", &Properties::referenceDistance)
                .def_readwrite("priority", &Properties::priority)
        ]
        .enum_("PlayState") [
            value("Play", PlayState::Play),
            value("Pause", PlayState::Pause),
            value("Stop", PlayState::Stop)
        ]
        .def(constructor<std::string, std::string>())
        .def("name", &Sound::name)
        .def("pause", &Sound::pause)
        .def("play", &Sound::play)
        .def("stop", &Sound::stop)
        .def_readonly("properties", &Sound::m_properties)
    ;
}


Sound::Sound()
  : Sound("", "")
{
}


Sound::Sound(
    std::string name,
    std::string filename
) : m_filename(filename),
    m_name(name)
{
}


std::string
Sound::filename() const {
    return m_filename;
}


void
Sound::load(
    const StorageContainer& storage
) {
    m_filename = storage.get<std::string>("filename");
    m_name = storage.get<std::string>("name");
    m_properties.playState = static_cast<PlayState>(
        storage.get<int16_t>("playState", PlayState::Stop)
    );
    m_properties.loop = storage.get<bool>("loop");
    m_properties.volume = storage.get<float>("volume");
    m_properties.maxDistance = storage.get<float>("maxDistance", -1.0f);
    m_properties.rolloffFactor = storage.get<float>("rolloffFactor", -1.0f);
    m_properties.referenceDistance = storage.get<float>("referenceDistance", 100.0f);
    m_properties.priority = storage.get<uint8_t>("priority");
}


std::string
Sound::name() const {
    return m_name;
}


void
Sound::play() {
    m_properties.playState = PlayState::Play;
    m_properties.touch();
}


void
Sound::pause() {
    m_properties.playState = PlayState::Pause;
    m_properties.touch();
}


void
Sound::stop() {
    m_properties.playState = PlayState::Stop;
    m_properties.touch();
}


StorageContainer
Sound::storage() const {
    StorageContainer storage;
    storage.set("filename", m_filename);
    storage.set("name", m_name);
    storage.set<int16_t>("playState", m_properties.playState);
    storage.set("loop", m_properties.loop);
    storage.set("volume", m_properties.volume);
    storage.set("maxDistance", m_properties.maxDistance);
    storage.set("rolloffFactor", m_properties.rolloffFactor);
    storage.set("referenceDistance", m_properties.referenceDistance);
    storage.set("priority", m_properties.priority);
    return storage;
}


////////////////////////////////////////////////////////////////////////////////
// SoundSourceComponent
////////////////////////////////////////////////////////////////////////////////

static bool
SoundSourceComponent_getRelativeToListener(
    const SoundSourceComponent* self
) {
    return self->m_relativeToListener;
}

static void
SoundSourceComponent_setRelativeToListener(
    SoundSourceComponent* self,
    bool value
) {
    self->m_relativeToListener = value;
}

luabind::scope
SoundSourceComponent::luaBindings() {
    using namespace luabind;
    return class_<SoundSourceComponent, Component>("SoundSourceComponent")
        .enum_("ID") [
            value("TYPE_ID", SoundSourceComponent::TYPE_ID)
        ]
        .scope [
            def("TYPE_NAME", &SoundSourceComponent::TYPE_NAME)
        ]
        .def(constructor<>())
        .def("addSound", &SoundSourceComponent::addSound)
        .def("removeSound", &SoundSourceComponent::removeSound)
        .property("relativeToListener", SoundSourceComponent_getRelativeToListener, SoundSourceComponent_setRelativeToListener)
    ;
}


Sound*
SoundSourceComponent::addSound(
    std::string name,
    std::string filename
) {
    auto sound = make_unique<Sound>(name, filename);
    Sound* rawSound = sound.get();
    m_sounds.emplace(name, std::move(sound));
    m_addedSounds.push_back(rawSound);
    return rawSound;
}


void
SoundSourceComponent::load(
    const StorageContainer& storage
) {
    Component::load(storage);
    m_relativeToListener = storage.get<bool>("relativeToListener");
    StorageList sounds = storage.get<StorageList>("sounds");
    for (const StorageContainer& soundStorage : sounds) {
        auto sound = make_unique<Sound>();
        sound->load(soundStorage);
        m_sounds.emplace(
            sound->name(),
            std::move(sound)
        );
    }
}


void
SoundSourceComponent::removeSound(
    std::string name
) {
    auto iterator = m_sounds.find(name);
    if (iterator != m_sounds.end()) {
        m_removedSounds.push_back(iterator->second.get());
        m_sounds.erase(iterator);
    }
}


StorageContainer
SoundSourceComponent::storage() const {
    StorageContainer storage = Component::storage();
    storage.set("relativeToListener", m_relativeToListener.get());
    StorageList sounds;
    sounds.reserve(m_sounds.size());
    for (const auto& pair : m_sounds) {
        sounds.push_back(pair.second->storage());
    }
    return storage;
}

REGISTER_COMPONENT(SoundSourceComponent)


////////////////////////////////////////////////////////////////////////////////
// SoundSourceSystem
////////////////////////////////////////////////////////////////////////////////

luabind::scope
SoundSourceSystem::luaBindings() {
    using namespace luabind;
    return class_<SoundSourceSystem, System>("SoundSourceSystem")
        .def(constructor<>())
    ;
}


struct SoundSourceSystem::Implementation {

    void
    removeAllSounds() {
        for (const auto& item : m_entities) {
            EntityId entityId = item.first;
            this->removeSoundsForEntity(entityId);
        }
        m_sounds.clear();
    }

    void
    removeSoundsForEntity(
        EntityId entityId
    ) {
        for (const auto& pair : m_sounds[entityId]) {
            OgreOggSound::OgreOggISound* sound = pair.second;
            this->removeSound(sound);
        }
    }

    void
    removeSound(
        OgreOggSound::OgreOggISound* sound
    ) {
        auto& soundManager = OgreOggSound::OgreOggSoundManager::getSingleton();
        if (sound) {
            Ogre::SceneNode* sceneNode = sound->getParentSceneNode();
            sceneNode->detachObject(sound);
            soundManager.destroySound(sound);
        }
    }

    void
    restoreAllSounds() {
        for (const auto& item : m_entities) {
            EntityId entityId = item.first;
            OgreSceneNodeComponent* sceneNodeComponent = std::get<0>(item.second);
            SoundSourceComponent* soundSourceComponent = std::get<1>(item.second);
            for (const auto& pair : soundSourceComponent->m_sounds) {
                Sound* sound = pair.second.get();
                this->restoreSound(
                    entityId, 
                    sceneNodeComponent, 
                    sound
                );
            }
        }
    }

    void
    restoreSound(
        EntityId entityId,
        OgreSceneNodeComponent* sceneNodeComponent,
        Sound* sound
    ) {
        static const bool STREAM = true;
        static const bool PREBUFFER = true;
        if (not sceneNodeComponent->m_sceneNode) {
            return;
        }
        auto& soundManager = OgreOggSound::OgreOggSoundManager::getSingleton();
        std::cout << "Looping: " << sound->m_properties.loop << std::endl;
        OgreOggSound::OgreOggISound* ogreSound = soundManager.createSound(
            sound->name(),
            sound->filename(),
            STREAM,
            sound->m_properties.loop,
            PREBUFFER
        );
        if (ogreSound) {
            sound->m_sound = ogreSound;
            m_sounds[entityId].emplace(sound->name(), ogreSound);
            sceneNodeComponent->m_sceneNode->attachObject(ogreSound);
        }
        else {
            //TODO: Log error. Or does OgreOggSound do this already?
        }
    }
    
    EntityFilter<OgreSceneNodeComponent, SoundSourceComponent> m_entities = {true};

    std::unordered_map<
        EntityId, 
        std::unordered_map<std::string, OgreOggSound::OgreOggISound*> 
    > m_sounds;

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
        m_impl->removeSoundsForEntity(entityId);
    }
    for (auto& value : m_impl->m_entities.addedEntities()) {
        EntityId entityId = value.first;
        OgreSceneNodeComponent* sceneNodeComponent = std::get<0>(value.second);
        SoundSourceComponent* soundSourceComponent = std::get<1>(value.second);
        for (const auto& pair : soundSourceComponent->m_sounds) {
            Sound* sound = pair.second.get();
            m_impl->restoreSound(
                entityId, 
                sceneNodeComponent, 
                sound
            );
        }
    }
    m_impl->m_entities.clearChanges();
    for (auto& value : m_impl->m_entities) {
        EntityId entityId = value.first;
        OgreSceneNodeComponent* sceneNodeComponent = std::get<0>(value.second);
        SoundSourceComponent* soundSourceComponent = std::get<1>(value.second);
        for (const auto& pair : soundSourceComponent->m_sounds) {
            Sound* sound = pair.second.get();
            if (not sound->m_sound) {
                m_impl->restoreSound(
                    entityId,
                    sceneNodeComponent,
                    sound
                );
            }
            if (sound->m_properties.hasChanges()) {
                const auto& properties = sound->m_properties;
                OgreOggSound::OgreOggISound* ogreSound = sound->m_sound;
                ogreSound->setVolume(properties.volume);
                ogreSound->setMaxDistance(properties.maxDistance);
                ogreSound->setRolloffFactor(properties.rolloffFactor);
                ogreSound->setReferenceDistance(properties.referenceDistance);
                ogreSound->setPriority(properties.priority);
                switch(properties.playState) {
                    case Sound::PlayState::Play:
                        ogreSound->play();
                        break;
                    case Sound::PlayState::Pause:
                        ogreSound->pause();
                        break;
                    case Sound::PlayState::Stop:
                        ogreSound->stop();
                        break;
                    default:
                        // Shut up GCC
                        break;
                }
                sound->m_properties.untouch();
            }
        }
    }
}

