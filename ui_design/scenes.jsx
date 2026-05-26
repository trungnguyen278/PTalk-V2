/* Scenes — contextual full-screen content, not just eyes.
   Inspired by Dasai-Mochi-style videos where the screen shows weather,
   clocks, music, etc. depending on context.

   Each scene follows the same variant contract as emotions:
     { id, label, duration, render: (t) => JSX }

   Loaded AFTER emotion-extras.jsx. Adds new categories with kind:'scene'
   to window.EMOTION_CATEGORIES + appends to EMOTION_CATEGORY_KEYS.
*/

const {
  W, H, STATUS_H, CY, GAP, LX, RX,
  TAU, lerp, clamp, wrap, ease, pulse, blink,
  Eye, Eyes, heartPath, starPath, arcPath,
} = window.EmotionCore;

const cats = window.EMOTION_CATEGORIES;

// Tag existing categories as emotions (in case the host wants to filter).
for (const k of Object.keys(cats)) {
  if (!cats[k].kind) cats[k].kind = 'emotion';
}

// Active scene area (below status bar).
const SY = STATUS_H + 8;          // top y of usable scene area
const SCX = W / 2;                // centre x

// --------- shared scene helpers ---------
const fmtTime = (sec) => {
  const m = Math.floor(sec / 60), s = Math.floor(sec % 60);
  return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
};
const bigText = (props, children) => (
  <text textAnchor="middle" fontFamily="ui-monospace, Menlo, monospace"
        fontWeight={700} fill="var(--eye)" {...props}>{children}</text>
);

