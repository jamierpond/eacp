import { useEffect, useRef, useState } from 'react';
import { backend } from './generated/backend';
import type { Stats, Tick } from './generated/schema';

export interface NativeTick extends Tick
{
    hz: number;
}

// Reads the `stats` sub-API: seeds from the nested command
// backend.stats.getStats(), then tracks the quoted "stats::updated" event.
// Demonstrates both nested-API codegen forms from one hook.
export function useNativeStats(): Stats
{
    const [stats, setStats] = useState<Stats>({ ticks: 0 });

    useEffect(() =>
    {
        backend.stats.getStats().then(setStats).catch(() => {});
        return backend.on?.('stats::updated', setStats);
    }, []);

    return stats;
}

const windowMs = 1000;

export function useNativeTick(): NativeTick
{
    const [tick, setTick] = useState<NativeTick>({ angle: 0, hz: 0 });
    const counter = useRef({ count: 0, since: performance.now() });

    useEffect(() => backend.on?.('tick', (next) =>
    {
        counter.current.count++;
        const elapsed = performance.now() - counter.current.since;

        setTick((prev) =>
        {
            if (elapsed < windowMs)
                return { angle: next.angle, hz: prev.hz };

            const hz = Math.round(counter.current.count * 1000 / elapsed);
            counter.current = { count: 0, since: performance.now() };
            return { angle: next.angle, hz };
        });
    }), []);

    return tick;
}
