/**
 * 游戏房间页面逻辑：棋盘绘制、WebSocket 事件、落子与认输
 */

const CELL_SIZE = 40;
const PADDING = 20;
const BOARD_SIZE = 15;

const gameState = {
    roomId: null,
    myColor: null,
    myUserId: null,
    isMyTurn: false,
    board: Array(BOARD_SIZE).fill(null).map(() => Array(BOARD_SIZE).fill(0)),
    opponent: null,
    gameActive: false,
    lastMove: null
};

let wsClient = null;
let shouldReconnect = true;

const elements = {
    opponentName: document.querySelector('.player-info.opponent .username'),
    opponentScore: document.querySelector('.player-info.opponent .score'),
    selfName: document.querySelector('.player-info.self .username'),
    selfScore: document.querySelector('.player-info.self .score'),
    turnIndicator: document.querySelector('.turn-indicator'),
    btnGiveup: document.getElementById('btn-giveup'),
    gameOverModal: document.getElementById('game-over-modal'),
    resultText: document.querySelector('.result-text'),
    scoreChange: document.querySelector('.score-change'),
    btnBackHall: document.getElementById('btn-back-hall'),
    notification: document.getElementById('notification'),
    connectionStatus: document.getElementById('connectionStatus')
};

function colorToBoardValue(color) {
    return color === 'black' ? 1 : 2;
}

function showNotification(message) {
    if (!elements.notification) return;
    elements.notification.textContent = message;
    elements.notification.classList.add('show');
    setTimeout(() => elements.notification.classList.remove('show'), 3000);
}

function updateConnectionStatus(status) {
    if (!elements.connectionStatus) return;
    elements.connectionStatus.className = `connection-status ${status}`;
    const labels = {
        connected: '已连接',
        disconnected: '已断开',
        connecting: '正在连接...'
    };
    elements.connectionStatus.textContent = labels[status] || status;
}

function updateTurnIndicator() {
    if (!elements.turnIndicator) return;
    if (!gameState.gameActive) {
        elements.turnIndicator.textContent = '等待游戏开始...';
        elements.turnIndicator.className = 'turn-indicator';
        return;
    }
    if (gameState.isMyTurn) {
        elements.turnIndicator.textContent = '轮到你了';
        elements.turnIndicator.className = 'turn-indicator my-turn';
    } else {
        elements.turnIndicator.textContent = '等待对手落子';
        elements.turnIndicator.className = 'turn-indicator opponent-turn';
    }
}

function updateUI() {
    const user = JSON.parse(sessionStorage.getItem('gobang_user') || '{}');
    if (elements.selfName) {
        elements.selfName.textContent = user.username || '我';
    }
    if (elements.selfScore) {
        elements.selfScore.textContent = `分数: ${user.score || 1000}`;
    }
    if (elements.opponentName && gameState.opponent) {
        elements.opponentName.textContent =
            gameState.opponent.username || `用户 ${gameState.opponent.user_id}`;
    }
    if (elements.btnGiveup) {
        elements.btnGiveup.disabled = !gameState.gameActive;
    }
    updateTurnIndicator();
}

function drawBoard() {
    const canvas = document.getElementById('board');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    ctx.strokeStyle = '#333';
    ctx.lineWidth = 1;
    for (let i = 0; i < BOARD_SIZE; i++) {
        ctx.beginPath();
        ctx.moveTo(PADDING, PADDING + i * CELL_SIZE);
        ctx.lineTo(PADDING + (BOARD_SIZE - 1) * CELL_SIZE, PADDING + i * CELL_SIZE);
        ctx.stroke();

        ctx.beginPath();
        ctx.moveTo(PADDING + i * CELL_SIZE, PADDING);
        ctx.lineTo(PADDING + i * CELL_SIZE, PADDING + (BOARD_SIZE - 1) * CELL_SIZE);
        ctx.stroke();
    }

    const starPoints = [[3, 3], [3, 11], [7, 7], [11, 3], [11, 11]];
    ctx.fillStyle = '#333';
    starPoints.forEach(([r, c]) => {
        ctx.beginPath();
        ctx.arc(PADDING + c * CELL_SIZE, PADDING + r * CELL_SIZE, 4, 0, Math.PI * 2);
        ctx.fill();
    });

    for (let r = 0; r < BOARD_SIZE; r++) {
        for (let c = 0; c < BOARD_SIZE; c++) {
            if (gameState.board[r][c] === 1) {
                drawPiece(r, c, 'black', false);
            } else if (gameState.board[r][c] === 2) {
                drawPiece(r, c, 'white', false);
            }
        }
    }

    if (gameState.lastMove) {
        drawLastMoveMarker(gameState.lastMove.row, gameState.lastMove.col);
    }
}

function drawPiece(row, col, color, markLast = true) {
    const canvas = document.getElementById('board');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const x = PADDING + col * CELL_SIZE;
    const y = PADDING + row * CELL_SIZE;

    ctx.beginPath();
    ctx.arc(x, y, 18, 0, Math.PI * 2);
    ctx.fillStyle = color === 'black' ? '#000' : '#fff';
    ctx.fill();
    ctx.strokeStyle = '#333';
    ctx.stroke();

    if (markLast) {
        gameState.lastMove = { row, col };
        drawLastMoveMarker(row, col);
    }
}

