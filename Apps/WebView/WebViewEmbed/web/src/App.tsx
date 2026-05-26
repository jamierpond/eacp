import { setParameters, useParameters } from './state';

export default function App()
{
    const params = useParameters();

    return (
        <main>
            <h1>Bidirectional parameters</h1>
            <p>
                Level can be edited from the slider <em>or</em> by the native
                auto-cycle loop. State lives in a C++ <code>StateValue&lt;Parameters&gt;</code>;
                the React hook keeps both sides in sync.
            </p>

            <div className="row">
                <label htmlFor="level">Level</label>
                <input
                    id="level"
                    type="range"
                    min={0}
                    max={1000}
                    value={Math.round(params.level * 1000)}
                    onChange={(e) =>
                        void setParameters({ ...params, level: Number(e.target.value) / 1000 })}
                />
                <span className="readout">{params.level.toFixed(2)}</span>
            </div>

            <div className="row">
                <label>
                    <input
                        type="checkbox"
                        checked={params.autoCycle}
                        onChange={(e) =>
                            void setParameters({ ...params, autoCycle: e.target.checked })}
                    />
                    Auto-cycle (native drives the level)
                </label>
            </div>

            <div className="status">
                Tick counter: <span className="readout">{params.counter}</span>
            </div>
        </main>
    );
}
