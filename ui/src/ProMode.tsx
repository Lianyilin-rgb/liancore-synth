// =============================================================================
// LianCore - Pro Mode Web UI
// 专业模式: 节点图 + 参数面板 + 波形/FFT/LFO 可视化
// =============================================================================
import React, { useState, useRef, useEffect, useCallback } from 'react';

// =============================================================================
// 类型定义
// =============================================================================
interface NodeInfo {
  id: string;
  type: string;
  name: string;
  x: number;
  y: number;
  enabled: boolean;
  params: Record<string, number>;
}

interface Connection {
  id: string;
  srcNode: string;
  srcPort: number;
  dstNode: string;
  dstPort: number;
}

interface WaveformData {
  samples: Float32Array;
  sampleRate: number;
}

interface SpectrumData {
  magnitudes: Float32Array;
  bins: number;
}

// =============================================================================
// 节点图组件
// =============================================================================
const NodeGraph: React.FC<{
  nodes: NodeInfo[];
  connections: Connection[];
  onNodeMove: (id: string, x: number, y: number) => void;
  onNodeSelect: (id: string) => void;
  selectedNode: string | null;
}> = ({ nodes, connections, onNodeMove, onNodeSelect, selectedNode }) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [dragging, setDragging] = useState<string | null>(null);
  const [offset, setOffset] = useState({ x: 0, y: 0 });
  const [scale, setScale] = useState(1);
  const [pan, setPan] = useState({ x: 0, y: 0 });

  // 绘制画布
  const draw = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);

    // 网格背景
    ctx.strokeStyle = 'rgba(108, 92, 231, 0.06)';
    ctx.lineWidth = 1;
    const gridSize = 40 * scale;
    const startX = pan.x % gridSize;
    const startY = pan.y % gridSize;
    for (let x = startX; x < w; x += gridSize) {
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
    }
    for (let y = startY; y < h; y += gridSize) {
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
    }

    // 绘制连接线
    connections.forEach(conn => {
      const src = nodes.find(n => n.id === conn.srcNode);
      const dst = nodes.find(n => n.id === conn.dstNode);
      if (!src || !dst) return;

      const sx = (src.x * scale) + pan.x + 75;
      const sy = (src.y * scale) + pan.y + 30;
      const dx = (dst.x * scale) + pan.x + 75;
      const dy = (dst.y * scale) + pan.y + 30;

      const mx = (sx + dx) / 2;

      ctx.strokeStyle = 'rgba(0, 206, 201, 0.4)';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(sx + 75, sy);
      ctx.bezierCurveTo(mx, sy, mx, dy, dx, dy);
      ctx.stroke();

      // 连接点
      ctx.fillStyle = '#00cec9';
      ctx.beginPath();
      ctx.arc(sx + 75, sy, 4, 0, Math.PI * 2);
      ctx.fill();
      ctx.beginPath();
      ctx.arc(dx, dy, 4, 0, Math.PI * 2);
      ctx.fill();
    });

    // 绘制节点
    const nodeWidth = 150;
    const nodeHeight = 60;
    nodes.forEach(node => {
      const nx = (node.x * scale) + pan.x;
      const ny = (node.y * scale) + pan.y;

      // 节点阴影
      ctx.shadowColor = selectedNode === node.id ? 'rgba(108, 92, 231, 0.5)' : 'rgba(0,0,0,0.3)';
      ctx.shadowBlur = selectedNode === node.id ? 15 : 8;

      // 节点背景
      const gradient = ctx.createLinearGradient(nx, ny, nx, ny + nodeHeight);
      if (selectedNode === node.id) {
        gradient.addColorStop(0, '#1e1e3a');
        gradient.addColorStop(1, '#12122a');
      } else {
        gradient.addColorStop(0, '#1a1a28');
        gradient.addColorStop(1, '#12121e');
      }
      ctx.fillStyle = gradient;
      ctx.beginPath();
      roundRect(ctx, nx, ny, nodeWidth, nodeHeight, 8);
      ctx.fill();

      ctx.shadowColor = 'transparent';
      ctx.shadowBlur = 0;

      // 边框
      ctx.strokeStyle = selectedNode === node.id ? '#6c5ce7' : '#2a2a3a';
      ctx.lineWidth = selectedNode === node.id ? 2 : 1;
      ctx.beginPath();
      roundRect(ctx, nx, ny, nodeWidth, nodeHeight, 8);
      ctx.stroke();

      // 启用/禁用指示器
      ctx.fillStyle = node.enabled ? '#2ecc71' : '#e74c3c';
      ctx.beginPath();
      ctx.arc(nx + 12, ny + 12, 4, 0, Math.PI * 2);
      ctx.fill();

      // 节点名称
      ctx.fillStyle = '#e0e0e0';
      ctx.font = '600 12px "JetBrains Mono", monospace';
      ctx.fillText(node.type, nx + 24, ny + 22);

      // 节点标签
      ctx.fillStyle = '#8888a0';
      ctx.font = '10px "JetBrains Mono", monospace';
      ctx.fillText(node.name, nx + 12, ny + 42);

      // 端口指示器
      if (node.type !== 'AudioOutput') {
        ctx.fillStyle = '#6c5ce7';
        ctx.beginPath();
        ctx.arc(nx + nodeWidth, ny + nodeHeight / 2, 5, 0, Math.PI * 2);
        ctx.fill();
      }
      if (node.type !== 'WavetableOscillator' && node.type !== 'VirtualAnalogOscillator' && node.type !== 'NoiseGenerator') {
        ctx.fillStyle = '#00cec9';
        ctx.beginPath();
        ctx.arc(nx, ny + nodeHeight / 2, 5, 0, Math.PI * 2);
        ctx.fill();
      }
    });
  }, [nodes, connections, selectedNode, scale, pan]);

  useEffect(() => {
    draw();
  }, [draw]);

  // 鼠标滚轮缩放
  const handleWheel = useCallback((e: React.WheelEvent) => {
    e.preventDefault();
    const delta = e.deltaY > 0 ? 0.9 : 1.1;
    setScale(s => Math.max(0.3, Math.min(2, s * delta)));
  }, []);

  // 鼠标拖拽
  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    const rect = canvasRef.current?.getBoundingClientRect();
    if (!rect) return;
    const mx = (e.clientX - rect.left - pan.x) / scale;
    const my = (e.clientY - rect.top - pan.y) / scale;

    // 检查是否点击了节点
    const clicked = nodes.find(n =>
      mx >= n.x && mx <= n.x + 150 && my >= n.y && my <= n.y + 60
    );
    if (clicked) {
      setDragging(clicked.id);
      onNodeSelect(clicked.id);
      setOffset({ x: mx - clicked.x, y: my - clicked.y });
    } else {
      setDragging('pan');
      setOffset({ x: e.clientX - pan.x, y: e.clientY - pan.y });
    }
  }, [nodes, pan, scale, onNodeSelect]);

  const handleMouseMove = useCallback((e: React.MouseEvent) => {
    if (!dragging) return;
    if (dragging === 'pan') {
      setPan({ x: e.clientX - offset.x, y: e.clientY - offset.y });
    } else {
      const rect = canvasRef.current?.getBoundingClientRect();
      if (!rect) return;
      const mx = (e.clientX - rect.left - pan.x) / scale;
      const my = (e.clientY - rect.top - pan.y) / scale;
      onNodeMove(dragging, mx - offset.x, my - offset.y);
    }
  }, [dragging, offset, pan, scale, onNodeMove]);

  const handleMouseUp = useCallback(() => {
    setDragging(null);
  }, []);

  return (
    <canvas
      ref={canvasRef}
      width={800}
      height={500}
      style={{ width: '100%', height: '100%', cursor: dragging ? 'grabbing' : 'grab' }}
      onWheel={handleWheel}
      onMouseDown={handleMouseDown}
      onMouseMove={handleMouseMove}
      onMouseUp={handleMouseUp}
      onMouseLeave={handleMouseUp}
    />
  );
};

