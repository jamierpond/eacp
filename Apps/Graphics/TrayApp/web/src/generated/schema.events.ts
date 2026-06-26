import type * as T from './schema';

export interface Events
{
    'playback': T.PlaybackState;
}

export type EventName = keyof Events;

export interface EventBus
{
    subscribe<K extends EventName>(
        name: K,
        handler: (payload: Events[K]) => void,
    ): () => void;
}