function drawLastMoveMarker(row, col) {
    const canvas = document.getElementById('board');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const x = PADDING + col * CELL_SIZE;
    const y = PADDING + row * CELL_SIZE;

    ctx.beginPath();
    ctx.arc(x, y, 6, 0, Math.PI * 2);
    ctx.strokeStyle = '#e74c3c';
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.lineWidth = 1;
}

function restoreBoard(boardStr) {
    if (!boardStr || boardStr.length !== BOARD_SIZE * BOARD_SIZE) {
        return;
    }
    for (let r = 0; r < BOARD_SIZE; r++) {
        for (let c = 0; c < BOARD_SIZE; c++) {
            const ch = boardStr.charAt(r * BOARD_SIZE + c);
            gameState.board[r][c] = ch === '1' ? 1 : ch === '2' ? 2 : 0;
        }
    }
    drawBoard();
}

function showGameOverModal(data) {
    if (!elements.gameOverModal) return;
    const result = data.result || '';
    const reason = data.reason || '';
    let text = '游戏结束';
    if (result.includes('black_win')) {
        text = gameState.myColor === 'black' ? '你赢了！' : '你输了';
    } else if (result.includes('white_win')) {
        text = gameState.myColor === 'white' ? '你赢了！' : '你输了';
    } else if (result === 'draw') {
        text = '平局';
    }
    if (reason === 'giveup') {
        text = data.winner && data.winner.user_id === gameState.myUserId
            ? '对手认输，你赢了！'
            : '你已认输';
    }
    if (elements.resultText) {
        elements.resultText.textContent = text;
    }
    if (elements.scoreChange) {
        const delta = data.score_change != null ? data.score_change : 0;
        elements.scoreChange.textContent = `积分变化: ${delta >= 0 ? '+' : ''}${delta}`;

        // 更新 sessionStorage 中的分数，使回到大厅后显示最新积分
        try {
            const userInfo = JSON.parse(sessionStorage.getItem('gobang_user') || '{}');
            if (userInfo.score != null) {
                userInfo.score = Math.max(0, userInfo.score + delta);
                sessionStorage.setItem('gobang_user', JSON.stringify(userInfo));
            }
        } catch (e) {
            // ignore sessionStorage errors
        }
    }
    elements.gameOverModal.classList.remove('hidden');
}

const eventHandlers = {
    'game.start': (data) => {
        gameState.roomId = data.room_id || gameState.roomId;
        gameState.opponent = data.opponent;
        gameState.myColor = data.color || gameState.myColor;
        gameState.isMyTurn = !!data.your_turn;
        gameState.gameActive = true;
        updateUI();
        updateTurnIndicator();
    },

    'game.move': (data) => {
        const { row, col, color, next_turn } = data;
        gameState.board[row][col] = colorToBoardValue(color);
        drawPiece(row, col, color);
        gameState.isMyTurn = next_turn === gameState.myColor;
        updateTurnIndicator();
    },

    'game.over': (data) => {
        gameState.gameActive = false;
        shouldReconnect = false;
        if (elements.btnGiveup) {
            elements.btnGiveup.disabled = true;
        }
        // 隐藏断线覆盖层（如果有）
        hideDisconnectOverlay();
        showGameOverModal(data);
    },

    'game.reconnect': (data) => {
        gameState.roomId = data.room_id || gameState.roomId;
        gameState.opponent = data.opponent;
        restoreBoard(data.board);
        gameState.isMyTurn = data.current_turn === gameState.myColor;
        gameState.gameActive = true;
        hideDisconnectOverlay();
        updateUI();
        updateTurnIndicator();
    },

    'opponent.disconnected': (data) => {
        const timeout = data.timeout_seconds || 60;
        showDisconnectOverlay(timeout);
    },

    'opponent.reconnected': (data) => {
        hideDisconnectOverlay();
        showNotification('对手已重连');
    },

    'error': (data) => {
        showNotification(data.message || '操作失败');
        if (data.code === 4009) {
            // 账号已在线，停止重连，跳转登录页
            shouldReconnect = false;
            setTimeout(() => {
                window.location.href = 'login.html';
            }, 2000);
        } else if (data.code === 4004) {
            // 房间不存在（已销毁），跳转大厅
            shouldReconnect = false;
            gameState.gameActive = false;
            setTimeout(() => {
                window.location.href = 'hall.html';
            }, 2000);
        }
    }
};

function initFromUrl() {
    const urlParams = new URLSearchParams(window.location.search);
    gameState.roomId = urlParams.get('room_id');
    gameState.myColor = urlParams.get('color');

    const token = sessionStorage.getItem('gobang_token');
    if (!token) {
        window.location.href = 'login.html';
        return false;
    }

    const user = JSON.parse(sessionStorage.getItem('gobang_user') || '{}');
    gameState.myUserId = user.id;
    if (!gameState.myUserId) {
        window.location.href = 'login.html';
        return false;
    }
    return true;
}

