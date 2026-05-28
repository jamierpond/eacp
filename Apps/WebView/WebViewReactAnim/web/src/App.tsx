import { useState } from 'react';
import { useNativeStats, useNativeTick } from './bridge';
import ShaderCanvas from './ShaderCanvas';

const petals = [0, 60, 120, 180, 240, 300];

interface Layer
{
    speed: number;
    size: number;
    hue: number;
}

const layers: Layer[] = [
    { speed: 1.0, size: 320, hue: 0 },
    { speed: -1.7, size: 220, hue: 120 },
    { speed: 2.6, size: 130, hue: 240 },
];

type ViewId = 'pinwheel' | 'shader';

interface ViewOption
{
    id: ViewId;
    label: string;
}

const visualOptions: ViewOption[] = [
    { id: 'pinwheel', label: 'SVG Pinwheel' },
    { id: 'shader', label: 'WebGL Mandelbulb' },
];

export default function App()
{
    const { angle, hz } = useNativeTick();
    const { ticks } = useNativeStats();
    const [view, setView] = useState<ViewId>('pinwheel');

    return (
        <div className="stage">
            <h1>Native-driven React animation</h1>
            <p>
                The angle is pushed from native ticks ({hz} Hz measured,{' '}
                {ticks} recorded via the stats sub-API).
            </p>

            <Toggle value={view} onChange={setView} options={visualOptions} />

            <div className="viewport">
                {view === 'pinwheel'
                    ? <Pinwheel angle={angle} />
                    : <ShaderCanvas angle={angle} />}
            </div>
        </div>
    );
}

interface ToggleProps
{
    value: ViewId;
    onChange: (next: ViewId) => void;
    options: ViewOption[];
}

function Toggle({ value, onChange, options }: ToggleProps)
{
    return (
        <div className="toggle">
            {options.map(opt => (
                <button
                    key={opt.id}
                    className={opt.id === value ? 'active' : ''}
                    onClick={() => onChange(opt.id)}>
                    {opt.label}
                </button>
            ))}
        </div>
    );
}

function Pinwheel({ angle }: { angle: number })
{
    const pulse = 1 + 0.18 * Math.sin(angle * Math.PI / 60);

    return (
        <div className="stack" style={{ transform: `scale(${pulse})` }}>
            {layers.map((layer, i) => (
                <Wheel key={i} angle={angle * layer.speed} {...layer} />
            ))}
        </div>
    );
}

interface WheelProps extends Layer
{
    angle: number;
}

function Wheel({ angle, size, hue }: WheelProps)
{
    const style = {
        width: size,
        height: size,
        transform: `rotate(${angle}deg)`,
        filter: `hue-rotate(${hue}deg) `
              + `drop-shadow(0 0 14px hsla(${hue}, 80%, 60%, 0.65))`,
    };

    return (
        <svg viewBox="-100 -100 200 200" style={style}>
            {petals.map(rotation => <Petal key={rotation} rotation={rotation} />)}
        </svg>
    );
}

function Petal({ rotation }: { rotation: number })
{
    return (
        <path
            transform={`rotate(${rotation})`}
            d="M 0 0 L 50 -25 L 90 0 L 50 25 Z"
            fill={`hsl(${rotation}, 70%, 60%)`}
        />
    );
}
