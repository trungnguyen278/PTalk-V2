/**
 * File:    scenes-extras.jsx
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

/* Scenes — EXTRAS
   More contextual full-screen scenes. Loaded AFTER scenes.jsx.
   Adds 11 new scene categories. Each variant follows the standard
   { id, label, duration, render: (t) => JSX } contract.
*/

const {
  W, H, STATUS_H, CY, GAP, LX, RX,
  TAU, lerp, clamp, wrap, ease, pulse, blink,
  Eye, Eyes, heartPath, starPath, arcPath,
} = window.EmotionCore;

const cats = window.EMOTION_CATEGORIES;

const SY = STATUS_H + 8;
const SCX = W / 2;

const bigText = (props, children) => (
  <text textAnchor="middle" fontFamily="ui-monospace, Menlo, monospace"
        fontWeight={700} fill="var(--eye)" {...props}>{children}</text>
);

// =============================================================
// SCENE: FITNESS — 3 variants
// =============================================================
cats.fitness = {
  label: 'Fitness',
  desc: 'Heart rate, steps, exercise.',
  kind: 'scene',
  variants: [

    { id: 'fitness-hr', label: 'Heart rate', duration: 1200,
      render: (t) => {
        // Beat envelope: two quick beats per cycle.
        const beat = Math.max(pulse(t, 0.15, 0.06), pulse(t, 0.35, 0.06));
        const bpm = 72 + Math.floor(Math.sin(t * TAU * 0.3) * 4);
        const hy = CY - 4;
        const hx = SCX - 60;
        const hs = 36 * (1 + beat * 0.18);
        // ECG path
        const ecgY = CY + 44;
        let d = `M 14 ${ecgY}`;
        const beats = 3;
        const seg = (W - 28) / beats;
        for (let i = 0; i < beats; i++) {
          const x0 = 14 + i * seg;
          d += ` L ${x0 + seg * 0.30} ${ecgY}`;
          d += ` L ${x0 + seg * 0.35} ${ecgY - 4}`;
          d += ` L ${x0 + seg * 0.40} ${ecgY + 18}`;
          d += ` L ${x0 + seg * 0.45} ${ecgY - 28}`;
          d += ` L ${x0 + seg * 0.50} ${ecgY + 6}`;
          d += ` L ${x0 + seg * 0.55} ${ecgY}`;
          d += ` L ${x0 + seg} ${ecgY}`;
        }
        // Sweep marker
        const sweepX = 14 + ((t * 2) % 1) * (W - 28);
        return (
          <g>
            <path d={heartPath(hx, hy, hs)} fill="var(--eye)" />
            {bigText({ x: SCX + 50, y: hy + 8, fontSize: 48 }, `${bpm}`)}
            {bigText({ x: SCX + 50, y: hy + 30, fontSize: 11, opacity: 0.65, letterSpacing: 2 }, 'BPM')}
            <path d={d} fill="none" stroke="var(--eye)" strokeWidth={2.5}
                  strokeLinejoin="round" strokeLinecap="round" opacity={0.85} />
            <line x1={sweepX} y1={ecgY - 34} x2={sweepX} y2={ecgY + 24}
                  stroke="var(--eye)" strokeWidth={1.5} opacity={0.4} />
          </g>
        );
      } },

    { id: 'fitness-steps', label: 'Step counter', duration: 4000,
      render: (t) => {
        const steps = 6420 + Math.floor(t * 1200);
        const goal = 10000;
        const pct = steps / goal;
        return (
          <g>
            {/* Footstep icons row */}
            {[0, 1, 2, 3].map((i) => {
              const phase = ((t * 3) - i * 0.18 + 1) % 1;
              const op = phase < 0.5 ? 1 : 1 - (phase - 0.5) * 2;
              return (
                <g key={i} fill="var(--eye)" opacity={clamp(op, 0.25, 1)}
                   transform={`translate(${SCX - 90 + i * 60} ${SY + 24})`}>
                  <ellipse cx={0} cy={0} rx={11} ry={7} />
                  <circle cx={-9} cy={-9} r={3} />
                  <circle cx={-4} cy={-11} r={3} />
                  <circle cx={2} cy={-11} r={3} />
                  <circle cx={8} cy={-9} r={3} />
                </g>
              );
            })}
            {/* Big step number */}
            {bigText({ x: SCX, y: CY + 30, fontSize: 56 }, steps.toLocaleString())}
            {bigText({ x: SCX, y: CY + 50, fontSize: 11, opacity: 0.65, letterSpacing: 2 },
              `STEPS · ${Math.round(pct * 100)}%`)}
            {/* Progress bar */}
            <rect x={28} y={H - 22} width={W - 56} height={6} rx={3}
                  fill="none" stroke="var(--eye)" strokeWidth={1} opacity={0.45} />
            <rect x={29} y={H - 21} width={(W - 58) * clamp(pct, 0, 1)} height={4} rx={2}
                  fill="var(--eye)" />
          </g>
        );
      } },

    { id: 'fitness-dumbbell', label: 'Dumbbell', duration: 1400,
      render: (t) => {
        const lift = Math.abs(Math.sin(t * TAU)) * 26;
        const cy = CY + 22 - lift;
        return (
          <g>
            <g fill="var(--eye)">
              <rect x={SCX - 50} y={cy - 5} width={100} height={10} rx={3} />
              <rect x={SCX - 60} y={cy - 14} width={14} height={28} rx={3} />
              <rect x={SCX + 46} y={cy - 14} width={14} height={28} rx={3} />
              <rect x={SCX - 68} y={cy - 18} width={8} height={36} rx={2} />
              <rect x={SCX + 60} y={cy - 18} width={8} height={36} rx={2} />
            </g>
            {bigText({ x: SCX, y: SY + 22, fontSize: 12, opacity: 0.7, letterSpacing: 3 }, 'WORKOUT')}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: CALL — 3 variants
// =============================================================
cats.call = {
  label: 'Call',
  desc: 'Phone / voice call.',
  kind: 'scene',
  variants: [

    { id: 'call-incoming', label: 'Incoming', duration: 1400,
      render: (t) => {
        const wob = Math.sin(t * TAU * 6) * 6;
        const cx = SCX - 56, cy = CY + 6;
        return (
          <g>
            {/* Handset shaking */}
            <g transform={`translate(${cx} ${cy}) rotate(${wob})`} fill="var(--eye)">
              <path d="M -20 -22 Q -18 -10, -8 -2 Q 0 6, 12 14 Q 22 20, 22 12 L 16 6
                       Q 10 4, 4 -2 Q -2 -8, -4 -14 L -10 -20 Z" />
            </g>
            {/* Outgoing waves */}
            {[0, 1, 2].map((i) => {
              const p = ((t * 2.4) + i * 0.33) % 1;
              const r = 24 + p * 22;
              return (
                <g key={i} stroke="var(--eye)" fill="none" strokeWidth={2.5}
                   opacity={(1 - p) * 0.7} strokeLinecap="round">
                  <path d={`M ${cx + 18} ${cy - r} Q ${cx + 18 + r * 0.4} ${cy}, ${cx + 18} ${cy + r}`} />
                </g>
              );
            })}
            {bigText({ x: SCX + 60, y: cy - 8, fontSize: 18 }, 'INCOMING')}
            {bigText({ x: SCX + 60, y: cy + 16, fontSize: 14, opacity: 0.75 }, 'CALL…')}
          </g>
        );
      } },

    { id: 'call-active', label: 'In call', duration: 1500,
      render: (t) => {
        const sec = Math.floor(t * 90) + 12;
        const mm = String(Math.floor(sec / 60)).padStart(2, '0');
        const ss = String(sec % 60).padStart(2, '0');
        return (
          <g>
            {/* Phone icon */}
            <g transform={`translate(${SCX} ${SY + 30})`} fill="var(--eye)">
              <path d="M -16 -18 Q -14 -8, -6 -2 Q 2 6, 10 12 Q 18 18, 18 10 L 12 4
                       Q 8 2, 2 -4 Q -4 -10, -6 -16 L -10 -20 Z" />
            </g>
            {bigText({ x: SCX, y: CY + 20, fontSize: 42 }, `${mm}:${ss}`)}
            {/* EQ bars at bottom */}
            {Array.from({ length: 11 }).map((_, i) => {
              const phase = (t * TAU * 3) + i * 0.7;
              const h = 4 + Math.abs(Math.sin(phase)) * 18;
              const x = 40 + i * 24;
              return <rect key={i} x={x - 3} y={H - 22 - h} width={6} height={h} rx={2}
                           fill="var(--eye)" />;
            })}
          </g>
        );
      } },

    { id: 'call-missed', label: 'Missed', duration: 2400,
      render: (t) => {
        const blinkO = Math.floor(t * 3) % 2 === 0 ? 1 : 0.5;
        return (
          <g>
            <g transform={`translate(${SCX} ${CY - 4})`} fill="var(--eye)">
              <path d="M -28 -8 Q -24 6, -10 14 Q 4 22, 20 18 L 24 8
                       Q 16 4, 12 -4 L 4 -2 L -4 -10 Q -10 -14, -16 -10 L -22 -14 Z" />
            </g>
            <g transform={`translate(${SCX + 18} ${CY - 18})`}
               opacity={blinkO}
               stroke="var(--eye)" strokeWidth={4} strokeLinecap="round">
              <line x1={-7} y1={-7} x2={7} y2={7} />
              <line x1={7} y1={-7} x2={-7} y2={7} />
            </g>
            {bigText({ x: SCX, y: H - 28, fontSize: 14, opacity: 0.8 }, 'MISSED CALL')}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: MESSAGE — 3 variants
// =============================================================
cats.message = {
  label: 'Message',
  desc: 'Chat / typing / mail.',
  kind: 'scene',
  variants: [

    { id: 'message-typing', label: 'Typing', duration: 1500,
      render: (t) => {
        const cx = SCX, cy = CY + 4;
        return (
          <g>
            {/* Bubble */}
            <g fill="var(--eye)">
              <rect x={cx - 64} y={cy - 26} width={128} height={52} rx={26} />
              <polygon points={`${cx - 30},${cy + 26} ${cx - 14},${cy + 26} ${cx - 28},${cy + 40}`} />
            </g>
            {[0, 1, 2].map((i) => {
              const phase = (t - i * 0.18 + 1) % 1;
              const r = phase < 0.4 ? 4 + (phase / 0.4) * 4 : 8 - (phase - 0.4) * 4;
              return (
                <circle key={i} cx={cx - 24 + i * 24} cy={cy}
                        r={clamp(r, 3, 8)} fill="var(--bg)" />
              );
            })}
          </g>
        );
      } },

    { id: 'message-envelope', label: 'New mail', duration: 2200,
      render: (t) => {
        const open = pulse(t, 0.55, 0.25);
        const cx = SCX, cy = CY + 8;
        return (
          <g>
            <g stroke="var(--eye)" strokeWidth={4} fill="var(--bg)">
              <rect x={cx - 60} y={cy - 30} width={120} height={70} rx={6} fill="var(--eye)" />
            </g>
            {/* Letter rising out */}
            {open > 0.1 && (
              <g transform={`translate(0 ${-open * 28})`}>
                <rect x={cx - 50} y={cy - 24} width={100} height={50}
                      fill="var(--bg)" stroke="var(--eye)" strokeWidth={2} />
                <line x1={cx - 36} y1={cy - 10} x2={cx + 36} y2={cy - 10}
                      stroke="var(--eye)" strokeWidth={2} />
                <line x1={cx - 36} y1={cy} x2={cx + 36} y2={cy}
                      stroke="var(--eye)" strokeWidth={2} />
                <line x1={cx - 36} y1={cy + 10} x2={cx + 12} y2={cy + 10}
                      stroke="var(--eye)" strokeWidth={2} />
              </g>
            )}
            {/* Envelope flap */}
            <polyline points={`${cx - 60},${cy - 30} ${cx},${cy + 4 - open * 30} ${cx + 60},${cy - 30}`}
                      fill="none" stroke="var(--bg)" strokeWidth={4} />
            {/* Notification badge */}
            <g transform={`translate(${cx + 50} ${cy - 34})`}>
              <circle r={11} fill="var(--bg)" stroke="var(--eye)" strokeWidth={2} />
              <text textAnchor="middle" y={4} fontFamily="ui-monospace, Menlo, monospace"
                    fontSize={12} fontWeight={700} fill="var(--eye)">1</text>
            </g>
          </g>
        );
      } },

    { id: 'message-bubbles', label: 'Chat bubbles', duration: 3000,
      render: (t) => {
        const items = [
          { x: SCX - 60, y: SY + 22, w: 80, side: 'L', at: 0.0 },
          { x: SCX + 50, y: SY + 64, w: 100, side: 'R', at: 0.33 },
          { x: SCX - 50, y: SY + 110, w: 70, side: 'L', at: 0.66 },
        ];
        return (
          <g>
            {items.map((m, i) => {
              const phase = clamp((t - m.at) / 0.25, 0, 1);
              const op = phase;
              const scale = lerp(0.7, 1, phase);
              return (
                <g key={i} opacity={op}
                   transform={`translate(${m.x} ${m.y}) scale(${scale})`}>
                  <rect x={-m.w / 2} y={-12} width={m.w} height={24} rx={12}
                        fill={m.side === 'L' ? 'none' : 'var(--eye)'}
                        stroke="var(--eye)" strokeWidth={2} />
                </g>
              );
            })}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: CAMERA — 2 variants
// =============================================================
cats.camera = {
  label: 'Camera',
  desc: 'Photo capture.',
  kind: 'scene',
  variants: [

    { id: 'camera-shutter', label: 'Shutter', duration: 2000,
      render: (t) => {
        const flash = t < 0.06 ? 1 : 0;
        const cx = SCX, cy = CY + 4;
        return (
          <g>
            {/* Viewfinder brackets at corners */}
            <g stroke="var(--eye)" strokeWidth={3} fill="none" strokeLinecap="round">
              <path d={`M ${cx - 70} ${cy - 40} L ${cx - 70} ${cy - 26} M ${cx - 70} ${cy - 40} L ${cx - 56} ${cy - 40}`} />
              <path d={`M ${cx + 70} ${cy - 40} L ${cx + 70} ${cy - 26} M ${cx + 70} ${cy - 40} L ${cx + 56} ${cy - 40}`} />
              <path d={`M ${cx - 70} ${cy + 40} L ${cx - 70} ${cy + 26} M ${cx - 70} ${cy + 40} L ${cx - 56} ${cy + 40}`} />
              <path d={`M ${cx + 70} ${cy + 40} L ${cx + 70} ${cy + 26} M ${cx + 70} ${cy + 40} L ${cx + 56} ${cy + 40}`} />
            </g>
            {/* Camera body inside */}
            <g fill="var(--eye)">
              <rect x={cx - 24} y={cy - 12} width={48} height={32} rx={4} />
              <circle cx={cx} cy={cy + 4} r={11} fill="var(--bg)" />
              <circle cx={cx} cy={cy + 4} r={6} fill="var(--eye)" />
              <rect x={cx + 12} y={cy - 18} width={10} height={6} rx={1} />
            </g>
            {/* Flash overlay */}
            {flash > 0 && (
              <rect x={0} y={0} width={W} height={H} fill="var(--eye)" opacity={0.85} />
            )}
            {bigText({ x: SCX, y: H - 28, fontSize: 12, opacity: 0.7, letterSpacing: 3 },
              t < 0.3 ? 'SNAP!' : 'SMILE')}
          </g>
        );
      } },

    { id: 'camera-focus', label: 'Auto focus', duration: 1800,
      render: (t) => {
        const target = ease.out(t < 0.7 ? t / 0.7 : 1);
        const sz = lerp(90, 36, target);
        const cx = SCX, cy = CY + 6;
        const ok = t > 0.7;
        return (
          <g>
            <g stroke="var(--eye)" strokeWidth={3} fill="none">
              <path d={`M ${cx - sz/2} ${cy - sz/2 + 12} L ${cx - sz/2} ${cy - sz/2} L ${cx - sz/2 + 12} ${cy - sz/2}`} />
              <path d={`M ${cx + sz/2 - 12} ${cy - sz/2} L ${cx + sz/2} ${cy - sz/2} L ${cx + sz/2} ${cy - sz/2 + 12}`} />
              <path d={`M ${cx - sz/2} ${cy + sz/2 - 12} L ${cx - sz/2} ${cy + sz/2} L ${cx - sz/2 + 12} ${cy + sz/2}`} />
              <path d={`M ${cx + sz/2 - 12} ${cy + sz/2} L ${cx + sz/2} ${cy + sz/2} L ${cx + sz/2} ${cy + sz/2 - 12}`} />
            </g>
            <circle cx={cx} cy={cy} r={3} fill="var(--eye)" />
            {ok && (
              <g opacity={(t - 0.7) / 0.3}>
                <circle cx={cx + 36} cy={cy - 22} r={12} fill="var(--eye)" />
                <path d={`M ${cx + 30} ${cy - 22} L ${cx + 35} ${cy - 17} L ${cx + 42} ${cy - 27}`}
                      fill="none" stroke="var(--bg)" strokeWidth={3} strokeLinecap="round" />
              </g>
            )}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: NAVIGATION — 3 variants
// =============================================================
cats.navigation = {
  label: 'Navigation',
  desc: 'Compass / GPS / map.',
  kind: 'scene',
  variants: [

    { id: 'navigation-compass', label: 'Compass', duration: 4800,
      render: (t) => {
        const cx = SCX, cy = CY + 4;
        const r = 64;
        // Needle drifts slightly, settles, drifts again.
        const ang = Math.sin(t * TAU * 0.6) * 28 + Math.sin(t * TAU * 3) * 3;
        return (
          <g>
            <circle cx={cx} cy={cy} r={r} fill="none"
                    stroke="var(--eye)" strokeWidth={3} />
            {/* Tick marks */}
            {Array.from({ length: 24 }).map((_, i) => {
              const a = (i / 24) * TAU;
              const long = i % 6 === 0;
              return (
                <line key={i}
                      x1={cx + Math.cos(a) * (r - (long ? 14 : 6))}
                      y1={cy + Math.sin(a) * (r - (long ? 14 : 6))}
                      x2={cx + Math.cos(a) * (r - 1)}
                      y2={cy + Math.sin(a) * (r - 1)}
                      stroke="var(--eye)" strokeWidth={long ? 2.5 : 1} />
              );
            })}
            {/* N letter */}
            {bigText({ x: cx, y: cy - r + 24, fontSize: 14, opacity: 0.85 }, 'N')}
            {bigText({ x: cx, y: cy + r - 12, fontSize: 12, opacity: 0.55 }, 'S')}
            {bigText({ x: cx - r + 10, y: cy + 4, fontSize: 12, opacity: 0.55 }, 'W')}
            {bigText({ x: cx + r - 10, y: cy + 4, fontSize: 12, opacity: 0.55 }, 'E')}
            {/* Needle */}
            <g transform={`rotate(${ang} ${cx} ${cy})`}>
              <polygon points={`${cx},${cy - r + 6} ${cx - 6},${cy + 8} ${cx + 6},${cy + 8}`}
                       fill="var(--eye)" />
              <polygon points={`${cx},${cy + r - 6} ${cx - 6},${cy - 8} ${cx + 6},${cy - 8}`}
                       fill="var(--eye)" opacity={0.35} />
            </g>
            <circle cx={cx} cy={cy} r={3} fill="var(--eye)" />
          </g>
        );
      } },

    { id: 'navigation-pin', label: 'GPS pin', duration: 1800,
      render: (t) => {
        const cx = SCX, cy = CY;
        // Grid lines
        return (
          <g>
            <g stroke="var(--eye)" strokeWidth={1} opacity={0.18}>
              {Array.from({ length: 8 }).map((_, i) => (
                <line key={`h${i}`} x1={0} y1={SY + i * 24} x2={W} y2={SY + i * 24} />
              ))}
              {Array.from({ length: 11 }).map((_, i) => (
                <line key={`v${i}`} x1={i * 30} y1={STATUS_H} x2={i * 30} y2={H} />
              ))}
            </g>
            {/* Pulsing ring */}
            {[0, 1].map((i) => {
              const p = ((t * 1.5) + i * 0.5) % 1;
              const rr = lerp(8, 60, p);
              return (
                <circle key={i} cx={cx} cy={cy} r={rr} fill="none"
                        stroke="var(--eye)" strokeWidth={2} opacity={(1 - p) * 0.6} />
              );
            })}
            {/* Pin */}
            <g transform={`translate(${cx} ${cy})`} fill="var(--eye)">
              <path d="M 0 -32 Q -16 -32, -16 -16 Q -16 -4, 0 12 Q 16 -4, 16 -16 Q 16 -32, 0 -32 Z" />
              <circle cx={0} cy={-16} r={6} fill="var(--bg)" />
            </g>
          </g>
        );
      } },

    { id: 'navigation-arrow', label: 'Route arrow', duration: 3000,
      render: (t) => {
        // Curved route from BL to TR with a moving arrow head.
        const x0 = 24, y0 = H - 30, x1 = W - 30, y1 = SY + 16;
        const p = t;
        const bx = (1 - p) * x0 + p * x1;
        const by = (1 - p) * y0 + p * y1 - Math.sin(p * Math.PI) * 30;
        // Direction angle approximation
        const dp = 0.02;
        const ang = Math.atan2(
          ((1 - (p + dp)) * y0 + (p + dp) * y1 - Math.sin((p + dp) * Math.PI) * 30) - by,
          ((1 - (p + dp)) * x0 + (p + dp) * x1) - bx,
        ) * 180 / Math.PI;
        return (
          <g>
            {/* Path */}
            <path d={`M ${x0} ${y0} Q ${(x0 + x1) / 2} ${(y0 + y1) / 2 - 60}, ${x1} ${y1}`}
                  fill="none" stroke="var(--eye)" strokeWidth={3}
                  strokeLinecap="round" strokeDasharray="6 6" opacity={0.45} />
            {/* Start pin */}
            <circle cx={x0} cy={y0} r={6} fill="var(--eye)" />
            {/* End pin */}
            <g transform={`translate(${x1} ${y1})`} fill="var(--eye)">
              <path d="M 0 -16 Q -10 -16, -10 -8 Q -10 -2, 0 6 Q 10 -2, 10 -8 Q 10 -16, 0 -16 Z" />
            </g>
            {/* Moving arrow */}
            <g transform={`translate(${bx} ${by}) rotate(${ang})`} fill="var(--eye)">
              <polygon points="-10,-8 14,0 -10,8 -4,0" />
            </g>
            {bigText({ x: SCX, y: H - 8, fontSize: 11, opacity: 0.65, letterSpacing: 3 },
              '320 M  ·  3 MIN')}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: GIFT — 2 variants
// =============================================================
cats.gift = {
  label: 'Gift',
  desc: 'Surprise / unboxing.',
  kind: 'scene',
  variants: [

    { id: 'gift-wrapped', label: 'Wrapped', duration: 1600,
      render: (t) => {
        const wob = Math.sin(t * TAU * 3) * 2;
        const cx = SCX + wob, cy = CY + 8;
        return (
          <g>
            <g fill="var(--eye)" transform={`translate(${wob} 0)`}>
              <rect x={cx - 50} y={cy - 30} width={100} height={70} rx={4} />
              <rect x={cx - 50} y={cy - 30} width={100} height={14} fill="var(--bg)" />
              <rect x={cx - 8} y={cy - 30} width={16} height={70} fill="var(--bg)" />
              <rect x={cx - 50} y={cy - 8} width={100} height={4} fill="var(--bg)" />
              <rect x={cx - 6} y={cy - 30} width={12} height={70} />
              <rect x={cx - 50} y={cy - 6} width={100} height={6} />
              {/* Ribbon bow */}
              <path d={`M ${cx} ${cy - 30}
                       Q ${cx - 20} ${cy - 46}, ${cx - 10} ${cy - 30}
                       Q ${cx - 20} ${cy - 36}, ${cx} ${cy - 30}
                       Q ${cx + 20} ${cy - 46}, ${cx + 10} ${cy - 30}
                       Q ${cx + 20} ${cy - 36}, ${cx} ${cy - 30} Z`} />
            </g>
            {bigText({ x: SCX, y: SY + 22, fontSize: 12, opacity: 0.65, letterSpacing: 3 }, 'FOR YOU')}
          </g>
        );
      } },

    { id: 'gift-open', label: 'Unboxing', duration: 2400,
      render: (t) => {
        const open = clamp(t / 0.4, 0, 1);
        const cx = SCX, cy = CY + 16;
        return (
          <g>
            {/* Box body */}
            <g fill="var(--eye)">
              <rect x={cx - 50} y={cy - 14} width={100} height={50} rx={4} />
            </g>
            {/* Lid lifting */}
            <g transform={`translate(0 ${-open * 30}) rotate(${-open * 14} ${cx - 50} ${cy - 14})`}
               fill="var(--eye)">
              <rect x={cx - 56} y={cy - 26} width={112} height={16} rx={3} />
            </g>
            {/* Sparkles + heart bursting out */}
            {open > 0.4 && (
              <g opacity={(t - 0.4) / 0.6}>
                <path d={heartPath(cx, cy - 30 - (t - 0.4) * 40, 18 + (t - 0.4) * 8)}
                      fill="var(--eye)" />
                {[0, 1, 2, 3].map((i) => {
                  const a = (i / 4) * TAU;
                  const r = 30 + (t - 0.4) * 30;
                  return <path key={i}
                    d={starPath(cx + Math.cos(a) * r, cy - 20 + Math.sin(a) * r * 0.6, 6, 2)}
                    fill="var(--eye)" opacity={1 - (t - 0.4)} />;
                })}
              </g>
            )}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: COFFEE — 2 variants
// =============================================================
cats.coffee = {
  label: 'Coffee',
  desc: 'Cafe / drink.',
  kind: 'scene',
  variants: [

    { id: 'coffee-pour', label: 'Pour', duration: 2400,
      render: (t) => {
        const cx = SCX, cy = CY + 22;
        const streamAlive = t > 0.2 && t < 0.85;
        const fill = clamp((t - 0.2) / 0.6, 0, 1);
        return (
          <g>
            {/* Pot */}
            <g stroke="var(--eye)" strokeWidth={4} fill="none">
              <path d={`M ${cx - 60} ${cy - 60}
                       L ${cx - 36} ${cy - 60}
                       L ${cx - 36} ${cy - 36}
                       L ${cx - 60} ${cy - 36} Z`} />
              <line x1={cx - 60} y1={cy - 56} x2={cx - 76} y2={cy - 52} strokeWidth={5} />
              {/* Spout */}
              <line x1={cx - 36} y1={cy - 50} x2={cx - 20} y2={cy - 46} strokeWidth={4} />
            </g>
            {/* Stream */}
            {streamAlive && (
              <line x1={cx - 18} y1={cy - 42} x2={cx - 4} y2={cy - 4}
                    stroke="var(--eye)" strokeWidth={3} strokeLinecap="round" />
            )}
            {/* Cup */}
            <g stroke="var(--eye)" strokeWidth={4} fill="none">
              <path d={`M ${cx - 32} ${cy - 4}
                       Q ${cx - 32} ${cy + 30}, ${cx} ${cy + 30}
                       Q ${cx + 32} ${cy + 30}, ${cx + 32} ${cy - 4}
                       Z`} />
              <path d={`M ${cx + 32} ${cy + 6} Q ${cx + 46} ${cy + 10}, ${cx + 38} ${cy + 22}`} />
            </g>
            {/* Coffee filling */}
            <path d={`M ${cx - 30} ${cy - 2 + (1 - fill) * 28}
                     L ${cx + 30} ${cy - 2 + (1 - fill) * 28}
                     L ${cx + 30} ${cy + 28}
                     Q ${cx} ${cy + 30}, ${cx - 30} ${cy + 28} Z`}
                  fill="var(--eye)" />
            {/* Steam (after fill) */}
            {t > 0.7 && (
              <g opacity={(t - 0.7) * 3.3}>
                {[0, 1].map((i) => {
                  const p = ((t * 0.8) + i * 0.5) % 1;
                  return (
                    <path key={i}
                          d={`M ${cx - 8 + i * 16} ${cy - 14 - p * 30}
                              Q ${cx - 2 + i * 16} ${cy - 22 - p * 30}, ${cx - 8 + i * 16} ${cy - 30 - p * 30}`}
                          fill="none" stroke="var(--eye)" strokeWidth={2.5}
                          strokeLinecap="round" opacity={(1 - p) * 0.85} />
                  );
                })}
              </g>
            )}
          </g>
        );
      } },

    { id: 'coffee-cup', label: 'Cup steam', duration: 2800,
      render: (t) => {
        const cx = SCX, cy = CY + 18;
        return (
          <g>
            {/* Steam plumes */}
            {[0, 1, 2].map((i) => {
              const p = ((t * 0.7) + i * 0.33) % 1;
              const x = cx - 14 + i * 14;
              const y = cy - 30 - p * 60;
              return (
                <path key={i}
                      d={`M ${x} ${y}
                          Q ${x + 8} ${y - 10}, ${x} ${y - 20}
                          Q ${x - 8} ${y - 30}, ${x} ${y - 40}`}
                      fill="none" stroke="var(--eye)" strokeWidth={3}
                      strokeLinecap="round" opacity={(1 - p) * 0.8} />
              );
            })}
            {/* Cup */}
            <g stroke="var(--eye)" strokeWidth={5} fill="var(--eye)">
              <path d={`M ${cx - 40} ${cy - 16}
                       L ${cx - 40} ${cy + 24}
                       Q ${cx - 40} ${cy + 40}, ${cx} ${cy + 40}
                       Q ${cx + 40} ${cy + 40}, ${cx + 40} ${cy + 24}
                       L ${cx + 40} ${cy - 16} Z`} />
              <ellipse cx={cx} cy={cy - 16} rx={40} ry={6} fill="var(--bg)" />
              <ellipse cx={cx} cy={cy - 16} rx={32} ry={4} fill="var(--eye)" />
            </g>
            {/* Handle */}
            <path d={`M ${cx + 40} ${cy - 6} Q ${cx + 58} ${cy}, ${cx + 50} ${cy + 18}`}
                  fill="none" stroke="var(--eye)" strokeWidth={5} />
            {/* Saucer */}
            <ellipse cx={cx} cy={cy + 46} rx={56} ry={8} fill="var(--eye)" />
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: PLANT — 2 variants
// =============================================================
cats.plant = {
  label: 'Plant',
  desc: 'Grow / care.',
  kind: 'scene',
  variants: [

    { id: 'plant-grow', label: 'Growing', duration: 4000,
      render: (t) => {
        const groundY = H - 28;
        const grow = ease.out(clamp(t, 0, 1));
        const stemTop = groundY - 40 - grow * 60;
        const flower = t > 0.7;
        return (
          <g>
            {/* Pot */}
            <g fill="var(--eye)">
              <path d={`M ${SCX - 28} ${groundY}
                       L ${SCX + 28} ${groundY}
                       L ${SCX + 22} ${groundY + 24}
                       L ${SCX - 22} ${groundY + 24} Z`} />
              <rect x={SCX - 32} y={groundY - 6} width={64} height={8} rx={2} />
            </g>
            {/* Stem */}
            <line x1={SCX} y1={groundY} x2={SCX} y2={stemTop}
                  stroke="var(--eye)" strokeWidth={4} strokeLinecap="round" />
            {/* Leaves */}
            {grow > 0.3 && (
              <g fill="var(--eye)" opacity={(grow - 0.3) / 0.7}>
                <ellipse cx={SCX - 18} cy={stemTop + 26} rx={14} ry={7}
                         transform={`rotate(-30 ${SCX - 18} ${stemTop + 26})`} />
                <ellipse cx={SCX + 18} cy={stemTop + 14} rx={14} ry={7}
                         transform={`rotate(30 ${SCX + 18} ${stemTop + 14})`} />
              </g>
            )}
            {/* Flower head */}
            {flower && (
              <g opacity={(t - 0.7) / 0.3}>
                {Array.from({ length: 6 }).map((_, i) => {
                  const a = (i / 6) * TAU;
                  return (
                    <ellipse key={i}
                             cx={SCX + Math.cos(a) * 14} cy={stemTop + Math.sin(a) * 14}
                             rx={10} ry={6}
                             transform={`rotate(${(i / 6) * 360} ${SCX + Math.cos(a) * 14} ${stemTop + Math.sin(a) * 14})`}
                             fill="var(--eye)" />
                  );
                })}
                <circle cx={SCX} cy={stemTop} r={8} fill="var(--bg)" />
              </g>
            )}
          </g>
        );
      } },

    { id: 'plant-water', label: 'Watering', duration: 2200,
      render: (t) => {
        const tilt = clamp((t - 0.1) / 0.4, 0, 1) * 26;
        const drops = t > 0.4;
        const cx = SCX - 30, cy = CY - 18;
        return (
          <g>
            {/* Watering can */}
            <g transform={`rotate(${tilt} ${cx} ${cy})`} fill="var(--eye)">
              <rect x={cx - 24} y={cy - 14} width={48} height={28} rx={4} />
              <rect x={cx - 36} y={cy - 4} width={12} height={6} />
              <rect x={cx - 44} y={cy - 14} width={8} height={20} rx={3} />
              <path d={`M ${cx + 24} ${cy - 14}
                       Q ${cx + 36} ${cy - 22}, ${cx + 36} ${cy + 2}
                       L ${cx + 30} ${cy + 4}
                       Q ${cx + 30} ${cy - 14}, ${cx + 24} ${cy - 14}`} />
            </g>
            {/* Drops */}
            {drops && [0, 1, 2, 3, 4].map((i) => {
              const p = ((t - 0.4) * 3 + i * 0.18) % 1;
              const x = cx + 38 + i * 4;
              const y = cy + 12 + p * 60;
              return (
                <ellipse key={i} cx={x} cy={y} rx={3} ry={5}
                         fill="var(--eye)" opacity={1 - p} />
              );
            })}
            {/* Plant on right */}
            <g transform={`translate(${SCX + 56} ${H - 30})`} fill="var(--eye)">
              <path d="M -16 0 L 16 0 L 12 18 L -12 18 Z" />
              <line x1={0} y1={0} x2={0} y2={-32}
                    stroke="var(--eye)" strokeWidth={3} strokeLinecap="round" />
              <ellipse cx={-12} cy={-20} rx={10} ry={6}
                       transform="rotate(-30 -12 -20)" />
              <ellipse cx={12} cy={-26} rx={10} ry={6}
                       transform="rotate(30 12 -26)" />
            </g>
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: MORNING — 2 variants
// =============================================================
cats.morning = {
  label: 'Morning',
  desc: 'Sunrise / wake up.',
  kind: 'scene',
  variants: [

    { id: 'morning-sunrise', label: 'Sunrise', duration: 5000,
      render: (t) => {
        const horizonY = H - 36;
        const rise = ease.out(clamp(t * 1.3, 0, 1));
        const sunY = lerp(horizonY + 30, horizonY - 40, rise);
        return (
          <g>
            {/* Horizon */}
            <line x1={0} y1={horizonY} x2={W} y2={horizonY}
                  stroke="var(--eye)" strokeWidth={2} />
            {/* Sun rays */}
            <g transform={`translate(${SCX} ${sunY})`} opacity={rise}>
              {Array.from({ length: 12 }).map((_, i) => {
                const a = (i / 12) * Math.PI;
                const x1 = Math.cos(a) * 40, y1 = -Math.sin(a) * 40;
                const x2 = Math.cos(a) * (54 + Math.sin(t * TAU * 2 + i) * 3);
                const y2 = -Math.sin(a) * (54 + Math.sin(t * TAU * 2 + i) * 3);
                return <line key={i} x1={x1} y1={y1} x2={x2} y2={y2}
                             stroke="var(--eye)" strokeWidth={3} strokeLinecap="round" />;
              })}
            </g>
            {/* Sun */}
            <circle cx={SCX} cy={sunY} r={28} fill="var(--eye)" />
            {/* Below-horizon mask */}
            <rect x={0} y={horizonY + 1} width={W} height={H - horizonY} fill="var(--bg)" />
            {/* Birds */}
            {t > 0.4 && [0, 1].map((i) => {
              const p = (t * 0.3 + i * 0.5) % 1;
              const x = lerp(-10, W + 10, p);
              const flap = Math.sin(t * TAU * 6) * 0.3 + 1;
              return (
                <g key={i} stroke="var(--eye)" strokeWidth={2} fill="none"
                   strokeLinecap="round" opacity={(1 - p) * 0.9}>
                  <path d={`M ${x} ${SY + 30 + i * 18}
                           Q ${x + 6} ${SY + 22 * flap + i * 18}, ${x + 12} ${SY + 30 + i * 18}
                           Q ${x + 18} ${SY + 22 * flap + i * 18}, ${x + 24} ${SY + 30 + i * 18}`} />
                </g>
              );
            })}
            {bigText({ x: SCX, y: SY + 22, fontSize: 14, opacity: 0.75, letterSpacing: 4 }, 'GOOD MORNING')}
          </g>
        );
      } },

    { id: 'morning-alarm-rays', label: 'Wake alarm', duration: 800,
      render: (t) => {
        const shake = Math.sin(t * TAU * 14) * 3;
        return (
          <g transform={`translate(${shake} 0)`}>
            <g fill="var(--eye)" transform={`translate(${SCX} ${CY + 6})`}>
              <path d="M 0 -36 Q -28 -36, -28 -8 L -32 4 L 32 4 L 28 -8 Q 28 -36, 0 -36 Z" />
              <rect x={-6} y={4} width={12} height={6} rx={2} />
              <rect x={-14} y={-44} width={4} height={10} rx={2} transform="rotate(-30 -12 -39)" />
              <rect x={10} y={-44} width={4} height={10} rx={2} transform="rotate(30 12 -39)" />
            </g>
            {/* Rays */}
            {[0, 1, 2, 3, 4, 5].map((i) => {
              const a = -Math.PI / 2 + (i - 2.5) * 0.25;
              const r0 = 50, r1 = 60 + Math.sin(t * TAU * 4 + i) * 4;
              return (
                <line key={i}
                      x1={SCX + Math.cos(a) * r0} y1={CY + 6 + Math.sin(a) * r0}
                      x2={SCX + Math.cos(a) * r1} y2={CY + 6 + Math.sin(a) * r1}
                      stroke="var(--eye)" strokeWidth={3} strokeLinecap="round"
                      opacity={Math.abs(Math.sin(t * TAU * 4 + i)) * 0.6 + 0.4} />
              );
            })}
            {bigText({ x: SCX, y: H - 20, fontSize: 11, opacity: 0.7, letterSpacing: 3 }, 'WAKE UP')}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: GAMING — 2 variants
// =============================================================
cats.gaming = {
  label: 'Gaming',
  desc: 'Controller / power-up.',
  kind: 'scene',
  variants: [

    { id: 'gaming-pad', label: 'Game pad', duration: 1600,
      render: (t) => {
        const cx = SCX, cy = CY + 8;
        const dpadDir = Math.floor((t * 4) % 4);
        const aPressed = (t * 4 % 1) > 0.5;
        return (
          <g>
            {/* Body */}
            <g fill="var(--eye)">
              <rect x={cx - 88} y={cy - 22} width={176} height={48} rx={24} />
              <rect x={cx - 88} y={cy - 22} width={176} height={48} rx={24}
                    fill="none" stroke="var(--bg)" strokeWidth={3} />
            </g>
            {/* D-pad */}
            <g fill="var(--bg)" transform={`translate(${cx - 58} ${cy})`}>
              <rect x={-18} y={-6} width={36} height={12} rx={2} />
              <rect x={-6} y={-18} width={12} height={36} rx={2} />
              {/* Active direction highlight */}
              {dpadDir === 0 && <rect x={-6} y={-18} width={12} height={12} fill="var(--eye)" />}
              {dpadDir === 1 && <rect x={6} y={-6} width={12} height={12} fill="var(--eye)" />}
              {dpadDir === 2 && <rect x={-6} y={6} width={12} height={12} fill="var(--eye)" />}
              {dpadDir === 3 && <rect x={-18} y={-6} width={12} height={12} fill="var(--eye)" />}
            </g>
            {/* A/B buttons */}
            <g transform={`translate(${cx + 56} ${cy})`}>
              <circle cx={-12} cy={6} r={10} fill="var(--bg)" />
              <circle cx={14} cy={-6} r={10}
                      fill={aPressed ? 'var(--eye)' : 'var(--bg)'} />
              <text x={-12} y={10} textAnchor="middle" fontFamily="ui-monospace, Menlo, monospace"
                    fontSize={12} fontWeight={700} fill="var(--eye)">B</text>
              <text x={14} y={-2} textAnchor="middle" fontFamily="ui-monospace, Menlo, monospace"
                    fontSize={12} fontWeight={700}
                    fill={aPressed ? 'var(--bg)' : 'var(--eye)'}>A</text>
            </g>
            {bigText({ x: SCX, y: SY + 22, fontSize: 12, opacity: 0.7, letterSpacing: 3 }, 'PLAY')}
          </g>
        );
      } },

    { id: 'gaming-powerup', label: 'Power up', duration: 1800,
      render: (t) => {
        const scale = lerp(0.5, 1.2, ease.out(clamp(t * 1.5, 0, 1)));
        const cx = SCX, cy = CY + 6;
        return (
          <g transform={`translate(${cx} ${cy}) scale(${scale})`}>
            {/* Star core */}
            <path d={starPath(0, 0, 44, 18)} fill="var(--eye)" />
            <path d={starPath(0, 0, 26, 10)} fill="var(--bg)" />
            <path d={starPath(0, 0, 12, 4)} fill="var(--eye)" />
            {/* Orbiting sparkles */}
            {[0, 1, 2, 3].map((i) => {
              const a = (i / 4) * TAU + t * TAU * 2;
              const r = 60;
              return (
                <path key={i}
                      d={starPath(Math.cos(a) * r, Math.sin(a) * r, 6, 2.5)}
                      fill="var(--eye)" opacity={0.85} />
              );
            })}
          </g>
        );
      } },
  ],
};

// =============================================================
// SCENE: CALENDAR — 2 variants
// =============================================================
cats.calendar = {
  label: 'Calendar',
  desc: 'Date / reminder.',
  kind: 'scene',
  variants: [

    { id: 'calendar-date', label: 'Date page', duration: 5000,
      render: (t) => {
        const d = new Date();
        const day = String(d.getDate()).padStart(2, '0');
        const wd = ['SUN','MON','TUE','WED','THU','FRI','SAT'][d.getDay()];
        const month = ['JAN','FEB','MAR','APR','MAY','JUN','JUL','AUG','SEP','OCT','NOV','DEC'][d.getMonth()];
        const wob = Math.sin(t * TAU) * 1.2;
        return (
          <g transform={`rotate(${wob} ${SCX} ${CY + 4})`}>
            {/* Card */}
            <rect x={SCX - 80} y={SY + 12} width={160} height={H - SY - 24} rx={8}
                  fill="none" stroke="var(--eye)" strokeWidth={3} />
            {/* Header bar (month) */}
            <rect x={SCX - 80} y={SY + 12} width={160} height={28}
                  fill="var(--eye)" />
            <text x={SCX} y={SY + 31} textAnchor="middle"
                  fontFamily="ui-monospace, Menlo, monospace"
                  fontSize={14} fontWeight={700} fill="var(--bg)" letterSpacing={2}>
              {month}
            </text>
            {/* Big day number */}
            {bigText({ x: SCX, y: SY + 92, fontSize: 64 }, day)}
            {bigText({ x: SCX, y: H - 36, fontSize: 14, opacity: 0.8, letterSpacing: 3 }, wd)}
            {/* Rings */}
            <circle cx={SCX - 36} cy={SY + 4} r={3} fill="var(--bg)"
                    stroke="var(--eye)" strokeWidth={2} />
            <circle cx={SCX + 36} cy={SY + 4} r={3} fill="var(--bg)"
                    stroke="var(--eye)" strokeWidth={2} />
          </g>
        );
      } },

    { id: 'calendar-reminder', label: 'Reminder', duration: 1400,
      render: (t) => {
        const shake = Math.sin(t * TAU * 10) * 4;
        return (
          <g>
            {/* Bell */}
            <g transform={`translate(${SCX - 50} ${CY + 6}) rotate(${shake})`} fill="var(--eye)">
              <path d="M 0 -32
                       Q -22 -32, -22 -12
                       L -28 6
                       L 28 6
                       L 22 -12
                       Q 22 -32, 0 -32 Z" />
              <circle cx={0} cy={14} r={5} />
              <rect x={-2} y={-40} width={4} height={8} rx={2} />
            </g>
            {/* Notification badge */}
            <g transform={`translate(${SCX - 26} ${CY - 18})`}>
              <circle r={11} fill="var(--bg)" stroke="var(--eye)" strokeWidth={2.5} />
              <text textAnchor="middle" y={4} fontFamily="ui-monospace, Menlo, monospace"
                    fontSize={12} fontWeight={700} fill="var(--eye)">3</text>
            </g>
            {/* Right side text */}
            {bigText({ x: SCX + 32, y: CY - 10, fontSize: 16, opacity: 0.9, textAnchor: 'start' }, 'MEETING')}
            {bigText({ x: SCX + 32, y: CY + 10, fontSize: 12, opacity: 0.65, letterSpacing: 2, textAnchor: 'start' }, '14:00')}
            {bigText({ x: SCX + 32, y: CY + 28, fontSize: 11, opacity: 0.55, letterSpacing: 2, textAnchor: 'start' }, 'IN 10 MIN')}
          </g>
        );
      } },
  ],
};

// ---------- append to key order ----------
const newSceneKeys = [
  'fitness', 'call', 'message', 'camera', 'navigation',
  'gift', 'coffee', 'plant', 'morning', 'gaming', 'calendar',
];
const existing = window.EMOTION_CATEGORY_KEYS || Object.keys(cats);
window.EMOTION_CATEGORY_KEYS = [
  ...existing.filter((k) => !newSceneKeys.includes(k)),
  ...newSceneKeys.filter((k) => k in cats),
];