// =============================================================================
// 圆角矩形辅助函数
// =============================================================================
function roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number) {
  ctx.moveTo(x + r, y);
  ctx.lineTo(x + w - r, y);
  ctx.arcTo(x + w, y, x + w, y + r, r);
  ctx.lineTo(x + w, y + h - r);
  ctx.arcTo(x + w, y + h, x + w - r, y + h, r);
  ctx.lineTo(x + r, y + h);
  ctx.arcTo(x, y + h, x, y + h - r, r);
  ctx.lineTo(x, y + r);
  ctx.arcTo(x, y, x + r, y, r);
}

// =============================================================================
// 波形可视化组件 (Canvas)
// =============================================================================
const WaveformVisualizer: React.FC<{ data: Float32Array | null }> = ({ data }) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || !data) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);

    // 网格
    ctx.strokeStyle = 'rgba(108, 92, 231, 0.1)';
    ctx.lineWidth = 1;
    for (let i = 1; i < 4; i++) {
      const y = (h / 4) * i;
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
    }
    ctx.strokeStyle = 'rgba(108, 92, 231, 0.2)';
    ctx.beginPath(); ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2); ctx.stroke();

    // 波形
    const gradient = ctx.createLinearGradient(0, 0, 0, h);
    gradient.addColorStop(0, '#6c5ce7');
    gradient.addColorStop(0.5, '#00cec9');
    gradient.addColorStop(1, '#6c5ce7');
    ctx.strokeStyle = gradient;
    ctx.lineWidth = 1.5;
    ctx.beginPath();

    const step = Math.max(1, Math.floor(data.length / w));
    for (let i = 0; i < w; i++) {
      let sum = 0;
      const start = i * step;
      const end = Math.min(start + step, data.length);
      for (let j = start; j < end; j++) sum += data[j];
      const avg = sum / (end - start);
      const y = h / 2 - avg * (h / 2) * 0.8;
      if (i === 0) ctx.moveTo(i, y);
      else ctx.lineTo(i, y);
    }
    ctx.stroke();
  }, [data]);

  return <canvas ref={canvasRef} width={300} height={80} style={{ width: '100%', height: '100%', borderRadius: 4 }} />;
};

