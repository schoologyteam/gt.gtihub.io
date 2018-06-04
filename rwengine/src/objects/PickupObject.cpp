#include "objects/PickupObject.hpp"

#include <cmath>

#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <btBulletDynamicsCommon.h>
#include <glm/gtc/quaternion.hpp>

#include "ai/PlayerController.hpp"
#include "data/WeaponData.hpp"
#include "engine/GameData.hpp"
#include "engine/GameState.hpp"
#include "engine/GameWorld.hpp"
#include "engine/ScreenText.hpp"
#include "objects/CharacterObject.hpp"
#include "objects/PickupObject.hpp"

uint32_t colours[14] = {
    0xff0000,  // bat, detonator, adrenaline
    0x00ff00,  // pistol
    0x8080ff,  // uzi
    0xffff00,  // shotgun
    0xff00ff,  // ak47
    0x00ffff,  // m16
    0xff8000,  // sniper
    0x00ff80,  // rocket
    0x8000ff,  // flame
    0x80ff00,  // molotov
    0xffffff,  // grenade. package1, floatpackge1
    0x80ff80,  // bodyarmour, bribe
    0x0000ff,  // info, killfrenzy
    0xffff00,  // health, bonus
};

bool PickupObject::doesRespawn(PickupType type) {
    switch (type) {
        case Once:
        case OnceTimeout:
        case Collectable:
        case OutOfStock:
        case Money:
        case MineInactive:
        case MineArmed:
        case NauticalMineInactive:
        case NauticalMineArmed:
        case FloatingPackage:
        case FloatingPackageFloating:
            return false;
        default:
            return true;
    }
}

float PickupObject::respawnTime(PickupType type) {
    switch (type) {
        case InShop:
            return 5.f;
        case OnStreet:
        case OnStreetSlow:
            return 30.f;
        default:
            return 30.f;
    }
}

uint32_t PickupObject::behaviourFlags(PickupType type) {
    switch (type) {
        case InShop:
        case OnStreet:
        case Once:
        case OnceTimeout:
        case Collectable:
        case Money:
        case OnStreetSlow:
            return PickupOnFoot;
        case OutOfStock:
        case MineInactive:
        case MineArmed:
        case NauticalMineInactive:
        case NauticalMineArmed:
            return 0;
        case FloatingPackage:
        case FloatingPackageFloating:
            return PickupInVehicle;
        default:
            return 0;
    }
}

PickupObject::PickupObject(GameWorld* world, const glm::vec3& position,
                           BaseModelInfo* modelinfo, PickupType type)
    : GameObject(world, position, glm::quat{1.0f, 0.0f, 0.0f, 0.0f}, modelinfo)
    , m_ghost(nullptr)
    , m_shape(nullptr)
    , m_enabled(false)
    , m_collected(false)
    , m_type(type) {
    btTransform tf;
    tf.setIdentity();
    tf.setOrigin(btVector3(position.x, position.y, position.z));

    m_ghost = new btPairCachingGhostObject;
    m_ghost->setUserPointer(this);
    m_ghost->setWorldTransform(tf);
    m_shape = new btSphereShape(0.5f);
    m_ghost->setCollisionShape(m_shape);
    m_ghost->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT |
                               btCollisionObject::CF_NO_CONTACT_RESPONSE);

    m_colourId = 0;
    if (modelinfo->name == "colt45")
        m_colourId = 1;
    else if (modelinfo->name == "uzi")
        m_colourId = 2;
    else if (modelinfo->name == "shotgun")
        m_colourId = 3;
    else if (modelinfo->name == "ak47")
        m_colourId = 4;
    else if (modelinfo->name == "m16")
        m_colourId = 5;
    else if (modelinfo->name == "sniper")
        m_colourId = 6;
    else if (modelinfo->name == "rocket")
        m_colourId = 7;
    else if (modelinfo->name == "flame")
        m_colourId = 8;
    else if (modelinfo->name == "molotov")
        m_colourId = 9;
    else if (modelinfo->name == "grenade" || modelinfo->name == "package1" ||
             modelinfo->name == "floatpackge1")
        m_colourId = 10;
    else if (modelinfo->name == "bodyarmour" || modelinfo->name == "bribe")
        m_colourId = 11;
    else if (modelinfo->name == "info" || modelinfo->name == "killfrenzy")
        m_colourId = 12;
    else if (modelinfo->name == "health" || modelinfo->name == "bonus")
        m_colourId = 13;

    m_corona = world->createEffect(VisualFX::Particle);
    m_corona->particle.position = getPosition();
    m_corona->particle.direction = glm::vec3(0.f, 0.f, 1.f);
    m_corona->particle.orientation = VisualFX::ParticleData::Camera;

    // @todo float package should float on the water
    if (m_type == FloatingPackage) {
        // verify offset and texture?
        m_corona->particle.position += glm::vec3(0.f, 0.f, 0.7f);
        m_corona->particle.texture =
            engine->data->findSlotTexture("particle", "coronastar");
    } else {
        m_corona->particle.texture =
            engine->data->findSlotTexture("particle", "coronaringa");
    }

    auto flags = behaviourFlags(m_type);
    RW_UNUSED(flags);

    RW_CHECK((flags & PickupInVehicle) == 0,
             "In Vehicle pickup not implemented yet");

    setEnabled(true);
    setCollected(false);
}

