// =============================================================================
// LianCore WebSocket 通信模块
// Beta Week 8: 安全增强 - 重连退避 + 消息速率限制
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
  | 'heartbeat'
  | 'morph'           // Beta Week 8: morphTo 渐变请求
  | 'morph_started'   // Beta Week 8: morph 启动确认
  | 'morph_progress'  // Beta Week 8: morph 进度推送
  | 'morph_status'    // Beta Week 8: morph 状态查询
  | 'onnx_status';    // Beta Week 8: ONNX 模型状态

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

// SEC-005: 消息速率限制配置
const RATE_LIMIT = {
  maxMessagesPerSecond: 100,
  windowMs: 1000,
};

class LianCoreWebSocket {
  private ws: WebSocket | null = null;
  private handlers: Map<MessageType, MessageHandler[]> = new Map();
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private url: string;

  // SEC-007: 重连退避策略
  private reconnectAttempts = 0;
  private readonly maxReconnectAttempts = 10;
  private readonly baseReconnectDelay = 2000; // 2秒
  private readonly maxReconnectDelay = 60000; // 60秒

  // SEC-005: 消息速率限制
  private messageTimestamps: number[] = [];

  constructor(url: string = 'ws://localhost:9001') {
    this.url = url;
  }

  connect(): void {
    // SEC-007: 连接前检查最大重试次数
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      console.error('[LianCore] Max reconnect attempts reached, stopping');
      return;
    }

    try {
      this.ws = new WebSocket(this.url);

      this.ws.onopen = () => {
        console.log('[LianCore] WebSocket connected');
        this.reconnectAttempts = 0; // 重置重连计数
        this.dispatch({ type: 'heartbeat', payload: { status: 'connected' }, timestamp: Date.now() });
      };

      this.ws.onmessage = (event: MessageEvent) => {
        try {
          // SEC-005: 基本消息大小检查 (防止超大数据包)
          if (typeof event.data === 'string' && event.data.length > 1024 * 1024) {
            console.warn('[LianCore] Message too large, ignoring:', event.data.length);
            return;
          }
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
    this.reconnectAttempts = 0;
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

  // SEC-005: 带速率限制的发送方法
  send(type: MessageType, payload: Record<string, unknown> = {}): void {
    if (!this.checkRateLimit()) {
      console.warn('[LianCore] Rate limit exceeded, dropping message:', type);
      return;
    }

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

  // SEC-005: 速率限制检查
  private checkRateLimit(): boolean {
    const now = Date.now();
    // 清理过期的时间戳
    this.messageTimestamps = this.messageTimestamps.filter(
      t => now - t < RATE_LIMIT.windowMs
    );
    if (this.messageTimestamps.length >= RATE_LIMIT.maxMessagesPerSecond) {
      return false;
    }
    this.messageTimestamps.push(now);
    return true;
  }

  private dispatch(msg: LianCoreMessage): void {
    const handlers = this.handlers.get(msg.type);
    if (handlers) {
      handlers.forEach(h => h(msg));
    }
  }

  // SEC-007: 带指数退避的重连调度
  private scheduleReconnect(): void {
    if (this.reconnectTimer) return;

    this.reconnectAttempts++;
    if (this.reconnectAttempts > this.maxReconnectAttempts) {
      console.error('[LianCore] Max reconnect attempts reached, giving up');
      return;
    }

    // 指数退避: delay = min(baseDelay * 2^attempts, maxDelay)
    const delay = Math.min(
      this.baseReconnectDelay * Math.pow(2, this.reconnectAttempts - 1),
      this.maxReconnectDelay
    );

    console.log(`[LianCore] Reconnecting in ${delay}ms (attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts})`);

    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      this.connect();
    }, delay);
  }
}

// 单例
export const liancore = new LianCoreWebSocket();
export default liancore;