# Atlas TF2 Lua API

Public API reference for Lua scripts executed by Atlas for Team Fortress 2.

**Runtime:** Lua 5.5.1  
**Platform:** Windows  
**Last updated:** 4 November 2026

> All identifiers are case-sensitive. Atlas uses lowercase `snake_case` for most APIs, but compatibility methods such as `GetPropInt`, `GetLocalPlayer`, and `Command` retain their exact capitalization.

## Contents

- [Quick start](#quick-start)
- [Runtime and limits](#runtime-and-limits)
- [Events and script lifecycle](#events-and-script-lifecycle)
- [Colors](#colors)
- [Vectors](#vectors)
- [`atlas`](#atlas)
- [`draw`](#draw)
- [`input`](#input)
- [HTML panels](#html-panels)
- [`entities`](#entities)
- [Entity methods](#entity-methods)
- [Network properties](#network-properties)
- [`engine` and `client`](#engine-and-client)
- [Console variables](#console-variables)
- [User commands](#user-commands)
- [Game events](#game-events)
- [Configuration](#configuration)
- [Constants and TF2 values](#constants-and-tf2-values)
- [Migration from other Lua APIs](#migration-from-other-lua-apis)
- [Complete examples](#complete-examples)
- [Error behavior](#error-behavior)
- [API availability summary](#api-availability-summary)

## Quick start

```lua
atlas.on("paint", function()
    if not engine.in_game() then
        return
    end

    draw.text(
        20,
        20,
        color(110, 185, 255, 255),
        "Atlas Lua is running",
        "menu_bold",
        "left",
        true
    )
end)
```

Scripts execute once at load time. Register persistent behavior with `atlas.on` or `events.listen`.

## Runtime and limits

### Available standard libraries

Atlas opens these Lua 5.5.1 libraries:

- Base
- `coroutine`
- `table`
- `string`
- `math`
- `utf8`

Atlas does not open:

- `package` or `require`
- `io`
- `os`
- `debug`

`dofile` and `loadfile` are disabled. Atlas replaces `load` with a text-only loader and replaces `print` with an Atlas log writer.

### Runtime limits

| Resource | Limit |
|---|---:|
| Loaded scripts | 16 |
| Stored scripts | 64 |
| Script name length | 31 characters |
| Script source | 3 MiB |
| Memory per script | 64 MiB |
| One top-level call or callback | 250 ms |
| Script log | 256 lines |
| Characters retained per log line | 191 |

Valid stored script names contain lowercase `a-z`, digits, `-`, or `_`.

### `print`

```lua
print(value1, value2, ...)
```

Values are converted with Lua string conversion, separated by spaces, and written to the Atlas script log. `print` returns no values.

### `load`

```lua
local chunk, message = load(source, chunk_name, mode, environment)
```

Atlas only accepts text chunks. The environment in argument four is supported. Binary chunks are rejected. File-based loaders are unavailable.

### Uncaught errors

An uncaught top-level or callback error records a traceback, displays an Atlas Lua error notification, and marks the script for unloading. Use `pcall` when a failure should remain local to the current operation.

## Events and script lifecycle

### `atlas.on`

```lua
atlas.on(event_name, handler)
```

Registers a handler and returns no values. Multiple handlers for the same event run in registration order.

| Event | Handler signature | Timing |
|---|---|---|
| `paint` | `function()` | Atlas rendering pass. Draw calls are legal here. |
| `create_move_pre_pred` | `function(cmd)` | Before movement prediction. |
| `create_move` | `function(cmd)` | Main user-command processing pass. |
| `create_move_post_pred` | `function(cmd)` | After movement prediction. |
| `frame_stage` | `function(stage)` | Client frame-stage transition. |
| `level_init` | `function()` | A level is initialized. |
| `level_shutdown` | `function()` | The current level shuts down. |
| `unload` | `function()` | The script is explicitly unloaded, replaced, or Atlas shuts down its script host. |

The `unload` event is not a recovery hook for an already-failed script. Objects owned by the Lua state, including HTML panels, are still cleaned up when the state closes.

### Frame-stage values

| Value | Stage |
|---:|---|
| `-1` | `FRAME_UNDEFINED` |
| `0` | `FRAME_START` |
| `1` | `FRAME_NET_UPDATE_START` |
| `2` | `FRAME_NET_UPDATE_POSTDATAUPDATE_START` |
| `3` | `FRAME_NET_UPDATE_POSTDATAUPDATE_END` |
| `4` | `FRAME_NET_UPDATE_END` |
| `5` | `FRAME_RENDER_START` |
| `6` | `FRAME_RENDER_END` |

## Colors

### `color`

```lua
color(red, green, blue, alpha) -> integer
```

Creates Atlas's packed `0xRRGGBBAA` color value. Each channel must be an integer from `0` through `255`.

Defaults:

```lua
color()             -- opaque black
color(255, 80, 40)  -- alpha defaults to 255
```

Packed colors are opaque API values accepted by drawing and configuration functions. Alpha occupies the low byte.

## Vectors

### Constructor

```lua
vec3(x, y, z) -> vec3
```

All components default to zero.

```lua
local origin = vec3(100, 200, 64)
local zero = vec3()
```

### Fields

`x`, `y`, and `z` are readable and writable.

```lua
local value = vec3(1, 2, 3)
value.z = 10
```

Assigning any other field raises an error.

### Methods

| Signature | Returns | Description |
|---|---|---|
| `value:length()` | number | Three-dimensional magnitude. |
| `value:length_2d()` | number | Magnitude using `x` and `y`. |
| `value:dot(other)` | number | Dot product. |
| `value:cross(other)` | vec3 | Cross product. |
| `value:distance(other)` | number | Distance to another vector. |
| `value:normalized()` | vec3 | Normalized copy. The original is unchanged. |
| `value:unpack()` | x, y, z | Returns three numbers. |

### Operators

| Expression | Behavior |
|---|---|
| `a + b` | Component addition. |
| `a - b` | Component subtraction. |
| `a * number` | Scalar multiplication. |
| `number * a` | Scalar multiplication. |
| `a * b` | Component-wise multiplication. |
| `a / number` | Scalar division. |
| `-a` | Component negation. |
| `a == b` | Exact component equality. |
| `tostring(a)` | `vec3(x, y, z)` representation. |

## `atlas`

| Signature | Returns | Description |
|---|---|---|
| `atlas.on(event_name, handler)` | nothing | Registers a lifecycle handler. |
| `atlas.name()` | string | Current Atlas user name. |
| `atlas.script()` | string | Name of the running script. |
| `atlas.notify(text, kind)` | nothing | Displays a notification titled with the script name. |
| `atlas.time()` | number | Monotonic system interrupt time in seconds. |

### Notifications

```lua
atlas.notify("Configuration loaded", "success")
```

Accepted kinds:

- `"info"`, the default
- `"success"`
- `"warning"`
- `"error"`

Any other kind raises an argument error.

## `draw`

Screen coordinates use pixels with `(0, 0)` at the upper-left corner.

Except for `draw.world_to_screen`, drawing functions only work inside a `paint` handler. Calling them elsewhere raises `draw functions only work inside a paint handler`.

### Functions

| Signature | Returns | Description |
|---|---|---|
| `draw.line(x1, y1, x2, y2, color, thickness)` | nothing | Draws a line. Thickness defaults to `1`. |
| `draw.rect(x, y, width, height, color)` | nothing | Draws an outlined rectangle. |
| `draw.rect_filled(x, y, width, height, color)` | nothing | Draws a filled rectangle. |
| `draw.circle(x, y, radius, color, thickness)` | nothing | Draws a circle. Thickness defaults to `1`. |
| `draw.text(x, y, color, text, font, align, outlined)` | nothing | Draws text. |
| `draw.world_to_screen(position)` | `x, y` or `nil` | Projects a `vec3` into screen coordinates. |

### Text options

```lua
draw.text(x, y, value, text, font, align, outlined)
```

- `font` defaults to `"esp"`.
- `align` defaults to `"left"`.
- `outlined` defaults to `false`.

Font identifiers:

| Identifier | Backing font |
|---|---|
| `esp` | Tahoma 11 |
| `esp_small` | Small Fonts 7 |
| `menu` | Tahoma 11 |
| `menu_bold` | Tahoma Bold 11 |
| `menu_small` | Tahoma 10 |
| `mono` | Consolas 12 |

Alignment identifiers:

- `left`
- `center`
- `right`

Atlas does not expose a Lua text-measurement function. Use center alignment for whole labels or a known fixed advance with the `mono` font when positioning characters independently.

### World-to-screen behavior

```lua
local x, y = draw.world_to_screen(world_position)
if x ~= nil then
    draw.circle(x, y, 4, color(255, 255, 255), 1)
end
```

Projection failure returns one `nil`, not two numeric values.

## `input`

Input keys are numeric Windows virtual-key codes in the range `0` through `255`. Input functions return false when the game window is not focused.

| Signature | Returns | Description |
|---|---|---|
| `input.is_down(key)` | boolean | True while the key is held. |
| `input.is_pressed(key)` | boolean | True on the key's press edge. |
| `input.mouse()` | `x, y` | Current mouse coordinates. |
| `input.menu_open()` | boolean | Whether the Atlas menu is open. |

Inside a create-move event, Atlas uses its command-batched input state. Outside create-move, it uses the current frame input state.

Common virtual-key values:

| Key | Value |
|---|---:|
| Left mouse | `0x01` |
| Right mouse | `0x02` |
| Middle mouse | `0x04` |
| Backspace | `0x08` |
| Tab | `0x09` |
| Enter | `0x0D` |
| Shift | `0x10` |
| Control | `0x11` |
| Alt | `0x12` |
| Escape | `0x1B` |
| Space | `0x20` |
| Insert | `0x2D` |
| Delete | `0x2E` |
| `0` through `9` | `0x30` through `0x39` |
| `A` through `Z` | `0x41` through `0x5A` |
| F1 through F12 | `0x70` through `0x7B` |

## HTML panels

Atlas can render local HTML or an HTTP(S) page into an off-screen panel.

### Constructors

| Signature | Returns | Description |
|---|---|---|
| `ui.html(width, height, source)` | html | Creates a panel from an HTML string. |
| `ui.web(width, height, url)` | html | Creates a panel from an `http://` or `https://` URL. |

Width and height must each be from `1` through `2048`. Atlas supports up to eight HTML panels globally. Local HTML source and web URLs are limited to 1 MiB.

Panel creation raises an error if Steam HTML is unavailable, the dimensions are invalid, the URL is invalid, or no panel slot can be opened.

### Panel methods

| Signature | Returns | Description |
|---|---|---|
| `panel:draw(x, y, alpha)` | nothing | Draws at the panel's fixed size. Alpha defaults to `255`. Paint-only. |
| `panel:load(source_or_url)` | boolean | Replaces the current local document or web URL. |
| `panel:send(text)` | boolean | Sends text to JavaScript through `window.atlas.receive`. |
| `panel:receive()` | string or `nil` | Removes and returns the oldest JavaScript message. |
| `panel:close()` | nothing | Releases the panel. |

The garbage collector closes a panel when its Lua object is collected. Explicitly closing it in `unload` is still useful when the script controls panel lifetime.

### Local HTML bridge

`ui.html` injects:

```javascript
window.atlas = {
    send: function (message) { alert(String(message)); },
    receive: null
};
```

Use `atlas.send(...)` in JavaScript to enqueue a message for Lua. Assign `atlas.receive` to accept messages sent from Lua.

```lua
local panel = ui.html(320, 120, [[
<style>
body {
    margin: 0;
    background: #101419;
    color: #e8eef5;
    font: 14px sans-serif;
}
button {
    margin: 20px;
}
</style>
<button onclick="atlas.send('clicked')">Send to Lua</button>
<script>
atlas.receive = function (message) {
    document.body.dataset.message = message;
};
</script>
]])

atlas.on("paint", function()
    panel:draw(100, 100)

    local message = panel:receive()
    if message ~= nil then
        print(message)
    end
end)

atlas.on("unload", function()
    panel:close()
end)
```

Local HTML uses a restrictive content policy:

- Inline scripts and styles are allowed.
- Images, fonts, and media may use `data:` URLs.
- External scripts, styles, frames, and network requests are blocked.

`ui.web` does not inject the local bridge. A web page that needs messaging must provide compatible `window.atlas.send` and `window.atlas.receive` behavior itself.

JavaScript-to-Lua messages are limited to 511 characters. Each panel queues up to 32 messages; when full, the oldest message is discarded. Lua-to-JavaScript messages are limited to 1 MiB. `panel:send` can return false while the browser is still opening.

## `entities`

| Signature | Returns | Description |
|---|---|---|
| `entities.players()` | table | Fresh 1-based array of cached player entities. |
| `entities.buildings()` | table | Fresh 1-based array of cached building entities. |
| `entities.projectiles()` | table | Fresh 1-based array of cached projectile entities. |
| `entities.local_player()` | entity or `nil` | Current local player. |
| `entities.GetLocalPlayer()` | entity or `nil` | Compatibility alias for `local_player`. |

Entity lists are not automatically filtered by life state, dormancy, team, or local-player identity.

```lua
for _, player in ipairs(entities.players()) do
    if player:valid() and player:alive() and not player:dormant() then
        print(player:index(), player:name())
    end
end
```

Atlas does not expose arbitrary entity lookup by index, user ID, class name, or a general all-entities list.

## Entity methods

Entity objects retain a cached handle, class ID, and kind. Most methods resolve the handle again on every call.

Always use `entity:valid()` before retaining an entity across frames. Calling most methods on an expired entity raises `entity is no longer valid`.

### Identity and general methods

| Signature | Returns | Entity kinds | Description |
|---|---|---|---|
| `entity:valid()` | boolean | all | Whether the cached handle currently resolves. |
| `entity:index()` | integer | all | Current entity index. |
| `entity:class_id()` | integer | all | Cached `ETFClassID`. Available after expiration. |
| `entity:kind()` | string | all | `player`, `building`, `projectile`, `weapon`, or `other`. |
| `entity:origin()` | vec3 | all | Absolute origin. |
| `entity:team()` | integer | all | Team number. |
| `entity:dormant()` | boolean | all | Dormancy state. |
| `entity:is_enemy()` | boolean | all | True when a local player exists and team numbers differ. |
| `entity:is_local()` | boolean | all | True when this is the local player. |
| `entity:health()` | integer | player, building | Current health. |
| `entity:max_health()` | integer | player, building | Maximum health. Player returns `0` if player resources are unavailable. |
| `entity:AttributeHookFloat(name, base)` | number | all | Applies an item/entity attribute hook. Base defaults to `0`. |

`left == right` compares entity handles. It does not require either handle to remain valid.

### Player-only methods

Calling these methods on another entity kind raises `player expected`.

| Signature | Returns | Description |
|---|---|---|
| `player:alive()` | boolean | Current alive state. |
| `player:class()` | integer | TF2 class number. |
| `player:name()` | string or `nil` | Player name, or nil if player information is unavailable. |
| `player:eye_position()` | vec3 | Absolute eye position. |
| `player:eye_angles()` | vec3 | Eye angles. |
| `player:velocity()` | vec3 | Networked velocity. |
| `player:flags()` | integer | `m_fFlags` bit field. |
| `player:in_cond(condition)` | boolean | Whether the player has the numeric `ETFCond`. |
| `player:active_weapon()` | entity or `nil` | Active weapon entity. |
| `player:GetEntityForLoadoutSlot(slot)` | entity or `nil` | Entity occupying a loadout slot. |

### Weapon-only methods

Calling these methods on another entity kind raises `weapon expected`.

| Signature | Returns | Description |
|---|---|---|
| `weapon:weapon_id()` | integer | TF2 weapon ID. |
| `weapon:item_index()` | integer | Item-definition index. |
| `weapon:clip()` | integer | Primary clip value, including engine sentinel values. |

### Entity kind classification

- Players are `player`.
- Sentries, dispensers, and teleporters are `building`.
- Types inheriting `DT_BaseCombatWeapon` are `weapon`.
- Types inheriting `DT_BaseProjectile` are `projectile`.
- Everything else is `other`.

## Network properties

Atlas provides direct access to client receive properties. Property names and capitalization must exactly match the current game build.

### Property paths

Getters receive one or more path strings:

```lua
entity:GetPropInt("m_iHealth")
entity:GetPropInt("localdata", "m_iAmmo")
```

Setters receive the value first, followed by the path:

```lua
entity:SetPropInt(125, "m_iHealth")
```

A dotted string is not split automatically. Use separate arguments for every path component.

Intermediate path components must be data-table properties. Atlas's netvar database is flattened, so every component is resolved against the entity class and its base-class chain.

Missing properties raise an error rather than returning nil.

### Getters

| Signature | Returns | Accepted property |
|---|---|---|
| `entity:GetPropInt(names...)` | integer | Plain 8-, 16-, or 32-bit integer, including a handle-classified integer. |
| `entity:GetPropFloat(names...)` | number | `DPT_Float`. |
| `entity:GetPropBool(names...)` | boolean | Integer property interpreted as nonzero. |
| `entity:GetPropEntity(names...)` | entity or `nil` | Entity handle. |
| `entity:GetPropVector(names...)` | vec3 | `DPT_Vector` or `DPT_VectorXY`. |
| `entity:GetPropString(names...)` | string | `DPT_String`. |
| `entity:GetPropDataTableInt(names...)` | table | Immediate integer elements of a data table. |
| `entity:GetPropDataTableFloat(names...)` | table | Immediate float elements of a data table. |
| `entity:GetPropDataTableBool(names...)` | table | Immediate integer elements converted to booleans. |
| `entity:GetPropDataTableEntity(names...)` | table | Immediate entity-handle elements. May contain holes. |

Data-table getters return a new 1-based table in declaration order. Nested data-table elements are skipped. The result does not preserve property names and is not recursive.

Integer reads use these representations:

- 8-bit values are unsigned.
- 16-bit values are unsigned.
- 32-bit values are signed.

A 32-bit all-ones value therefore reads as `-1`.

### Setters

| Signature | Accepted property | Description |
|---|---|---|
| `entity:SetPropInt(value, names...)` | Integer or handle storage | Writes an integer value. |
| `entity:SetPropFloat(value, names...)` | `DPT_Float` | Requires a finite value after float conversion. |
| `entity:SetPropBool(value, names...)` | Integer storage | Requires an actual Lua boolean. |
| `entity:SetPropEntity(value, names...)` | Entity handle | Accepts an entity or nil. Nil writes an invalid handle. |
| `entity:SetPropVector(value, names...)` | `DPT_Vector` or `DPT_VectorXY` | Writes a `vec3`. |

`SetPropInt` ranges:

| Storage | Accepted range |
|---|---:|
| 8-bit | `0` through `255` |
| 16-bit | `0` through `65535` |
| 32-bit or handle | `-2147483648` through `4294967295` |

Property setters modify client memory directly. They do not replicate values, save them, restore them, or prevent the engine from overwriting them on a later frame.

There is no string setter, recursive data-table getter, data-table setter, or vector/string data-table getter.

## `engine` and `client`

### `engine`

| Signature | Returns | Description |
|---|---|---|
| `engine.in_game()` | boolean | Engine in-game state. |
| `engine.local_index()` | integer | Local player index from the engine. |
| `engine.max_clients()` | integer | Current maximum client count. |
| `engine.screen_size()` | width, height | Cached screen dimensions as two integers. |
| `engine.cur_time()` | number | Game simulation time. |
| `engine.real_time()` | number | Engine real time. |
| `engine.frame_time()` | number | Absolute frame time. |
| `engine.tick_count()` | integer | Current global tick count. |
| `engine.interval()` | number | Seconds per tick. |
| `engine.latency()` | number | Outgoing network latency in seconds, or `0` without a channel. |
| `engine.command(text)` | nothing | Executes an unrestricted client console command. |

### `client`

```lua
client.Command(text)
```

`client.Command` is an exact compatibility alias for `engine.command`. Its capital `C` is required.

## Console variables

### Lookup

```lua
local variable = cvar.find("name")
```

Returns a cvar object or nil when the variable does not exist.

### Methods

| Signature | Returns | Description |
|---|---|---|
| `variable:get_int()` | integer | Integer value. |
| `variable:get_float()` | number | Floating-point value. |
| `variable:get_string()` | string | String value. |
| `variable:set_int(value)` | nothing | Sets an integer value. |
| `variable:set_float(value)` | nothing | Sets a floating-point value. |
| `variable:set_string(value)` | nothing | Sets a string value. |

Atlas adds no extra range, finite-number, flag, callback, or persistence rules. Engine-side cvar behavior still applies.

```lua
local fov = cvar.find("fov_desired")
if fov ~= nil then
    print(fov:get_int())
end
```

## User commands

The create-move events receive a transient command proxy:

```lua
atlas.on("create_move", function(cmd)
    print(cmd.command_number, cmd.tick_count)
end)
```

The proxy only works during `create_move_pre_pred`, `create_move`, or `create_move_post_pred`. Access elsewhere raises `the command only exists inside create_move`.

### Properties

| Property | Read type | Writable |
|---|---|---|
| `cmd.command_number` | integer | No |
| `cmd.tick_count` | integer | No |
| `cmd.viewangles` | vec3 | Yes |
| `cmd.forwardmove` | number | Yes |
| `cmd.sidemove` | number | Yes |
| `cmd.upmove` | number | Yes |
| `cmd.buttons` | integer | Yes |

Unknown reads return nil while a command exists. Writing an unknown or read-only field raises `'<field>' cannot be written`.

### Button masks

```lua
atlas.on("create_move", function(cmd)
    if input.is_down(0x20) then
        cmd.buttons = cmd.buttons | buttons.jump
    end
end)
```

Clear a bit with Lua's bitwise operators:

```lua
cmd.buttons = cmd.buttons & ~buttons.attack
```

Atlas does not expose `send_packet`, weapon selection, random seed, mouse delta, or other `CUserCmd` fields.

## Game events

Game-event subscriptions are separate from `atlas.on` lifecycle events.

### `events.listen`

```lua
events.listen(event_name, handler)
```

Registers a callback and returns no values. The engine validates the event name. An unknown name raises `unknown game event '<name>'`.

There is no Lua API to remove, pause, or register a one-shot listener. Duplicate registrations are allowed and run in registration order.

```lua
events.listen("player_hurt", function(event)
    local user_id = event:get_int("userid")
    local damage = event:get_int("damageamount")
    print(user_id, damage)
end)
```

### Event methods

| Signature | Returns | Missing-key behavior |
|---|---|---|
| `event:name()` | string | Event name always exists. |
| `event:get_int(key, fallback)` | integer | Fallback defaults to `0`. |
| `event:get_float(key, fallback)` | number | Fallback defaults to `0`. |
| `event:get_string(key)` | string | Empty string. |
| `event:get_bool(key)` | boolean | False. |

The event object is a shared transient proxy. It only works during its handler. Retaining it does not retain event data; copy any required fields into your own Lua values.

Game events are dispatched only while Atlas considers the client running.

## Configuration

### Functions

```lua
config.get(path) -> boolean | number | integer
config.set(path, value)
```

Configuration paths are dot-separated, case-sensitive names rooted directly at Atlas's configuration object.

```lua
local enabled = config.get("misc.bhop")
config.set("misc.bhop", not enabled)

local accent = config.get("misc.accent")
config.set("misc.accent", color(90, 160, 255, 255))

local x = config.get("misc.watermark_pos.x")
config.set("misc.watermark_pos.x", x + 10)
```

Invalid paths raise `'<path>' is not a config value`. They do not return nil.

### Path rules

- Structures require another field component.
- Arrays require a numeric component.
- Configuration array indices are zero-based.
- Array indices contain decimal digits only.
- Multidimensional arrays consume one index component at a time.
- Colors are indivisible packed values. `.r`, `.g`, `.b`, and `.a` are not valid color suffixes through this API.
- `point_t` values expose `.x` and `.y`.
- `bind_t` values expose `.key` and `.mode`.
- A whole structure or array cannot be fetched or set as a table.
- A final trailing dot is accepted by the current path parser, but portable scripts should omit it.

Array example:

```lua
local profile_name_color = config.get("esp.profiles.0.name_color")
```

### Supported leaf types

| Atlas value | `config.get` | `config.set` |
|---|---|---|
| Boolean | boolean | Requires a Lua boolean. |
| Real number | number | Must remain finite after native conversion. |
| Enum | integer | Must fit the enum range. |
| Other integer | integer | Must fit the native type and signedness. |
| Color | packed `0xRRGGBBAA` integer | Use `color(...)` or a valid packed integer. |

Enum ranges in the current accessor:

- ESP anchors accept `0` through `3`.
- Other registered enum types accept `0` through `2`.

Bind integer writes must also be no greater than `255`.

Every successful conversion is validated against the complete Atlas configuration. A rejected value restores the previous leaf and raises `value out of range`. Setting `misc.accent` also updates the active Atlas theme accent immediately.

The Lua API does not expose configuration enumeration, profile save/load, reset, or batch transactions.

### Complete configuration vocabulary

The lists below contain every public field name registered by the current TF2 build. Array lengths and defaults belong to the corresponding software build and are not introspectable through Lua.

#### Root names

`esp`, `hitmarker`, `chams`, `glow`, `weather`, `tracers`, `arrows`, `skybox`, `world`, `fog`, `post_process`, `viewmodel`, `misc`, `queue`, `exploits`, `ragebot`, `melee`, `rjump`, `lagcomp`, `resolver`, `crits`, `pills`, `auto_vacc`, `auto_det`, `air_blast`, `skins`, `radio`, `soundboard`, `projectile_path`, `trajectory`, `projectile_warning`, `streamer_mode`, `medibot`, `events`, `player_tags`, `player_groups`, `recorder`, `anti_hack`

The configuration root `events` is unrelated to the Lua `events.listen` table.

<details>
<summary>Visual configuration fields</summary>

- `cham_profile_t`: `visible`, `visible_overlay`, `invisible`, `invisible_overlay`, `visible_color`, `occluded_color`, `visible_overlay_color`, `occluded_overlay_color`
- `esp_profile_t`: `elements`, `chams`, `order`, `anchors`, `aligns`, `box_color`, `name_color`, `distance_color`, `health_style`, `health_color`, `health_low_color`, `overheal_color`, `health_divisions`, `weapon_color`, `skeleton_color`
- `esp`: `profiles`, `target_group`, `edited`, `dormant_time`, `avatars`, `target_style`, `target_chevron`, `target_animate`, `target_color`, `building_elements`, `projectile_elements`, `pickup_elements`, `intel_elements`, `npc_elements`, `projectile_teams`, `draw_teammates_conditionally`
- `hitmarker`: `enabled`, `show_damage`, `render_mode`, `color`, `edited_event`, `override_color`, `event_color`
- `chams`: `buildings`, `projectiles`, `pickups`, `intel`, `npcs`, `viewmodel`, `viewmodel_overlay`, `weapon`, `weapon_overlay`, `viewmodel_color`, `viewmodel_overlay_color`, `weapon_color`, `weapon_overlay_color`, `local_model`, `enemy_model`, `backtrack`, `backtrack_records`, `backtrack_color`, `backtrack_overlay_color`
- `glow_profile_t`: `stencil`, `blur`, `color`
- `glow`: `players`, `buildings`, `projectiles`, `pickups`, `intel`, `npcs`, `backtrack`, `viewmodel`, `weapon`
- `weather`: `type`, `density`, `wind_speed`, `wind_direction`
- `tracers`: `enabled`, `style`, `local_only`, `local_color`, `local_impact_color`, `entity_color`, `entity_impact_color`, `duration`, `impact_marker`, `impact_style`
- `arrows`: `enabled`, `color`, `radius`, `size`, `distance`
- `viewmodel`: `enabled`, `forward`, `right`, `up`, `bob`, `sway`, `aim`, `aim_swing`
- `skybox`: `index`
- `world`: `modulate`, `color`, `skybox`, `props`
- `fog`: `mode`, `start`, `end`, `density`, `color`, `skybox`
- `post_process`: `enabled`, `exposure`, `contrast`, `saturation`, `vibrance`, `shadows`, `highlights`, `temperature`, `tint`, `vignette`, `bloom`, `bloom_threshold`, `motion_blur`
- `projectile_path`: `enabled`, `style`, `outline`, `duration`, `animate`, `color`, `impact_box`, `impact_color`
- `trajectory`: `enabled`, `style`, `color`, `outline`, `gradient`, `gradient_color`, `dim_occluded`, `marker_color`, `splash`, `impact_box`, `flight_time`
- `projectile_warning`: `enabled`, `style`, `color`, `danger_color`, `only_threats`, `resting_stickies`

</details>

<details>
<summary>Miscellaneous configuration fields</summary>

Direct children of `misc`:

- UI and indicators: `removals`, `accent`, `shadow`, `menu_blur`, `snap_indicators`, `dock_editor`, `crit_indicator_pos`, `tick_indicator_pos`, `menu_particles`, `menu_sounds`, `menu_sound_volume`, `draw_watermark`, `spectator_list`, `spectator_list_position`, `seed_indicator_pos`, `party_network`, `watchdog`, `watchdog_post_to_party`, `watermark_pos`
- Movement and weapon audio: `bhop`, `fast_stop`, `edge_jump`, `edge_jump_key`, `jump_bug`, `jump_bug_key`, `jump_bug_indicator`, `edge_bug`, `edge_bug_key`, `fan_jump`, `fan_jump_key`, `taunt_spin`, `taunt_spin_speed`, `giant_weapon_sounds`, `apply_to_alternative`, `weapon_pitch`, `auto_strafe`, `auto_strafe_mode`, `auto_strafe_retrack`
- Camera and aspect: `fov_override`, `fov`, `thirdperson`, `thirdperson_key`, `thirdperson_distance`, `thirdperson_side`, `thirdperson_height`, `thirdperson_collision`, `thirdperson_smooth`, `thirdperson_smooth_time`, `freecam`, `freecam_key`, `freecam_speed`, `projectile_camera`, `projectile_camera_mode`, `projectile_camera_hud`, `projectile_camera_distance`, `projectile_camera_width`, `projectile_camera_pos`, `aspect_ratio_changer`, `aspect_ratio_amt`
- Remaining: `noisemaker_spam`, `noisemaker_spam_mode`, `clean_screenshots`, `clean_demos`

</details>

<details>
<summary>Combat, movement, and automation fields</summary>

- `queue`: `enabled`, `max_ping`, `desired`, `delay_join`, `delay_join_time`
- `exploits`: `seed_prediction`, `seed_indicator`, `fake_latency`, `fake_latency_key`, `fake_latency_ms`, `ping_reducer`, `ping_reducer_cmdrate`, `rapid_fire`, `rapid_fire_key`, `anti_warp`, `indicator`, `warp`, `warp_key`, `warp_ticks`, `recharge_key`, `passive_recharge`
- `ragebot_hitscan_profile_t`: `hitscan_aimpoint_priority`, `hitscan_boxes`, `hitscan_multipoint`, `hitscan_multipoint_scale`, `hitscan_hitchance`, `hitscan_roll_angle`, `use_history`
- `ragebot_projectile_profile_t`: `projectile_aimpoint_priority`, `projectile_splashbot`, `projectile_splashbot_mode`, `projectile_aimpoints`, `projectile_max_targets`, `projectile_auto_charge`, `projectile_roll_angle`, `projectile_max_sim_time`, `projectile_hitchance`, `projectile_position_mode`
- `ragebot`: `enabled`, `activate_key`, `fov`, `draw_fov`, `fov_color`, `draw_crosshair`, `auto_shoot`, `ragebot_targets`, `ragebot_ignores`, `ragebot_preferences`, `prevent_bait`, `bait_override_key`, `splash_quality`, `target_classes`, `projectile_draw_path`, `projectile_path_color`, `projectile_path_low_confidence_color`, `projectile_path_style`, `projectile_path_outline`, `projectile_path_fade`, `projectile_path_fade_time`, `projectile_path_animate`, `projectile_path_gradient`, `projectile_path_color_end`, `projectile_path_bounds`, `projectile_path_bounds_style`, `projectile_path_bounds_color`, `hitscan_draw_hitboxes`, `hitscan_hitbox_color`, `hitscan_target_color`, `hitscan_draw_shot`, `hitscan_shot_color`, `hitscan_draw_duration`, `auto_charge_sticky_targets`, `huntsman_wait_for_kill`, `hitscan_profiles`, `projectile_profiles`
- `melee`: `enabled`, `activate_key`, `auto_swing`, `auto_backstab`, `swing_prediction`, `always_crit`
- `rjump`: `enabled`, `key`, `priority`, `land_shot`, `pogo`, `pogo_crouch`
- `lagcomp`: `draw_records`, `extrapolate`
- `resolver`: `enabled`
- `crits`: `enabled`, `key`, `force`, `avoid`, `indicator`
- `pills`: `seed_spin`, `crit_priority`
- `auto_det`: `enabled`, `key`, `radius_scale`, `min_targets`, `avoid_self`, `target_buildings`, `dormant`
- `air_blast`: `enabled`, `key`, `aim`, `redirect`, `prefer_crits`, `extinguish`, `push`
- `auto_vacc`: `enabled`, `auto_pop`, `idle_resist`, `reactivity`, `request_bullet_key`, `request_blast_key`, `request_fire_key`
- `medibot`: `enabled`, `activate_key`, `prefer_mode`, `auto_uber`, `auto_kritz`

</details>

<details>
<summary>Skins and cosmetic fields</summary>

- `skin_t`: `model`, `paintkit`, `wear`, `seed`, `australium`, `festivized`, `unusual`, `killstreak`, `sheen`, `killstreaker`
- `skin_entry_t`: `definition`, `skin`
- `wearable_skin_t`: `unusual`
- `wearable_entry_t`: `definition`, `skin`
- `spells_t`: `pumpkin_bombs`, `voices_from_below`, `green_flames`, `footprints`, `footprint_custom`, `footprint_color`, `jingle`, `replace_step`, `full_moon`
- `ragdolls_t`: `enabled`, `targets`, `gold`, `ice`, `dissolve`, `burning`, `electrocuted`, `ash`
- `skins`: `enabled`, `shared`, `weapons`, `sheen_custom`, `sheen_color`, `wearable_slot`, `wearables`, `spells`, `ragdolls`

</details>

<details>
<summary>Audio, recorder, social, and player-record fields</summary>

- `radio`: `enabled`, `key`, `station`, `volume`, `scale_to_game_volume`, `indicator`, `indicator_style`, `indicator_position`, `duck_on_voice`, `duck_on_self`, `duck_on_others`, `duck_level`, `mute_unfocused`, `crossfade`, `next_key`, `previous_key`, `louder_key`, `quieter_key`
- `soundboard`: `key`, `loop`, `volume`
- `recorder`: `enabled`, `record_key`, `replay_key`, `replay_attacks`, `camera_follows`, `markers`, `marker_color`, `marker_distance`, `route`, `route_color`, `traveled_color`, `route_style`, `key_overlay`, `key_overlay_position`
- `streamer_mode`: `enabled`, `hide_targets`, `stream_proof`
- Configuration `events`: `enabled`, `event_targets`, `vote_start_notify_party`
- `player_tag_t`: `account`, `tag`, `alias`, `groups`
- `player_group_t`: `name`, `color`

</details>

<details>
<summary>Anti-aim and fakelag fields</summary>

- `anti_aim_profile_t`: `yaw_base`, `real_yaw_mode`, `fake_yaw_mode`, `real_pitch_mode`, `fake_pitch_mode`, `static_real_angle`, `static_fake_angle`, `cylical_pos_1_real`, `cylical_pos_2_real`, `cylical_pos_1_fake`, `cylical_pos_2_fake`, `real_cycle_time`, `fake_cycle_time`, `apply_jitter_real`, `apply_jitter_fake`, `jitter_amount`, `spin_amount`, `break_pitch`
- `fakelag_profile_t`: `fakelag_mode`, `static_amount`, `random_min`, `random_max`, `adaptive_randomized`
- `anti_hack`: `aa_enabled`, `fl_enabled`, `aa_key`, `fl_key`, `aa_mode`, `hs_threat_only`, `anti_backstab`, `aa_profiles`, `fl_profiles`, `draw_real`, `draw_fake`, `real_draw_mode`, `fake_draw_mode`, `real_base_color`, `fake_base_color`, `real_highlight_color`, `fake_highlight_color`, `fake_chams`, `fake_chams_color`, `fake_glow`

The spelling `cylical` is the actual public field spelling. There is no `cyclical` alias.

</details>

## Constants and TF2 values

### `buttons`

| Field | Value |
|---|---:|
| `buttons.attack` | `1` |
| `buttons.jump` | `2` |
| `buttons.duck` | `4` |
| `buttons.forward` | `8` |
| `buttons.back` | `16` |
| `buttons.use` | `32` |
| `buttons.move_left` | `512` |
| `buttons.move_right` | `1024` |
| `buttons.attack2` | `2048` |
| `buttons.reload` | `8192` |
| `buttons.attack3` | `33554432` |

The `buttons` table is an ordinary mutable Lua table. Treat it as read-only.

### Global loadout constants

These constants are globals, not fields of a namespace table.

| Constant | Value |
|---|---:|
| `LOADOUT_POSITION_INVALID` | `-1` |
| `LOADOUT_POSITION_PRIMARY` | `0` |
| `LOADOUT_POSITION_SECONDARY` | `1` |
| `LOADOUT_POSITION_MELEE` | `2` |
| `LOADOUT_POSITION_UTILITY` | `3` |
| `LOADOUT_POSITION_BUILDING` | `4` |
| `LOADOUT_POSITION_PDA` | `5` |
| `LOADOUT_POSITION_PDA2` | `6` |
| `LOADOUT_POSITION_HEAD` | `7` |
| `LOADOUT_POSITION_MISC` | `8` |
| `LOADOUT_POSITION_ACTION` | `9` |
| `LOADOUT_POSITION_MISC2` | `10` |
| `LOADOUT_POSITION_TAUNT` | `11` |
| `LOADOUT_POSITION_TAUNT2` | `12` |
| `LOADOUT_POSITION_TAUNT3` | `13` |
| `LOADOUT_POSITION_TAUNT4` | `14` |
| `LOADOUT_POSITION_TAUNT5` | `15` |
| `LOADOUT_POSITION_TAUNT6` | `16` |
| `LOADOUT_POSITION_TAUNT7` | `17` |
| `LOADOUT_POSITION_TAUNT8` | `18` |

### Team values

| Value | Team |
|---:|---|
| `0` | Unassigned |
| `1` | Spectator |
| `2` | RED |
| `3` | BLU |

### Player class values

Atlas does not export class constants. `player:class()` returns:

| Value | Class |
|---:|---|
| `0` | Undefined |
| `1` | Scout |
| `2` | Sniper |
| `3` | Soldier |
| `4` | Demoman |
| `5` | Medic |
| `6` | Heavy |
| `7` | Pyro |
| `8` | Spy |
| `9` | Engineer |

### TF2 condition values

Atlas does not export condition constants. Pass these numeric IDs to `player:in_cond(id)`.

<details>
<summary>Condition IDs 0 through 130</summary>

| ID | Condition | ID | Condition | ID | Condition |
|---:|---|---:|---|---:|---|
| 0 | `AIMING` | 44 | `CRITBOOSTED_RAGE_BUFF` | 88 | `HALLOWEEN_KART_CAGE` |
| 1 | `ZOOMED` | 45 | `DEFENSEBUFF_HIGH` | 89 | `DONOTUSE_0` |
| 2 | `DISGUISING` | 46 | `SNIPERCHARGE_RAGE_BUFF` | 90 | `RUNE_STRENGTH` |
| 3 | `DISGUISED` | 47 | `DISGUISE_WEARINGOFF` | 91 | `RUNE_HASTE` |
| 4 | `STEALTHED` | 48 | `MARKEDFORDEATH_SILENT` | 92 | `RUNE_REGEN` |
| 5 | `INVULNERABLE` | 49 | `DISGUISED_AS_DISPENSER` | 93 | `RUNE_RESIST` |
| 6 | `TELEPORTED` | 50 | `SAPPED` | 94 | `RUNE_VAMPIRE` |
| 7 | `TAUNTING` | 51 | `INVULNERABLE_HIDE_UNLESS_DAMAGED` | 95 | `RUNE_REFLECT` |
| 8 | `INVULNERABLE_WEARINGOFF` | 52 | `INVULNERABLE_USER_BUFF` | 96 | `RUNE_PRECISION` |
| 9 | `STEALTHED_BLINK` | 53 | `HALLOWEEN_BOMB_HEAD` | 97 | `RUNE_AGILITY` |
| 10 | `SELECTED_TO_TELEPORT` | 54 | `HALLOWEEN_THRILLER` | 98 | `GRAPPLINGHOOK` |
| 11 | `CRITBOOSTED` | 55 | `RADIUSHEAL_ON_DAMAGE` | 99 | `GRAPPLINGHOOK_SAFEFALL` |
| 12 | `TMPDAMAGEBONUS` | 56 | `CRITBOOSTED_CARD_EFFECT` | 100 | `GRAPPLINGHOOK_LATCHED` |
| 13 | `FEIGN_DEATH` | 57 | `INVULNERABLE_CARD_EFFECT` | 101 | `GRAPPLINGHOOK_BLEEDING` |
| 14 | `PHASE` | 58 | `MEDIGUN_UBER_BULLET_RESIST` | 102 | `AFTERBURN_IMMUNE` |
| 15 | `STUNNED` | 59 | `MEDIGUN_UBER_BLAST_RESIST` | 103 | `RUNE_KNOCKOUT` |
| 16 | `OFFENSEBUFF` | 60 | `MEDIGUN_UBER_FIRE_RESIST` | 104 | `RUNE_IMBALANCE` |
| 17 | `SHIELD_CHARGE` | 61 | `MEDIGUN_SMALL_BULLET_RESIST` | 105 | `CRITBOOSTED_RUNE_TEMP` |
| 18 | `DEMO_BUFF` | 62 | `MEDIGUN_SMALL_BLAST_RESIST` | 106 | `PASSTIME_INTERCEPTION` |
| 19 | `ENERGY_BUFF` | 63 | `MEDIGUN_SMALL_FIRE_RESIST` | 107 | `SWIMMING_NO_EFFECTS` |
| 20 | `RADIUSHEAL` | 64 | `STEALTHED_USER_BUFF` | 108 | `PURGATORY` |
| 21 | `HEALTH_BUFF` | 65 | `MEDIGUN_DEBUFF` | 109 | `RUNE_KING` |
| 22 | `BURNING` | 66 | `STEALTHED_USER_BUFF_FADING` | 110 | `RUNE_PLAGUE` |
| 23 | `HEALTH_OVERHEALED` | 67 | `BULLET_IMMUNE` | 111 | `RUNE_SUPERNOVA` |
| 24 | `URINE` | 68 | `BLAST_IMMUNE` | 112 | `PLAGUE` |
| 25 | `BLEEDING` | 69 | `FIRE_IMMUNE` | 113 | `KING_BUFFED` |
| 26 | `DEFENSEBUFF` | 70 | `PREVENT_DEATH` | 114 | `TEAM_GLOWS` |
| 27 | `MAD_MILK` | 71 | `MVM_BOT_STUN_RADIOWAVE` | 115 | `KNOCKED_INTO_AIR` |
| 28 | `MEGAHEAL` | 72 | `HALLOWEEN_SPEED_BOOST` | 116 | `COMPETITIVE_WINNER` |
| 29 | `REGENONDAMAGEBUFF` | 73 | `HALLOWEEN_QUICK_HEAL` | 117 | `COMPETITIVE_LOSER` |
| 30 | `MARKEDFORDEATH` | 74 | `HALLOWEEN_GIANT` | 118 | `HEALING_DEBUFF` |
| 31 | `NOHEALINGDAMAGEBUFF` | 75 | `HALLOWEEN_TINY` | 119 | `PASSTIME_PENALTY_DEBUFF` |
| 32 | `SPEED_BOOST` | 76 | `HALLOWEEN_IN_HELL` | 120 | `GRAPPLED_TO_PLAYER` |
| 33 | `CRITBOOSTED_PUMPKIN` | 77 | `HALLOWEEN_GHOST_MODE` | 121 | `GRAPPLED_BY_PLAYER` |
| 34 | `CRITBOOSTED_USER_BUFF` | 78 | `MINICRITBOOSTED_ON_KILL` | 122 | `PARACHUTE_DEPLOYED` |
| 35 | `CRITBOOSTED_DEMO_CHARGE` | 79 | `OBSCURED_SMOKE` | 123 | `GAS` |
| 36 | `SODAPOPPER_HYPE` | 80 | `PARACHUTE_ACTIVE` | 124 | `BURNING_PYRO` |
| 37 | `CRITBOOSTED_FIRST_BLOOD` | 81 | `BLASTJUMPING` | 125 | `ROCKETPACK` |
| 38 | `CRITBOOSTED_BONUS_TIME` | 82 | `HALLOWEEN_KART` | 126 | `LOST_FOOTING` |
| 39 | `CRITBOOSTED_CTF_CAPTURE` | 83 | `HALLOWEEN_KART_DASH` | 127 | `AIR_CURRENT` |
| 40 | `CRITBOOSTED_ON_KILL` | 84 | `BALLOON_HEAD` | 128 | `HALLOWEEN_HELL_HEAL` |
| 41 | `CANNOT_SWITCH_FROM_MELEE` | 85 | `MELEE_ONLY` | 129 | `POWERUPMODE_DOMINANT` |
| 42 | `DEFENSEBUFF_NO_CRIT_BLOCK` | 86 | `SWIMMING_CURSE` | 130 | `IMMUNE_TO_PUSHBACK` |
| 43 | `REPROGRAMMED` | 87 | `FREEZE_INPUT` |  |  |

</details>

`player:class_id()` and `weapon:weapon_id()` return engine enum values that are not exported as Lua constants. Entity class IDs are tied to the software's compiled TF2 build and should not be treated as stable across incompatible builds.

## Migration from other Lua APIs

| Common external API | Atlas equivalent |
|---|---|
| `callbacks.Register("Draw", fn)` | `atlas.on("paint", fn)` |
| `callbacks.Register("CreateMove", fn)` | `atlas.on("create_move", fn)` |
| `entities.GetLocalPlayer()` | `entities.local_player()` or compatibility alias `entities.GetLocalPlayer()` |
| `entities.FindByClass("CTFPlayer")` | `entities.players()` |
| `globals.RealTime()` | `engine.real_time()` |
| `globals.CurTime()` | `engine.cur_time()` |
| `draw.Color(r, g, b, a)` | Pass `color(r, g, b, a)` directly to each draw call. |
| `draw.Text(...)` | `draw.text(...)` |
| `draw.TextShadow(...)` | `draw.text(..., outlined = true)` |
| `client.Command(...)` | Compatibility alias remains available; primary API is `engine.command(...)`. |
| `entity:IsAlive()` | `entity:alive()` |
| `entity:IsDormant()` | `entity:dormant()` |
| `entity:GetTeamNumber()` | `entity:team()` |
| `entity:GetPlayerClass()` | `entity:class()` |
| `entity:GetIndex()` | `entity:index()` |
| `entity:GetAbsOrigin()` | `entity:origin()` |

Atlas has no direct `GetScreenBox` or Lua text-size function. Build a screen box by projecting collision-bound corners, or place a centered label at a projected world position.

## Complete examples

### Enemy player labels

```lua
local function screen_box(entity)
    local origin = entity:origin()
    local mins = entity:GetPropVector("m_vecMins")
    local maxs = entity:GetPropVector("m_vecMaxs")

    local corners = {
        vec3(mins.x, mins.y, mins.z),
        vec3(mins.x, mins.y, maxs.z),
        vec3(mins.x, maxs.y, mins.z),
        vec3(mins.x, maxs.y, maxs.z),
        vec3(maxs.x, mins.y, mins.z),
        vec3(maxs.x, mins.y, maxs.z),
        vec3(maxs.x, maxs.y, mins.z),
        vec3(maxs.x, maxs.y, maxs.z)
    }

    local left
    local top
    local right
    local bottom

    for _, corner in ipairs(corners) do
        local x, y = draw.world_to_screen(origin + corner)
        if x == nil then
            return nil
        end

        left = left == nil and x or math.min(left, x)
        top = top == nil and y or math.min(top, y)
        right = right == nil and x or math.max(right, x)
        bottom = bottom == nil and y or math.max(bottom, y)
    end

    return left, top, right - left, bottom - top
end

atlas.on("paint", function()
    local me = entities.local_player()
    if me == nil or not me:valid() or not me:alive() then
        return
    end

    for _, player in ipairs(entities.players()) do
        if player:valid()
            and player:alive()
            and not player:dormant()
            and not player:is_local()
            and player:is_enemy() then

            local x, y, width = screen_box(player)
            if x ~= nil then
                draw.text(
                    x + width * 0.5,
                    y - 14,
                    color(255, 255, 255, 255),
                    player:name() or "player",
                    "esp",
                    "center",
                    true
                )
            end
        end
    end
end)
```

### Configuration toggle with a notification

```lua
local enabled = not config.get("misc.bhop")
config.set("misc.bhop", enabled)
atlas.notify("Bunny hop: " .. tostring(enabled), "success")
```

### Track a game event

```lua
local damage_total = 0

events.listen("player_hurt", function(event)
    damage_total = damage_total + event:get_int("damageamount")
end)

atlas.on("paint", function()
    draw.text(
        20,
        40,
        color(255, 210, 90, 255),
        "Damage: " .. damage_total,
        "menu_bold",
        "left",
        true
    )
end)
```

### Read the local weapon

```lua
atlas.on("paint", function()
    local me = entities.local_player()
    if me == nil or not me:valid() or not me:alive() then
        return
    end

    local weapon = me:active_weapon()
    if weapon == nil or not weapon:valid() then
        return
    end

    draw.text(
        20,
        60,
        color(180, 220, 255, 255),
        string.format("Item %d | Clip %d", weapon:item_index(), weapon:clip()),
        "mono",
        "left",
        true
    )
end)
```

## Error behavior

### Values that return nil or a fallback

| Situation | Result |
|---|---|
| No cached local player | `nil` |
| No cached active weapon | `nil` |
| No entity in a loadout slot | `nil` |
| Entity-handle netprop does not resolve | `nil` |
| Cvar does not exist | `nil` |
| Player information cannot be read | `nil` |
| World-to-screen projection fails | `nil` |
| Expired entity checked with `valid()` | `false` |
| Player resource unavailable for `max_health()` | `0` |
| Network channel unavailable for `engine.latency()` | `0` |
| Unknown command field during create-move | `nil` |
| Missing game-event integer/float/string/bool | `0`, `0`, `""`, or `false` |

### Conditions that raise errors

- Unknown lifecycle event name.
- Draw call outside `paint`.
- Expired entity used by a live-entity method.
- Player-only or weapon-only method used on the wrong kind.
- Missing or incorrectly typed network property.
- Invalid configuration path.
- Invalid configuration value.
- Command proxy accessed outside create-move.
- Event proxy accessed outside its game-event handler.
- Unknown game-event name passed to `events.listen`.
- Unknown text font, text alignment, or notification kind.
- Color or HTML alpha channel outside `0` through `255`.
- HTML panel dimensions outside `1` through `2048`.

## API availability summary

### Global functions

- `vec3`
- `color`
- `print`
- text-only `load`

### Global tables

- `atlas`
- `draw`
- `input`
- `ui`
- `entities`
- `engine`
- `client`
- `cvar`
- `config`
- `events`
- `buttons`

### Contextual objects

- `vec3`
- `html`
- `entity`
- `cvar`
- create-move command proxy
- game-event proxy

### APIs not exposed

- Filesystem access
- Lua package loading or `require`
- Lua sockets or HTTP requests
- Generic entity lookup
- Text measurement
- Game-event unsubscription
- Configuration enumeration
- Command `send_packet`
- Trace, ray-cast, bone, or hitbox Lua APIs
- Automatic netprop replication or restoration
