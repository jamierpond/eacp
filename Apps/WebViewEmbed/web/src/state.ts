import { backend } from './generated/backend';
import { makeNativeState } from './generated/react';
import type { Parameters } from './generated/schema';

export const useParameters = makeNativeState<Parameters>({
    backend,
    event: 'parameters',
    setCommand: backend.setParameters,
    initial: { level: 0.5, autoCycle: false, counter: 0 },
});
