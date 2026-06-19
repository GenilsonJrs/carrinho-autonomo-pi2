'use strict';

// Limites do protocolo (conforme setpoint_logic.pdf)
const MAX_PATHS = 8;
const MAX_ACOES = 16; // 4 bits = 0-15
const MAX_VALOR = 63; // 6 bits = 0-63

const estado = {
  operacao:     0,          // 0 = append, 1 = restart
  agendamento:  'immediate', // immediate, end_action, end_path, end_route
  paths:        [],
  uid:          1,
  acaoEditando: null
};

// --- Utilitários ---

function calcularValorReal(tipo, valor) {
  if (tipo === 'mover') return `${(valor * 0.5).toFixed(1)} m`;
  if (tipo === 'girar') return `${valor * 6}°`;
  return valor;
}

function opcoesDirecao(tipo) {
  if (tipo === 'mover') return [['forward', 'Frente'], ['backward', 'Ré']];
  if (tipo === 'girar') return [['clockwise', 'Horário'], ['anticlockwise', 'Anti-horário']];
  return [];
}

function direcaoPadrao(tipo) {
  if (tipo === 'mover') return 'forward';
  if (tipo === 'girar') return 'clockwise';
  return 'nenhuma';
}

function direcaoBit(direcao) {
  return (direcao === 'forward' || direcao === 'clockwise') ? 1 : 0;
}

function escaparHtml(str) {
  return String(str).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;');
}

function notificar(mensagem, tipo = 'info') {
  const el = document.getElementById('notificacao');
  el.textContent = mensagem;
  el.className = `visivel ${tipo}`;
  clearTimeout(notificar._timer);
  notificar._timer = setTimeout(() => { el.className = ''; }, 3000);
}

// --- Ações ---

function definirConfigEnvio(modo) {
  // Resetar classes ativas
  document.querySelectorAll('.sched-grid .btn-build').forEach(btn => btn.classList.remove('active'));
  document.getElementById(`mode-${modo}`).classList.add('active');

  const helpText = {
    'restart': 'O robô apagará toda a memória e iniciará esta nova rota imediatamente.',
    'append': 'As novas ações serão coladas ao final da lista atual e processadas agora.',
    'end_path': 'As ações serão adicionadas à fila, mas o robô só as executará após terminar o trecho atual.',
    'end_route': 'As ações entrarão na fila de espera e só começarão após a conclusão de toda a missão atual.'
  };

  // Mapeamento para o protocolo
  estado.operacao = (modo === 'restart') ? 1 : 0;
  estado.agendamento = (modo === 'restart' || modo === 'append') ? 'immediate' : modo;

  const helpBox = document.getElementById('envio-help');
  if (helpBox) helpBox.textContent = helpText[modo] || '';

  atualizarVisualizacao();
}

function adicionarAcaoRapida(tipo) {
  // Se não houver path, cria o primeiro
  if (estado.paths.length === 0) {
    adicionarPath();
  }
  
  const path = estado.paths.at(-1);
  if (path.acoes.length >= MAX_ACOES) {
    notificar(`Limite de ${MAX_ACOES} ações atingido neste trecho.`, 'erro');
    return;
  }

  const acao = {
    id:      estado.uid++,
    tipo:    tipo,
    valor:   1, // valor padrão
    direcao: direcaoPadrao(tipo)
  };
  
  path.acoes.push(acao);
  renderizar();
  atualizarVisualizacao();
  notificar(`${tipo.charAt(0).toUpperCase() + tipo.slice(1)} adicionado.`, 'sucesso');
}

function adicionarPathManual() {
  if (estado.paths.length >= MAX_PATHS) {
    notificar(`Limite de ${MAX_PATHS} trechos atingido.`, 'erro');
    return;
  }
  adicionarPath();
  renderizar();
  atualizarVisualizacao();
  notificar('Novo trecho criado.', 'sucesso');
}

function adicionarPath() {
  const path = {
    id:     estado.uid++,
    nome:   `Trecho ${estado.paths.length + 1}`,
    aberto: true,
    acoes:  []
  };
  estado.paths.push(path);
  return path;
}

function resetarRota() {
  if (estado.paths.length > 0 && !confirm('Limpar toda a rota?')) return;
  estado.paths = [];
  estado.uid = 1;
  estado.acaoEditando = null;
  renderizar();
  atualizarVisualizacao();
  desenharRota();
  notificar('Rota limpa.', 'info');
}

