# PTalk Robot Emotion Library v2.0 — Requirements

Canonical spec for the PTalk screen-eye robot emotion library. Keep this
in sync with the code. When porting to another project, **read this file
first**.

Product: **PTalk v2.0** — a desktop companion robot built at PTIT
(Posts and Telecommunications Institute of Technology).

### What's new in v2.0

| Area               | Change |
| ------------------ | ------ |
| Color              | TFT 9-tone curated palette (was mono cyan). Two-layer rule: face stays cyan, accessories take the category tone. See §4. |
| Multi-color        | New `tone: 'multi'` for variants that need several colors at once (confetti, fireworks). |
| Boot scene         | New `boot` scene category with 4 variants — PTIT-branded power-on sequence. See §6.5.1. |
| Connectivity       | New `network` scene category with 6 variants covering the full wifi/BLE/server lifecycle. See §6.5.2. |
| Emotion categories | 29 → 37 (added disgusted, nervous, embarrassed, curious, annoyed, cool, suspicious, determined). |
| Total variants     | 117 → 227. |
| Status bar         | Always rendered in fixed cyan (never tinted), regardless of active category. |

---

---

## 1. Project intent

An **original** robot character built around a 2-inch screen that shows
expressive eyes — and, when context calls for it, **full-screen scenes**
(weather, clock, music, timers, etc.) like Dasai-Mochi-style videos.
Implementation must be independently designed — do not copy proprietary
frames, transitions, or distinctive marks from those products.

**No brand text appears inside the eye/scene SVG** (except the dedicated
`boot` scene, which intentionally shows PTIT branding during power-on).
The viewer chrome (HTML header, version badge) is allowed to identify
the product as PTalk v2.0 — it's not part of the rendered display.

Personality: friendly, slightly mischievous, expressive.

The library has **two kinds** of content:

| Kind        | Purpose                                                       |
| ----------- | ------------------------------------------------------------- |
| `emotion`   | Eyes-based expressions. Tied to dialog / mood.                |
| `scene`     | Full-screen contextual displays (weather/clock/music/timer…). |

Every category has a `kind` field. The host can filter or pick from one
pool independently of the other.

---

## 2. Hardware target

| Spec                | Value                                                   |
| ------------------- | ------------------------------------------------------- |
| Display             | 320 × 240, landscape, ~2 inch TFT (color)               |
| Reserved top strip  | 20 px (status bar: clock left, dot centre, wifi+battery right) |
| Active emotion area | 320 × 220 (y = 20 … 240)                                |
| Color mode          | RGB565 — 9-tone curated palette on near-black background |
| Frame rate          | **30 fps default** (range 12 – 60 acceptable; 30 recommended for liveliness) |

The display can show color, but to keep the "real robot display" feel we
restrict every pixel to one of a small **curated palette** of 9 emotional
tones. See § 4 for the color system.

---

## 3. Eye geometry

| Token   | Value | Notes                                                |
| ------- | ----- | ---------------------------------------------------- |
| `EYE_W` | 88    | Each eye width                                       |
| `EYE_H` | 96    | Each eye height                                      |
| `EYE_RX`| 22    | Default corner radius                                |
| `GAP`   | **150** | Centre-to-centre between the two eyes              |
| `LX`    | 85    | Left eye centre X                                    |
| `RX`    | 235   | Right eye centre X                                   |
| `CY`    | 130   | Eye centre Y (mid of active area)                    |

The wider 150 px gap pushes the eyes near the screen edges so the display
reads as showing "both ends of the face" — while leaving ~41 px of outer
margin for accessory overlays (sparkles, tears, Zs, hearts, audio waves,
status icons).

---

## 4. Visual system — color

### 4.1 Palette

Defined in `emotion-core.jsx` as `EYE_TONES`. Each tone has an `eye` hex
and a matching glow rgba. All values are RGB565-safe.

