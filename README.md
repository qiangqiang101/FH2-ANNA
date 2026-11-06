<h1 align="center">FH2 ANNA</h1>

**ANNA**, the in-car assistant from Forza Horizon 3/4/5/6, brought to **Forza Horizon 2** on PC (Azimuth). Forza Horizon 2 kept ANNA's HUD hidden away; FH2 ANNA turns it back on with Forza Horizon 3's look, round minimap, glowing edge and the list beside it, and fills it with things to do while you drive. Other mods can add their own entries to it.

FH2 ANNA is a mod for the [FH2 Mod Platform](https://github.com/Trinculo54/fh2-mod-platform).

## What's in the menu

**Car**

- **Enable Auto Drive**: the car drives itself along your GPS route. Greyed out until a route is set. A few seconds in, the camera switches to slow cinematic shots and the HUD hides; touch the gas, brake or steering to take over. Works online too.
- **Lights**: headlights, hazard lights, left and right indicators. Headlights shine in daylight too, and pop-up headlights open and close with them.
- **Doors**: open or close all, the roof, hood, trunk, each door, storage compartments, pop-up headlights and active spoilers, whatever the car can move. The Lights and Doors lists stay open so you can flip things back and forth.

**Set Route To Nearest**: Race, Bucket List, Festival, Barn Find or Bonus Board.

## Controls

| Controller | Keyboard ([FH2 Mouse & Keyboard](https://github.com/qiangqiang101/FH2Mouse)) | Does |
|---|---|---|
| Tap D-pad down | C | Open ANNA (C again closes it) |
| D-pad up / down | I / K | Move up / down the list |
| D-pad right | L | Choose |
| D-pad left | J | Back (closes ANNA from the top list) |

Holding D-pad down still opens the telemetry, as before. While ANNA is open, the D-pad is ANNA's, so it doesn't change the radio station.

FH2 ANNA reads the controller only. The keyboard keys come from FH2 Mouse & Keyboard, which presses the D-pad for you and shows its keys in ANNA's button pictures while you play on the keyboard. Change them on **Pause > Settings > Keyboard & Mouse** (the ANNA rows).

## Installing

1. Download the latest `FH2ANNA-<version>.zip` from [Releases](../../releases).
2. Extract it into your Azimuth folder, the one with `azimuth.exe` in it. You should end up with `DWrite.dll` next to `azimuth.exe`, and `FH2ANNA.dll` in the `fh2-mods` folder.

> [!WARNING]
> `FH2ANNA.dll` goes in **`fh2-mods`**, not in Extra Options' `mods` folder. FH2 ANNA is a platform mod, not an Extra Options plugin: in `mods` it won't load properly.
3. Start the game. ANNA's label sits under the minimap; tap D-pad down while driving.

The zip includes the FH2 Mod Platform (`DWrite.dll`). If you already have it from another mod, keep whichever is newer.

**ANNA needs the minimap on** in the game's HUD options. With the minimap off, ANNA's off too.

## Settings

On **Pause > Settings > MODS**:

| Setting | Default | Does |
|---|---|---|
| Autodrive cinematic camera | On | The cinematic shots and hidden HUD during Auto Drive |
| Hide hood and trunk in ANNA | Off | Leaves the hood and trunk out of Doors, and Open All leaves them shut: for cars with an invisible trunk or a fake engine |
| Close doors when moving | On | The doors, hood, trunk and storage ANNA opened shut by themselves once the car drives off (the roof stays as it is) |

They're saved in `fh2-mods\FH2ANNA.ini`.

## With other mods

- **[Extra Options](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions)**: FH2 ANNA has its own autodrive, indicators, hazards and headlights, so you don't need Extra Options' for those. Extra Options puts its autodrive on D-pad down and its hazard lights on D-pad up, which are ANNA's buttons, so turn those two off in `FH2_ExtraOptions.ini` (next to `azimuth.exe`), under `[Bindings]`:

  ```ini
  AutodriveXbox=None
  HazardsXbox=None
  ```

  Otherwise opening ANNA also starts Extra Options' autodrive, and moving up ANNA's list switches the hazard lights. Extra Options' headlight key still works alongside ANNA's; whichever you used last wins.
- **FH2 Mouse & Keyboard**: gives ANNA its keyboard keys and key pictures (see Controls).

## For mod developers: adding to ANNA's menu

Other platform mods can add entries and lists to ANNA through its interface, [`include/fh2anna.h`](include/fh2anna.h):

```c
#include "fh2anna.h"

static void honk(void* user, const char* key) { /* ... */ }

/* from a thread of yours, after api->wait_for_mods(...) */
const FH2AnnaApi* anna = (const FH2AnnaApi*)api->get_interface(FH2ANNA_INTERFACE, 4);
if (anna) {
    FH2AnnaEntry e = {sizeof e};
    e.key       = "honkhonk.horn";   /* unique: start it with your mod's name */
    e.parent    = "car";             /* or NULL for the top list, or a list of yours */
    e.label     = "Honk";
    e.on_choose = honk;
    anna->add_entry(&e);
}
```

ANNA's own lists are `car`, `car.lights`, `car.doors` and `route`. An entry without `on_choose` is a list of its own. Entries can hide themselves (`shown`), change their label each time their list opens (`label_now`), keep ANNA open after they're chosen (`stays_open`), and show greyed out (`enabled`). `on_choose` runs on the game's thread. The header has the details.

## Building

You need CMake and either Visual Studio 2022 or [llvm-mingw](https://github.com/mstorsjo/llvm-mingw). The FH2 Mod Platform is a submodule:

```sh
git clone --recursive https://github.com/qiangqiang101/FH2ANNA.git
cd FH2ANNA

# Visual Studio 2022
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target fh2anna     # build/Release/FH2ANNA.dll

# or llvm-mingw (what the release workflow uses)
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=platform/cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build                                       # build/FH2ANNA.dll
```

Releases are built by GitHub Actions ([`.github/workflows/build.yml`](.github/workflows/build.yml)) when a release is published, and on demand from the Actions tab (**Run workflow**). A release's tag has to match `VERSION` in `CMakeLists.txt`.

### The FH3 assets

`assets/` holds Forza Horizon 3's ANNA textures, converted to Forza Horizon 2's format, and FH3's ANNA HUD control ported to FH2. Both are built into the DLL. They're made from an FH3 install with the scripts in `tools/`:

```sh
python tools/fh3_textures.py "<FH3 folder>"     # assets/*.swatchbin
python tools/port_anna_xaml.py "<FH3 folder>"   # assets/HUDAnnaControl.xaml (the look: colours, rows, layout)
```

## Credits

- **Playground Games**: ANNA, and Forza Horizon 3's ANNA design and textures.
- **[Extra Options](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions)** by RokkuDayo: how FH2's autodrive, indicators headlights, the autodrive cinematic camera and daylight headlights are ported from.
- **[FH2 Mod Platform](https://github.com/Trinculo54/fh2-mod-platform)**, which includes [MinHook](https://github.com/TsudaKageyu/minhook).

## License

[MIT](LICENSE).
