#include "Config.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraDefines.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"

// Riding spell IDs
enum RidingSpells
{
    SPELL_APPRENTICE_RIDING = 33388,
    SPELL_JOURNEYMAN_RIDING = 33391,
    SPELL_EXPERT_RIDING     = 34090,
    SPELL_ARTISAN_RIDING    = 34091
};

// Flying mounts this fast (Ashes of Al'ar, the arena drakes, the 310% versions of Invincible
// and the Celestial Steed...) keep their own speed: the scaling can raise it, never lower it.
static constexpr int32 FLYER_310_SPEED = 310;

// Config values
static bool   sEnabled              = true;
static float  sGroundSpeedPerLevel  = 2.5f;
static float  sGroundMinSpeed       = 20.0f;
static float  sGroundMaxSpeed       = 100.0f;
static float  sGroundJourneymanPerLevel = 2.5f;
static float  sGroundJourneymanMin  = 100.0f;
static float  sGroundJourneymanMax  = 150.0f;
static float  sFlyingExpertPerLevel = 5.0f;
static float  sFlyingExpertMin      = 150.0f;
static float  sFlyingExpertMax      = 200.0f;
static float  sFlyingArtisanPerLevel = 8.0f;
static float  sFlyingArtisanMin     = 200.0f;
static float  sFlyingArtisanMax     = 280.0f;

static int32 CalculateGroundSpeed(Player* player)
{
    if (!player->HasSpell(SPELL_APPRENTICE_RIDING))
        return 0;

    uint8 level = player->GetLevel();

    if (player->HasSpell(SPELL_JOURNEYMAN_RIDING))
    {
        float speed = sGroundJourneymanMin + (level - 40) * sGroundJourneymanPerLevel;
        speed = std::clamp(speed, sGroundJourneymanMin, sGroundJourneymanMax);
        return static_cast<int32>(speed);
    }

    float speed = std::max(sGroundMinSpeed, level * sGroundSpeedPerLevel);
    speed = std::min(speed, sGroundMaxSpeed);
    return static_cast<int32>(speed);
}

static int32 CalculateFlyingSpeed(Player* player)
{
    uint8 level = player->GetLevel();

    if (player->HasSpell(SPELL_ARTISAN_RIDING))
    {
        float speed = sFlyingArtisanMin + (level - 70) * sFlyingArtisanPerLevel;
        speed = std::clamp(speed, sFlyingArtisanMin, sFlyingArtisanMax);
        return static_cast<int32>(speed);
    }

    if (player->HasSpell(SPELL_EXPERT_RIDING))
    {
        float speed = sFlyingExpertMin + (level - 60) * sFlyingExpertPerLevel;
        speed = std::clamp(speed, sFlyingExpertMin, sFlyingExpertMax);
        return static_cast<int32>(speed);
    }

    return 0;
}

// The flying speed for one mount aura effect: the level-scaled speed, but never below the
// mount's own speed if it's a 310% flyer. 0 means leave it alone.
static int32 CalculateFlyingSpeed(Player* player, AuraEffect const* effect)
{
    int32 speed = CalculateFlyingSpeed(player);
    if (!speed)
        return 0;

    int32 const mountSpeed = effect->GetSpellInfo()->Effects[effect->GetEffIndex()].CalcValue();
    if (mountSpeed >= FLYER_310_SPEED)
        speed = std::max(speed, mountSpeed);

    return speed;
}

class MountScalingWorldScript : public WorldScript
{
public:
    MountScalingWorldScript() : WorldScript("MountScalingWorldScript", {WORLDHOOK_ON_AFTER_CONFIG_LOAD}) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sEnabled              = sConfigMgr->GetOption<bool>("MountScaling.Enable", true);
        sGroundSpeedPerLevel  = sConfigMgr->GetOption<float>("MountScaling.Ground.SpeedPerLevel", 2.5f);
        sGroundMinSpeed       = sConfigMgr->GetOption<float>("MountScaling.Ground.MinSpeed", 20.0f);
        sGroundMaxSpeed       = sConfigMgr->GetOption<float>("MountScaling.Ground.MaxSpeed", 100.0f);
        sGroundJourneymanPerLevel = sConfigMgr->GetOption<float>("MountScaling.Ground.Journeyman.SpeedPerLevel", 2.5f);
        sGroundJourneymanMin  = sConfigMgr->GetOption<float>("MountScaling.Ground.Journeyman.MinSpeed", 100.0f);
        sGroundJourneymanMax  = sConfigMgr->GetOption<float>("MountScaling.Ground.Journeyman.MaxSpeed", 150.0f);
        sFlyingExpertPerLevel = sConfigMgr->GetOption<float>("MountScaling.Flying.Expert.SpeedPerLevel", 5.0f);
        sFlyingExpertMin      = sConfigMgr->GetOption<float>("MountScaling.Flying.Expert.MinSpeed", 150.0f);
        sFlyingExpertMax      = sConfigMgr->GetOption<float>("MountScaling.Flying.Expert.MaxSpeed", 200.0f);
        sFlyingArtisanPerLevel = sConfigMgr->GetOption<float>("MountScaling.Flying.Artisan.SpeedPerLevel", 8.0f);
        sFlyingArtisanMin     = sConfigMgr->GetOption<float>("MountScaling.Flying.Artisan.MinSpeed", 200.0f);
        sFlyingArtisanMax     = sConfigMgr->GetOption<float>("MountScaling.Flying.Artisan.MaxSpeed", 280.0f);
    }
};

class MountScalingUnitScript : public UnitScript
{
public:
    MountScalingUnitScript() : UnitScript("MountScalingUnitScript", true, {UNITHOOK_ON_AURA_APPLY}) { }

    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        if (!sEnabled)
            return;

        Player* player = unit->ToPlayer();
        if (!player)
            return;

        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            AuraEffect* effect = aura->GetEffect(i);
            if (!effect)
                continue;

            AuraType auraType = effect->GetAuraType();

            if (auraType == SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED)
            {
                int32 newAmount = CalculateGroundSpeed(player);
                if (newAmount > 0)
                    effect->ChangeAmount(newAmount);
            }
            else if (auraType == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED)
            {
                int32 newAmount = CalculateFlyingSpeed(player, effect);
                if (newAmount > 0)
                    effect->ChangeAmount(newAmount);
            }
        }
    }
};

class MountScalingPlayerScript : public PlayerScript
{
public:
    MountScalingPlayerScript() : PlayerScript("MountScalingPlayerScript", {PLAYERHOOK_ON_LEVEL_CHANGED}) { }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (!sEnabled || !player->IsMounted())
            return;

        // Update ground mount speed auras
        for (AuraEffect* effect : player->GetAuraEffectsByType(SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED))
        {
            int32 newAmount = CalculateGroundSpeed(player);
            if (newAmount > 0)
                effect->ChangeAmount(newAmount);
        }

        // Update flying mount speed auras
        for (AuraEffect* effect : player->GetAuraEffectsByType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED))
        {
            int32 newAmount = CalculateFlyingSpeed(player, effect);
            if (newAmount > 0)
                effect->ChangeAmount(newAmount);
        }
    }
};

void AddMountScalingScripts()
{
    new MountScalingWorldScript();
    new MountScalingUnitScript();
    new MountScalingPlayerScript();
}