| Tone     | Hex       | Used for                                          |
| -------- | --------- | ------------------------------------------------- |
| `cyan`   | `#5be9ff` | Default / neutral / robotic states. **Eyes always.** |
| `warm`   | `#ffd166` | Happy, smug, proud, excited, gold                 |
| `rose`   | `#ff6b9d` | Love, shy, embarrassed, music                     |
| `red`    | `#ff5b6e` | Angry, error, dead, focused, determined, urgent   |
| `blue`   | `#76b8ff` | Sad, crying, rain, water                          |
| `green`  | `#7be88e` | Charging, disgusted, plant, fresh                 |
| `purple` | `#b48cff` | Sleepy, sleeping, mischievous, dizzy, suspicious  |
| `orange` | `#ff9d5b` | Hungry, curious, confused, annoyed, nervous, fire |
| `white`  | `#f0f4ff` | Surprised, scared, camera flash, snow             |

Background is always `--bg = #06090d` (near-black).

### 4.2 Color rules

The robot has a **face**. The face must stay readable across every emotion.
So we draw two layers:

| Layer       | CSS var       | Used for                                              | Color                                    |
| ----------- | ------------- | ----------------------------------------------------- | ---------------------------------------- |
| Face        | `--eye`       | Eye rectangles, closed-eye arcs, brows, mouth          | **Always cyan** for every emotion variant |
| Accessory   | `--accent`    | Tears, hearts, sparkles, audio bars, mic, Zs, fire…    | The category's tone                       |
| Glow        | `--eye-glow`  | SVG drop-shadow filter on the screen                   | Cyan glow (emotions) / tone glow (scenes) |
| Background  | `--bg`        | Anywhere the screen needs to "punch through" the eye   | Always `#06090d`                          |

For **scenes**, there is no face — the variant is a full-screen overlay.
So both `--eye` and `--accent` are set to the variant's tone.

The **status bar** is system chrome — it ignores tones entirely and always
draws in fixed cyan (`#5be9ff`). This holds whether the active category is
an emotion or a scene.

### 4.3 Multi-color variants (`tone: 'multi'`)

A handful of variants need more than one color in the same frame (e.g.
`celebrate-confetti` showers six colors, `celebrate-firework` has three
differently-colored bursts). They set `tone: 'multi'`. The Screen treats
this as cyan for the base palette but **also exposes every tone as a CSS
variable** (`--tone-cyan`, `--tone-warm`, `--tone-rose`, …) so the render
function can paint specific colors:

```jsx
<rect fill="var(--tone-warm)" />
<circle fill="var(--tone-rose)" />
```

These extra CSS vars are only exposed when `kind === 'scene'`.

### 4.4 Per-variant tone

Variants may set their own `tone` field to override the category's
default. Used heavily by scenes (e.g. `weather-sunny` is warm,
`weather-rainy` is blue, `weather-storm` is purple — same category, three
different tones). Resolution:

```
variant.tone || category.tone || 'cyan'
```

The full per-variant tone table lives in `emotion-color.jsx`
(`SCENE_VARIANT_TONE`).

### 4.5 Mono fallback

The viewer's **◐ Color / ◯ Mono** button forces every screen to render in
cyan, ignoring tones — useful for firmware ports targeting mono displays
and for verifying that nothing relies on color to be legible.

### 4.6 Misc

- Subtle CRT scanlines + glow applied as CSS overlay.
- Smooth vector shapes (`rx` corners, arcs, `<path>`) — not pixel-art.
- Status bar font: monospace, ~11 pt at 1×.
- Status bar centre is intentionally empty — render a small dot or leave blank.

---

## 5. Animation rules

- Time `t ∈ [0, 1)` cycles every `duration` ms per variant.
- `t` is **quantized** to the chosen fps before being passed into render —
  produces the chunky "real robot display" feel without sacrificing resolution.
- Default fps **30**. Renderer supports 12 – 60 fps with no code changes.
- Each variant declares its own `duration` (typical 0.6 – 5 s).
- Each variant is a pure function `t ⇒ SVG JSX` — no internal state.

---

## 6. Conceptual model — "normal" vs "idle"

The category called **`normal`** is the *default look* of the robot — a set
of variants the host can pick from when nothing else is happening.

**Idle is a behaviour, not a category.** Firmware "idle" should:

