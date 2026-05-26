/* MOCHI-P emotion viewer
   Categories → variants. Stage on the left, category picker + variant grid on right,
   one-thumb-per-category sheet at the bottom (1x native size).
*/

const { useState, useEffect, useRef, useMemo, useCallback } = React;

const CATS = window.EMOTION_CATEGORIES;
const CAT_KEYS = window.EMOTION_CATEGORY_KEYS;
const { W, H, STATUS_H, EYE_TONES } = window.EmotionCore;

const CYAN_EYE = EYE_TONES.cyan.eye;
const CYAN_GLOW = EYE_TONES.cyan.glow;

// Pre-built CSS-var map of every tone's eye color (--tone-cyan, --tone-warm…)
// so multi-color variants can paint with specific colors without
// hard-coding hex values inline.
const MULTI_TONE_VARS = (() => {
  const out = {};
  for (const [name, t] of Object.entries(EYE_TONES)) {
    out[`--tone-${name}`] = t.eye;
  }
  return out;
})();

// Resolve the CSS-var palette for a screen.
//   Emotions: --eye is ALWAYS cyan (eyes + mouth + brows are part of the
//   robot's face). The category tone only colors accessories via --accent.
//   Scenes: full takeover — --eye becomes the tone (no face to preserve).
//   'multi' is a marker tone for variants like confetti/firework: defaults
//   to cyan but the variant should reference --tone-warm/--tone-rose/….
function toneVars(tone, kind) {
  const isScene = kind === 'scene';
  const resolvedTone = (tone === 'multi') ? 'cyan' : tone;
  const td = EYE_TONES[resolvedTone] || EYE_TONES.cyan;
  const base = isScene
    ? { '--eye': td.eye,   '--accent': td.eye, '--eye-glow': td.glow }
    : { '--eye': CYAN_EYE, '--accent': td.eye, '--eye-glow': CYAN_GLOW };
  // Scenes get the full palette so multi-color variants can reference any
  // tone directly (`fill="var(--tone-rose)"` etc).
  return isScene ? { ...base, ...MULTI_TONE_VARS } : base;
}

// Per-variant tone wins over per-category tone.
function resolveTone(variant, cat) {
  return (variant && variant.tone) || (cat && cat.tone) || 'cyan';
}

// ---------- 320x240 Screen (renders one variant) ----------
function Screen({ variant, fps = 30, scale = 3, showStatus = true, statusTime = '14:32', label = '', tone = 'cyan', kind = 'emotion' }) {
  const [tick, setTick] = useState(0);
  const startRef = useRef(performance.now());

  useEffect(() => {
    startRef.current = performance.now();
    let raf, lastFrame = 0;
    const interval = 1000 / fps;
    const loop = (now) => {
      if (now - lastFrame >= interval) {
        lastFrame = now;
        setTick(now);
      }
      raf = requestAnimationFrame(loop);
    };
    raf = requestAnimationFrame(loop);
    return () => cancelAnimationFrame(raf);
  }, [fps, variant.id]);

  const elapsed = Math.max(0, tick - startRef.current);
  const frame = Math.floor((elapsed / 1000) * fps);
  const totalFrames = Math.max(1, Math.round((variant.duration / 1000) * fps));
  const t = (((frame % totalFrames) + totalFrames) % totalFrames) / totalFrames;

  const swPx = W * scale;

  return (
    <div
      className="screen-wrap"
      style={{ width: '100%', maxWidth: swPx, aspectRatio: `${W} / ${H}`, ...toneVars(tone, kind) }}
    >
      <svg
        viewBox={`0 0 ${W} ${H}`}
        preserveAspectRatio="xMidYMid meet"
        style={{ display: 'block', width: '100%', height: '100%',
                 background: 'var(--bg)', imageRendering: 'pixelated' }}
      >
        {showStatus && (
          <g>
            {/* Status bar is fixed-color system chrome — never tinted. */}
            <rect x={0} y={0} width={W} height={STATUS_H} fill="var(--bg)" />
            <line x1={0} y1={STATUS_H} x2={W} y2={STATUS_H}
                  stroke={CYAN_EYE} strokeWidth={0.5} opacity={0.25} />
            <text x={8} y={14} fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={11} fill={CYAN_EYE} opacity={0.85}>{statusTime}</text>
            {label && (
              <text x={W / 2} y={14} textAnchor="middle"
                    fontFamily="ui-monospace, Menlo, monospace" fontSize={11}
                    fontWeight={600} fill={CYAN_EYE} opacity={0.6} letterSpacing={2}>
                {label}
              </text>
            )}
            {!label && (
              <circle cx={W / 2} cy={10} r={2} fill={CYAN_EYE} opacity={0.7} />
            )}
            <g transform={`translate(${W - 46}, 6)`} fill="none"
               stroke={CYAN_EYE} strokeWidth={1.2} opacity={0.85}>
              <path d="M 0 6 Q 5 1, 10 6" />
              <path d="M 2 8 Q 5 5, 8 8" />
              <circle cx={5} cy={10} r={0.8} fill={CYAN_EYE} />
            </g>
            <g transform={`translate(${W - 28}, 5)`}>
              <rect x={0} y={0} width={20} height={10} rx={1.5} fill="none"
                    stroke={CYAN_EYE} strokeWidth={1} opacity={0.85} />
              <rect x={20} y={3} width={2} height={4} fill={CYAN_EYE} opacity={0.85} />
              <rect x={2} y={2} width={13} height={6} fill={CYAN_EYE} opacity={0.85} />
            </g>
          </g>
        )}
        <g>{variant.render(t)}</g>
      </svg>
    </div>
  );
}

