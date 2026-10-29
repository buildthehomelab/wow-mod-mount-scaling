#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using uint8 = std::uint8_t;
using uint32 = std::uint32_t;
using int32 = std::int32_t;
constexpr uint8 MAX_SPELL_EFFECTS = 3;
enum AuraType { SPELL_AURA_NONE, SPELL_AURA_MOUNTED, SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED,
                SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED };
enum SpellCastResult { SPELL_CAST_OK, SPELL_FAILED_EXPERT_RIDING_REQUIREMENT, SPELL_FAILED_OTHER };
constexpr uint32 SPELL_ATTR4_ONLY_FLYING_AREAS = 1;
constexpr uint32 SKILL_MOUNTS = 777;
enum WorldHook { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED };
enum UnitHook { UNITHOOK_ON_AURA_APPLY, UNITHOOK_ON_DAMAGE };
enum PlayerHook { PLAYERHOOK_ON_LEVEL_CHANGED, PLAYERHOOK_ON_PLAYER_JUST_DIED };
enum SpellHook { ALLSPELLHOOK_ON_SPELL_CHECK_CAST, ALLSPELLHOOK_ON_CALC_MAX_DURATION };

namespace TestCore
{
    inline std::map<std::string, float> options;
    struct ConfigMgr
    {
        template <typename T> T GetOption(char const* name, T fallback) const
        {
            auto const it = options.find(name);
            return it == options.end() ? fallback : static_cast<T>(it->second);
        }
    };
    inline ConfigMgr config;
}
inline auto* sConfigMgr = &TestCore::config;

struct SpellEffectInfo
{
    int32 value = 0;
    int32 CalcValue() const { return value; }
};
struct SpellInfo
{
    std::set<AuraType> auras;
    uint32 attributes = 0;
    bool mountSkill = false;
    std::array<SpellEffectInfo, MAX_SPELL_EFFECTS> Effects;
    bool HasAura(AuraType type) const { return auras.count(type) != 0; }
    bool HasAttribute(uint32 flag) const { return (attributes & flag) != 0; }
    bool IsAbilityOfSkillType(uint32 skill) const { return skill == SKILL_MOUNTS && mountSkill; }
};
struct AuraEffect
{
    SpellInfo const* info;
    AuraType type;
    uint8 index;
    int32 amount;
    unsigned changes = 0;
    AuraType GetAuraType() const { return type; }
    SpellInfo const* GetSpellInfo() const { return info; }
    uint8 GetEffIndex() const { return index; }
    void ChangeAmount(int32 value) { amount = value; ++changes; }
};
struct Aura
{
    SpellInfo const* info;
    std::array<AuraEffect*, MAX_SPELL_EFFECTS> effects{};
    SpellInfo const* GetSpellInfo() const { return info; }
    AuraEffect* GetEffect(uint8 index) const { return effects.at(index); }
};
struct Player;
struct Unit
{
    virtual ~Unit() = default;
    virtual Player* ToPlayer() { return nullptr; }
};
struct Player : Unit
{
    uint8 level = 1;
    bool mounted = false;
    std::set<uint32> spells;
    std::vector<AuraEffect*> effects;
    Player* ToPlayer() override { return this; }
    uint8 GetLevel() const { return level; }
    bool IsMounted() const { return mounted; }
    bool HasSpell(uint32 spell) const { return spells.count(spell) != 0; }
    std::vector<AuraEffect*> GetAuraEffectsByType(AuraType type) const
    {
        std::vector<AuraEffect*> result;
        for (auto* effect : effects)
            if (effect->type == type)
                result.push_back(effect);
        return result;
    }
};
struct Spell
{
    Unit* caster;
    SpellInfo const* info;
    bool triggered = false;
    bool IsTriggered() const { return triggered; }
    SpellInfo const* GetSpellInfo() const { return info; }
    Unit* GetCaster() const { return caster; }
};

struct ScriptBase
{
    static inline std::vector<ScriptBase*> registry;
    virtual ~ScriptBase() = default;
    bool attached;
    explicit ScriptBase(bool add = true) : attached(add) { if (add) registry.push_back(this); }
};
template <typename Hook> struct HookSet : ScriptBase
{
    std::vector<Hook> hooks;
    explicit HookSet(std::initializer_list<Hook> value, bool add = true) : ScriptBase(add), hooks(value) { }
    bool HasHook(Hook hook) const { return hooks.empty() || std::find(hooks.begin(), hooks.end(), hook) != hooks.end(); }
};
struct WorldScript : HookSet<WorldHook>
{
    WorldScript(char const*, std::initializer_list<WorldHook> value) : HookSet(value) { }
    virtual void OnAfterConfigLoad(bool) { }
};
struct UnitScript : HookSet<UnitHook>
{
    UnitScript(char const*, bool add, std::initializer_list<UnitHook> value) : HookSet(value, add) { }
    virtual void OnAuraApply(Unit*, Aura*) { }
};
struct PlayerScript : HookSet<PlayerHook>
{
    PlayerScript(char const*, std::initializer_list<PlayerHook> value) : HookSet(value) { }
    virtual void OnPlayerLevelChanged(Player*, uint8) { }
};
struct AllSpellScript : HookSet<SpellHook>
{
    AllSpellScript(char const*, std::initializer_list<SpellHook> value) : HookSet(value) { }
    virtual void OnSpellCheckCast(Spell*, bool, SpellCastResult&) { }
};