1. Stay on category `normal`.
2. Every N seconds (recommend 3 – 6 s), call `pickVariant('normal',
   recentIds)` and play the result.
3. Keep a 4 – 6-entry ring buffer of recent ids so the same variant doesn't
   repeat back-to-back.

The viewer (`app.jsx`) implements this in the **"◉ Idle mode"** button.

---

## 6.5 Boot & connectivity flow

Firmware uses two `scene` categories — `boot` and `network` — to drive
the entire startup-to-conversation pipeline. Both **block** the normal
emotion loop while playing; the host should suspend autoplay until the
flow exits.

### 6.5.1 Cold boot sequence (happy path)

```
  POWER ON
     ↓
  boot-poweron       (2.4 s)  — single bright dot expands, splits
                                into two eyes ("the robot wakes up")
     ↓
  boot-logo          (3.0 s)  — PTIT mark fades in with a sweep ring,
                                subtitle "POSTS · TELECOMMUNICATIONS"
     ↓
  boot-checks        (4.2 s)  — self-test for DISPLAY / AUDIO / MIC /
                                NETWORK / AI CORE, each row gets a
                                spinner that flips to OK ✓
     ↓
  →→ [ enter connectivity sub-flow § 6.5.2 ] →→
     ↓
  boot-ready         (3.0 s)  — progress bar fills, "READY" stamp
     ↓
  HAND OFF to `normal` idle pool (§ 6)
```

Total happy-path duration is ~12.6 s of boot + however long the
connectivity sub-flow takes.

If a check **fails** during `boot-checks`, leave the failed row showing
the spinner instead of OK for 1 s, then jump straight to the matching
status scene:

| Failed step | Jump to               |
| ----------- | --------------------- |
| DISPLAY     | (panic — firmware halts; no UI possible) |
| AUDIO / MIC | continue boot, raise `error-warning` later (non-fatal) |
| NETWORK     | enter § 6.5.2 immediately, skip `boot-ready` |
| AI CORE     | `error-bang` then halt (model load failed) |

### 6.5.2 Connectivity sub-flow (FSM)

```
           ┌── [ user holds the pair button ≥3 s ] ──┐
           ↓                                          ↓
     network-wifi-scan                          network-ble-pair
      (search APs)                              (host serves BLE GATT;
           ↓                                     waits for PTalk app to
           ↓ stored SSID found                   push credentials → then
           ↓                                     loop back to wifi-connect)
     network-wifi-connect
      (associate + DHCP)
           ↓
     ┌────┼───────┐
  success   fail (1–3)
     ↓        ↓
     ↓   network-wifi-retry
     ↓    (attempt N/3, orange)
     ↓        ↓
     ↓        → fail again → attempts < 3 → loop back to wifi-connect
     ↓        → fail × 3       → network-offline (red)
     ↓                                  ↓
     ↓                                  → user can hold pair button → ble-pair
     ↓                                  → or wait → retry every 60 s
     ↓
  ping ${PTALK_CLOUD}/health (3 s timeout)
           ↓                            ↓
      reachable                    unreachable
           ↓                            ↓
     EXIT to boot-ready          network-server-error (red, ERROR 503)
                                       ↓
                                       → backoff: 2 s, 5 s, 10 s, 30 s
                                       → success → EXIT
                                       → still failing → stay on this scene
```

### 6.5.3 Mid-session network loss

While a conversation is running:

1. **TCP / WebSocket drops** while talking to the cloud: pause the active
   emotion (freeze on current frame, NOT idle pool), show
   `network-server-error` for up to 5 s of fast retries (200 ms / 500 ms /
   1 s / 2 s / 5 s exponential). On any successful reconnect, fade back
   to the prior emotion. On 5 s timeout, drop into `network-offline`.
2. **Wi-Fi link itself drops**: skip the server-error layer; go straight
   to `network-offline`. Host stays on that scene until link returns,
   then jumps to `network-wifi-connect` for the reassociation handshake.
3. **Long offline (≥30 s)**: optionally play `error-noconn` emotion
   variant once to acknowledge to the user (friendlier than holding the
   `offline` status scene forever).