// =============================================================================
// FFT 频谱可视化组件
// =============================================================================
const SpectrumVisualizer: React.FC<{ data: Float32Array | null }> = ({ data }) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || !data) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);

    const barWidth = Math.max(1, w / data.length);
    const gradient = ctx.createLinearGradient(0, h, 0, 0);
    gradient.addColorStop(0, '#00cec9');
    gradient.addColorStop(0.5, '#6c5ce7');
    gradient.addColorStop(1, '#e74c3c');

    for (let i = 0; i < data.length; i++) {
      const barHeight = data[i] * h * 0.9;
      const x = i * barWidth;
      const alpha = 0.3 + (data[i] * 0.7);
      ctx.fillStyle = gradient;
      ctx.globalAlpha = alpha;
      ctx.fillRect(x, h - barHeight, barWidth - 1, barHeight);
    }
    ctx.globalAlpha = 1;
  }, [data]);

  return <canvas ref={canvasRef} width={300} height={80} style={{ width: '100%', height: '100%', borderRadius: 4 }} />;
};

// =============================================================================
// LFO 动态曲线可视化
// =============================================================================
const LFOVisualizer: React.FC<{ waveform: string; rate: number; running: boolean }> = ({ waveform, rate, running }) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const phaseRef = useRef(0);
  const animRef = useRef<number>(0);

  useEffect(() => {
    if (!running) {
      cancelAnimationFrame(animRef.current);
      return;
    }

    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const animate = () => {
      const w = canvas.width;
      const h = canvas.height;
      ctx.clearRect(0, 0, w, h);

      // 网格
      ctx.strokeStyle = 'rgba(0, 206, 201, 0.1)';
      ctx.lineWidth = 1;
      ctx.beginPath(); ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2); ctx.stroke();

      // 历史轨迹
      ctx.strokeStyle = 'rgba(0, 206, 201, 0.15)';
      ctx.lineWidth = 1;
      ctx.beginPath();
      for (let i = 0; i < w; i++) {
        const histPhase = phaseRef.current - (w - i) * rate * 0.001;
        const y = h / 2 - getWaveformValue(histPhase, waveform) * (h / 2) * 0.8;
        if (i === 0) ctx.moveTo(i, y);
        else ctx.lineTo(i, y);
      }
      ctx.stroke();

      // 当前点
      const currentY = h / 2 - getWaveformValue(phaseRef.current, waveform) * (h / 2) * 0.8;
      ctx.fillStyle = '#00cec9';
      ctx.shadowColor = 'rgba(0, 206, 201, 0.8)';
      ctx.shadowBlur = 10;
      ctx.beginPath();
      ctx.arc(w - 10, currentY, 5, 0, Math.PI * 2);
      ctx.fill();
      ctx.shadowBlur = 0;

      phaseRef.current += rate * 0.016;
      if (phaseRef.current > Math.PI * 2) phaseRef.current -= Math.PI * 2;
      animRef.current = requestAnimationFrame(animate);
    };
    animate();

    return () => cancelAnimationFrame(animRef.current);
  }, [waveform, rate, running]);

  return <canvas ref={canvasRef} width={200} height={60} style={{ width: '100%', height: '100%', borderRadius: 4 }} />;
};

