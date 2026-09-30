# mod-mount-scaling

An [AzerothCore](https://www.azerothcore.org) module that replaces WoW's binary mount speed system with smooth, level-based speed progression. Instead of jumping from 0% to 60% to 100% at fixed level thresholds, mount speed scales gradually as your character levels up.

## How It Works

In vanilla WotLK, mount speeds are fixed values tied to riding skill tier. This module overrides those fixed values with a formula that scales speed based on the player's current level:

- **Apprentice Riding** can be learned at **level 1** (down from 20) and starts at 20% speed, increasing with each level.
- **Journeyman, Expert, and Artisan** riding tiers retain their original level requirements but gain progressive speed scaling within their tier.
- Speed updates **in real time** when leveling up while mounted.

## Speed Tiers

### Ground Mounts

| Riding Skill | Level Req | Speed Range | Formula |
|---|---|---|---|
| Apprentice (Spell 33388) | **1** (changed from 20) | 20% - 100% | `max(20, level * 2.5)` |
| Journeyman (Spell 33391) | 40 (unchanged) | 100% - 150% | `100 + (level - 40) * 2.5` |

### Flying Mounts

| Riding Skill | Level Req | Speed Range | Formula |
|---|---|---|---|
| Expert (Spell 34090) | 60 (unchanged) | 150% - 200% | `150 + (level - 60) * 5` |
| Artisan (Spell 34091) | 70 (unchanged) | 200% - 280% | `200 + (level - 70) * 8` |

### Speed Progression Examples

**Ground (Apprentice Riding):**

| Level | Mount Speed | Total Movement |
|---|---|---|
| 1 | 20% | 120% |
| 8 | 20% (floor) | 120% |
| 10 | 25% | 125% |
| 20 | 50% | 150% |
| 30 | 75% | 175% |
| 40 | 100% (cap) | 200% |

**Ground (Journeyman Riding, learned at 40):**

| Level | Mount Speed | Total Movement |
|---|---|---|
| 40 | 100% | 200% |
| 44 | 110% | 210% |
| 50 | 125% | 225% |
| 60 | 150% (cap) | 250% |

**Flying (Expert Riding, learned at 60):**

| Level | Mount Speed | Total Movement |
|---|---|---|
| 60 | 150% | 250% |
| 65 | 175% | 275% |
| 70 | 200% (cap) | 300% |

**Flying (Artisan Riding, learned at 70):**

| Level | Mount Speed | Total Movement |
|---|---|---|
| 70 | 200% | 300% |
| 75 | 240% | 340% |
| 80 | 280% (cap) | 380% |

### 310% Flying Mounts

Mounts that fly at 310% on their own (Ashes of Al'ar, the arena and Glory drakes, and the 310%
versions of scaling mounts like Invincible and the Celestial Steed) keep 310% at every level. The
scaling only ever raises their speed, never lowers it. Normal Artisan mounts still follow the
Artisan curve above.

## Installation

This module follows the standard AzerothCore module structure. Place it in the `modules/` directory:

```
modules/mod-mount-scaling/
├── README.md
├── src/
│   ├── MountScaling_loader.cpp
│   └── MountScaling.cpp
├── conf/
│   └── mount_scaling.conf.dist
└── data/
    └── sql/
        └── db_world/
            └── mount_scaling_trainer.sql
```

AzerothCore's build system automatically discovers modules in the `modules/` directory. No `CMakeLists.txt` is needed.

### Build

```bash
# Local (Docker)
docker compose up -d --build

# On a remote server
cd /opt/wow-server && git pull && docker compose up -d --build
```

### SQL

The SQL in `data/sql/db_world/` must be applied to the `acore_world` database. It makes two changes:

1. Sets `ReqLevel = 1` for Apprentice Riding in the `trainer_spell` table, allowing it to be purchased at any level.
2. Sets `RequiredLevel = 1` for all ground mount items that previously required level 20 or below, so they can be equipped immediately.

**Important:** After applying the SQL, players must **delete their WoW client's `Cache` folder** (specifically `Cache/WDB/enUS/itemcache.wdb`) for the item level requirement change to take effect on their client.

## Configuration

All speed formula parameters are configurable via `mount_scaling.conf.dist`. Copy it to `mount_scaling.conf` to customize values.

| Config Option | Default | Description |
|---|---|---|
| `MountScaling.Enable` | `1` | Enable/disable the module |
| `MountScaling.Ground.SpeedPerLevel` | `2.5` | Apprentice speed gain per level |
| `MountScaling.Ground.MinSpeed` | `20` | Apprentice minimum speed floor |
| `MountScaling.Ground.MaxSpeed` | `100` | Apprentice maximum speed cap |
| `MountScaling.Ground.Journeyman.SpeedPerLevel` | `2.5` | Journeyman speed gain per level above 40 |
| `MountScaling.Ground.Journeyman.MinSpeed` | `100` | Journeyman minimum speed |
| `MountScaling.Ground.Journeyman.MaxSpeed` | `150` | Journeyman maximum speed cap |
| `MountScaling.Flying.Expert.SpeedPerLevel` | `5` | Expert speed gain per level above 60 |
| `MountScaling.Flying.Expert.MinSpeed` | `150` | Expert minimum flying speed |
| `MountScaling.Flying.Expert.MaxSpeed` | `200` | Expert maximum flying speed cap |
| `MountScaling.Flying.Artisan.SpeedPerLevel` | `8` | Artisan speed gain per level above 70 |
| `MountScaling.Flying.Artisan.MinSpeed` | `200` | Artisan minimum flying speed |
| `MountScaling.Flying.Artisan.MaxSpeed` | `280` | Artisan maximum flying speed cap |

## Technical Details

### How Mount Speed Works in AzerothCore

Mount spells apply auras to the player:
- **Ground speed:** `SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED` (aura type 32)
- **Flying speed:** `SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED` (aura type 207)

The aura's "amount" value equals the speed percentage (e.g., 60 for a 60% speed mount). The game engine reads this value via `GetMaxPositiveAuraModifier()` in `Unit::UpdateSpeed()` to determine the player's movement speed.

### How This Module Overrides Speed

The module uses three AzerothCore script hooks:

1. **`UnitScript::OnAuraApply`** — When a mount speed aura is applied to a player, the module checks their riding skill and level, calculates the appropriate speed, and calls `AuraEffect::ChangeAmount()` to override the aura's amount. This internally triggers `UpdateSpeed()` so the change takes effect immediately.

2. **`PlayerScript::OnPlayerLevelChanged`** — When a mounted player levels up, the module finds their active mount speed auras via `GetAuraEffectsByType()` and updates the amounts with the new level-based calculation.

3. **`WorldScript::OnAfterConfigLoad`** — Loads configuration values from `mount_scaling.conf` into memory on server startup and when the config is reloaded.

### Riding Skill Detection

The module checks which riding tier a player has by testing for the riding skill spells:

| Spell ID | Riding Tier |
|---|---|
| 33388 | Apprentice Riding (75 skill) |
| 33391 | Journeyman Riding (150 skill) |
| 34090 | Expert Riding (225 skill) |
| 34091 | Artisan Riding (300 skill) |

Higher tiers always take priority. For example, a level 45 player with both Apprentice and Journeyman Riding will use the Journeyman formula.

## Testing

Use these GM commands in-game to verify the module:

```
-- Check speed with this macro (create via /macro):
/script DEFAULT_CHAT_FRAME:AddMessage("Speed: "..string.format("%.1f%%", GetUnitSpeed("player")/7*100))

-- Learn Apprentice Riding and get a mount
.learn 33388
.additem 2411

-- Level up while mounted to see speed change in real time
.levelup 10

-- Reset to level 1 for testing
.character level YourName 1

-- Learn higher riding tiers
.learn 33391          -- Journeyman
.learn 34090          -- Expert
.learn 34091          -- Artisan
```

## Compatibility

- **AzerothCore:** WotLK 3.3.5a (game build 12340)
- **Cold Weather Flying** (spell 54197, level 77): Not affected by this module. It is a zone unlock, not a speed modifier.
- **Other modules:** No known conflicts. The module only modifies mount speed aura amounts and does not alter core game files.
