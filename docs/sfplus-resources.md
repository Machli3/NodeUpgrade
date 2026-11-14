# Ficsit Farming and KLib resources, with SF+ mapping notes

Research date: 2026-11-14. These are mappings from the public K-Mods wiki export, not a dump of the user's installed game. The export contains historical asset names that differ from current display names. It is not a complete inventory of all world nodes or fluids.

## Existing keys reused by SF+

The Modular Miner raw extraction definitions reference these vanilla descriptors. The existing `solid` cost entries therefore also apply to these SF+ resources if the world actors have the standard node type. Do not add duplicate keys: the parser ignores a second entry with the same kind and class.

| SF+ display name in export | Resource class | Existing cost entry |
|---|---|---|
| Siterite Ore | `Desc_OreIron_C` | iron |
| Callanite Ore | `Desc_OreCopper_C` | copper |
| Larrussite Ore | `Desc_Stone_C` | limestone |
| Aurovite Ore | `Desc_OreGold_C` | caterium |
| Kyalite | `Desc_RawQuartz_C` | raw_quartz |
| Montanion Ore | `Desc_OreBauxite_C` | bauxite |
| Coal | `Desc_Coal_C` | coal |
| SAM | `Desc_SAM_C` | sam |

Other Modular Miner definitions reference `Desc_OreUranium_C` and `Desc_Sulfur_C`, also already covered by the existing solid entries. Existing payments have been preserved. These mappings do not establish that every vanilla payment item is obtainable or appropriately priced at the same stage in SF+.

## Additional entries

Dirt, Peat, and Loam belong to Ficsit Farming; Kerr Crystal belongs to KLib. These optional cost entries are not specific to SF+.

| Resource | Proposed node kind | Resource/payment class | Impure to Normal | Normal to Pure |
|---|---|---|---:|---:|
| Dirt | solid | `Desc_FF_Dirt_C` | 675 | 1800 |
| Peat | solid | `Desc_FF_Dirt_Fertilized_C` | 675 | 1800 |
| Loam | solid | `Desc_FF_Dirt_Wet_C` | 675 | 1800 |
| Kerr Crystal | solid | `Res_KerrCrystal_C` | 200 | 500 |

All four exported resource descriptors have `mForm = RF_SOLID`, and are referenced by Modular Miner extraction definitions. The `_C` suffix is the generated Blueprint class name expected by NodeUpgrade. The `solid` world node kind and those runtime class names must still be confirmed with `/nu info` in the installed SF+ version; descriptor form alone does not prove the actor's node type.

Amounts are provisional choices for this configuration, not prices published by SF+ or derived from its recipes. Each entry charges its own raw resource to avoid assuming a processed-item recipe. Missing optional mods leave these entries unused, but the current startup validation and `/nu validate` will warn about their missing payment classes.

## Sources and reproduction

- [Official Modular Miner export directory](https://k-mods.com/static/wiki/SatisfactoryPlus/AssetDatas/Miner/). The compact research snapshot in [sfplus-miner-research.json](sfplus-miner-research.json) records each extraction definition's resource, name, drill tier, and export timestamp. These are descriptor references, not a world actor scan.
- [Dirt descriptor](https://k-mods.com/static/wiki/FicsitFarming/Resources/RawResources/Dirt/Desc_FF_Dirt.json)
- [Peat descriptor](https://k-mods.com/static/wiki/FicsitFarming/Resources/RawResources/Dirt/Desc_FF_Dirt_Fertilized.json)
- [Loam descriptor](https://k-mods.com/static/wiki/FicsitFarming/Resources/RawResources/Dirt/Desc_FF_Dirt_Wet.json)
- [Kerr Crystal descriptor](https://k-mods.com/static/wiki/KLib/Assets/Ressources/Raw/Nodes/QuantumCystal/Res_KerrCrystal.json)

In-game, aim at each resource and run `/nu info` to verify `Class` and `kind`. Run `/nu reload` after editing the server's costs, then `/nu validate` and test an upgrade and downgrade. This research does not establish compatibility of SF+'s custom Modular Miner production caches with NodeUpgrade's vanilla extractor refresh code; verify actual extraction rates as well as the purity shown in the UI. Fluids, custom actors, and resources introduced by other installed mods require additional inspection.
