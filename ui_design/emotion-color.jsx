/* MOCHI-P emotion library — COLOR pass.
   Assigns a `tone` (key into window.EmotionCore.EYE_TONES) to every
   emotion + scene category. The renderer reads `cat.tone` and applies
   it as a CSS variable on the screen, so every `var(--eye)` inside the
   variant SVG picks up the new color — no per-variant changes needed.

   The robot stays "one bright color on near-black" (TFT-friendly),
   but each category now reads with the right emotional temperature.

   Loaded AFTER emotion-more.jsx, scenes-extras.jsx (anything that
   creates a category). Loaded BEFORE app.jsx.
*/

const cats = window.EMOTION_CATEGORIES;
const TONES = window.EmotionCore.EYE_TONES;

// emotion tones — chosen for emotional temperature, not literal color
const EMOTION_TONE = {
  // calm / neutral / robotic states stay cyan
  normal:       'cyan',
  greet:        'cyan',
  bored:        'cyan',
  cool:         'cyan',
  listening:    'cyan',
  thinking:     'cyan',
  loading:      'cyan',
  mute:         'cyan',

  // warm + positive
  happy:        'warm',
  wink:         'warm',
  smug:         'warm',
  proud:        'warm',
  excited:      'warm',

  // pink / affection
  love:         'rose',
  shy:          'rose',
  embarrassed:  'rose',

  // red / hot states
  angry:        'red',
  focused:      'red',
  determined:   'red',
  error:        'red',
  dead:         'red',

  // sad / cold blue
  sad:          'blue',
  crying:       'blue',

  // sick / disgusted / power-up green
  disgusted:    'green',
  charging:     'green',

  // sleepy / dreamy / mischievous purple
  sleepy:       'purple',
  sleeping:     'purple',
  mischievous:  'purple',
  dizzy:        'purple',
  suspicious:   'purple',

  // orange — appetite, curiosity, mild irritation
  hungry:       'orange',
  curious:      'orange',
  confused:     'orange',
  annoyed:      'orange',
  nervous:      'orange',

  // bright pop — shock
  surprised:    'white',
  scared:       'white',
};

// scene tones — picked per scene category
const SCENE_TONE = {
  weather:    'cyan',
  clock:      'cyan',
  music:      'rose',
  timer:      'orange',
  cooking:    'orange',
  walking:    'green',
  celebrate:  'warm',
  night:      'purple',
  fitness:    'red',
  call:       'green',
  message:    'cyan',
  camera:     'white',
  navigation: 'cyan',
  gift:       'rose',
  coffee:     'warm',
  plant:      'green',
  morning:    'warm',
  gaming:     'purple',
  calendar:   'cyan',
};

// scene tones — per-variant overrides (variants in the same category can
// each have their own emotional temperature). Format: 'category-variant-id'.
const SCENE_VARIANT_TONE = {
  // weather: sun=warm, rain=blue, cloudy=cyan, snow=white, storm=purple
  'weather-sunny':        'warm',
  'weather-rainy':        'blue',
  'weather-cloudy':       'cyan',
  'weather-snow':         'white',
  'weather-storm':        'purple',

  // clock: alarm screams red, others stay cyan
  'clock-digital':        'cyan',
  'clock-analog':         'cyan',
  'clock-alarm':          'red',

  // music: notes are rose, bars are cyan (eq look), wave is purple (dreamy)
  'music-notes':          'rose',
  'music-bars':           'cyan',
  'music-wave':           'purple',

  // timer: countdown burns warm/orange, hourglass = warm sand, progress = cyan
  'timer-countdown':      'orange',
  'timer-progress':       'cyan',
  'timer-hourglass':      'warm',

  // cooking: pan = red (fire), pot = blue (water steam)
  'cooking-pan':          'red',
  'cooking-pot':          'blue',

  // walking: footprints = green (nature), runner = red (energy)
  'walking-footprints':   'green',
  'walking-runner':       'red',

  // celebrate: trophy gold; confetti + firework opt into multi-color renderers
  'celebrate-trophy':     'warm',
  'celebrate-confetti':   'multi',
  'celebrate-firework':   'multi',

  // night: moon dreamy purple, starfield white
  'night-moon':           'purple',
  'night-stars':          'white',

  // fitness: heart rate red, steps green (health), dumbbell warm
  'fitness-hr':           'red',
  'fitness-steps':        'green',
  'fitness-dumbbell':     'warm',

  // call: incoming green, active cyan, missed red
  'call-incoming':        'green',
  'call-active':          'cyan',
  'call-missed':          'red',

  // message: typing cyan, envelope white, bubbles warm
  'message-typing':       'cyan',
  'message-envelope':     'white',
  'message-bubbles':      'warm',

  // camera: shutter white (flash), focus cyan
  'camera-shutter':       'white',
  'camera-focus':         'cyan',

  // navigation: compass cyan, pin red, arrow green
  'navigation-compass':   'cyan',
  'navigation-pin':       'red',
  'navigation-arrow':     'green',

  // gift: wrapped rose, opened warm
  'gift-wrapped':         'rose',
  'gift-open':            'warm',

  // coffee: both warm (caramel)
  'coffee-pour':          'warm',
  'coffee-cup':           'warm',

  // plant: growing green, watering blue
  'plant-grow':           'green',
  'plant-water':          'blue',

  // morning: sunrise warm, alarm rays red (urgent)
  'morning-sunrise':      'warm',
  'morning-alarm-rays':   'red',

  // gaming: pad purple (gamer aesthetic), powerup warm/gold
  'gaming-pad':           'purple',
  'gaming-powerup':       'warm',

  // calendar: date cyan, reminder red
  'calendar-date':        'cyan',
  'calendar-reminder':    'red',
};

// apply
for (const k of Object.keys(cats)) {
  const fromEmotion = EMOTION_TONE[k];
  const fromScene = SCENE_TONE[k];
  const tone = fromEmotion || fromScene || 'cyan';
  if (TONES[tone]) cats[k].tone = tone;
  else cats[k].tone = 'cyan';

  // Per-variant tone overrides (mostly for scenes).
  if (cats[k].variants) {
    for (const v of cats[k].variants) {
      const override = SCENE_VARIANT_TONE[v.id];
      if (override) v.tone = override;
    }
  }
}

// expose canonical maps for the host
window.EMOTION_TONE = EMOTION_TONE;
window.SCENE_TONE = SCENE_TONE;
window.SCENE_VARIANT_TONE = SCENE_VARIANT_TONE;
