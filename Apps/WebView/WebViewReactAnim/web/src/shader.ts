const VERTEX = `
attribute vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
`;

export interface ShaderRenderer
{
    draw(angle: number): void;
    dispose(): void;
}

export function createShaderRenderer(
    canvas: HTMLCanvasElement,
    fragmentSource: string): ShaderRenderer
{
    const gl = canvas.getContext('webgl');
    if (!gl) throw new Error('WebGL not supported');

    const program = link(gl, VERTEX, fragmentSource);
    gl.useProgram(program);

    const buffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    gl.bufferData(gl.ARRAY_BUFFER,
        new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);

    const aPos = gl.getAttribLocation(program, 'aPos');
    gl.enableVertexAttribArray(aPos);
    gl.vertexAttribPointer(aPos, 2, gl.FLOAT, false, 0, 0);

    const uResolution = gl.getUniformLocation(program, 'uResolution');
    const uAngle = gl.getUniformLocation(program, 'uAngle');

    return {
        draw(angle)
        {
            gl.viewport(0, 0, canvas.width, canvas.height);
            gl.uniform2f(uResolution, canvas.width, canvas.height);
            gl.uniform1f(uAngle, angle);
            gl.drawArrays(gl.TRIANGLES, 0, 3);
        },
        dispose()
        {
            gl.deleteProgram(program);
            gl.deleteBuffer(buffer);
        },
    };
}

function link(
    gl: WebGLRenderingContext,
    vertexSource: string,
    fragmentSource: string): WebGLProgram
{
    const program = gl.createProgram();
    if (!program) throw new Error('Failed to create WebGL program');
    gl.attachShader(program, compile(gl, gl.VERTEX_SHADER, vertexSource));
    gl.attachShader(program, compile(gl, gl.FRAGMENT_SHADER, fragmentSource));
    gl.linkProgram(program);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS))
        throw new Error(gl.getProgramInfoLog(program) ?? 'link failed');
    return program;
}

function compile(
    gl: WebGLRenderingContext,
    type: GLenum,
    source: string): WebGLShader
{
    const shader = gl.createShader(type);
    if (!shader) throw new Error('Failed to create WebGL shader');
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
        throw new Error(gl.getShaderInfoLog(shader) ?? 'compile failed');
    return shader;
}
