// =============================================================================
// LianCore WebSocket 通信模块
// 与 VST3 插件 C++ 核心层通信
// =============================================================================

export type MessageType =
  | 'param_change'
  | 'param_batch'
  | 'ai_generate_request'
  | 'ai_generate_result'
  | 'ai_generate_progress'
  | 'preset_list'
  | 'preset_load'
  | 'preset_save'
  | 'cpu_usage'
  | 'memory_usage'
  | 'emotion'
  | 'emotion_applied'
  | 'generate'
  | 'error'
  | 'heartbeat';

export interface LianCoreMessage {
  type: MessageType;
  payload: Record<string, unknown>;
  timestamp: number;
  requestId?: string;
}

export interface ParameterMapping {
  parameterId: string;
  value: number;
  explanation: string;
}

export interface GenerationResult {
  parameters: ParameterMapping[];
  presetName: string;
  confidence: number;
}

export interface Preset {
  id: number;
  name: string;
  category: string;
  tags: string[];
  rating: number;
}

type MessageHandler = (msg: LianCoreMessage) => void;

class LianCoreWebSocket {
  private ws: WebSocket | null = null;
  private handlers: Map<MessageType, MessageHandler[]> = new Map();
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private url: string;

  constructor(url: string = 'ws://localhost:9001') {
    this.url = url;
  }

  connect(): void {
    try {
      this.ws = new WebSocket(this.url);

      this.ws.onopen = () => {
        console.log('[LianCore] WebSocket connected');
        this.dispatch({ type: 'heartbeat', payload: { status: 'connected' }, timestamp: Date.now() });
      };

      this.ws.onmessage = (event: MessageEvent) => {
        try {
          const msg: LianCoreMessage = JSON.parse(event.data);
          this.dispatch(msg);
        } catch (e) {
          console.error('[LianCore] Failed to parse message:', e);
        }
      };

      this.ws.onclose = () => {
        console.log('[LianCore] WebSocket disconnected, reconnecting...');
        this.scheduleReconnect();
      };

      this.ws.onerror = (err) => {
        console.error('[LianCore] WebSocket error:', err);
      };
    } catch (e) {
      console.error('[LianCore] Failed to connect:', e);
      this.scheduleReconnect();
    }
  }

  disconnect(): void {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    this.ws?.close();
    this.ws = null;
  }

  on(type: MessageType, handler: MessageHandler): () => void {
    if (!this.handlers.has(type)) {
      this.handlers.set(type, []);
    }
    this.handlers.get(type)!.push(handler);
    return () => {
      const handlers = this.handlers.get(type);
      if (handlers) {
        const idx = handlers.indexOf(handler);
        if (idx >= 0) handlers.splice(idx, 1);
      }
    };
  }

  send(type: MessageType, payload: Record<string, unknown> = {}): void {
    if (this.ws?.readyState === WebSocket.OPEN) {
      const msg: LianCoreMessage = {
        type,
        payload,
        timestamp: Date.now(),
        requestId: crypto.randomUUID?.() ?? Math.random().toString(36).slice(2),
      };
      this.ws.send(JSON.stringify(msg));
    }
  }

  // AI 生成请求
  requestGeneration(prompt: string, styleTags: string[] = []): void {
    this.send('ai_generate_request', { prompt, styleTags });
  }

  // 参数变更
  setParameter(parameterId: string, value: number): void {
    this.send('param_change', { parameterId, value });
  }

  setParameterBatch(parameters: ParameterMapping[]): void {
    this.send('param_batch', { parameters });
  }

  // 预设操作
  requestPresetList(): void {
    this.send('preset_list');
  }

  loadPreset(presetId: number): void {
    this.send('preset_load', { presetId });
  }

  savePreset(name: string, category: string): void {
    this.send('preset_save', { name, category });
  }

  // 情感滑块实时发送 (Beta Week 6)
  sendEmotion(warmth: number, energy: number, tension: number): void {
    this.send('emotion', { warmth, energy, tension });
  }

  // 联合生成请求 (文本 + 情感) (Beta Week 6)
  requestGenerationWithEmotion(
    text: string,
    warmth: number,
    energy: number,
    tension: number,
    styleTags: string[] = []
  ): void {
    this.send('generate', {
      text,
      emotion: { warmth, energy, tension },
      styleTags,
    });
  }

  private dispatch(msg: LianCoreMessage): void {
    const handlers = this.handlers.get(msg.type);
    if (handlers) {
      handlers.forEach(h => h(msg));
    }
  }

  private scheduleReconnect(): void {
    if (this.reconnectTimer) return;
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      this.connect();
    }, 2000);
  }
}

// 单例
export const liancore = new LianCoreWebSocket();
export default liancore;