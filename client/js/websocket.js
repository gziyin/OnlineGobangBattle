/**
 * 在线五子棋 WebSocket 客户端封装
 *
 * 功能：
 * - 连接建立/断开处理
 * - 消息发送/接收
 * - 心跳维护
 * - 事件分发
 */

class GobangWebSocket {
    constructor(options) {
        this.url = options.url;
        this.token = options.token;

        // 回调函数
        this.onOpen = options.onOpen || (() => {});
        this.onClose = options.onClose || (() => {});
        this.onError = options.onError || (() => {});
        this.onEvent = options.onEvent || (() => {});

        // 内部状态
        this.ws = null;
        this.connected = false;
        this.heartbeatTimer = null;
        this.heartbeatInterval = 30000; // 30秒心跳
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 3000; // 3秒重连延迟
    }

    /**
     * 建立连接
     */
    connect() {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            console.log('WebSocket already connected');
            return;
        }

        try {
            this.ws = new WebSocket(this.url);

            this.ws.onopen = () => {
                console.log('WebSocket connected');
                this.connected = true;
                this.reconnectAttempts = 0;
                this.startHeartbeat();
                this.onOpen();
            };

            this.ws.onclose = (event) => {
                console.log('WebSocket closed:', event.code, event.reason);
                this.connected = false;
                this.stopHeartbeat();
                this.onClose(event);
            };

            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.onError(error);
            };

            this.ws.onmessage = (event) => {
                this.handleMessage(event.data);
            };

        } catch (error) {
            console.error('WebSocket connect failed:', error);
            this.onError(error);
        }
    }

    /**
     * 重连
     */
    reconnect() {
        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            console.log('Max reconnect attempts reached');
            return;
        }

        this.reconnectAttempts++;
        console.log(`Reconnecting... attempt ${this.reconnectAttempts}`);

        setTimeout(() => {
            this.connect();
        }, this.reconnectDelay);
    }

    /**
     * 关闭连接
     */
    close() {
        this.stopHeartbeat();
        if (this.ws) {
            this.ws.close(1000, 'Client close');
            this.ws = null;
        }
        this.connected = false;
    }

    /**
     * 发送消息
     * @param {string} event - 事件类型
     * @param {object} data - 数据对象
     */
    send(event, data = {}) {
        if (!this.connected || !this.ws) {
            console.warn('WebSocket not connected, cannot send:', event);
            return false;
        }

        const message = {
            event: event,
            data: data
        };

        try {
            this.ws.send(JSON.stringify(message));
            console.log('Sent:', event, data);
            return true;
        } catch (error) {
            console.error('Send failed:', error);
            return false;
        }
    }

    /**
     * 处理接收的消息
     * @param {string} data - 原始消息数据
     */
    handleMessage(data) {
        try {
            const message = JSON.parse(data);
            const event = message.event;
            const eventData = message.data || {};

            console.log('Received:', event, eventData);

            // 分发事件
            this.dispatchEvent(event, eventData);

        } catch (error) {
            console.error('Parse message failed:', error, data);
        }
    }

    /**
     * 分发事件
     * @param {string} event - 事件类型
     * @param {object} data - 事件数据
     */
    dispatchEvent(event, data) {
        // 调用用户注册的事件处理器
        this.onEvent(event, data);
    }

    /**
     * 启动心跳
     */
    startHeartbeat() {
        this.stopHeartbeat();

        this.heartbeatTimer = setInterval(() => {
            if (this.connected) {
                this.send('ping', { timestamp: Date.now() });
            }
        }, this.heartbeatInterval);

        console.log('Heartbeat started');
    }

    /**
     * 停止心跳
     */
    stopHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = null;
            console.log('Heartbeat stopped');
        }
    }

    /**
     * 检查连接状态
     */
    isConnected() {
        return this.connected && this.ws && this.ws.readyState === WebSocket.OPEN;
    }

    /**
     * 获取原生 WebSocket 实例
     */
    getRawSocket() {
        return this.ws;
    }
}

// 导出（支持 ES6 模块和全局变量）
if (typeof module !== 'undefined' && module.exports) {
    module.exports = GobangWebSocket;
} else {
    window.GobangWebSocket = GobangWebSocket;
}