// =============================================================
// SCENE: WEATHER — 5 variants
// =============================================================
cats.weather = {
  label: 'Weather',
  desc: 'Forecast / current conditions.',
  kind: 'scene',
  variants: [

    { id: 'weather-sunny', label: 'Sunny', duration: 4000,
      render: (t) => {
        const cx = SCX - 60, cy = CY - 10;
        return (
          <g>
            {/* Sun rays rotating */}
            <g transform={`translate(${cx} ${cy}) rotate(${t * 360 * 0.5})`}>
              {Array.from({ length: 8 }).map((_, i) => {
                const a = (i / 8) * TAU;
                return (
                  <line key={i}
                        x1={Math.cos(a) * 36} y1={Math.sin(a) * 36}
                        x2={Math.cos(a) * 52} y2={Math.sin(a) * 52}
                        stroke="var(--eye)" strokeWidth={4} strokeLinecap="round" />
                );
              })}
            </g>
            <circle cx={cx} cy={cy} r={28} fill="var(--eye)" />
            <circle cx={cx} cy={cy} r={20} fill="var(--bg)" />
            {/* Temperature + label */}
            {bigText({ x: SCX + 60, y: cy - 4, fontSize: 36 }, '28°')}
            {bigText({ x: SCX + 60, y: cy + 26, fontSize: 12, opacity: 0.7, letterSpacing: 2 }, 'SUNNY')}
          </g>
        );
      } },

    { id: 'weather-rainy', label: 'Rainy', duration: 1800,
      render: (t) => {
        const cx = SCX - 60, cy = CY - 14;
        return (
          <g>
            {/* Cloud */}
            <g fill="var(--eye)">
              <ellipse cx={cx - 14} cy={cy} rx={20} ry={16} />
              <ellipse cx={cx + 14} cy={cy} rx={22} ry={18} />
              <ellipse cx={cx} cy={cy - 10} rx={18} ry={14} />
              <rect x={cx - 30} y={cy + 4} width={62} height={14} rx={7} />
            </g>
            {/* Rain drops */}
            {Array.from({ length: 7 }).map((_, i) => {
              const p = ((t * 2) + i * 0.14) % 1;
              const x = cx - 24 + i * 8;
              const y = cy + 22 + p * 50;
              return (
                <line key={i} x1={x} y1={y} x2={x - 4} y2={y + 10}
                      stroke="var(--eye)" strokeWidth={2.5}
                      strokeLinecap="round" opacity={1 - p} />
              );
            })}
            {bigText({ x: SCX + 60, y: cy + 4, fontSize: 34 }, '18°')}
            {bigText({ x: SCX + 60, y: cy + 32, fontSize: 12, opacity: 0.7, letterSpacing: 2 }, 'RAINY')}
          </g>
        );
      } },

    { id: 'weather-cloudy', label: 'Cloudy', duration: 5000,
      render: (t) => {
        const cx = SCX - 60, cy = CY - 8;
        const drift = Math.sin(t * TAU) * 4;
        return (
          <g>
            {/* Two clouds */}
            <g fill="var(--eye)" transform={`translate(${drift} 0)`}>
              <ellipse cx={cx - 16} cy={cy + 2} rx={16} ry={12} />
              <ellipse cx={cx + 16} cy={cy + 2} rx={18} ry={14} />
              <ellipse cx={cx} cy={cy - 6} rx={14} ry={10} />
              <rect x={cx - 28} y={cy + 6} width={56} height={10} rx={5} />
            </g>
            <g fill="var(--eye)" opacity={0.5} transform={`translate(${-drift} ${-22})`}>
              <ellipse cx={cx - 24} cy={cy + 2} rx={12} ry={9} />
              <ellipse cx={cx} cy={cy + 2} rx={14} ry={11} />
              <ellipse cx={cx - 12} cy={cy - 4} rx={10} ry={8} />
            </g>
            {bigText({ x: SCX + 60, y: cy + 4, fontSize: 34 }, '22°')}
            {bigText({ x: SCX + 60, y: cy + 32, fontSize: 12, opacity: 0.7, letterSpacing: 2 }, 'CLOUDY')}
          </g>
        );
      } },

    { id: 'weather-snow', label: 'Snow', duration: 4500,
      render: (t) => {
        const cx = SCX - 60, cy = CY - 14;
        return (
          <g>
            <g fill="var(--eye)">
              <ellipse cx={cx - 14} cy={cy} rx={20} ry={16} />
              <ellipse cx={cx + 14} cy={cy} rx={22} ry={18} />
              <rect x={cx - 30} y={cy + 4} width={62} height={14} rx={7} />
            </g>
            {Array.from({ length: 9 }).map((_, i) => {
              const p = ((t * 0.8) + i * 0.11) % 1;
              const x = cx - 30 + i * 8 + Math.sin(p * TAU * 2 + i) * 4;
              const y = cy + 22 + p * 56;
              return (
                <circle key={i} cx={x} cy={y} r={2.5} fill="var(--eye)"
                        opacity={1 - p * 0.4} />
              );
            })}
            {bigText({ x: SCX + 60, y: cy + 4, fontSize: 34 }, '-2°')}
            {bigText({ x: SCX + 60, y: cy + 32, fontSize: 12, opacity: 0.7, letterSpacing: 2 }, 'SNOW')}
          </g>
        );
      } },

    { id: 'weather-storm', label: 'Storm', duration: 2400,
      render: (t) => {
        const cx = SCX - 60, cy = CY - 14;
        const flash = (t > 0.4 && t < 0.5) || (t > 0.7 && t < 0.74);
        return (
          <g>
            <g fill="var(--eye)">
              <ellipse cx={cx - 14} cy={cy} rx={20} ry={16} />
              <ellipse cx={cx + 14} cy={cy} rx={22} ry={18} />
              <rect x={cx - 30} y={cy + 4} width={62} height={14} rx={7} />
            </g>
            {/* Lightning bolt */}
            <polygon
              points={`${cx - 6},${cy + 22}
                       ${cx + 4},${cy + 38}
                       ${cx - 2},${cy + 38}
                       ${cx + 8},${cy + 60}
                       ${cx - 4},${cy + 44}
                       ${cx + 2},${cy + 44}`}
              fill="var(--eye)" opacity={flash ? 1 : 0.5} />
            {/* Rain */}
            {Array.from({ length: 5 }).map((_, i) => {
              const p = ((t * 2.5) + i * 0.2) % 1;
              const x = cx - 24 + i * 12;
              const y = cy + 24 + p * 50;
              return (
                <line key={i} x1={x} y1={y} x2={x - 4} y2={y + 10}
                      stroke="var(--eye)" strokeWidth={2.5}
                      strokeLinecap="round" opacity={1 - p} />
              );
            })}
            {bigText({ x: SCX + 60, y: cy + 4, fontSize: 34 }, '15°')}
            {bigText({ x: SCX + 60, y: cy + 32, fontSize: 12, opacity: 0.7, letterSpacing: 2 }, 'STORM')}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: CLOCK — 3 variants
// =============================================================
cats.clock = {
  label: 'Clock',
  desc: 'Time displays.',
  kind: 'scene',
  variants: [

    { id: 'clock-digital', label: 'Digital', duration: 60000,
      render: (t) => {
        const d = new Date();
        const hh = String(d.getHours()).padStart(2, '0');
        const mm = String(d.getMinutes()).padStart(2, '0');
        const blinkColon = Math.floor(t * 2) % 2 === 0 ? 1 : 0.2;
        return (
          <g>
            {bigText({ x: SCX - 56, y: CY + 18, fontSize: 90 }, hh)}
            {bigText({ x: SCX, y: CY + 12, fontSize: 80, opacity: blinkColon }, ':')}
            {bigText({ x: SCX + 56, y: CY + 18, fontSize: 90 }, mm)}
          </g>
        );
      } },

    { id: 'clock-analog', label: 'Analog', duration: 60000,
      render: (t) => {
        const d = new Date();
        const sec = d.getSeconds() + d.getMilliseconds() / 1000;
        const min = d.getMinutes() + sec / 60;
        const hr = (d.getHours() % 12) + min / 60;
        const cx = SCX, cy = CY + 4;
        const r = 76;
        return (
          <g>
            <circle cx={cx} cy={cy} r={r} fill="none"
                    stroke="var(--eye)" strokeWidth={3} />
            {/* Hour ticks */}
            {Array.from({ length: 12 }).map((_, i) => {
              const a = (i / 12) * TAU - Math.PI / 2;
              const inner = i % 3 === 0 ? r - 14 : r - 8;
              return (
                <line key={i}
                      x1={cx + Math.cos(a) * inner} y1={cy + Math.sin(a) * inner}
                      x2={cx + Math.cos(a) * (r - 2)} y2={cy + Math.sin(a) * (r - 2)}
                      stroke="var(--eye)" strokeWidth={i % 3 === 0 ? 3 : 1.5} />
              );
            })}
            {/* Hour hand */}
            <line x1={cx} y1={cy}
                  x2={cx + Math.cos(hr / 12 * TAU - Math.PI / 2) * 38}
                  y2={cy + Math.sin(hr / 12 * TAU - Math.PI / 2) * 38}
                  stroke="var(--eye)" strokeWidth={6} strokeLinecap="round" />
            {/* Minute hand */}
            <line x1={cx} y1={cy}
                  x2={cx + Math.cos(min / 60 * TAU - Math.PI / 2) * 58}
                  y2={cy + Math.sin(min / 60 * TAU - Math.PI / 2) * 58}
                  stroke="var(--eye)" strokeWidth={4} strokeLinecap="round" />
            {/* Second hand */}
            <line x1={cx} y1={cy}
                  x2={cx + Math.cos(sec / 60 * TAU - Math.PI / 2) * 64}
                  y2={cy + Math.sin(sec / 60 * TAU - Math.PI / 2) * 64}
                  stroke="var(--eye)" strokeWidth={1.5} opacity={0.85} />
            <circle cx={cx} cy={cy} r={4} fill="var(--eye)" />
          </g>
        );
      } },

    { id: 'clock-alarm', label: 'Alarm ring', duration: 800,
      render: (t) => {
        const shake = Math.sin(t * TAU * 12) * 4;
        const cx = SCX + shake, cy = CY + 8;
        const r = 56;
        return (
          <g transform={`translate(${shake} 0)`}>
            {/* Bell body */}
            <path d={`M ${cx - r} ${cy + 16}
                     Q ${cx} ${cy + 24}, ${cx + r} ${cy + 16}
                     L ${cx + r - 6} ${cy + 24}
                     L ${cx - r + 6} ${cy + 24} Z`} fill="var(--eye)" />
            <path d={`M ${cx} ${cy - 50}
                     L ${cx - 40} ${cy + 16}
                     L ${cx + 40} ${cy + 16} Z`}
                  fill="none" stroke="var(--eye)" strokeWidth={6}
                  strokeLinejoin="round" />
            <circle cx={cx} cy={cy - 50} r={6} fill="var(--eye)" />
            <circle cx={cx} cy={cy + 32} r={6} fill="var(--eye)" />
            {/* Sound waves */}
            {[0, 1, 2].map((i) => {
              const p = ((t * 3) + i * 0.33) % 1;
              return (
                <g key={i} stroke="var(--eye)" strokeWidth={3}
                   fill="none" opacity={1 - p} strokeLinecap="round">
                  <path d={`M ${cx - 50 - p * 10} ${cy - 10} Q ${cx - 60 - p * 14} ${cy + 4}, ${cx - 50 - p * 10} ${cy + 18}`} />
                  <path d={`M ${cx + 50 + p * 10} ${cy - 10} Q ${cx + 60 + p * 14} ${cy + 4}, ${cx + 50 + p * 10} ${cy + 18}`} />
                </g>
              );
            })}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: MUSIC — 3 variants
// =============================================================
cats.music = {
  label: 'Music',
  desc: 'Playing audio.',
  kind: 'scene',
  variants: [

    { id: 'music-notes', label: 'Floating notes', duration: 3000,
      render: (t) => {
        // Vinyl / disc on the left, notes floating across.
        const cx = 60, cy = CY + 4;
        return (
          <g>
            {/* Disc */}
            <g transform={`rotate(${t * 360} ${cx} ${cy})`}>
              <circle cx={cx} cy={cy} r={42} fill="var(--eye)" />
              <circle cx={cx} cy={cy} r={28} fill="var(--bg)" />
              <circle cx={cx} cy={cy} r={20} fill="var(--eye)" />
              <circle cx={cx} cy={cy} r={6} fill="var(--bg)" />
              <line x1={cx} y1={cy - 16} x2={cx} y2={cy - 12}
                    stroke="var(--bg)" strokeWidth={2} />
            </g>
            {/* Notes */}
            {[0, 1, 2, 3].map((i) => {
              const p = ((t * 0.8) + i * 0.25) % 1;
              const x = lerp(120, W - 24, p);
              const y = CY + Math.sin(p * TAU * 1.5 + i) * 22;
              const fadeIn = Math.min(1, p * 4);
              const fadeOut = Math.min(1, (1 - p) * 4);
              const op = fadeIn * fadeOut;
              return (
                <g key={i} opacity={op}>
                  <ellipse cx={x} cy={y + 8} rx={6} ry={4.5}
                           transform={`rotate(-20 ${x} ${y + 8})`}
                           fill="var(--eye)" />
                  <line x1={x + 5} y1={y + 6} x2={x + 5} y2={y - 18}
                        stroke="var(--eye)" strokeWidth={2.5} />
                  <path d={`M ${x + 5} ${y - 18} Q ${x + 16} ${y - 14}, ${x + 14} ${y - 4}`}
                        fill="none" stroke="var(--eye)" strokeWidth={2.5} />
                </g>
              );
            })}
          </g>
        );
      } },

    { id: 'music-bars', label: 'Equalizer', duration: 1200,
      render: (t) => {
        const n = 14;
        const gap = 4;
        const bw = (W - 60 - gap * (n - 1)) / n;
        return (
          <g fill="var(--eye)">
            {Array.from({ length: n }).map((_, i) => {
              const phase = (t * TAU * 2) + i * 0.5;
              const h = 14 + Math.abs(Math.sin(phase)) * 80;
              const x = 30 + i * (bw + gap);
              const y = H - 28 - h;
              return <rect key={i} x={x} y={y} width={bw} height={h} rx={3} />;
            })}
            <line x1={20} y1={H - 24} x2={W - 20} y2={H - 24}
                  stroke="var(--eye)" strokeWidth={1.5} opacity={0.4} />
          </g>
        );
      } },

    { id: 'music-wave', label: 'Waveform', duration: 1600,
      render: (t) => {
        const samples = 80;
        const baseY = CY + 8;
        let d = `M 20 ${baseY}`;
        for (let i = 0; i <= samples; i++) {
          const x = 20 + (i / samples) * (W - 40);
          const phase = (i / samples) * TAU * 3 + t * TAU * 2;
          const y = baseY + Math.sin(phase) * 30 * Math.abs(Math.sin((i / samples) * Math.PI));
          d += ` L ${x} ${y}`;
        }
        return (
          <g>
            <path d={d} fill="none" stroke="var(--eye)" strokeWidth={3} strokeLinecap="round" />
            {/* Play triangle */}
            <polygon points={`${SCX - 8},${SY + 12} ${SCX + 8},${SY + 22} ${SCX - 8},${SY + 32}`}
                     fill="var(--eye)" />
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: TIMER — 3 variants
// =============================================================
cats.timer = {
  label: 'Timer',
  desc: 'Countdown / pomodoro / hourglass.',
  kind: 'scene',
  variants: [

    { id: 'timer-countdown', label: 'Countdown', duration: 4000,
      render: (t) => {
        const remaining = Math.max(0, Math.ceil(120 * (1 - t / 4)));
        const remain = remaining > 0 ? remaining : 0;
        const pulseN = remain <= 3 ? 1 + Math.sin(t * TAU * 8) * 0.05 : 1;
        return (
          <g>
            {bigText({ x: SCX, y: CY + 28, fontSize: 88 * pulseN },
              fmtTime(remain))}
            {bigText({ x: SCX, y: SY + 22, fontSize: 12, opacity: 0.6, letterSpacing: 3 },
              'TIMER')}
          </g>
        );
      } },

    { id: 'timer-progress', label: 'Ring progress', duration: 4000,
      render: (t) => {
        const cx = SCX, cy = CY + 6;
        const r = 64;
        const progress = t;
        const circ = TAU * r;
        return (
          <g>
            <circle cx={cx} cy={cy} r={r} fill="none"
                    stroke="var(--eye)" strokeWidth={6} opacity={0.2} />
            <circle cx={cx} cy={cy} r={r} fill="none"
                    stroke="var(--eye)" strokeWidth={6}
                    strokeDasharray={`${circ * progress} ${circ}`}
                    strokeLinecap="round"
                    transform={`rotate(-90 ${cx} ${cy})`} />
            {bigText({ x: cx, y: cy + 12, fontSize: 36 }, `${Math.round((1 - progress) * 100)}%`)}
          </g>
        );
      } },

    { id: 'timer-hourglass', label: 'Hourglass', duration: 4000,
      render: (t) => {
        const cx = SCX, cy = CY + 4;
        const topFill = Math.max(0, 1 - t);
        const botFill = t;
        return (
          <g>
            {/* Frame */}
            <g stroke="var(--eye)" strokeWidth={4} fill="none" strokeLinejoin="round">
              <line x1={cx - 28} y1={cy - 50} x2={cx + 28} y2={cy - 50} />
              <line x1={cx - 28} y1={cy + 50} x2={cx + 28} y2={cy + 50} />
              <path d={`M ${cx - 28} ${cy - 50}
                       L ${cx + 28} ${cy - 50}
                       L ${cx + 4} ${cy}
                       L ${cx + 28} ${cy + 50}
                       L ${cx - 28} ${cy + 50}
                       L ${cx - 4} ${cy} Z`} />
            </g>
            {/* Top sand */}
            <path d={`M ${cx - 24 * topFill} ${cy - 50}
                     L ${cx + 24 * topFill} ${cy - 50}
                     L ${cx + 4} ${cy - (50 * (1 - topFill))}
                     L ${cx - 4} ${cy - (50 * (1 - topFill))} Z`}
                  fill="var(--eye)" />
            {/* Bottom sand */}
            <path d={`M ${cx - 24 * botFill} ${cy + 50}
                     L ${cx + 24 * botFill} ${cy + 50}
                     L ${cx + 4} ${cy + (50 * (1 - botFill))}
                     L ${cx - 4} ${cy + (50 * (1 - botFill))} Z`}
                  fill="var(--eye)" />
            {/* Falling grain */}
            {t > 0.02 && t < 0.98 && (
              <circle cx={cx} cy={cy + Math.sin(t * TAU * 4) * 4 + 12}
                      r={1.6} fill="var(--eye)" />
            )}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: COOKING — 2 variants
// =============================================================
cats.cooking = {
  label: 'Cooking',
  desc: 'Pan / pot / kitchen.',
  kind: 'scene',
  variants: [

    { id: 'cooking-pan', label: 'Sizzling pan', duration: 1600,
      render: (t) => {
        const cx = SCX, cy = CY + 24;
        return (
          <g>
            {/* Pan */}
            <g stroke="var(--eye)" strokeWidth={5} fill="none" strokeLinecap="round">
              <path d={`M ${cx - 70} ${cy} L ${cx + 70} ${cy}
                       Q ${cx + 70} ${cy + 32}, ${cx + 56} ${cy + 32}
                       L ${cx - 56} ${cy + 32}
                       Q ${cx - 70} ${cy + 32}, ${cx - 70} ${cy} Z`} />
              <line x1={cx + 70} y1={cy + 6} x2={cx + 108} y2={cy - 6} strokeWidth={6} />
            </g>
            {/* Sizzling food bubbles */}
            {[0, 1, 2, 3].map((i) => {
              const p = ((t * 2) + i * 0.25) % 1;
              const x = cx - 36 + i * 24;
              const y = cy + 16 - p * 8;
              return (
                <circle key={i} cx={x} cy={y} r={4 - p * 2}
                        fill="var(--eye)" opacity={1 - p} />
              );
            })}
            {/* Flame below */}
            <g transform={`translate(${cx} ${cy + 50})`}>
              <path d={`M -28 0
                       Q -18 ${-20 - Math.sin(t * TAU * 6) * 4}, -10 0
                       Q -2 ${-30 - Math.sin(t * TAU * 7) * 3}, 6 0
                       Q 14 ${-22 - Math.sin(t * TAU * 5) * 4}, 24 0 Z`}
                    fill="var(--eye)" opacity={0.85} />
            </g>
            {bigText({ x: SCX, y: SY + 20, fontSize: 12, opacity: 0.6, letterSpacing: 3 }, 'COOKING')}
          </g>
        );
      } },

    { id: 'cooking-pot', label: 'Boiling pot', duration: 2400,
      render: (t) => {
        const cx = SCX, cy = CY + 32;
        return (
          <g>
            {/* Pot */}
            <g stroke="var(--eye)" strokeWidth={5} fill="none">
              <rect x={cx - 50} y={cy - 16} width={100} height={40} rx={4} />
              <line x1={cx - 60} y1={cy - 16} x2={cx + 60} y2={cy - 16} strokeWidth={6} />
              <line x1={cx - 60} y1={cy - 16} x2={cx - 66} y2={cy - 10} strokeWidth={5} />
              <line x1={cx + 60} y1={cy - 16} x2={cx + 66} y2={cy - 10} strokeWidth={5} />
            </g>
            {/* Bubbles inside */}
            {[0, 1, 2, 3, 4].map((i) => {
              const p = ((t * 1.5) + i * 0.2) % 1;
              const x = cx - 32 + i * 16;
              const y = cy + 16 - p * 22;
              return (
                <circle key={i} cx={x} cy={y} r={3 + (1 - p) * 2}
                        fill="var(--eye)" opacity={1 - p} />
              );
            })}
            {/* Steam plumes */}
            {[0, 1, 2].map((i) => {
              const p = ((t * 0.8) + i * 0.33) % 1;
              const x = cx - 24 + i * 24;
              const y = cy - 26 - p * 50;
              return (
                <path key={i}
                      d={`M ${x} ${y} Q ${x + 6} ${y - 8}, ${x} ${y - 16} Q ${x - 6} ${y - 24}, ${x} ${y - 32}`}
                      fill="none" stroke="var(--eye)" strokeWidth={3}
                      strokeLinecap="round" opacity={(1 - p) * 0.8} />
              );
            })}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: WALKING — 2 variants
// =============================================================
cats.walking = {
  label: 'Walking',
  desc: 'Movement / footsteps.',
  kind: 'scene',
  variants: [

    { id: 'walking-footprints', label: 'Footprints', duration: 2400,
      render: (t) => {
        const groundY = H - 30;
        return (
          <g>
            <line x1={10} y1={groundY} x2={W - 10} y2={groundY}
                  stroke="var(--eye)" strokeWidth={1} opacity={0.3} strokeDasharray="3 4" />
            {[0, 1, 2, 3, 4, 5].map((i) => {
              const seq = ((t * 1) + i * 0.16) % 1;
              const x = lerp(W + 24, -24, seq);
              const isLeft = i % 2 === 0;
              const dy = isLeft ? -6 : 6;
              const op = Math.min(1, seq * 4) * Math.min(1, (1 - seq) * 4);
              return (
                <g key={i} fill="var(--eye)" opacity={op * 0.9}>
                  <ellipse cx={x} cy={groundY + dy} rx={9} ry={6} />
                  {/* toe pads */}
                  <circle cx={x - 8} cy={groundY + dy - 8} r={2.5} />
                  <circle cx={x - 4} cy={groundY + dy - 10} r={2.5} />
                  <circle cx={x + 1} cy={groundY + dy - 10} r={2.5} />
                  <circle cx={x + 6} cy={groundY + dy - 8} r={2.5} />
                </g>
              );
            })}
            {bigText({ x: SCX, y: SY + 22, fontSize: 12, opacity: 0.55, letterSpacing: 3 }, 'WALKING')}
          </g>
        );
      } },

    { id: 'walking-runner', label: 'Runner', duration: 1200,
      render: (t) => {
        const groundY = H - 30;
        const bob = Math.abs(Math.sin(t * TAU * 4)) * 4;
        const cx = SCX, cy = groundY - 40 - bob;
        const armA = Math.sin(t * TAU * 4) * 26;
        const legA = Math.sin(t * TAU * 4) * 22;
        return (
          <g stroke="var(--eye)" strokeWidth={4} strokeLinecap="round" fill="none">
            {/* Ground */}
            <line x1={10} y1={groundY} x2={W - 10} y2={groundY}
                  strokeWidth={1.5} opacity={0.4} />
            {/* Body */}
            <circle cx={cx} cy={cy - 24} r={10} fill="var(--eye)" stroke="none" />
            <line x1={cx} y1={cy - 14} x2={cx + 2} y2={cy + 12} />
            {/* Arms */}
            <line x1={cx} y1={cy - 8} x2={cx - 12 + armA} y2={cy + 4 - armA / 2} />
            <line x1={cx} y1={cy - 8} x2={cx + 12 - armA} y2={cy + 4 + armA / 2} />
            {/* Legs */}
            <line x1={cx + 2} y1={cy + 12} x2={cx - 8 + legA} y2={groundY - bob} />
            <line x1={cx + 2} y1={cy + 12} x2={cx + 10 - legA} y2={groundY - bob} />
            {/* Speed lines */}
            {[0, 1, 2].map((i) => {
              const p = ((t * 4) + i * 0.33) % 1;
              return (
                <line key={i} x1={cx - 50 - p * 30} y1={cy - 8 + i * 12}
                      x2={cx - 30 - p * 20} y2={cy - 8 + i * 12}
                      strokeWidth={2} opacity={(1 - p) * 0.7} />
              );
            })}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: CELEBRATE — 3 variants
// =============================================================
cats.celebrate = {
  label: 'Celebrate',
  desc: 'Confetti / trophy / fireworks.',
  kind: 'scene',
  variants: [

    { id: 'celebrate-trophy', label: 'Trophy', duration: 2400,
      render: (t) => {
        const cx = SCX, cy = CY + 16;
        const shine = Math.sin(t * TAU) * 0.3 + 0.7;
        return (
          <g>
            {/* Trophy cup */}
            <g fill="var(--eye)" opacity={shine}>
              <path d={`M ${cx - 24} ${cy - 30}
                       L ${cx + 24} ${cy - 30}
                       L ${cx + 22} ${cy + 4}
                       Q ${cx} ${cy + 14}, ${cx - 22} ${cy + 4} Z`} />
              <rect x={cx - 8} y={cy + 12} width={16} height={12} />
              <rect x={cx - 22} y={cy + 22} width={44} height={6} rx={2} />
            </g>
            {/* Handles */}
            <g stroke="var(--eye)" strokeWidth={3} fill="none">
              <path d={`M ${cx - 24} ${cy - 22} Q ${cx - 40} ${cy - 18}, ${cx - 36} ${cy - 4}`} />
              <path d={`M ${cx + 24} ${cy - 22} Q ${cx + 40} ${cy - 18}, ${cx + 36} ${cy - 4}`} />
            </g>
            {/* Star sparkles */}
            {[0, 1, 2, 3].map((i) => {
              const p = ((t * 1.2) + i * 0.25) % 1;
              const a = i * (TAU / 4) + t * TAU * 0.4;
              const x = cx + Math.cos(a) * 60;
              const y = cy + Math.sin(a) * 30;
              const s = lerp(8, 2, p);
              return <path key={i} d={starPath(x, y, s, s * 0.4)} fill="var(--eye)" opacity={1 - p} />;
            })}
          </g>
        );
      } },

    { id: 'celebrate-confetti', label: 'Confetti', duration: 3000,
      render: (t) => {
        // Each piece picks a color from the full tone palette — rainbow rain.
        const PIECE_COLORS = [
          'var(--tone-warm)', 'var(--tone-rose)', 'var(--tone-cyan)',
          'var(--tone-green)', 'var(--tone-purple)', 'var(--tone-orange)',
        ];
        return (
          <g>
            {bigText({ x: SCX, y: CY + 12, fontSize: 28, letterSpacing: 4 }, 'YAY!')}
            {Array.from({ length: 18 }).map((_, i) => {
              const p = ((t + i * 0.06) % 1);
              const x = (i * 53 + Math.sin(p * TAU + i) * 8) % W;
              const y = lerp(SY, H - 8, p);
              const rot = p * 360 * 2 + i * 30;
              const shape = i % 3;
              const color = PIECE_COLORS[i % PIECE_COLORS.length];
              return (
                <g key={i} transform={`translate(${x} ${y}) rotate(${rot})`}
                   fill={color} opacity={(1 - p) * 0.9}>
                  {shape === 0 && <rect x={-3} y={-2} width={6} height={4} />}
                  {shape === 1 && <circle cx={0} cy={0} r={2.5} />}
                  {shape === 2 && <rect x={-1.5} y={-4} width={3} height={8} />}
                </g>
              );
            })}
          </g>
        );
      } },

    { id: 'celebrate-firework', label: 'Fireworks', duration: 2000,
      render: (t) => {
        // Each burst gets its own color.
        const BURSTS = [
          { x: SCX - 60, y: CY - 6,  phase: 0,   color: 'var(--tone-warm)'  },
          { x: SCX + 50, y: CY + 14, phase: 0.4, color: 'var(--tone-rose)'  },
          { x: SCX,      y: CY - 20, phase: 0.7, color: 'var(--tone-green)' },
        ];
        return (
          <g>
            {BURSTS.map((fw, i) => {
              const p = ((t + fw.phase) % 1);
              const r = lerp(2, 50, p);
              const op = (1 - p);
              return (
                <g key={i} stroke={fw.color} strokeWidth={2.5}
                   fill="none" opacity={op}>
                  {Array.from({ length: 10 }).map((_, j) => {
                    const a = (j / 10) * TAU;
                    return (
                      <line key={j}
                            x1={fw.x + Math.cos(a) * r * 0.5}
                            y1={fw.y + Math.sin(a) * r * 0.5}
                            x2={fw.x + Math.cos(a) * r}
                            y2={fw.y + Math.sin(a) * r} />
                    );
                  })}
                  <circle cx={fw.x} cy={fw.y} r={3 + (1 - p) * 3} fill={fw.color} />
                </g>
              );
            })}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: NIGHT — 2 variants
// =============================================================
cats.night = {
  label: 'Night',
  desc: 'Moon / stars / dreaming.',
  kind: 'scene',
  variants: [

    { id: 'night-moon', label: 'Moon & stars', duration: 6000,
      render: (t) => {
        const cx = SCX - 50, cy = CY - 8;
        return (
          <g>
            {/* Crescent moon */}
            <circle cx={cx} cy={cy} r={32} fill="var(--eye)" />
            <circle cx={cx + 12} cy={cy - 4} r={28} fill="var(--bg)" />
            {/* Stars (twinkle) */}
            {[
              { x: SCX + 30, y: SY + 16, ph: 0 },
              { x: SCX + 70, y: SY + 40, ph: 0.3 },
              { x: SCX + 40, y: CY + 28, ph: 0.6 },
              { x: SCX + 88, y: CY - 8, ph: 0.15 },
              { x: SCX - 90, y: SY + 36, ph: 0.5 },
            ].map((s, i) => {
              const tw = 0.4 + Math.abs(Math.sin((t + s.ph) * TAU)) * 0.6;
              return <path key={i} d={starPath(s.x, s.y, 5 * tw, 2)}
                           fill="var(--eye)" opacity={tw} />;
            })}
          </g>
        );
      } },

    { id: 'night-stars', label: 'Starfield', duration: 8000,
      render: (t) => (
        <g>
          {Array.from({ length: 24 }).map((_, i) => {
            const x = (i * 47 + 13) % (W - 16) + 8;
            const y = SY + ((i * 31 + 7) % (H - SY - 20));
            const ph = (i % 8) / 8;
            const tw = 0.3 + Math.abs(Math.sin((t + ph) * TAU)) * 0.7;
            const sz = ((i % 3) + 1) * 1.5;
            return <circle key={i} cx={x} cy={y} r={sz} fill="var(--eye)" opacity={tw} />;
          })}
          {bigText({ x: SCX, y: H - 22, fontSize: 12, opacity: 0.5, letterSpacing: 4 }, 'GOOD NIGHT')}
        </g>
      ) },
  ],
};

// ---------- merge into the key list ----------
const sceneKeys = ['weather', 'clock', 'music', 'timer', 'cooking', 'walking', 'celebrate', 'night'];
const existing = window.EMOTION_CATEGORY_KEYS || Object.keys(cats);
window.EMOTION_CATEGORY_KEYS = [
  ...existing.filter((k) => !sceneKeys.includes(k)),
  ...sceneKeys.filter((k) => k in cats),
];