### 6.5.4 Tone choices (recap)

- `boot-*` — **red** across the sequence (PTIT brand identity layer)
- `wifi-scan` / `wifi-connect` — **cyan** (neutral, in-progress)
- `wifi-retry` — **orange** (caution; user should be aware but isn't fatal)
- `offline` / `server-error` — **red** (failure)
- `ble-pair` — **purple** (special mode; visually distinct from any normal
  connect state so users don't confuse it with a hung wifi-connect)

---

## 7. Categories at a glance

### Emotions (kind: 'emotion')

| Category       | Variants | Tone     | Purpose                                       |
| -------------- | -------- | -------- | --------------------------------------------- |
| `normal`       | **16**   | cyan     | Default state — random-played in idle         |
| `greet`        | 4        | cyan     | Hello / wake-up handshake                     |
| `happy`        | 8        | warm     | Positive acknowledgement                      |
| `wink`         | 4        | warm     | Playful acknowledgement                       |
| `sad`          | 6        | blue     | Negative outcome / empathy                    |
| `crying`       | 4        | blue     | Strong sad                                    |
| `angry`        | 5        | red      | Frustration / boundary                        |
| `annoyed`      | 3        | orange   | Mild irritation (less than angry)             |
| `disgusted`    | 3        | green    | Yuck / rejection                              |
| `surprised`    | 4        | white    | Sudden new info                               |
| `scared`       | 4        | white    | Frightened response                           |
| `nervous`      | 3        | orange   | Anxious / jittery                             |
| `love`         | 5        | rose     | Affection / praise                            |
| `shy`          | 4        | rose     | Bashful / hiding                              |
| `embarrassed`  | 3        | rose     | Flushed / mortified                           |
| `smug`         | 4        | warm     | Confident / self-satisfied                    |
| `proud`        | 4        | warm     | Accomplished / chest-out                      |
| `cool`         | 3        | cyan     | Chill / smooth confidence                     |
| `mischievous`  | 4        | purple   | Sneaky / playful                              |
| `suspicious`   | 3        | purple   | Skeptical / squinting                         |
| `curious`      | 3        | orange   | Leaning in / wondering                        |
| `confused`     | 4        | orange   | Didn't understand request                     |
| `sleepy`       | 4        | purple   | Low-power / late-hour mood                    |
| `sleeping`     | 4        | purple   | Deep idle / standby                           |
| `excited`      | 5        | warm     | High-positive event                           |
| `bored`        | 4        | cyan     | No interaction for a while                    |
| `hungry`       | 4        | orange   | Wants food / craving                          |
| `listening`    | **7** ✦  | cyan     | While receiving voice input                   |
| `thinking`     | **7** ✦  | cyan     | While generating response / processing        |
| `focused`      | 4        | red      | Locked-on task attention                      |
| `determined`   | 3        | red      | Resolute / locked-in                          |
| `loading`      | 5        | cyan     | Background task                               |
| `charging`     | 4        | green    | Battery charging / power up                   |
| `dizzy`        | 4        | purple   | Error / overload                              |
| `dead`         | 4        | red      | Critical failure                              |
| `error`        | 4        | red      | System / connection issue                     |
| `mute`         | 4        | cyan     | Mic off / silenced                            |

**Emotion subtotal: 37 categories · 168 variants.**

### Scenes (kind: 'scene')

Full-screen contextual displays — used when the host wants to show data,
not an emotional reaction. Eyes are not drawn. Per-variant tones in `()`.

| Category      | Variants | Purpose                                       |
| ------------- | -------- | --------------------------------------------- |
| `boot`        | 4        | **PTIT power-on sequence** (poweron → logo → checks → ready, all red) |
| `network`     | 6        | **Connectivity status**: wifi-scan / wifi-connect / wifi-retry (orange) / offline (red) / ble-pair (purple) / server-error (red) |
| `weather`     | 5        | Sunny (warm) / rainy (blue) / cloudy (cyan) / snow (white) / storm (purple) |
| `clock`       | 3        | Digital (cyan), analog (cyan), alarm (red)    |
| `music`       | 3        | Notes (rose), bars (cyan), wave (purple)      |
| `timer`       | 3        | Countdown (orange), progress (cyan), hourglass (warm) |
| `cooking`     | 2        | Pan / fire (red), pot / water (blue)          |
| `walking`     | 2        | Footprints (green), runner (red)              |
| `celebrate`   | 3        | Trophy (warm), confetti (**multi**), firework (**multi**) |
| `night`       | 2        | Moon (purple), starfield (white)              |
| `fitness`     | 3        | Heart-rate (red), steps (green), dumbbell (warm) |
| `call`        | 3        | Incoming (green), active (cyan), missed (red) |
| `message`     | 3        | Typing (cyan), envelope (white), bubbles (warm) |
| `camera`      | 2        | Shutter / flash (white), focus (cyan)         |
| `navigation`  | 3        | Compass (cyan), pin (red), arrow (green)      |
| `gift`        | 2        | Wrapped (rose), opened (warm)                 |
| `coffee`      | 2        | Pour (warm), cup (warm)                       |
| `plant`       | 2        | Growing (green), watering (blue)              |
| `morning`     | 2        | Sunrise (warm), alarm-rays (red)              |
| `gaming`      | 2        | Pad (purple), powerup (warm)                  |
| `calendar`    | 2        | Date page (cyan), reminder (red)              |

**Scene subtotal: 21 categories · 59 variants.**

**Grand total: 58 categories · 227 variants.**

✦ `listening` and `thinking` are **mandatory** — this is a conversational
robot. Every dialog turn enters one of these two states.

### Variant contract

```js
{
  id: 'normal-look',        // unique kebab-case across the whole library
  label: 'Look around',     // human label
  duration: 4000,           // ms per loop
  tone: 'warm',             // (optional) overrides category.tone; 'multi' for
                            //   variants that want every tone available as
                            //   --tone-warm/--tone-rose/... CSS vars
  render: (t) => /* JSX */, // pure function of t ∈ [0, 1)
}
```

Variants in the same category must be **visually distinct** — different
motion, posture, or accessory — so back-to-back picks don't read as the
same animation.

---

## 8. Full variant inventory

Use these `id`s when calling from firmware. Listed in display order.

### normal (16) — idle pool
`normal-steady` · `normal-breathe` · `normal-look` · `normal-hum` ·
`normal-twitch` · `normal-tilt` · `normal-double-blink` · `normal-drift` ·
`normal-search` · `normal-look-up` · `normal-tired-blink` · `normal-stare` ·
`normal-peek` · `normal-pendulum` · `normal-puff` · `normal-rem`

### greet (4)
`greet-bow` · `greet-wave` · `greet-nod-twice` · `greet-sparkle-hi`

### happy (8)
`happy-arc` · `happy-bouncy` · `happy-closed` · `happy-cheek` ·
`happy-laugh` · `happy-giggle` · `happy-shimmer` · `happy-skip`

### wink (4)
`wink-right` · `wink-left` · `wink-double` · `wink-slow`

### sad (6)
`sad-droop` · `sad-look-down` · `sad-big-droopy` · `sad-quiver` ·
`sad-sigh` · `sad-rain`

### crying (4)
`crying-tears` · `crying-waterfall` · `crying-sob` · `crying-trickle`

### angry (5)
`angry-glare` · `angry-steam` · `angry-shake` · `angry-narrow` ·
`angry-pouty`

### annoyed (3)
`annoyed-flat` · `annoyed-twitch` · `annoyed-sigh-side`

### disgusted (3)
`disgusted-yuck` · `disgusted-gag` · `disgusted-sour`

### surprised (4)
`surprised-pop` · `surprised-exclaim` · `surprised-flash` ·
`surprised-mild`

### scared (4)
`scared-tremble` · `scared-shrink` · `scared-flinch` · `scared-darting`

### nervous (3)
`nervous-sweat` · `nervous-fidget` · `nervous-gulp`

### love (5)
`love-hearts` · `love-floating` · `love-wink` · `love-blush` ·
`love-shower`

### shy (4)
`shy-away` · `shy-down` · `shy-peek` · `shy-blush`

### embarrassed (3)
`embarrassed-blush` · `embarrassed-hide` · `embarrassed-cringe`

### smug (4)
`smug-smirk` · `smug-side` · `smug-brow` · `smug-blink`

### proud (4)
`proud-puffed` · `proud-glow` · `proud-medal` · `proud-rise`

### cool (3)
`cool-shades` · `cool-smirk` · `cool-swag`

### mischievous (4)
`mischievous-grin` · `mischievous-laugh` · `mischievous-side` ·
`mischievous-twinkle`

### suspicious (3)
`suspicious-squint` · `suspicious-side` · `suspicious-scan`

### curious (3)
`curious-lean` · `curious-sparkle` · `curious-scan`

### confused (4)
`confused-qmark` · `confused-tilt` · `confused-mismatch` · `confused-spin-q`

### sleepy (4)
`sleepy-heavy` · `sleepy-yawn` · `sleepy-nod` · `sleepy-blink-slow`

### sleeping (4)
`sleeping-zzz` · `sleeping-calm` · `sleeping-deep` · `sleeping-bubble`

### excited (5)
`excited-stars` · `excited-bounce` · `excited-grin` · `excited-giddy` ·
`excited-shockwave`

### bored (4)
`bored-half` · `bored-sideways` · `bored-eye-roll` · `bored-yawn-small`

### hungry (4)
`hungry-eye-food` · `hungry-drool` · `hungry-lick` · `hungry-stomach`

### listening (7) ✦
`listening-attentive` · `listening-waves` · `listening-eq` ·
`listening-cup` · `listening-focused` · `listening-mic` ·
`listening-bars`

### thinking (7) ✦
`thinking-look-up` · `thinking-spinner` · `thinking-scanning` ·
`thinking-dots` · `thinking-hmm` · `thinking-aha` · `thinking-cog`

### focused (4)
`focused-laser` · `focused-scan` · `focused-target` · `focused-pinpoint`

### determined (3)
`determined-stare` · `determined-fire` · `determined-lock`

### loading (5)
`loading-dots` · `loading-bar` · `loading-spinner-big` · `loading-rings` ·
`loading-bricks`

### charging (4)
`charging-bolt` · `charging-fill` · `charging-spark` · `charging-glow`

### dizzy (4)
`dizzy-spirals` · `dizzy-wobble` · `dizzy-stars` · `dizzy-figure8`

### dead (4)
`dead-x` · `dead-fade` · `dead-glitch` · `dead-droop`

### error (4)
`error-bang` · `error-noconn` · `error-pixels` · `error-warning`

### mute (4)
`mute-x` · `mute-zip` · `mute-hush` · `mute-line`

---

## 8b. Scene inventory

### boot (4) — PTIT power-on sequence
`boot-poweron` (red) · `boot-logo` (red) · `boot-checks` (red) · `boot-ready` (red)

Firmware boot flow: play `boot-poweron` (2.4 s) → `boot-logo` (3 s) →
`boot-checks` (4.2 s) → `boot-ready` (3 s) → hand off to `normal` idle pool.
Logo + mark are an original geometric construction inspired by PTIT
iconography — not a copy of the official school seal.

### network (6) — connectivity status
`network-wifi-scan` (cyan) · `network-wifi-connect` (cyan) ·
`network-wifi-retry` (orange) · `network-offline` (red) ·
`network-ble-pair` (purple) · `network-server-error` (red)

Firmware connectivity FSM (suggested):
```
  poweron complete
     ↓
  network-wifi-scan  (no stored SSID)  →  network-ble-pair  (user holds button)
     ↓ SSID known
  network-wifi-connect
     ↓ fail (3 attempts)         ↑ success
  network-wifi-retry  →  network-offline
     ↓ wifi up
  (ping cloud)
     ↓ reachable                  ↑ unreachable
  hand off to emotions          network-server-error  (poll + retry)
```
While any `network-*` scene is playing the host should suppress
user-facing emotions — these are blocking status displays.


### weather (5)
`weather-sunny` (warm) · `weather-rainy` (blue) · `weather-cloudy` (cyan) ·
`weather-snow` (white) · `weather-storm` (purple)

### clock (3)
`clock-digital` (cyan) · `clock-analog` (cyan) · `clock-alarm` (red)

### music (3)
`music-notes` (rose) · `music-bars` (cyan) · `music-wave` (purple)

### timer (3)
`timer-countdown` (orange) · `timer-progress` (cyan) ·
`timer-hourglass` (warm)

### cooking (2)
`cooking-pan` (red) · `cooking-pot` (blue)

### walking (2)
`walking-footprints` (green) · `walking-runner` (red)

### celebrate (3)
`celebrate-trophy` (warm) · `celebrate-confetti` (**multi**) ·
`celebrate-firework` (**multi**)

### night (2)
`night-moon` (purple) · `night-stars` (white)

### fitness (3)
`fitness-hr` (red) · `fitness-steps` (green) · `fitness-dumbbell` (warm)

### call (3)
`call-incoming` (green) · `call-active` (cyan) · `call-missed` (red)

### message (3)
`message-typing` (cyan) · `message-envelope` (white) ·
`message-bubbles` (warm)

### camera (2)
`camera-shutter` (white) · `camera-focus` (cyan)

### navigation (3)
`navigation-compass` (cyan) · `navigation-pin` (red) ·
`navigation-arrow` (green)

### gift (2)
`gift-wrapped` (rose) · `gift-open` (warm)

### coffee (2)
`coffee-pour` (warm) · `coffee-cup` (warm)

### plant (2)
`plant-grow` (green) · `plant-water` (blue)

### morning (2)
`morning-sunrise` (warm) · `morning-alarm-rays` (red)

### gaming (2)
`gaming-pad` (purple) · `gaming-powerup` (warm)

### calendar (2)
`calendar-date` (cyan) · `calendar-reminder` (red)

---

## 9. File layout

```
MOCHI-P Emotion Sheet.html   ← entry point (CSS, React loader)
emotion-core.jsx             ← primitives, constants, EYE_TONES palette
emotion-list.jsx             ← base emotion variants (29 cats from v1 spec)
emotion-extras.jsx           ← additional variants + new categories
scenes-boot.jsx              ← PTIT boot sequence (1 scene cat, 4 variants)
scenes-network.jsx           ← Connectivity status (wifi/ble/server, 6 variants)
emotion-balance.jsx          ← balance pass: brings every cat to ≥4 variants
emotion-more.jsx             ← 8 brand-new emotion categories (disgusted,
                              nervous, embarrassed, curious, annoyed, cool,
                              suspicious, determined) + KEYS reorder
scenes.jsx                   ← base scene categories (8 cats)
scenes-extras.jsx            ← +11 more scene categories
emotion-color.jsx            ← assigns category + per-variant `tone` fields
app.jsx                      ← viewer UI (also implements the Color/Mono toggle)
REQUIREMENTS.md              ← this file
```

### Load order

```
1. emotion-core.jsx        ← publishes window.EmotionCore (+ EYE_TONES)
2. emotion-list.jsx        ← publishes window.EMOTION_CATEGORIES + KEYS
3. emotion-extras.jsx      ← adds more emotion variants + categories
4. scenes.jsx              ← tags emotion cats with kind:'emotion',
                              adds scene cats with kind:'scene'
5. scenes-extras.jsx       ← adds 11 more scene categories
6. scenes-boot.jsx         ← adds the PTIT boot scene
7. scenes-network.jsx      ← adds the connectivity-status scene
8. emotion-balance.jsx     ← pushes balance variants onto existing cats
9. emotion-more.jsx        ← adds 8 new emotion cats; inserts into KEYS
10. emotion-color.jsx      ← assigns cat.tone + variant.tone
11. app.jsx (or your host) ← consumes
```

All loaders mutate `window.EMOTION_CATEGORIES` and update
`window.EMOTION_CATEGORY_KEYS`. The final `KEYS` ordering keeps emotion
categories grouped together followed by scene categories.

---

## 10. Random-picker reference (firmware side)

```js
function pickVariant(categoryKey, recentIds) {
  const cat = EMOTION_CATEGORIES[categoryKey];
  const pool = cat.variants.filter(v => !recentIds.includes(v.id));
  const choices = pool.length ? pool : cat.variants;
  return choices[Math.floor(Math.random() * choices.length)];
}

// Filter by kind:
const emotionKeys = Object.keys(EMOTION_CATEGORIES)
  .filter(k => EMOTION_CATEGORIES[k].kind !== 'scene');
const sceneKeys = Object.keys(EMOTION_CATEGORIES)
  .filter(k => EMOTION_CATEGORIES[k].kind === 'scene');

// Idle loop (firmware default behaviour):
function tickIdle(state) {
  const v = pickVariant('normal', state.recent);
  state.recent = [...state.recent, v.id].slice(-6);
  play(v);
  schedule(tickIdle, 3000 + Math.random() * 3000); // 3–6 s
}

// Resolving color for any variant:
function toneFor(cat, variant) {
  return variant.tone || cat.tone || 'cyan';
}
```

**Scenes are picked explicitly** by the host — they should not enter the
random idle pool. Use scenes when there is data to display
(`showScene('weather', 'sunny')`), then return to emotions when done.

---

## 11. Porting checklist

1. Copy every `*.jsx` file under § 9 + `REQUIREMENTS.md`. Mind the load
   order — color assignment depends on every category being registered first.
2. Replace the SVG primitives in `emotion-core.jsx` (`Eye`, `Eyes`,
   `heartPath`, `starPath`, `arcPath`, `lidPath`) with your renderer's
   native draw calls. Renderer is called once per frame with quantized `t`.
3. Build a color LUT keyed by tone name (`EYE_TONES` in `emotion-core.jsx`
   is the source of truth). At draw time, resolve the active tone with
   `variant.tone || cat.tone || 'cyan'`.
4. **Honor the two-layer color rule (§ 4.2):**
   * Emotion variants: use cyan for face shapes (eyes, mouth, brows) and
     the resolved tone for accessories. The split is already encoded in
     the JSX as `var(--eye)` vs `var(--accent)` — keep two color slots in
     your renderer.
   * Scene variants: paint everything in the resolved tone.
   * Status bar: always cyan, regardless of category.
5. For `tone: 'multi'` variants, expose every tone in the palette as a
   named slot — the variant renders by referencing specific colors
   (`--tone-warm`, `--tone-rose`, etc.).
6. Implement `pickVariant` (§ 10) with your RNG and a per-category recent
   ring buffer. Keep ring length 4 – 6.
7. Wire an idle scheduler that picks from `normal` every 3 – 6 s when no
   other category is active.
8. Implement `showScene(catKey, variantId?)` for explicit data displays.
9. Honour the geometry: `W=320`, `H=240`, `STATUS_H=20`, `LX=85`, `RX=235`,
   `EYE_W=88`, `EYE_H=96`, `GAP=150`. Variants assume these.

---

## 12. Out of scope (future)

- Sprite-sheet PNG / GIF export per variant.
- Inter-emotion transitions (e.g. `listening → thinking` smooth blend).
- Tweaks panel for eye color / shape / personality preset.
- User-customizable tone per category (currently fixed by `EMOTION_TONE`).
- 16-color palette per scene (currently 9 tones).

---

## 13. Design constraints worth keeping

- Eyes fill the screen generously — outer margin only ~41 px each side.
- One visual language across all variants. Same eye-shape vocabulary, same
  stroke weights, same accessory style (dots, arcs, stars, hearts, Zs).
- Status bar always visible during emotion playback. Emotions render in the
  active area only (y = 20 … 240). Overlays **may** drift into the status
  band briefly — that's fine.
- Status bar centre slot is intentionally empty (no brand text).
- Status bar is fixed cyan — never tinted by category tone.
- Face shapes (eyes, mouth, brows) are fixed cyan across every emotion —
  the category's tone only colors accessories.
- No emoji. No raster images. SVG only.
- One bright accent color per frame (except `tone: 'multi'` variants).

---