// --- Renderização ---

function renderizar() {
  const container = document.getElementById('listaPaths');
  
  if (estado.paths.length === 0) {
    container.innerHTML = `
      <div style="text-align: center; padding: 40px; color: var(--text-muted);">
        <p>A rota está vazia.<br>Use os botões acima para adicionar movimentos.</p>
      </div>`;
    return;
  }

  container.innerHTML = estado.paths.map((path, idx) => `
    <div class="path-group" style="margin-bottom: 24px;">
      <h3 style="font-size: 14px; margin-bottom: 12px; color: var(--text-muted);">${path.nome}</h3>
      <div class="acoes-list" style="display: flex; flex-direction: column; gap: 8px;">
        ${path.acoes.map((acao, i) => _htmlAcao(path.id, acao, i)).join('')}
      </div>
    </div>
  `).join('');
}

function _htmlAcao(pathId, acao, indice) {
  const corAcao = acao.tipo === 'mover' ? '#3b82f6' : '#8b5cf6';
  const label = acao.tipo === 'girar' ? 'Giro' : 'Movimento';

  const valorNatural = acao.tipo === 'mover' ? (acao.valor * 0.5) : (acao.valor * 6);
  const unidade      = acao.tipo === 'mover' ? 'm' : '°';
  const stepNatural  = acao.tipo === 'mover' ? 0.5 : 6;
  const maxNatural   = acao.tipo === 'mover' ? (MAX_VALOR * 0.5) : (MAX_VALOR * 6);

  const opcoes = opcoesDirecao(acao.tipo);
  const htmlDirecao = `
    <div class="direcao-selector" style="display: flex; gap: 4px; background: #f3f4f6; padding: 2px; border-radius: 6px;">
      ${opcoes.map(([val, txt]) => `
        <button onclick="atualizarDirecaoAcao(${pathId}, ${acao.id}, '${val}')"
          style="padding: 2px 8px; font-size: 11px; border: none; border-radius: 4px; cursor: pointer; transition: all 0.2s;
          ${acao.direcao === val ? 'background: white; box-shadow: 0 1px 2px rgba(0,0,0,0.1); font-weight: 700;' : 'background: transparent; color: #6b7280;'}">
          ${txt}
        </button>
      `).join('')}
    </div>
  `;

  return `
    <div class="acao-item" draggable="true"
      data-path-id="${pathId}" data-acao-id="${acao.id}"
      style="border: 1px solid var(--border); border-radius: 12px; padding: 12px 16px; background: white; display: flex; align-items: center; gap: 12px; box-shadow: var(--shadow-sm);">
      <span class="drag-handle">☰</span>
      <span style="font-weight: 700; color: var(--text-muted); font-size: 12px;">#${indice + 1}</span>
      <div style="width: 10px; height: 10px; border-radius: 50%; background: ${corAcao};"></div>
      <div style="flex: 1; display: flex; align-items: center; gap: 16px;">
        <span style="font-weight: 700; min-width: 80px;">${label}</span>

        <div style="display: flex; align-items: center; gap: 6px;">
          <input type="number" value="${valorNatural}" min="0" max="${maxNatural}" step="${stepNatural}"
            style="width: 72px; border: 1.5px solid var(--border); border-radius: 6px; padding: 4px 8px; text-align: center; font-size: 14px; font-weight: 600; font-family: inherit;"
            oninput="atualizarValorAcao(${pathId}, ${acao.id}, this.value, '${acao.tipo}')">
          <span style="font-size: 13px; color: var(--text-muted); font-weight: 600;">${unidade}</span>
        </div>

        ${htmlDirecao}
      </div>
      <button class="btn-danger" style="padding: 4px 8px; font-size: 10px;" onclick="removerAcao(${pathId}, ${acao.id})">Remover</button>
    </div>
  `;
}

function atualizarValorAcao(pathId, acaoId, valorNatural, tipo) {
  const path = estado.paths.find(p => p.id === pathId);
  if (!path) return;
  const acao = path.acoes.find(a => a.id === acaoId);
  if (acao) {
    const passos = tipo === 'mover'
      ? Math.round(Number.parseFloat(valorNatural) / 0.5)
      : Math.round(Number.parseFloat(valorNatural) / 6);
    acao.valor = Math.max(0, Math.min(MAX_VALOR, passos || 0));
    atualizarVisualizacao();
    desenharRota();
    // sem renderizar() — evita recriar o input e perder o foco
  }
}