PickupObject::~PickupObject() {
    if (m_ghost) {
        setEnabled(false);
        engine->destroyEffect(m_corona);
        delete m_corona;
        delete m_ghost;
        delete m_shape;
    }
}

void PickupObject::tick(float dt) {
    if (isRampage() && engine->state->scriptOnMissionFlag != nullptr) {
        if (*(engine->state->scriptOnMissionFlag) != 0 && isEnabled()) {
            setEnabled(false);
        } else if (*(engine->state->scriptOnMissionFlag) == 0 && !isEnabled() &&
                   !isCollected()) {
            setEnabled(true);
        }
    }

    if (!m_enabled) {
        // Check if our type of pickup respawns
        if (doesRespawn(m_type)) {
            m_enableTimer -= dt;
            if (m_enableTimer <= 0.f) {
                setEnabled(true);
                setCollected(false);
            }
        }
    }

    float time = engine->getGameTime();
    float colourValue = 0.5f * (std::sin(time * 3.0664064f) * 0.3f + 0.3f);
    uint32_t* colour = &colours[m_colourId];
    float red = (*colour >> 16) & 0xFF;
    float green = (*colour >> 8) & 0xFF;
    float blue = *colour & 0xFF;
    m_corona->particle.colour =
        glm::vec4(red / 255.f, green / 255.f, blue / 255.f, 1.f) * colourValue;

    if (!isCollected()) {
        // Sort out interactions with things that may or may not be players.
        btManifoldArray manifoldArray;
        btBroadphasePairArray& pairArray =
            m_ghost->getOverlappingPairCache()->getOverlappingPairArray();
        int numPairs = pairArray.size();
        auto flags = behaviourFlags(m_type);

        for (int i = 0; i < numPairs; i++) {
            manifoldArray.clear();

            const btBroadphasePair& pair = pairArray[i];
            auto otherObject = static_cast<const btCollisionObject*>(
                pair.m_pProxy0->m_clientObject == m_ghost
                    ? pair.m_pProxy1->m_clientObject
                    : pair.m_pProxy0->m_clientObject);
            if (otherObject->getUserPointer()) {
                GameObject* object =
                    static_cast<GameObject*>(otherObject->getUserPointer());
                if ((flags & PickupOnFoot) == PickupOnFoot &&
                    object->type() == Character) {
                    CharacterObject* character =
                        static_cast<CharacterObject*>(object);

                    if (character->isPlayer()) {
                        setCollected(onCharacterTouch(character));
                        setEnabled(!isCollected());

                        if (!m_enabled) {
                            m_enableTimer = respawnTime(m_type);
                        }
                    }
                }
            }
        }
    }
}

void PickupObject::setEnabled(bool enabled) {
    if (!m_enabled && enabled) {
        engine->dynamicsWorld->addCollisionObject(
            m_ghost, btBroadphaseProxy::SensorTrigger);
        m_corona->particle.size = glm::vec2(1.5f, 1.5f);
    } else if (m_enabled && !enabled) {
        engine->dynamicsWorld->removeCollisionObject(m_ghost);
        m_corona->particle.size = glm::vec2(0.f, 0.f);
    }

    m_enabled = enabled;
}

ItemPickup::ItemPickup(GameWorld* world, const glm::vec3& position,
                       BaseModelInfo* modelinfo, PickupType type,
                       WeaponData* item)
    : PickupObject(world, position, modelinfo, type), item(item) {
}

