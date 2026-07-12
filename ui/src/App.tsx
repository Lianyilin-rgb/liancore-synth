// =============================================================================
// LianCore - App 主组件 (极简模式)
// =============================================================================
import React, { useEffect, useState, useCallback, useRef } from 'react';
import liancore, { GenerationResult, Preset, ParameterMapping } from './websocket';
import ProModeApp from './ProMode';

// =============================================================================
// App 组件
// =============================================================================
const App: React.FC = () => {
  const [mode, setMode] = useState<'minimal' | 'pro'>('minimal');

  // 专业模式
  if (mode === 'pro') return <ProModeApp />;

  const [connected, setConnected] = useState(false);
  const [aiPrompt, setAiPrompt] = useState('');
  const [generating, setGenerating] = useState(false);
  const [generationResult, setGenerationResult] = useState<GenerationResult | null>(null);
  const [presets, setPresets] = useState<Preset[]>([]);
  const [category, setCategory] = useState('全部');
  const [cpuUsage, setCpuUsage] = useState(0);
  const [memUsage, setMemUsage] = useState(0);
  const [activeStyleTags, setActiveStyleTags] = useState<string[]>([]);
  const [emotions, setEmotions] = useState({ warmth: 0.5, energy: 0.5, tension: 0.5 });
  const [emotionApplied, setEmotionApplied] = useState<{ paramCount: number } | null>(null);

  const categories = ['全部', '贝斯', '主音', '铺底', '打击', '键盘', '弦乐', '管乐', '特效', 'AI生成'];

  const styleTags = ['明亮', '温暖', '暗', '尖锐', '柔和', '复古', '现代', '电子', '经典', '管弦', '贝斯', '主音', '铺底'];

  // =========================================================================
  // WebSocket 连接
  // =========================================================================
  useEffect(() => {
    liancore.connect();

    const unsubs = [
      liancore.on('heartbeat', () => setConnected(true)),
      liancore.on('ai_generate_result', (msg) => {
        setGenerating(false);
        setGenerationResult(msg.payload as unknown as GenerationResult);
      }),
      liancore.on('ai_generate_progress', (msg) => {
        setGenerating(true);
      }),
      liancore.on('preset_list', (msg) => {
        setPresets((msg.payload.presets as Preset[]) || []);
      }),
      liancore.on('cpu_usage', (msg) => {
        setCpuUsage(msg.payload.usage as number);
      }),
      liancore.on('memory_usage', (msg) => {
        setMemUsage(msg.payload.mb as number);
      }),
      liancore.on('emotion_applied', (msg) => {
        setEmotionApplied({
          paramCount: msg.payload.paramCount as number,
        });
        // 3秒后清除确认提示
        setTimeout(() => setEmotionApplied(null), 3000);
      }),
    ];

    // 请求初始数据
    liancore.requestPresetList();

    // 模拟心跳检测
    const heartbeat = setInterval(() => {
      if (liancore) {
        liancore.send('heartbeat');
      }
    }, 5000);

    return () => {
      unsubs.forEach(fn => fn());
      clearInterval(heartbeat);
      liancore.disconnect();
    };
  }, []);

  // =========================================================================
  // AI 生成
  // =========================================================================
  const handleGenerate = useCallback(() => {
    if (!aiPrompt.trim() || generating) return;
    setGenerating(true);
    setGenerationResult(null);
    // 情感滑块非默认时 → 联合生成
    if (emotions.warmth !== 0.5 || emotions.energy !== 0.5 || emotions.tension !== 0.5) {
      liancore.requestGenerationWithEmotion(
        aiPrompt.trim(),
        emotions.warmth,
        emotions.energy,
        emotions.tension,
        activeStyleTags
      );
    } else {
      liancore.requestGeneration(aiPrompt.trim(), activeStyleTags);
    }
  }, [aiPrompt, generating, activeStyleTags, emotions]);

  // 键盘快捷键
  const handleKeyDown = useCallback((e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleGenerate();
    }
  }, [handleGenerate]);

  // =========================================================================
  // 情感滑块变更 (实时 WebSocket 发送)
  // =========================================================================
  const handleEmotionChange = useCallback((name: string, value: number) => {
    const newEmotions = { ...emotions, [name]: value };
    setEmotions(newEmotions);
    // 实时发送情感向量到合成器引擎 (Beta Week 6)
    liancore.sendEmotion(newEmotions.warmth, newEmotions.energy, newEmotions.tension);
  }, [emotions]);

  // =========================================================================
  // 预设加载
  // =========================================================================
  const handlePresetClick = useCallback((preset: Preset) => {
    liancore.loadPreset(preset.id);
  }, []);

  // =========================================================================
  // 应用生成结果
  // =========================================================================
  const handleApplyResult = useCallback(() => {
    if (generationResult?.parameters) {
      liancore.setParameterBatch(generationResult.parameters);
    }
  }, [generationResult]);

  // =========================================================================
  // 过滤预设
  // =========================================================================
  const filteredPresets = category === '全部'
    ? presets
    : presets.filter(p => p.category === category);

  return (
    <div className="app-container">
      {/* 顶部栏 */}
      <div className="top-bar">
        <span className="logo">LianCore</span>
        <span className="version-badge">BETA</span>
        <button
          onClick={() => setMode(mode === 'pro' ? 'minimal' : 'pro')}
          style={{
            padding: '3px 10px', background: 'rgba(108,92,231,0.15)',
            border: '1px solid #2a2a3a', borderRadius: 3, color: '#8888a0',
            cursor: 'pointer', fontSize: 10, marginLeft: 8,
          }}
        >
          {mode === 'pro' ? 'PRO' : '极简'}
        </button>
        <span className={`status-dot ${connected ? 'connected' : 'disconnected'}`}
              title={connected ? '已连接' : '未连接'} />
      </div>

      {/* AI 输入栏 */}
      <div className="ai-input-section">
        <div className="ai-input-wrapper">
          <input
            className="ai-text-input"
            type="text"
            placeholder="描述你想要的声音... 例如: '温暖的模拟合成器铺底'"
            value={aiPrompt}
            onChange={e => setAiPrompt(e.target.value)}
            onKeyDown={handleKeyDown}
          />
          <button
            className="ai-generate-btn"
            onClick={handleGenerate}
            disabled={generating || !aiPrompt.trim()}
          >
            {generating ? '生成中...' : 'AI 生成'}
          </button>
        </div>
        <div className="style-tags">
          {styleTags.map(tag => (
            <span
              key={tag}
              className={`style-tag ${activeStyleTags.includes(tag) ? 'active' : ''}`}
              onClick={() => setActiveStyleTags(prev =>
                prev.includes(tag) ? prev.filter(t => t !== tag) : [...prev, tag]
              )}
            >
              {tag}
            </span>
          ))}
        </div>
      </div>

      {/* 生成结果 */}
      {generating && (
        <div className="generation-progress">
          AI 正在生成音色...
        </div>
      )}
      {generationResult && !generating && (
        <div className="ai-input-section" style={{ borderColor: 'var(--accent)' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <div>
              <span style={{ color: 'var(--accent)', fontWeight: 600 }}>
                {generationResult.presetName}
              </span>
              <span style={{ marginLeft: 8, fontSize: 12, color: 'var(--text-secondary)' }}>
                置信度: {(generationResult.confidence * 100).toFixed(0)}%
              </span>
            </div>
            <button className="ai-generate-btn" onClick={handleApplyResult}>
              应用参数
            </button>
          </div>
          <div style={{ marginTop: 8, fontSize: 12, color: 'var(--text-secondary)' }}>
            {generationResult.parameters?.slice(0, 5).map((p, i) => (
              <span key={i} style={{ marginRight: 12 }}>
                {p.explanation}
              </span>
            ))}
            {(generationResult.parameters?.length || 0) > 5 && (
              <span style={{ color: 'var(--text-muted)' }}>
                +{(generationResult.parameters?.length || 0) - 5} 项
              </span>
            )}
          </div>
        </div>
      )}

      {/* 情感滑块 */}
      <div className="emotion-sliders">
        <h3>情感控制</h3>
        {emotionApplied && (
          <div className="emotion-applied-badge">
            已应用 {emotionApplied.paramCount} 个参数
          </div>
        )}
        <div className="emotion-grid">
          {[
            { id: 'warmth', label: '温暖度', icon: '🔥' },
            { id: 'energy', label: '能量感', icon: '⚡' },
            { id: 'tension', label: '紧张度', icon: '🎯' },
          ].map(({ id, label, icon }) => (
            <div key={id} className="emotion-item">
              <div className="emotion-label">
                <span>{icon} {label}</span>
                <span>{(emotions[id as keyof typeof emotions] * 100).toFixed(0)}%</span>
              </div>
              <input
                type="range"
                className="emotion-slider"
                min="0"
                max="1"
                step="0.01"
                value={emotions[id as keyof typeof emotions]}
                onChange={e => handleEmotionChange(id, parseFloat(e.target.value))}
              />
            </div>
          ))}
        </div>
      </div>

      {/* 预设浏览器 */}
      <div className="preset-section">
        <div className="preset-header">
          <h3>预设</h3>
          <select
            className="preset-category-select"
            value={category}
            onChange={e => setCategory(e.target.value)}
          >
            {categories.map(c => (
              <option key={c} value={c}>{c}</option>
            ))}
          </select>
        </div>
        <div className="preset-grid">
          {filteredPresets.length === 0 ? (
            <div style={{ gridColumn: '1 / -1', textAlign: 'center', padding: 40, color: 'var(--text-muted)', fontSize: 13 }}>
              暂无预设 - 使用 AI 生成创建第一个！
            </div>
          ) : (
            filteredPresets.map(preset => (
              <div
                key={preset.id}
                className="preset-card"
                onClick={() => handlePresetClick(preset)}
              >
                <div className="preset-card-name">{preset.name}</div>
                <div className="preset-card-category">{preset.category}</div>
                <div className="preset-card-tags">
                  {preset.tags?.slice(0, 3).map((tag, i) => (
                    <span key={i} className="preset-card-tag">{tag}</span>
                  ))}
                </div>
              </div>
            ))
          )}
        </div>
      </div>

      {/* 状态栏 */}
      <div className="status-bar">
        <div className="status-item">
          <span className="status-label">CPU</span>
          <span className={`status-value ${cpuUsage > 80 ? 'warning' : 'good'}`}>
            {cpuUsage.toFixed(1)}%
          </span>
        </div>
        <div className="status-item">
          <span className="status-label">内存</span>
          <span className="status-value">{memUsage.toFixed(0)} MB</span>
        </div>
        <div className="status-item">
          <span className="status-label">延迟</span>
          <span className="status-value good">2.3ms</span>
        </div>
        <div className="status-item" style={{ marginLeft: 'auto' }}>
          <span className="status-label">引擎</span>
          <span className="status-value">Beta v3.0</span>
        </div>
      </div>
    </div>
  );
};

export default App;