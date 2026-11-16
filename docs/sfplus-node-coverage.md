# SF+ node coverage

Checked 2026-11-16 against the official K-Mods export dated 2026-08-30. This is an export inventory, not a scan of the current playthrough or a guarantee that later SF+ versions contain no additional nodes.

## Covered resource groups

All resource classes referenced by the exported Modular Miner extraction definitions are covered by `Resources/costs.json`: iron/Siterite, copper/Callanite, limestone/Larrussite, caterium/Aurovite, quartz/Kyalite, bauxite/Montanion, coal, sulfur, uranium, SAM, Dirt, Peat, Loam and Kerr Crystal. Existing descriptor classes are reused despite different SF+ display names; duplicate entries are unnecessary.

Other covered sources are oil nodes, oil/water/nitrogen well satellites, geysers, Algal Mass nodes and Chlorine Gas well satellites. The table now contains 21 entries, including vanilla entries.

- **Algal Mass:** `Res_LiquidBioWater_C`, `fluid_node`. The map lists ordinary `FGResourceNode` actors, and the Algal Mass Extractor uses `FGBuildableResourceExtractor`, which the existing targeting and refresh code handles.
- **Chlorine Gas:** `Desc_Gas_Chlor_C`, `fracking_satellite`. The map lists three chlorine well cores with purity-bearing satellites. The standard fracking extractor explicitly permits this resource. Upgrade an individual satellite, not the central core.
- Surface water collection is not a finite purity-bearing node. The existing water entry applies to well satellites.

## Separate compatibility work

**Toxic Air** (`Desc_ToxicAir_C`) is present as `KLToxicResourceNode` actors. Its Air Collector is `KLBuildableAirCollector`, with scan-based recipes specifying range, hit counts, amount limits and cycle duration. The export does not establish a purity-based production multiplier. Adding a paid upgrade without verifying a production benefit would be misleading, so no cost entry is added. Runtime inheritance, targeting, collector yield and refresh behavior require separate investigation before support is claimed.

Refined Power water-dam and turbine nodes also appear in the shared export, but belong to a different mod and are outside this SF+ inventory. Scannable resources alone are not evidence of ordinary upgradeable extraction nodes.

## Proposed costs for the newly covered resources

| Resource | Impure to Normal | Normal to Pure |
|---|---|---|
| Algal Mass | 100 Reinforced Iron Plates + 25 Smart Plating | 100 Rotors + 75 Smart Plating |
| Chlorine satellite | 200 Steel Pipes + 20 Modular Engines + 5 Adaptive Control Units | 250 Steel Beams + 60 Modular Engines + 15 Adaptive Control Units |

Costs are provisional suggestions. They use inventory items rather than unpackaged fluids. Optional-mod entries may produce missing-class warnings when their mods are absent.

## Validation still required

After installing the config on the server, run `/nu reload` and `/nu validate`. Aim at each new resource or its extractor and run `/nu info` to confirm its descriptor and kind. Verify payment, real output rate, downgrade/refund, reconnect, and save/restart persistence. Export compatibility is not runtime validation. No C++ or binary changes are needed for these cost entries.

## Primary sources

- [World node export](https://k-mods.com/static/wiki/map.json)
- [Scannable resource allowlist](https://k-mods.com/static/wiki/allowedScannableResources.json)
- [Extractor allowlist](https://k-mods.com/static/wiki/allowedExtractors.json)
- [Modular Miner extraction definitions](https://k-mods.com/static/wiki/SatisfactoryPlus/AssetDatas/Miner/)
- [Algal Mass descriptor](https://k-mods.com/static/wiki/KLib/Assets/Ressources/Raw/Ores/Fluids/BioWater/Res_LiquidBioWater.json)
- [Algal Mass Extractor](https://k-mods.com/static/wiki/KLib/Assets/Buildings/BioWaterExtractor/Build_BioWaterExtractor_SF+.json)
- [Chlorine descriptor](https://k-mods.com/static/wiki/KLib/Assets/Ressources/Raw/Gas/Desc_Gas_Chlor.json)
- [Fracking Extractor](https://k-mods.com/static/wiki/Game/FactoryGame/Buildable/Factory/FrackingExtractor/Build_FrackingExtractor.json)
- [Air Collector](https://k-mods.com/static/wiki/KLib/Assets/Buildings/AirCollector/Build_AirCollector.json)
- [Toxic Air node scan definition](https://k-mods.com/static/wiki/SatisfactoryPlus/AssetDatas/Air/AirCollector_ToxicAirNode_AirCol1.json)