function atualizarDirecaoAcao(pathId, acaoId, direcao) {
  const path = estado.paths.find(p => p.id === pathId);
  if (!path) return;
  const acao = path.acoes.find(a => a.id === acaoId);
  if (acao) {
    acao.direcao = direcao;
    atualizarVisualizacao();
    desenharRota();
    renderizar();
  }
}

function removerAcao(pathId, acaoId) {
  const path = estado.paths.find(p => p.id === pathId);
  if (!path) return;
  path.acoes = path.acoes.filter(a => a.id !== acaoId);
  renderizar();
  atualizarVisualizacao();
  desenharRota();
}

function atualizarVisualizacao() {
  const json = construirJSON();
  document.getElementById('previaJson').textContent = JSON.stringify(json, null, 2);
  
  const elPilha = document.getElementById('listaPilha');
  const pilha = construirPilha();
  
  if (pilha.length === 0) {
    elPilha.innerHTML = '<p style="font-size: 12px; color: var(--text-muted); text-align: center;">Nenhuma ação na pilha</p>';
  } else {
    elPilha.innerHTML = pilha.map(e => `
      <div style="font-size: 11px; padding: 8px 12px; border-bottom: 1px solid #f0eee9; display: flex; justify-content: space-between;">
        <span style="font-family: monospace; font-weight: 700; color: var(--accent);">${e.frame_hex}</span>
        <span style="color: var(--text-muted);">${e.tipo}</span>
      </div>
    `).join('');
  }
  
  setupDragAndDrop();
}

function construirPilha() {
  const entradas = [];
  estado.paths.forEach((path, ip) => {
    path.acoes.forEach((acao, ia) => {
      // Byte 1: [ op(1) | path(3) | action(4) ]
      const byte1 = ((estado.operacao & 0x01) << 7) | ((ip & 0x07) << 4) | (ia & 0x0F);
      
      // Byte 2: [ type(1) | value(6) | dir(1) ]
      // Type: Linear = 1, Angular = 0
      const tipoBit = acao.tipo === 'mover' ? 1 : 0;

      const byte2 = ((tipoBit & 0x01) << 7) | ((acao.valor & 0x3F) << 1) | direcaoBit(acao.direcao);
      
      const frame = (byte1 << 8) | byte2;

      entradas.push({
        tipo: acao.tipo,
        frame_hex: '0x' + frame.toString(16).toUpperCase().padStart(4, '0')
      });
    });
  });
  return entradas;
}

function construirJSON() {
  return {
    operacao: estado.operacao === 0 ? 'append' : 'restart',
    agendamento: estado.agendamento,
    total_acoes: estado.paths.reduce((s, p) => s + p.acoes.length, 0),
    rota: estado.paths.map(p => ({
      trecho: p.nome,
      acoes: p.acoes.map(a => ({
        tipo: a.tipo,
        valor: a.valor,
        unidade: a.tipo === 'mover' ? 'metros' : 'graus',
        valor_real: calcularValorReal(a.tipo, a.valor),
        direcao: a.direcao
      }))
    }))
  };
}