function getWaveformValue(phase: number, type: string): number {
  switch (type) {
    case 'sine': return Math.sin(phase);
    case 'triangle': return 2 * Math.abs(2 * (phase / (Math.PI * 2) - Math.floor(phase / (Math.PI * 2) + 0.5))) - 1;
    case 'saw': return 2 * (phase / (Math.PI * 2) - Math.floor(phase / (Math.PI * 2) + 0.5));
    case 'square': return Math.sin(phase) >= 0 ? 1 : -1;
    case 'random': return Math.sin(phase * 7.13) * Math.sin(phase * 3.59);
    default: return Math.sin(phase);
  }
}

// =============================================================================
// 参数面板 (旋钮 + 滑块)
// =============================================================================
const ParameterKnob: React.FC<{
  label: string;
  value: number;
  min?: number;
  max?: number;
  onChange: (v: number) => void;
  unit?: string;
}> = ({ label, value, min = 0, max = 1, onChange, unit = '' }) => {
  const [dragging, setDragging] = useState(false);
  const knobRef = useRef<HTMLDivElement>(null);

  const displayValue = unit === '%' ? (value * 100).toFixed(0) : unit === 'Hz' ? frequencyDisplay(value) : value.toFixed(2);

  const handleMouseMove = useCallback((e: MouseEvent) => {
    if (!dragging) return;
    const delta = e.movementY * -0.005;
    onChange(Math.max(min, Math.min(max, value + delta)));
  }, [dragging, value, min, max, onChange]);

  useEffect(() => {
    if (dragging) {
      window.addEventListener('mousemove', handleMouseMove);
      window.addEventListener('mouseup', () => setDragging(false));
      return () => {
        window.removeEventListener('mousemove', handleMouseMove);
        window.removeEventListener('mouseup', () => setDragging(false));
      };
    }
  }, [dragging, handleMouseMove]);

  const rotation = -135 + (value / (max - min)) * 270;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 4 }}>
      <div
        ref={knobRef}
        onMouseDown={() => setDragging(true)}
        style={{
          width: 44, height: 44, borderRadius: '50%',
          background: '#1a1a28', border: '2px solid #2a2a3a',
          position: 'relative', cursor: 'ns-resize',
          display: 'flex', alignItems: 'center', justifyContent: 'center',
        }}
      >
        <div style={{
          width: 2, height: 14, background: '#6c5ce7', borderRadius: 1,
          position: 'absolute', bottom: '50%', left: '50%',
          transform: `translateX(-50%) rotate(${rotation}deg)`,
          transformOrigin: 'bottom center',
        }} />
        <div style={{
          width: 28, height: 28, borderRadius: '50%',
          background: 'radial-gradient(circle, #1e1e3a, #12121e)',
          border: '1px solid #2a2a3a',
        }} />
      </div>
      <span style={{ fontSize: 9, color: '#8888a0', textAlign: 'center', maxWidth: 60, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{label}</span>
      <span style={{ fontSize: 10, color: '#e0e0e0', fontFamily: '"JetBrains Mono", monospace' }}>{displayValue}{unit}</span>
    </div>
  );
};

function frequencyDisplay(normalized: number): string {
  const hz = 20 * Math.pow(10, normalized * 3);
  if (hz >= 10000) return (hz / 1000).toFixed(1) + 'k';
  if (hz >= 1000) return (hz / 1000).toFixed(2) + 'k';
  return hz.toFixed(0);
}