bool ItemPickup::onCharacterTouch(CharacterObject* character) {
    auto totalRounds = 0;

    switch (item->modelID) {
        case 173: /* Pistol */
            totalRounds = 45;
            break;
        case 178: /* Uzi */
            totalRounds = 125;
            break;
        case 176: /* Shotgun */
            totalRounds = 25;
            break;
        case 170: /* Grenade */
            totalRounds = 5;
            break;
        case 174: /* Molotov */
            totalRounds = 5;
            break;
        case 181: /* Flame thrower */
            totalRounds = 25;
            break;
        case 171: /* AK */
            totalRounds = 150;
            break;
        case 180: /* M16 */
            totalRounds = 300;
            break;
        case 177: /* Sniper Rifle */
            totalRounds = 25;
            break;
    }

    if (getPickupType() == OnStreet || getPickupType() == OnStreetSlow) {
        totalRounds /= 5;
    }

    character->addToInventory(item->inventorySlot, totalRounds);

    return true;
}

DummyPickup::DummyPickup(GameWorld* world, const glm::vec3& position,
                         BaseModelInfo* modelinfo, PickupType type)
    : PickupObject(world, position, modelinfo, type) {
}

bool DummyPickup::onCharacterTouch(CharacterObject* character) {
    RW_UNUSED(character);
    return true;
}

RampagePickup::RampagePickup(GameWorld* world, const glm::vec3& position,
                             BaseModelInfo* modelinfo, PickupType type)
    : PickupObject(world, position, modelinfo, type) {
}

bool RampagePickup::onCharacterTouch(CharacterObject* character) {
    RW_UNUSED(character);

    if (engine->state->scriptOnMissionFlag == nullptr) {
        return false;
    }

    if (*(engine->state->scriptOnMissionFlag) != 0) {
        return false;
    }

    return true;
}

HealthPickup::HealthPickup(GameWorld* world, const glm::vec3& position,
                           BaseModelInfo* modelinfo, PickupType type)
    : PickupObject(world, position, modelinfo, type) {
}

bool HealthPickup::onCharacterTouch(CharacterObject* character) {
    if (character->getCurrentState().health >= 100.f) {
        return false;
    }

    character->getCurrentState().health = 100.f;

    return true;
}

ArmourPickup::ArmourPickup(GameWorld* world, const glm::vec3& position,
                           BaseModelInfo* modelinfo, PickupType type)
    : PickupObject(world, position, modelinfo, type) {
}

bool ArmourPickup::onCharacterTouch(CharacterObject* character) {
    if (character->getCurrentState().armour >= 100.f) {
        return false;
    }

    character->getCurrentState().armour = 100.f;

    return true;
}

CollectablePickup::CollectablePickup(GameWorld* world,
                                     const glm::vec3& position,
                                     BaseModelInfo* modelinfo, PickupType type)
    : PickupObject(world, position, modelinfo, type) {
}

bool CollectablePickup::onCharacterTouch(CharacterObject* character) {
    RW_UNUSED(character);

    GameState* state = engine->state;

    if (state->playerInfo.hiddenPackagesCollected ==
        state->playerInfo.hiddenPackageCount) {
        state->playerInfo.money += 1000000;

        const auto gxtEntry = "CO_ALL";

        state->text.addText<ScreenTextType::HiddenPackageText>(
            ScreenTextEntry::makeHiddenPackageText(
                gxtEntry, engine->data->texts.text(gxtEntry)));

    } else {
        state->playerInfo.hiddenPackagesCollected++;
        state->playerInfo.money += 1000;

        const auto gxtEntry = "CO_ONE";

        auto text = ScreenText::format(
            engine->data->texts.text(gxtEntry),
            GameStringUtil::fromString(
                std::to_string(state->playerInfo.hiddenPackagesCollected)),
            GameStringUtil::fromString(
                std::to_string(state->playerInfo.hiddenPackageCount)));

        state->text.addText<ScreenTextType::HiddenPackageText>(
            ScreenTextEntry::makeHiddenPackageText(gxtEntry, text));
    }

    return true;
}

AdrenalinePickup::AdrenalinePickup(GameWorld* world, const glm::vec3& position,
                                   BaseModelInfo* modelinfo, PickupType type)
    : PickupObject(world, position, modelinfo, type) {
}

bool AdrenalinePickup::onCharacterTouch(CharacterObject* character) {
    static_cast<PlayerController*>(character->controller)
        ->activateAdrenalineEffect();

    return true;
}

MoneyPickup::MoneyPickup(GameWorld* world, const glm::vec3& position,
                         BaseModelInfo* modelinfo, PickupType type,
                         uint32_t money)
    : PickupObject(world, position, modelinfo, type), money(money) {
}

bool MoneyPickup::onCharacterTouch(CharacterObject* character) {
    RW_UNUSED(character);

    engine->state->playerInfo.money += money;

    return true;
}
