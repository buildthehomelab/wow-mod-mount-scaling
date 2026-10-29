#include "stubs/Core.h"
#include <iostream>

#ifndef MOUNT_SCALING_SOURCE
#define MOUNT_SCALING_SOURCE "../src/MountScaling.cpp"
#endif
#include MOUNT_SCALING_SOURCE

void Addmod_mount_scalingScripts();
namespace
{
    unsigned checks = 0;
    void Check(bool condition, char const* reason)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(reason);
    }
    struct Fixture
    {
        MountScalingWorldScript world;
        MountScalingUnitScript unit;
        MountScalingPlayerScript playerScript;
        MountScalingSpellScript spellScript;
        Player player;
        Fixture()
        {
            TestCore::options.clear();
            ScriptBase::registry.clear(); // Fixture scripts have automatic lifetime.
            Load();
        }
        void Load()
        {
            Check(world.HasHook(WORLDHOOK_ON_AFTER_CONFIG_LOAD), "config hook subscribed");
            world.OnAfterConfigLoad(false);
        }
        void Apply(Unit* target, Aura* aura)
        {
            Check(unit.attached && unit.HasHook(UNITHOOK_ON_AURA_APPLY), "aura hook subscribed");
            unit.OnAuraApply(target, aura);
        }
        void LevelChanged()
        {
            Check(playerScript.HasHook(PLAYERHOOK_ON_LEVEL_CHANGED), "level hook subscribed");
            playerScript.OnPlayerLevelChanged(&player, 1);
        }
        SpellCastResult Cast(SpellInfo const& info, bool triggered = false,
                             SpellCastResult initial = SPELL_CAST_OK, Unit* caster = nullptr)
        {
            Check(spellScript.HasHook(ALLSPELLHOOK_ON_SPELL_CHECK_CAST), "cast hook subscribed");
            Spell spell{caster ? caster : &player, &info, triggered};
            spellScript.OnSpellCheckCast(&spell, false, initial);
            return initial;
        }
    };

    void CoreRegistrationContracts()
    {
        ScriptBase::registry.clear();
        WorldScript world("all", {});
        UnitScript unit("all", true, {});
        PlayerScript player("all", {});
        AllSpellScript spell("all", {});
        Check(world.HasHook(WORLDHOOK_ON_AFTER_CONFIG_LOAD), "empty world hook list enables all hooks");
        Check(unit.HasHook(UNITHOOK_ON_AURA_APPLY), "empty unit hook list enables all hooks");
        Check(player.HasHook(PLAYERHOOK_ON_LEVEL_CHANGED), "empty player hook list enables all hooks");
        Check(spell.HasHook(ALLSPELLHOOK_ON_SPELL_CHECK_CAST), "empty spell hook list enables all hooks");
        auto const registered = ScriptBase::registry.size();
        UnitScript detached("detached", false, {UNITHOOK_ON_AURA_APPLY});
        Check(!detached.attached && ScriptBase::registry.size() == registered,
              "unit script addToScripts false does not register");
        WorldScript otherWorld("specific", {WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED});
        UnitScript otherUnit("specific", true, {UNITHOOK_ON_DAMAGE});
        PlayerScript otherPlayer("specific", {PLAYERHOOK_ON_PLAYER_JUST_DIED});
        AllSpellScript otherSpell("specific", {ALLSPELLHOOK_ON_CALC_MAX_DURATION});
        Check(!otherWorld.HasHook(WORLDHOOK_ON_AFTER_CONFIG_LOAD), "nonempty world list filters hooks");
        Check(!otherUnit.HasHook(UNITHOOK_ON_AURA_APPLY), "nonempty unit list filters hooks");
        Check(!otherPlayer.HasHook(PLAYERHOOK_ON_LEVEL_CHANGED), "nonempty player list filters hooks");
        Check(!otherSpell.HasHook(ALLSPELLHOOK_ON_SPELL_CHECK_CAST), "nonempty spell list filters hooks");
        ScriptBase::registry.clear();
    }

    void RidingCurves()
    {
        Fixture f;
        f.player.level = 80;
        Check(CalculateGroundSpeed(&f.player) == 20, "untrained stays at apprentice floor at 80");
        Check(CalculateFlyingSpeed(&f.player) == 0, "untrained has no scaled flight");
        f.player.spells = {SPELL_APPRENTICE_RIDING};
        for (auto const& sample : std::vector<std::pair<uint8, int32>>{{1,20},{8,20},{10,25},{20,50},{30,75},{40,100},{80,100}})
        {
            f.player.level = sample.first;
            Check(CalculateGroundSpeed(&f.player) == sample.second, "apprentice level curve");
        }
        for (uint32 skill : {SPELL_JOURNEYMAN_RIDING, SPELL_EXPERT_RIDING, SPELL_ARTISAN_RIDING})
        {
            f.player.spells = {skill};
            for (auto const& sample : std::vector<std::pair<uint8, int32>>{{1,100},{40,100},{44,110},{50,125},{60,150},{80,150}})
            {
                f.player.level = sample.first;
                Check(CalculateGroundSpeed(&f.player) == sample.second, "higher riding tier has journeyman ground curve");
            }
        }
        f.player.spells = {SPELL_APPRENTICE_RIDING, SPELL_JOURNEYMAN_RIDING};
        f.player.level = 45;
        Check(CalculateGroundSpeed(&f.player) == 112, "journeyman wins and fractional speed truncates");
        f.player.spells = {SPELL_EXPERT_RIDING};
        for (auto const& sample : std::vector<std::pair<uint8, int32>>{{1,150},{60,150},{65,175},{70,200},{80,200}})
        {
            f.player.level = sample.first;
            Check(CalculateFlyingSpeed(&f.player) == sample.second, "expert flight clamps both ends");
        }
        f.player.spells = {SPELL_EXPERT_RIDING, SPELL_ARTISAN_RIDING};
        for (auto const& sample : std::vector<std::pair<uint8, int32>>{{1,200},{70,200},{75,240},{80,280},{90,280}})
        {
            f.player.level = sample.first;
            Check(CalculateFlyingSpeed(&f.player) == sample.second, "artisan wins and clamps both ends");
        }
    }

    void ConfigurableCurves()
    {
        Fixture f;
        TestCore::options = {{"MountScaling.Ground.SpeedPerLevel",3}, {"MountScaling.Ground.MinSpeed",12},
          {"MountScaling.Ground.MaxSpeed",95}, {"MountScaling.Ground.Journeyman.SpeedPerLevel",4},
          {"MountScaling.Ground.Journeyman.MinSpeed",105}, {"MountScaling.Ground.Journeyman.MaxSpeed",145},
          {"MountScaling.Flying.Expert.SpeedPerLevel",6}, {"MountScaling.Flying.Expert.MinSpeed",155},
          {"MountScaling.Flying.Expert.MaxSpeed",195}, {"MountScaling.Flying.Artisan.SpeedPerLevel",7},
          {"MountScaling.Flying.Artisan.MinSpeed",205}, {"MountScaling.Flying.Artisan.MaxSpeed",275}};
        f.world.OnAfterConfigLoad(true);
        Check(CalculateGroundSpeed(&f.player) == 12, "reloaded untrained floor");
        f.player.spells = {SPELL_APPRENTICE_RIDING}; f.player.level = 10;
        Check(CalculateGroundSpeed(&f.player) == 30, "custom apprentice rate");
        f.player.level = 80;
        Check(CalculateGroundSpeed(&f.player) == 95, "custom apprentice cap");
        f.player.spells = {SPELL_JOURNEYMAN_RIDING}; f.player.level = 41;
        Check(CalculateGroundSpeed(&f.player) == 109, "custom journeyman rate and floor");
        f.player.level = 80;
        Check(CalculateGroundSpeed(&f.player) == 145, "custom journeyman cap");
        f.player.spells = {SPELL_EXPERT_RIDING}; f.player.level = 61;
        Check(CalculateFlyingSpeed(&f.player) == 161, "custom expert rate and floor");
        f.player.level = 80;
        Check(CalculateFlyingSpeed(&f.player) == 195, "custom expert cap");
        f.player.spells = {SPELL_ARTISAN_RIDING}; f.player.level = 71;
        Check(CalculateFlyingSpeed(&f.player) == 212, "custom artisan rate and floor");
        f.player.level = 90;
        Check(CalculateFlyingSpeed(&f.player) == 275, "custom artisan cap");
    }

    SpellInfo MountInfo(bool flying = false, int32 base = 280)
    {
        SpellInfo info;
        info.auras = {SPELL_AURA_MOUNTED};
        info.auras.insert(flying ? SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED : SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED);
        info.mountSkill = true;
        info.Effects[1].value = base;
        return info;
    }

    void AuraApplicationAndLevels()
    {
        Fixture f;
        auto ground = MountInfo();
        auto flight = MountInfo(true, 310);
        AuraEffect groundEffect{&ground,SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED,0,60};
        AuraEffect flightEffect{&flight,SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED,1,310};
        AuraEffect other{&ground,SPELL_AURA_NONE,2,77};
        Aura groundAura{&ground,{&groundEffect,nullptr,&other}};
        Aura flightAura{&flight,{nullptr,&flightEffect,nullptr}};
        f.player.level = 80;
        f.Apply(&f.player,&groundAura);
        Check(groundEffect.amount == 20 && groundEffect.changes == 1, "untrained ground mount scaled on apply");
        Check(other.amount == 77 && other.changes == 0, "unrelated aura unchanged");
        Unit npc;
        f.Apply(&npc,&groundAura);
        Check(groundEffect.changes == 1, "non-player aura ignored");
        f.Apply(&f.player,&flightAura);
        Check(flightEffect.changes == 0, "untrained externally triggered flight left alone");
        f.player.spells = {SPELL_EXPERT_RIDING};
        f.Apply(&f.player,&flightAura);
        Check(flightEffect.amount == 310, "native 310 flyer never lowered");
        flight.Effects[1].value = 280;
        f.player.level = 65;
        f.Apply(&f.player,&flightAura);
        Check(flightEffect.amount == 175, "normal flyer uses scaled rate and correct effect index");
        SpellInfo event;
        AuraEffect eventGround{&event,SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED,0,88};
        Aura eventAura{&event,{&eventGround,nullptr,nullptr}};
        f.Apply(&f.player,&eventAura);
        Check(eventGround.amount == 88 && eventGround.changes == 0, "event ground speed buff not a mount");
        AuraEffect separateFlight{&event,SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED,0,99};
        Aura separateAura{&event,{&separateFlight,nullptr,nullptr}};
        f.player.mounted = false;
        f.Apply(&f.player,&separateAura);
        Check(separateFlight.changes == 0, "standalone flight buff on unmounted player untouched");
        f.player.mounted = true;
        f.Apply(&f.player,&separateAura);
        Check(separateFlight.amount == 175 && separateFlight.changes == 1, "separate flight aura on mounted player scales");
        f.player.effects = {&groundEffect,&eventGround,&flightEffect};
        f.player.level = 70;
        f.LevelChanged();
        Check(groundEffect.amount == 150 && flightEffect.amount == 200, "level-up updates ground and flight");
        Check(eventGround.amount == 88 && eventGround.changes == 0, "level-up preserves event buff");
        f.player.spells.clear();
        auto const flightChanges = flightEffect.changes;
        f.LevelChanged();
        Check(flightEffect.changes == flightChanges, "untrained level-up leaves external flight unchanged");
        f.player.mounted = false;
        auto const groundChanges = groundEffect.changes;
        f.LevelChanged();
        Check(groundEffect.changes == groundChanges, "unmounted level-up does nothing");
        TestCore::options["MountScaling.Enable"] = 0; f.Load();
        f.player.mounted = true;
        f.Apply(&f.player,&groundAura); f.LevelChanged();
        Check(groundEffect.changes == groundChanges, "disabled module does not update auras");
        Check(f.Cast(flight) == SPELL_CAST_OK, "disabled module does not restrict casts");
    }

    void NativeFastFlyers()
    {
        Fixture f;
        auto info = MountInfo(true, 310);
        AuraEffect effect{&info,SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED,1,310};
        f.player.spells = {SPELL_ARTISAN_RIDING}; f.player.level = 70;
        Check(CalculateFlyingSpeed(&f.player,&effect) == 310, "native flyer floor at artisan entry");
        info.Effects[1].value = 350;
        Check(CalculateFlyingSpeed(&f.player,&effect) == 350, "faster native flyer preserved");
        TestCore::options["MountScaling.Flying.Artisan.MaxSpeed"] = 400;
        TestCore::options["MountScaling.Flying.Artisan.SpeedPerLevel"] = 20;
        f.Load(); f.player.level = 80;
        Check(CalculateFlyingSpeed(&f.player,&effect) == 400, "custom scaling may exceed native flyer");
    }

    void FlyingCastEligibility()
    {
        Fixture f;
        auto flying = MountInfo(true);
        Check(f.Cast(flying) == SPELL_FAILED_EXPERT_RIDING_REQUIREMENT, "untrained flying mount refused");
        f.player.spells = {SPELL_APPRENTICE_RIDING};
        Check(f.Cast(flying) == SPELL_FAILED_EXPERT_RIDING_REQUIREMENT, "apprentice cannot fly");
        f.player.spells = {SPELL_JOURNEYMAN_RIDING};
        Check(f.Cast(flying) == SPELL_FAILED_EXPERT_RIDING_REQUIREMENT, "journeyman cannot fly");
        for (uint32 skill : {SPELL_EXPERT_RIDING,SPELL_ARTISAN_RIDING})
        {
            f.player.spells = {skill};
            Check(f.Cast(flying) == SPELL_CAST_OK, "expert or artisan permits flight");
        }
        f.player.spells.clear();
        Check(f.Cast(flying,true) == SPELL_CAST_OK, "triggered flight preserved");
        Check(f.Cast(flying,false,SPELL_FAILED_OTHER) == SPELL_FAILED_OTHER, "pre-existing cast error preserved");
        auto ground = MountInfo();
        Check(f.Cast(ground) == SPELL_CAST_OK, "untrained ground mount allowed");
        auto scripted = ground; scripted.attributes = SPELL_ATTR4_ONLY_FLYING_AREAS;
        Check(f.Cast(scripted) == SPELL_FAILED_EXPERT_RIDING_REQUIREMENT, "flagged scripted flyer requires training");
        auto quest = flying; quest.mountSkill = false;
        Check(f.Cast(quest) == SPELL_CAST_OK, "quest flight outside mount skill allowed");
        quest = flying; quest.auras.erase(SPELL_AURA_MOUNTED);
        Check(f.Cast(quest) == SPELL_CAST_OK, "non-mount flight spell allowed");
        Unit npc;
        Check(f.Cast(flying,false,SPELL_CAST_OK,&npc) == SPELL_CAST_OK, "non-player caster unaffected");
    }

    void Loader()
    {
        ScriptBase::registry.clear();
        Addmod_mount_scalingScripts();
        Check(ScriptBase::registry.size() == 4, "loader registers all four scripts");
        bool world=false, unit=false, player=false, spell=false;
        for (auto* script : ScriptBase::registry)
        {
            if (auto* value = dynamic_cast<WorldScript*>(script)) world = value->HasHook(WORLDHOOK_ON_AFTER_CONFIG_LOAD);
            if (auto* value = dynamic_cast<UnitScript*>(script)) unit = value->HasHook(UNITHOOK_ON_AURA_APPLY);
            if (auto* value = dynamic_cast<PlayerScript*>(script)) player = value->HasHook(PLAYERHOOK_ON_LEVEL_CHANGED);
            if (auto* value = dynamic_cast<AllSpellScript*>(script)) spell = value->HasHook(ALLSPELLHOOK_ON_SPELL_CHECK_CAST);
        }
        Check(world && unit && player && spell, "all script families subscribe required hooks");
        for (auto* script : ScriptBase::registry) delete script;
        ScriptBase::registry.clear();
    }
}
int main()
{
    try
    {
        CoreRegistrationContracts(); RidingCurves(); ConfigurableCurves(); AuraApplicationAndLevels();
        NativeFastFlyers(); FlyingCastEligibility(); Loader();
        std::cout << "PASS " << checks << " checks\n";
    }
    catch (std::exception const& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