function baixarJSON() {
  const blob = new Blob([JSON.stringify(construirJSON(), null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = 'route.json';
  link.click();
}

async function enviarRota() {
  if (estado.paths.reduce((s, p) => s + p.acoes.length, 0) === 0) {
    notificar('Adicione ações à rota antes de enviar.', 'erro');
    return;
  }
  const rota = [];
  estado.paths.forEach(p => p.acoes.forEach(a => {
    const valor = a.tipo === 'mover' ? Math.round(a.valor * 50) : (a.valor * 6);
    rota.push({ tipo: a.tipo, valor, direcao: a.direcao });
  }));
  const payload = { operacao: estado.operacao === 1 ? 'restart' : 'append', rota };
  notificar('Enviando rota para o controlador...', 'info');
  try {
    const r = await fetch(apiBase() + '/api/route', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const d = await r.json();
    notificar('Rota enviada — ' + (d.enfileirados ?? 0) + ' ações na fila.', 'sucesso');
  } catch (e) {
    notificar('Erro ao enviar: ' + e.message, 'erro');
  }
}

async function carregarJSON(event) {
  const arquivo = event.target.files[0];
  if (!arquivo) return;

  try {
    const texto = await arquivo.text();
    const dados = JSON.parse(texto);
    if (!dados.rota || !Array.isArray(dados.rota)) {
      throw new Error('Formato de arquivo inválido.');
    }

    estado.paths = [];
    estado.operacao = dados.operacao === 'restart' ? 1 : 0;
    estado.uid = 1;

    dados.rota.forEach((trecho, idx) => {
      const path = {
        id: estado.uid++,
        nome: trecho.trecho || `Trecho ${idx + 1}`,
        aberto: true,
        acoes: (trecho.acoes || []).map(a => ({
          id: estado.uid++,
          tipo: a.tipo,
          valor: a.valor,
          direcao: a.direcao || direcaoPadrao(a.tipo)
        }))
      };
      estado.paths.push(path);
    });

    renderizar();
    atualizarVisualizacao();
    desenharRota();
    notificar('Rota carregada com sucesso!', 'sucesso');
    event.target.value = '';
  } catch (err) {
    notificar('Erro ao carregar arquivo: ' + err.message, 'erro');
  }
}

// --- Controle Manual e Telemetria (Mock) ---

let caracteristica_bt = null;

function apiBase() {
  if (location.protocol !== 'file:') return '';
  const el = document.getElementById('ipMestre');
  const ip = (el && el.value.trim()) || '192.168.1.102';
  return 'http://' + ip;
}

async function conectarBluetooth() {
  if (!navigator.bluetooth) {
    notificar('Web Bluetooth indisponivel aqui. Abra esta pagina como arquivo local (file://) no Chrome/Edge.', 'erro');
    return;
  }
  try {
    const dispositivo = await navigator.bluetooth.requestDevice({
      filters: [{ name: 'ROBO_BB8' }],
      optionalServices: [0x00FF]
    });
    dispositivo.addEventListener('gattserverdisconnected', () => {
      caracteristica_bt = null;
      const b = document.getElementById('btnBt');
      if (b) { b.textContent = '🔵 Conectar Bluetooth'; b.style.background = ''; b.style.color = ''; }
      notificar('Bluetooth desconectado.', 'info');
    });
    const servidor = await dispositivo.gatt.connect();
    const servico = await servidor.getPrimaryService(0x00FF);
    caracteristica_bt = await servico.getCharacteristic(0xFF01);
    const b = document.getElementById('btnBt');
    if (b) { b.textContent = '🔵 Conectado'; b.style.background = 'var(--success)'; b.style.color = '#fff'; }
    notificar('Bluetooth conectado a ROBO_BB8.', 'sucesso');
  } catch (e) {
    notificar('Falha no Bluetooth: ' + e.message, 'erro');
  }
}

function comandoManual(direcao) {
  const mapa = { frente: 'F', re: 'B', esquerda: 'L', direita: 'R', parar: 'S' };
  const ch = mapa[direcao] || 'S';
  if (!caracteristica_bt) {
    if (direcao !== 'parar') notificar('Conecte o Bluetooth primeiro.', 'erro');
    return;
  }
  caracteristica_bt.writeValue(new TextEncoder().encode(ch)).catch(e => console.log(e));
}

async function paradaEmergenciaGlobal() {
  notificar('PARADA DE EMERGÊNCIA ATIVADA!', 'erro');
  try {
    await fetch(apiBase() + '/api/emergency', { method: 'POST' });
  } catch (e) {
    console.log(e);
  }
}

function iniciarTelemetriaMock() {
  setInterval(() => {
    const bateria = (85 + Math.random() * 5).toFixed(1);
    const yaw = (Math.random() * 360).toFixed(1);
    const encEsq = Math.floor(Math.random() * 5000);
    const encDir = Math.floor(Math.random() * 5000);

    const elBat = document.getElementById('telBateria');
    const elYaw = document.getElementById('telYaw');
    const elEsq = document.getElementById('telEncEsq');
    const elDir = document.getElementById('telEncDir');

    if (elBat) elBat.textContent = `${bateria}%`;
    if (elYaw) elYaw.textContent = `${yaw}°`;
    if (elEsq) elEsq.textContent = encEsq;
    if (elDir) elDir.textContent = encDir;
  }, 2000);
}

// --- Visualização Canvas ---

let canvasOffset = { x: 0, y: 0 };
let isDraggingCanvas = false;
let lastMousePos = { x: 0, y: 0 };

function setupCanvasInteractivity() {
  const canvas = document.getElementById('canvasRota');
  if (!canvas) return;

  const startDrag = (x, y) => {
    isDraggingCanvas = true;
    lastMousePos = { x, y };
  };

  const moveDrag = (x, y) => {
    if (!isDraggingCanvas) return;
    const dx = x - lastMousePos.x;
    const dy = y - lastMousePos.y;
    canvasOffset.x += dx;
    canvasOffset.y += dy;
    lastMousePos = { x, y };
    desenharRota();
  };

  const endDrag = () => {
    isDraggingCanvas = false;
  };

  canvas.addEventListener('mousedown', e => startDrag(e.clientX, e.clientY));
  globalThis.addEventListener('mousemove', e => moveDrag(e.clientX, e.clientY));
  globalThis.addEventListener('mouseup', endDrag);

  canvas.addEventListener('touchstart', e => {
    const touch = e.touches[0];
    startDrag(touch.clientX, touch.clientY);
  }, { passive: true });

  globalThis.addEventListener('touchmove', e => {
    const touch = e.touches[0];
    moveDrag(touch.clientX, touch.clientY);
  }, { passive: false });

  globalThis.addEventListener('touchend', endDrag);
}

function desenharRota() {
  const canvas = document.getElementById('canvasRota');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  
  canvas.width = canvas.offsetWidth;
  canvas.height = canvas.offsetHeight;
  
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  
  let x = canvas.width / 2 + canvasOffset.x;
  let y = canvas.height / 2 + canvasOffset.y;
  let angulo = -Math.PI / 2; 

  ctx.beginPath();
  ctx.lineWidth = 3;
  ctx.strokeStyle = '#bc9f77';
  ctx.lineJoin = 'round';
  ctx.lineCap = 'round';
  ctx.moveTo(x, y);

  // Desenhar ponto de partida
  ctx.save();
  ctx.fillStyle = '#4f772d';
  ctx.beginPath();
  ctx.arc(x, y, 6, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();

  let distTotalMetros = 0;

  estado.paths.forEach(path => {
    path.acoes.forEach(acao => {
      if (acao.tipo === 'mover') {
        const distPixels = acao.valor * 5; 
        const metros = acao.valor * 0.5;
        distTotalMetros += metros;

        const dir = acao.direcao === 'backward' ? -1 : 1;
        x += Math.cos(angulo) * distPixels * dir;
        y += Math.sin(angulo) * distPixels * dir;
        ctx.lineTo(x, y);
      } else if (acao.tipo === 'girar') {
        const giro = (acao.valor * 6) * (Math.PI / 180);
        angulo += (acao.direcao === 'anticlockwise' ? -giro : giro);
      }
    });
  });

  ctx.stroke();

  // Desenhar seta na ponta final
  ctx.save();
  ctx.translate(x, y);
  ctx.rotate(angulo);
  ctx.beginPath();
  ctx.moveTo(0, 0);
  ctx.lineTo(-12, -6);
  ctx.lineTo(-12, 6);
  ctx.closePath();
  ctx.fillStyle = '#ae2012';
  ctx.fill();
  ctx.restore();

  // Atualizar contador de distância
  const elDist = document.getElementById('distanciaTotal');
  if (elDist) elDist.textContent = `Total: ${distTotalMetros.toFixed(1)} m`;

  // Desenhar robô animado sobre a rota
  if (sim.ativo) {
    const rx = canvas.width / 2 + canvasOffset.x + sim.x;
    const ry = canvas.height / 2 + canvasOffset.y + sim.y;
    _desenharRobo(ctx, rx, ry, sim.angulo);
  }
}

function _desenharRobo(ctx, x, y, angulo) {
  ctx.save();
  ctx.translate(x, y);

  // Sombra
  ctx.shadowColor = 'rgba(0,0,0,0.25)';
  ctx.shadowBlur = 6;

  // Corpo
  ctx.beginPath();
  ctx.arc(0, 0, 9, 0, Math.PI * 2);
  ctx.fillStyle = '#5e503f';
  ctx.fill();
  ctx.strokeStyle = 'white';
  ctx.lineWidth = 2.5;
  ctx.stroke();

  ctx.shadowBlur = 0;

  // Seta de direção
  ctx.rotate(angulo);
  ctx.beginPath();
  ctx.moveTo(4, 0);
  ctx.lineTo(14, 0);
  ctx.strokeStyle = 'white';
  ctx.lineWidth = 2.5;
  ctx.lineCap = 'round';
  ctx.stroke();

  ctx.restore();
}

function centralizarCanvas() {
  canvasOffset = { x: 0, y: 0 };
  desenharRota();
}

function limparCanvas() {
  const canvas = document.getElementById('canvasRota');
  const ctx = canvas.getContext('2d');
  ctx.clearRect(0, 0, canvas.width, canvas.height);
}

// --- Simulação de Rota ---

const sim = {
  ativo: false,
  pausado: false,
  velocidade: 1,
  segmentos: [],
  segAtual: 0,
  progresso: 0,
  frameId: null,
  ultimoTempo: null,
  x: 0,
  y: 0,
  angulo: -Math.PI / 2,
};

function _calcularSegmentos() {
  let x = 0, y = 0, angulo = -Math.PI / 2;
  const segs = [];

  estado.paths.forEach(path => {
    path.acoes.forEach(acao => {
      if (acao.tipo === 'mover') {
        const px = acao.valor * 5;
        const dir = acao.direcao === 'backward' ? -1 : 1;
        const endX = x + Math.cos(angulo) * px * dir;
        const endY = y + Math.sin(angulo) * px * dir;
        segs.push({ tipo: 'mover', startX: x, startY: y, endX, endY, angulo,
          label: `Movimento ${(acao.valor * 0.5).toFixed(1)} m ${acao.direcao === 'backward' ? 'ré' : 'frente'}` });
        x = endX;
        y = endY;
      } else if (acao.tipo === 'girar') {
        const delta = (acao.valor * 6) * (Math.PI / 180) * (acao.direcao === 'anticlockwise' ? -1 : 1);
        segs.push({ tipo: 'girar', x, y, startAngulo: angulo, endAngulo: angulo + delta,
          label: `Giro ${acao.valor * 6}° ${acao.direcao === 'anticlockwise' ? 'anti-horário' : 'horário'}` });
        angulo += delta;
      }
    });
  });

  return segs;
}

function _duracaoSegmento(seg) {
  if (seg.tipo === 'mover') {
    const dist = Math.hypot(seg.endX - seg.startX, seg.endY - seg.startY);
    return Math.max(dist / (60 * sim.velocidade), 0.05);
  }
  return Math.max(Math.abs(seg.endAngulo - seg.startAngulo) / (Math.PI * sim.velocidade), 0.05);
}

function _tickSimulacao(timestamp) {
  if (!sim.ativo || sim.pausado) return;

  if (!sim.ultimoTempo) sim.ultimoTempo = timestamp;
  const delta = (timestamp - sim.ultimoTempo) / 1000;
  sim.ultimoTempo = timestamp;

  const seg = sim.segmentos[sim.segAtual];
  if (!seg) { _finalizarSimulacao(); return; }

  sim.progresso += delta / _duracaoSegmento(seg);

  if (sim.progresso >= 1) {
    sim.progresso = 0;
    // Fixar posição exata no fim do segmento
    if (seg.tipo === 'mover') { sim.x = seg.endX; sim.y = seg.endY; }
    else { sim.angulo = seg.endAngulo; }
    sim.segAtual++;
    if (sim.segAtual >= sim.segmentos.length) { _finalizarSimulacao(); return; }
  } else {
    const t = sim.progresso;
    if (seg.tipo === 'mover') {
      sim.x = seg.startX + (seg.endX - seg.startX) * t;
      sim.y = seg.startY + (seg.endY - seg.startY) * t;
      sim.angulo = seg.angulo;
    } else {
      sim.x = seg.x;
      sim.y = seg.y;
      sim.angulo = seg.startAngulo + (seg.endAngulo - seg.startAngulo) * t;
    }
  }

  const total = sim.segmentos.length;
  const elStatus = document.getElementById('simStatus');
  if (elStatus) elStatus.textContent = `Ação ${sim.segAtual + 1}/${total} — ${seg.label}`;

  desenharRota();
  sim.frameId = requestAnimationFrame(_tickSimulacao);
}

function _mostrarControlesSim(rodando) {
  document.getElementById('btnSimular').style.display = rodando ? 'none' : '';
  document.getElementById('simRunning').style.display = rodando ? '' : 'none';
}

function _finalizarSimulacao() {
  sim.ativo = false;
  sim.pausado = false;
  cancelAnimationFrame(sim.frameId);
  document.getElementById('simStatus').textContent = 'Simulação concluída.';
  _mostrarControlesSim(false);
  desenharRota();
}

function iniciarSimulacao() {
  const totalAcoes = estado.paths.reduce((s, p) => s + p.acoes.length, 0);
  if (totalAcoes === 0) { notificar('Adicione ações à rota antes de simular.', 'erro'); return; }

  sim.segmentos = _calcularSegmentos();
  sim.segAtual = 0;
  sim.progresso = 0;
  sim.x = 0;
  sim.y = 0;
  sim.angulo = -Math.PI / 2;
  sim.ativo = true;
  sim.pausado = false;
  sim.ultimoTempo = null;
  sim.velocidade = 1;

  document.getElementById('btnVelocidade').textContent = '2×';
  document.getElementById('btnPausar').textContent = '❚❚ Pausar';
  document.getElementById('simStatus').textContent = '';
  _mostrarControlesSim(true);

  sim.frameId = requestAnimationFrame(_tickSimulacao);
}

function pausarSimulacao() {
  const btn = document.getElementById('btnPausar');
  if (sim.pausado) {
    sim.pausado = false;
    sim.ultimoTempo = null;
    btn.textContent = '❚❚ Pausar';
    sim.frameId = requestAnimationFrame(_tickSimulacao);
  } else {
    sim.pausado = true;
    cancelAnimationFrame(sim.frameId);
    btn.textContent = '▶ Continuar';
  }
}

function reiniciarSimulacao() {
  cancelAnimationFrame(sim.frameId);
  sim.ativo = false;
  sim.pausado = false;
  document.getElementById('simStatus').textContent = '';
  _mostrarControlesSim(false);
  desenharRota();
}

function alternarVelocidade() {
  const btn = document.getElementById('btnVelocidade');
  if (sim.velocidade === 1) {
    sim.velocidade = 2;
    btn.textContent = '4×';
  } else if (sim.velocidade === 2) {
    sim.velocidade = 4;
    btn.textContent = '1×';
  } else {
    sim.velocidade = 1;
    btn.textContent = '2×';
  }
}

// --- Drag and Drop ---

let dragSource = null;

function setupDragAndDrop() {
  const items = document.querySelectorAll('.acao-item');
  items.forEach(item => {
    item.addEventListener('dragstart', handleDragStart);
    item.addEventListener('dragover', handleDragOver);
    item.addEventListener('drop', handleDrop);
    item.addEventListener('dragend', handleDragEnd);
  });
}

function handleDragStart(e) {
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'BUTTON') {
    e.preventDefault();
    return;
  }
  dragSource = e.currentTarget;
  e.currentTarget.classList.add('dragging');
}

function handleDragOver(e) {
  e.preventDefault();
  return false;
}

function handleDrop(e) {
  e.stopPropagation();
  const target = e.currentTarget;
  if (dragSource !== target) {
    const fromPathId = Number.parseInt(dragSource.dataset.pathId);
    const fromAcaoId = Number.parseInt(dragSource.dataset.acaoId);
    const toPathId = Number.parseInt(target.dataset.pathId);
    const toAcaoId = Number.parseInt(target.dataset.acaoId);

    reordenarAcoes(fromPathId, fromAcaoId, toPathId, toAcaoId);
  }
  return false;
}

function handleDragEnd(e) {
  e.currentTarget.classList.remove('dragging');
}

function reordenarAcoes(fromPathId, fromAcaoId, toPathId, toAcaoId) {
  const fromPath = estado.paths.find(p => p.id === fromPathId);
  const toPath = estado.paths.find(p => p.id === toPathId);
  
  const fromIndex = fromPath.acoes.findIndex(a => a.id === fromAcaoId);
  const toIndex = toPath.acoes.findIndex(a => a.id === toAcaoId);
  
  const [acao] = fromPath.acoes.splice(fromIndex, 1);
  toPath.acoes.splice(toIndex, 0, acao);
  
  renderizar();
  atualizarVisualizacao();
  desenharRota();
}

// Inicialização
window.addEventListener('load', () => {
  renderizar();
  atualizarVisualizacao();
  desenharRota();
  setupCanvasInteractivity();
  iniciarTelemetriaMock();
});
