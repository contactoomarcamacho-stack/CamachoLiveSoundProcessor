#pragma once
namespace vpx {
struct EnvironmentPreset { const char* name; float eq[6]; };
static constexpr EnvironmentPreset presets[] = {
 {"Flat / Manual", {.50f,.50f,.50f,.50f,.50f,.50f}},
 {"Club Live", {.54f,.48f,.46f,.50f,.53f,.52f}},
 {"Festival Outdoor", {.55f,.51f,.49f,.52f,.53f,.51f}},
 {"Warm Club", {.53f,.54f,.52f,.49f,.47f,.46f}},
 {"Vocal Boost", {.46f,.48f,.52f,.57f,.55f,.51f}},
 {"Sub Heavy", {.59f,.53f,.46f,.47f,.49f,.48f}}
};
static constexpr int presetCount = sizeof(presets)/sizeof(presets[0]);
}