// =============================================================================
// 主应用 - 专业模式
// =============================================================================
const ProModeApp: React.FC = () => {
  const [mode, setMode] = useState<'pro' | 'minimal'>('pro');
  const [selectedNode, setSelectedNode] = useState<string | null>(null);
  const [nodes, setNodes] = useState<NodeInfo[]>([
    { id: 'n1', type: 'WavetableOscillator', name: 'OSC 1', x: 50, y: 100, enabled: true, params: {} },
    { id: 'n2', type: 'FilterProcessor', name: 'Filter', x: 280, y: 100, enabled: true, params: {} },
    { id: 'n3', type: 'Distortion', name: 'Drive', x: 280, y: 220, enabled: false, params: {} },
    { id: 'n4', type: 'Delay', name: 'Echo', x: 480, y: 100, enabled: true, params: {} },
    { id: 'n5', type: 'Reverb', name: 'Space', x: 480, y: 220, enabled: true, params: {} },
    { id: 'n6', type: 'AudioOutput', name: 'Output', x: 680, y: 160, enabled: true, params: {} },
    { id: 'n7', type: 'LFOGenerator', name: 'LFO 1', x: 50, y: 280, enabled: true, params: {} },
    { id: 'n8', type: 'EnvelopeGenerator', name: 'Env', x: 50, y: 380, enabled: true, params: {} },
  ]);
  const [connections] = useState<Connection[]>([
    { id: 'c1', srcNode: 'n1', srcPort: 0, dstNode: 'n2', dstPort: 0 },
    { id: 'c2', srcNode: 'n2', srcPort: 0, dstNode: 'n4', dstPort: 0 },
    { id: 'c3', srcNode: 'n4', srcPort: 0, dstNode: 'n5', dstPort: 0 },
    { id: 'c4', srcNode: 'n5', srcPort: 0, dstNode: 'n6', dstPort: 0 },
    { id: 'c5', srcNode: 'n7', srcPort: 0, dstNode: 'n1', dstPort: 0 },
    { id: 'c6', srcNode: 'n8', srcPort: 0, dstNode: 'n2', dstPort: 0 },
  ]);

  // 模拟波形数据
  const [waveformData] = useState(() => {
    const arr = new Float32Array(256);
    for (let i = 0; i < 256; i++) {
      arr[i] = Math.sin(i * 0.1) * 0.5 + Math.sin(i * 0.23) * 0.3 + Math.sin(i * 0.47) * 0.2;
    }
    return arr;
  });

  // 模拟频谱数据
  const [spectrumData] = useState(() => {
    const arr = new Float32Array(64);
    for (let i = 0; i < 64; i++) {
      arr[i] = Math.exp(-i * 0.05) * (0.3 + Math.random() * 0.3);
    }
    arr[0] = 0.8;
    arr[1] = 0.6;
    arr[2] = 0.4;
    arr[3] = 0.2;
    return arr;
  });

  const selectedNodeInfo = nodes.find(n => n.id === selectedNode);
  const [lfoRunning, setLfoRunning] = useState(true);

  return (
    <div style={{
      display: 'flex', flexDirection: 'column', height: '100vh',
      background: '#0a0a0f', color: '#e0e0e0', fontFamily: '"JetBrains Mono", "PingFang SC", monospace',
      overflow: 'hidden',
    }}>
      {/* 顶部工具栏 */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: 12, padding: '6px 16px',
        background: '#0d0d14', borderBottom: '1px solid #1a1a2a',
        fontSize: 11, minHeight: 36,
      }}>
        <span style={{ color: '#6c5ce7', fontWeight: 700, fontSize: 13, letterSpacing: 1 }}>LianCore</span>
        <span style={{ color: '#6c5ce7', fontSize: 9, padding: '1px 6px', background: 'rgba(108,92,231,0.15)', borderRadius: 3 }}>PRO</span>
        <span style={{ color: '#555568', marginLeft: 8 }}>|</span>
        <button
          onClick={() => setMode(mode === 'pro' ? 'minimal' : 'pro')}
          style={{
            padding: '3px 10px', background: mode === 'pro' ? 'rgba(108,92,231,0.2)' : 'transparent',
            border: '1px solid #2a2a3a', borderRadius: 3, color: '#8888a0', cursor: 'pointer', fontSize: 10,
          }}
        >
          {mode === 'pro' ? 'PRO 模式' : '极简模式'}
        </button>
        <span style={{ marginLeft: 'auto', color: '#555568' }}>节点: {nodes.length}</span>
        <span style={{ color: '#555568' }}>|</span>
        <span style={{ color: '#555568' }}>连接: {connections.length}</span>
        <span style={{ color: '#555568' }}>|</span>
        <span style={{ color: '#2ecc71' }}>CPU 2.4ms</span>
      </div>

      {/* 主体 */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0 }}>
        {/* 左侧: 节点图 */}
        <div style={{
          flex: 1, background: '#0a0a14', borderRight: '1px solid #1a1a2a',
          position: 'relative', overflow: 'hidden',
        }}>
          <NodeGraph
            nodes={nodes}
            connections={connections}
            selectedNode={selectedNode}
            onNodeMove={(id, x, y) => setNodes(prev => prev.map(n => n.id === id ? { ...n, x, y } : n))}
            onNodeSelect={setSelectedNode}
          />
          {/* 底部提示 */}
          <div style={{
            position: 'absolute', bottom: 8, left: 12, fontSize: 9, color: '#444458',
          }}>
            滚轮缩放 | 拖拽移动 | 点击选择节点
          </div>
        </div>

        {/* 右侧: 参数面板 + 可视化 */}
        <div style={{
          width: 320, background: '#0d0d14', borderLeft: '1px solid #1a1a2a',
          display: 'flex', flexDirection: 'column', overflow: 'hidden',
        }}>
          {/* 可视化区域 */}
          <div style={{ padding: '10px 12px', borderBottom: '1px solid #1a1a2a' }}>
            <div style={{ fontSize: 9, color: '#555568', textTransform: 'uppercase', letterSpacing: 1, marginBottom: 6 }}>波形 <span style={{ color: '#6c5ce7' }}>OSC 1</span></div>
            <div style={{ height: 80, background: '#0a0a14', borderRadius: 4, overflow: 'hidden' }}>
              <WaveformVisualizer data={waveformData} />
            </div>
            <div style={{ fontSize: 9, color: '#555568', textTransform: 'uppercase', letterSpacing: 1, marginTop: 8, marginBottom: 6 }}>频谱 <span style={{ color: '#00cec9' }}>FFT</span></div>
            <div style={{ height: 80, background: '#0a0a14', borderRadius: 4, overflow: 'hidden' }}>
              <SpectrumVisualizer data={spectrumData} />
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginTop: 8, marginBottom: 4 }}>
              <span style={{ fontSize: 9, color: '#555568', textTransform: 'uppercase', letterSpacing: 1 }}>LFO <span style={{ color: '#00cec9' }}>动态</span></span>
              <button onClick={() => setLfoRunning(!lfoRunning)} style={{
                fontSize: 8, padding: '1px 6px', background: 'transparent', border: '1px solid #2a2a3a',
                borderRadius: 2, color: lfoRunning ? '#2ecc71' : '#e74c3c', cursor: 'pointer',
              }}>
                {lfoRunning ? '运行中' : '已暂停'}
              </button>
            </div>
            <div style={{ height: 60, background: '#0a0a14', borderRadius: 4, overflow: 'hidden' }}>
              <LFOVisualizer waveform="sine" rate={2.5} running={lfoRunning} />
            </div>
          </div>

          {/* 参数面板 */}
          <div style={{ flex: 1, padding: '10px 12px', overflow: 'auto' }}>
            <div style={{ fontSize: 9, color: '#555568', textTransform: 'uppercase', letterSpacing: 1, marginBottom: 8 }}>
              {selectedNodeInfo ? `${selectedNodeInfo.type} 参数` : '选择节点以编辑参数'}
            </div>

            {selectedNodeInfo ? (
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 10 }}>
                {selectedNodeInfo.type === 'FilterProcessor' && (
                  <>
                    <ParameterKnob label="截止频率" value={0.5} onChange={() => {}} unit="Hz" />
                    <ParameterKnob label="共振" value={0.3} onChange={() => {}} unit="%" />
                    <ParameterKnob label="类型" value={0.4} onChange={() => {}} />
                    <ParameterKnob label="驱动" value={0.1} onChange={() => {}} unit="%" />
                    <ParameterKnob label="干湿比" value={0.8} onChange={() => {}} unit="%" />
                  </>
                )}
                {selectedNodeInfo.type === 'WavetableOscillator' && (
                  <>
                    <ParameterKnob label="频率" value={0.5} onChange={() => {}} unit="Hz" />
                    <ParameterKnob label="音量" value={0.8} onChange={() => {}} unit="%" />
                    <ParameterKnob label="Unison" value={0.3} onChange={() => {}} />
                    <ParameterKnob label="Detune" value={0.15} onChange={() => {}} />
                    <ParameterKnob label="帧位置" value={0.5} onChange={() => {}} />
                    <ParameterKnob label="FM深度" value={0.0} onChange={() => {}} unit="%" />
                  </>
                )}
                {selectedNodeInfo.type === 'Delay' && (
                  <>
                    <ParameterKnob label="时间" value={0.4} onChange={() => {}} />
                    <ParameterKnob label="反馈" value={0.6} onChange={() => {}} unit="%" />
                    <ParameterKnob label="混合" value={0.3} onChange={() => {}} unit="%" />
                    <ParameterKnob label="LP滤波" value={0.7} onChange={() => {}} unit="Hz" />
                    <ParameterKnob label="HP滤波" value={0.2} onChange={() => {}} unit="Hz" />
                  </>
                )}
                {selectedNodeInfo.type === 'Reverb' && (
                  <>
                    <ParameterKnob label="房间大小" value={0.6} onChange={() => {}} />
                    <ParameterKnob label="衰减" value={0.5} onChange={() => {}} />
                    <ParameterKnob label="混合" value={0.3} onChange={() => {}} unit="%" />
                    <ParameterKnob label="阻尼" value={0.4} onChange={() => {}} unit="%" />
                    <ParameterKnob label="预延迟" value={0.2} onChange={() => {}} />
                  </>
                )}
                {selectedNodeInfo.type === 'LFOGenerator' && (
                  <>
                    <ParameterKnob label="速率" value={0.4} onChange={() => {}} unit="Hz" />
                    <ParameterKnob label="深度" value={0.5} onChange={() => {}} unit="%" />
                    <ParameterKnob label="波形" value={0.2} onChange={() => {}} />
                  </>
                )}
                {selectedNodeInfo.type === 'EnvelopeGenerator' && (
                  <>
                    <ParameterKnob label="起音" value={0.1} onChange={() => {}} />
                    <ParameterKnob label="衰减" value={0.3} onChange={() => {}} />
                    <ParameterKnob label="保持" value={0.7} onChange={() => {}} unit="%" />
                    <ParameterKnob label="释音" value={0.5} onChange={() => {}} />
                  </>
                )}
                {selectedNodeInfo.type === 'Distortion' && (
                  <>
                    <ParameterKnob label="驱动" value={0.7} onChange={() => {}} unit="%" />
                    <ParameterKnob label="类型" value={0.3} onChange={() => {}} />
                    <ParameterKnob label="输出" value={0.5} onChange={() => {}} unit="%" />
                    <ParameterKnob label="混合" value={0.8} onChange={() => {}} unit="%" />
                  </>
                )}
              </div>
            ) : (
              <div style={{ color: '#444458', fontSize: 11, textAlign: 'center', paddingTop: 40 }}>
                点击左侧节点图中的节点<br />以查看和编辑参数
              </div>
            )}
          </div>

          {/* 底部状态 */}
          <div style={{
            padding: '6px 12px', borderTop: '1px solid #1a1a2a',
            fontSize: 9, color: '#555568', display: 'flex', gap: 12,
          }}>
            <span>选中: {selectedNodeInfo?.name || '无'}</span>
            <span style={{ color: '#2ecc71' }}>●</span>
            <span>48kHz</span>
            <span style={{ marginLeft: 'auto' }}>LianCore V3 Beta</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default ProModeApp;
export { WaveformVisualizer, SpectrumVisualizer, LFOVisualizer, ParameterKnob, NodeGraph };