// ---------- Tiny native-size (1x) preview ----------
function TinyScreen({ variant, fps = 30, tone = 'cyan', kind = 'emotion' }) {
  return (
    <div className="tiny-wrap">
      <Screen variant={variant} fps={fps} scale={1} showStatus={false} tone={tone} kind={kind} />
      <div className="tiny-label">{variant.label}</div>
    </div>
  );
}

// ---------- Variant thumb ----------
function VariantThumb({ variant, active, onClick, fps = 30, tone = 'cyan', kind = 'emotion' }) {
  return (
    <button
      className={`thumb ${active ? 'active' : ''}`}
      onClick={onClick}
      type="button"
    >
      <div className="thumb-inner">
        <Screen variant={variant} fps={fps} scale={0.6} showStatus={false} tone={tone} kind={kind} />
      </div>
      <div className="thumb-label">{variant.label}</div>
    </button>
  );
}

// ---------- Category pill ----------
function CategoryPill({ cat, catKey, active, onClick, scene = false, showTone = true }) {
  const td = EYE_TONES[cat.tone] || EYE_TONES.cyan;
  return (
    <button
      type="button"
      className={`pill ${active ? 'active' : ''} ${scene ? 'scene' : ''}`}
      onClick={onClick}
    >
      {showTone && (
        <span className="pill-dot" aria-hidden="true"
              style={{ background: td.eye, boxShadow: `0 0 6px ${td.glow}` }} />
      )}
      <span className="pill-name">{cat.label}</span>
      <span className="pill-count">{cat.variants.length}</span>
    </button>
  );
}

// ---------- Bezel ----------
function Bezel({ children, tone = 'cyan', kind = 'emotion' }) {
  return (
    <div className="bezel" style={toneVars(tone, kind)}>
      <div className="bezel-top">
        <span className="dot" />
        <span className="dot" />
        <span className="dot" />
      </div>
      <div className="bezel-screen">{children}</div>
      <div className="bezel-bottom">
        <div className="model">v0.3 · 320×240 · IPS · mono cyan</div>
      </div>
    </div>
  );
}