function initWebSocket() {
    const token = sessionStorage.getItem('gobang_token');
    const wsHost = window.location.hostname || '127.0.0.1';

    wsClient = new GobangWebSocket({
        url: `ws://${wsHost}:8080/ws`,
        token: token,

        onOpen: () => {
            updateConnectionStatus('connected');
            wsClient.send('auth', { token: token });

            // 房间页带 room_id：game.start 可能已发往已关闭的大厅连接，需主动拉取状态
            if (gameState.roomId) {
                wsClient.send('game.reconnect', {
                    token: token,
                    room_id: gameState.roomId
                });
            }
        },

        onClose: () => {
            updateConnectionStatus('disconnected');
            if (shouldReconnect && gameState.gameActive) {
                updateConnectionStatus('connecting');
                wsClient.reconnect();
            }
        },

        onError: () => {
            showNotification('连接错误');
            updateConnectionStatus('disconnected');
        },

        onEvent: (event, data) => {
            if (eventHandlers[event]) {
                eventHandlers[event](data);
            }
        }
    });

    wsClient.connect();
}

function setupBoardClick() {
    const canvas = document.getElementById('board');
    if (!canvas) return;

    canvas.addEventListener('click', (e) => {
        if (!gameState.gameActive || !gameState.isMyTurn || !wsClient) return;

        const rect = canvas.getBoundingClientRect();
        const x = e.clientX - rect.left - PADDING;
        const y = e.clientY - rect.top - PADDING;

        const col = Math.round(x / CELL_SIZE);
        const row = Math.round(y / CELL_SIZE);

        if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) return;
        if (gameState.board[row][col] !== 0) return;

        wsClient.send('game.move', {
            token: sessionStorage.getItem('gobang_token'),
            row: row,
            col: col
        });
    });
}

function setupActions() {
    if (elements.btnGiveup) {
        elements.btnGiveup.addEventListener('click', () => {
            if (!gameState.gameActive || !wsClient) return;
            if (confirm('确定要认输吗？')) {
                wsClient.send('game.giveup', {
                    token: sessionStorage.getItem('gobang_token')
                });
            }
        });
    }

    if (elements.btnBackHall) {
        elements.btnBackHall.addEventListener('click', () => {
            shouldReconnect = false;
            if (wsClient) {
                wsClient.close();
            }
            window.location.href = 'hall.html';
        });
    }
}

// ===== 对手断线覆盖层 =====

let disconnectOverlayTimer = null;

function showDisconnectOverlay(timeoutSeconds) {
    // 移除已有覆盖层
    hideDisconnectOverlay();

    const overlay = document.createElement('div');
    overlay.id = 'disconnect-overlay';
    overlay.style.cssText = `
        position: fixed; inset: 0; background: rgba(0,0,0,0.6);
        display: flex; align-items: center; justify-content: center;
        z-index: 900; flex-direction: column; gap: 16px;
    `;

    const text = document.createElement('div');
    text.style.cssText = 'font-size: 20px; color: #fff;';
    text.textContent = '对手已断线，等待重连中...';

    const countdown = document.createElement('div');
    countdown.id = 'disconnect-countdown';
    countdown.style.cssText = 'font-size: 48px; color: #ffd700; font-weight: bold;';
    countdown.textContent = timeoutSeconds;

    const hint = document.createElement('div');
    hint.style.cssText = 'font-size: 14px; color: #999;';
    hint.textContent = '超时后将自动判对方负';

    overlay.appendChild(text);
    overlay.appendChild(countdown);
    overlay.appendChild(hint);
    document.body.appendChild(overlay);

    let remaining = timeoutSeconds;
    if (disconnectOverlayTimer) clearInterval(disconnectOverlayTimer);
    disconnectOverlayTimer = setInterval(() => {
        remaining--;
        const el = document.getElementById('disconnect-countdown');
        if (el) el.textContent = remaining;
        if (remaining <= 0) {
            clearInterval(disconnectOverlayTimer);
            disconnectOverlayTimer = null;
            // 倒计时结束，隐藏覆盖层，等待服务器发送 game.over
            hideDisconnectOverlay();
            showNotification('对手重连超时，等待游戏结算...');
        }
    }, 1000);
}

function hideDisconnectOverlay() {
    const overlay = document.getElementById('disconnect-overlay');
    if (overlay) overlay.remove();
    if (disconnectOverlayTimer) {
        clearInterval(disconnectOverlayTimer);
        disconnectOverlayTimer = null;
    }
}

window.addEventListener('load', () => {
    if (!initFromUrl()) {
        return;
    }
    drawBoard();
    updateUI();
    setupBoardClick();
    setupActions();
    initWebSocket();
});

window.addEventListener('beforeunload', () => {
    shouldReconnect = false;
    if (wsClient) {
        wsClient.close();
    }
});
