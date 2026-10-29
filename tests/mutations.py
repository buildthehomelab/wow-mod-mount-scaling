#!/usr/bin/env python3
"""Prove behavioral tests reject representative faults in production code."""
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/MountScaling.cpp").read_text()
MUTATIONS = [('unit registration disabled',
  'UnitScript("MountScalingUnitScript", true',
  'UnitScript("MountScalingUnitScript", false'),
 ('Disable ground speed calculation',
  'float speed = sGroundJourneymanMin + (level - 40) * sGroundJourneymanPerLevel;',
  'float speed = sGroundJourneymanMin;'),
 ('Skip journeyman check for ground speed',
  'if (player->HasSpell(SPELL_JOURNEYMAN_RIDING) || HasFlyingRiding(player))',
  'if (false)'),
 ('Always use apprentice floor for ground speed',
  'if (!player->HasSpell(SPELL_APPRENTICE_RIDING))',
  'if (true)'),
 ('Zero ground speed multiplier',
  'float speed = std::max(sGroundMinSpeed, level * sGroundSpeedPerLevel);',
  'float speed = sGroundMinSpeed;'),
 ('Disable flying expert calculation', 'if (player->HasSpell(SPELL_EXPERT_RIDING))', 'if (false)'),
 ('Skip artisan check for flying speed', 'if (player->HasSpell(SPELL_ARTISAN_RIDING))', 'if (false)'),
 ('Return zero flying speed always', 'return 0;', 'return 1;'),
 ('Always clamp flying speed to min',
  'speed = std::clamp(speed, sFlyingExpertMin, sFlyingExpertMax);',
  'speed = sFlyingExpertMin;'),
 ('Disable mount speed check in flight calc',
  'if (mountSpeed >= FLYER_310_SPEED)',
  'if (mountSpeed >= FLYER_310_SPEED && false)'),
 ('Always use mount speed for flying', 'speed = std::max(speed, mountSpeed);', 'speed = mountSpeed;'),
 ('Skip level scaling for artisan flying',
  'float speed = sFlyingArtisanMin + (level - 70) * sFlyingArtisanPerLevel;',
  'float speed = sFlyingArtisanMin;'),
 ('Disable config loading in world script',
  'sEnabled              = sConfigMgr->GetOption<bool>("MountScaling.Enable", true);',
  'sEnabled = false;'),
 ('wrong world hook', '{WORLDHOOK_ON_AFTER_CONFIG_LOAD}', '{WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED}'),
 ('wrong unit hook', '{UNITHOOK_ON_AURA_APPLY}', '{UNITHOOK_ON_DAMAGE}'),
 ('wrong player hook', '{PLAYERHOOK_ON_LEVEL_CHANGED}', '{PLAYERHOOK_ON_PLAYER_JUST_DIED}'),
 ('wrong spell hook', '{ALLSPELLHOOK_ON_SPELL_CHECK_CAST}', '{ALLSPELLHOOK_ON_CALC_MAX_DURATION}'),
 ('triggered casts restricted', ' || spell->IsTriggered()', ''),
 ('existing cast error overwritten', ' || res != SPELL_CAST_OK', ''),
 ('quest skill filter omitted', ' || !spellInfo->IsAbilityOfSkillType(SKILL_MOUNTS)', ''),
 ('non-mount filter omitted', '!spellInfo->HasAura(SPELL_AURA_MOUNTED) || ', ''),
 ('flagged scripted flyer ignored',
  '\n            && !spellInfo->HasAttribute(SPELL_ATTR4_ONLY_FLYING_AREAS)',
  ''),
 ('event ground buffs scaled', '&& isMount)', '&& true)'),
 ('separate flight aura ignored', '(isMount || player->IsMounted())', '(isMount)'),
 ('unmounted level-up updated', ' || !player->IsMounted()', ''),
 ('disabled aura hook still active', 'if (!sEnabled)', 'if (!sEnabled && false)'),
 ('trained flyer refused', 'if (player && !HasFlyingRiding(player))', 'if (player)'),
 ('310 boundary not preserved', 'if (mountSpeed >= FLYER_310_SPEED)', 'if (mountSpeed > FLYER_310_SPEED)')]


def check_mutation(mutation):
    name, old, new = mutation
    if old not in SOURCE:
        return name, False, "Mutation anchor missing: update the sentinel for the new source"
    with tempfile.TemporaryDirectory(prefix="mount-scaling-mutation-") as temporary:
        source = Path(temporary) / "MountScaling.cpp"
        source.write_text(SOURCE.replace(old, new, 1))
        environment = dict(os.environ, MOUNT_SCALING_SOURCE=str(source))
        environment.pop("BUILD_DIR", None)
        environment.pop("CXXFLAGS", None)
        result = subprocess.run([sys.executable, str(ROOT / "tests/run.py")], env=environment,
                                capture_output=True, text=True, timeout=120)
        output = result.stdout + result.stderr
        # Compile errors do not count as a caught behavioral regression.
        killed = result.returncode != 0 and "FAIL:" in output and "error:" not in output
        return name, killed, output[-1500:]


if __name__ == "__main__":
    subprocess.run([sys.executable, str(ROOT / "tests/run.py")], check=True)
    workers = min(4, os.cpu_count() or 1)
    with ThreadPoolExecutor(max_workers=workers) as pool:
        results = list(pool.map(check_mutation, MUTATIONS))
    for name, killed, output in results:
        print(f'{"CAUGHT" if killed else "MISSED"}: {name}')
        if not killed:
            print(output)
    caught = sum(killed for _, killed, _ in results)
    print(f"Caught {caught}/{len(results)} representative behavioral regressions")
    sys.exit(0 if caught == len(results) else 1)