// ---------- App ----------
function App() {
  const [catKey, setCatKey] = useState('normal');
  const [variantIdx, setVariantIdx] = useState(0);
  const [fps, setFps] = useState(30);
  const [autoplay, setAutoplay] = useState(false);
  const [idleMode, setIdleMode] = useState(false);
  const [autoplayMs, setAutoplayMs] = useState(3500);
  const [colorMode, setColorMode] = useState(true);

  const toneFor = useCallback((k) => {
    if (!colorMode) return 'cyan';
    return (CATS[k] && CATS[k].tone) || 'cyan';
  }, [colorMode]);

  const cat = CATS[catKey];
  const variant = cat.variants[variantIdx];
  const recentRef = useRef([]); // track recent variant ids globally

  const emotionKeys = useMemo(() => CAT_KEYS.filter((k) => CATS[k].kind !== 'scene'), []);
  const sceneKeys   = useMemo(() => CAT_KEYS.filter((k) => CATS[k].kind === 'scene'), []);

  const pickVariant = useCallback((nextCatKey) => {
    const c = CATS[nextCatKey];
    const recent = recentRef.current;
    const pool = c.variants.filter((v) => !recent.includes(v.id));
    const choices = pool.length ? pool : c.variants;
    const v = choices[Math.floor(Math.random() * choices.length)];
    const idx = c.variants.indexOf(v);
    // update ring buffer (keep last 6)
    recentRef.current = [...recent, v.id].slice(-6);
    return idx;
  }, []);

  // Autoplay: cycle category → pick a fresh variant
  useEffect(() => {
    if (!autoplay) return;
    const id = setInterval(() => {
      setCatKey((prev) => {
        const i = CAT_KEYS.indexOf(prev);
        const next = CAT_KEYS[(i + 1) % CAT_KEYS.length];
        setVariantIdx(pickVariant(next));
        return next;
      });
    }, autoplayMs);
    return () => clearInterval(id);
  }, [autoplay, autoplayMs, pickVariant]);

  // Idle mode: stay on 'normal' category and randomly cycle variants.
  // This is what the firmware will do when the robot has nothing else to do.
  useEffect(() => {
    if (!idleMode) return;
    setCatKey('normal');
    const tick = () => setVariantIdx(pickVariant('normal'));
    tick();
    const id = setInterval(tick, autoplayMs);
    return () => clearInterval(id);
  }, [idleMode, autoplayMs, pickVariant]);

  // Live clock for status bar
  const [clock, setClock] = useState(() => formatClock(new Date()));
  useEffect(() => {
    const id = setInterval(() => setClock(formatClock(new Date())), 1000 * 30);
    return () => clearInterval(id);
  }, []);

  const onSelectCat = (k) => {
    setAutoplay(false);
    setIdleMode(false);
    setCatKey(k);
    setVariantIdx(0);
  };

  const cycleVariant = (dir) => {
    setAutoplay(false);
    setIdleMode(false);
    setVariantIdx((i) => wrap(i + dir, cat.variants.length));
  };

  const randomVariant = () => {
    setAutoplay(false);
    setIdleMode(false);
    const next = pickVariant(catKey);
    setVariantIdx(next);
  };

  // Total variant count
  const totalVariants = useMemo(
    () => CAT_KEYS.reduce((sum, k) => sum + CATS[k].variants.length, 0),
    [],
  );

  return (
    <div className="app" data-screen-label="PTalk Emotion Sheet">
      <header className="hdr">
        <div className="hdr-name">
          <span className="logo">◉</span>
          <span>PTalk · Emotion Library</span>
          <span className="hdr-version">v2.0</span>
        </div>
        <div className="hdr-meta">
          <span>{emotionKeys.length} emotions</span>
          <span>·</span>
          <span>{sceneKeys.length} scenes</span>
          <span>·</span>
          <span>{totalVariants} variants</span>
          <span>·</span>
          <span>{fps} fps</span>
          <span>·</span>
          <span>320 × 240 · {colorMode ? 'TFT color' : 'mono cyan'}</span>
        </div>
      </header>

      <main className="main">
        {/* Stage */}
        <section className="stage">
          <Bezel tone={colorMode ? resolveTone(variant, cat) : 'cyan'} kind={cat.kind || 'emotion'}>
            <Screen
              variant={variant}
              fps={fps}
              scale={3}
              showStatus={true}
              statusTime={clock}
              tone={colorMode ? resolveTone(variant, cat) : 'cyan'}
              kind={cat.kind || 'emotion'}
            />
          </Bezel>

          <div className="stage-meta">
            <div className="stage-cat">
              {(cat.kind === 'scene') ? 'SCENE · ' : ''}{cat.label.toUpperCase()}
            </div>
            <div className="big-label">
              <span className="big-label-key">
                {String(variantIdx + 1).padStart(2, '0')}/{String(cat.variants.length).padStart(2, '0')}
              </span>
              <span className="big-label-name">{variant.label}</span>
            </div>
            <div className="big-sub">
              <span className="mono">{variant.id}</span>
              <span className="sep">·</span>
              loop {variant.duration}ms
              <span className="sep">·</span>
              {Math.round((variant.duration / 1000) * fps)} frames @ {fps}fps
            </div>
          </div>

          <div className="controls">
            <div className="ctrl-group">
              <button className="ctrl-ico" onClick={() => cycleVariant(-1)} type="button" aria-label="Previous variant">←</button>
              <button className="ctrl-ico" onClick={randomVariant} type="button" aria-label="Random variant">⤬</button>
              <button className="ctrl-ico" onClick={() => cycleVariant(1)} type="button" aria-label="Next variant">→</button>
            </div>
            <button
              className={`ctrl ${colorMode ? 'on-color' : ''}`}
              onClick={() => setColorMode((v) => !v)}
              type="button"
              title="Toggle TFT color mode (per-category accent) vs mono cyan"
            >
              {colorMode ? '◐ Color' : '◯ Mono'}
            </button>
            <button
              className={`ctrl ${idleMode ? 'on-idle' : ''}`}
              onClick={() => { setAutoplay(false); setIdleMode((v) => !v); }}
              type="button"
              title="Random play within 'normal' — simulates firmware idle behaviour"
            >
              {idleMode ? '■ Stop idle' : '◉ Idle mode'}
            </button>
            <button
              className={`ctrl ${autoplay ? 'on' : ''}`}
              onClick={() => { setIdleMode(false); setAutoplay((v) => !v); }}
              type="button"
            >
              {autoplay ? '■ Stop autoplay' : '▶ Autoplay all'}
            </button>
            <label className="ctrl-slider">
              <span>fps</span>
              <input
                type="range" min={12} max={60} step={1}
                value={fps}
                onChange={(e) => setFps(+e.target.value)}
              />
              <span className="mono">{fps}</span>
            </label>
            <label className="ctrl-slider">
              <span>hold</span>
              <input
                type="range" min={1500} max={6000} step={500}
                value={autoplayMs}
                onChange={(e) => setAutoplayMs(+e.target.value)}
                disabled={!autoplay && !idleMode}
              />
              <span className="mono">{(autoplayMs / 1000).toFixed(1)}s</span>
            </label>
          </div>
        </section>

        {/* Right panel */}
        <aside className="lib-panel">
          <h2 className="panel-title">Library</h2>
          <p className="panel-sub">Pick a category → choose a variant. Listening &amp; Thinking are required for conversational dialog. <strong>Scenes</strong> are non-eye contexts (weather, clock, music…) for whole-screen displays.</p>

          <div className="cat-section-label">Emotions <span className="sec-count">{emotionKeys.length}</span></div>
          <div className="cat-rail">
            {emotionKeys.map((k) => (
              <CategoryPill
                key={k}
                cat={CATS[k]}
                catKey={k}
                active={k === catKey}
                onClick={() => onSelectCat(k)}
                showTone={colorMode}
              />
            ))}
          </div>

          <div className="cat-section-label scene">Scenes <span className="sec-count">{sceneKeys.length}</span></div>
          <div className="cat-rail">
            {sceneKeys.map((k) => (
              <CategoryPill
                key={k}
                cat={CATS[k]}
                catKey={k}
                active={k === catKey}
                onClick={() => onSelectCat(k)}
                scene
                showTone={colorMode}
              />
            ))}
          </div>

          <div className="cat-header">
            <div>
              <div className="cat-header-name">{cat.label}</div>
              <div className="cat-header-desc">{cat.desc}</div>
            </div>
            <div className="cat-header-count">
              {cat.variants.length} variants
            </div>
          </div>

          <div className="grid">
            {cat.variants.map((v, i) => (
              <VariantThumb
                key={v.id}
                variant={v}
                active={i === variantIdx}
                onClick={() => { setAutoplay(false); setIdleMode(false); setVariantIdx(i); }}
                fps={fps}
                tone={colorMode ? resolveTone(v, cat) : 'cyan'}
                kind={cat.kind || 'emotion'}
              />
            ))}
          </div>
        </aside>
      </main>

      {/* Footer sheet — one tile per category at native 1x size */}
      <footer className="strip-section">
        <div className="strip-title">
          <span>Actual size · 1× sheet</span>
          <span className="strip-sub">first variant of each category, native 320×240</span>
        </div>
        <div className="cat-section-label">Emotions</div>
        <div className="strip">
          {emotionKeys.map((k) => (
            <div key={k} className="strip-cell">
              <div className="strip-cell-cat">{CATS[k].label}</div>
              <TinyScreen variant={CATS[k].variants[0]}
                          fps={fps}
                          tone={colorMode ? resolveTone(CATS[k].variants[0], CATS[k]) : 'cyan'}
                          kind="emotion" />
            </div>
          ))}
        </div>
        <div className="cat-section-label scene" style={{ marginTop: 20 }}>Scenes</div>
        <div className="strip">
          {sceneKeys.map((k) => (
            <div key={k} className="strip-cell scene">
              <div className="strip-cell-cat">{CATS[k].label}</div>
              <TinyScreen variant={CATS[k].variants[0]}
                          fps={fps}
                          tone={colorMode ? resolveTone(CATS[k].variants[0], CATS[k]) : 'cyan'}
                          kind="scene" />
            </div>
          ))}
        </div>
      </footer>
    </div>
  );
}

function wrap(i, n) { return ((i % n) + n) % n; }
function formatClock(d) {
  const h = String(d.getHours()).padStart(2, '0');
  const m = String(d.getMinutes()).padStart(2, '0');
  return `${h}:${m}`;
}

ReactDOM.createRoot(document.getElementById('root')).render(<App />);
