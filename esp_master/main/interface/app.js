'use strict';

const rota = [];

function addPasso() {
    const tipo = document.getElementById('tipo').value;
    const valor = parseInt(document.getElementById('valor').value) || 0;
    const direcao = document.getElementById('direcao').value;
    rota.push({ tipo, valor, direcao });
    render();
}

function render() {
    const ol = document.getElementById('lista');
    ol.innerHTML = rota.map(p =>
        `<li>${p.tipo === 'girar' ? 'Girar' : 'Mover'} ${p.valor}${p.tipo === 'girar' ? '°' : ' cm'} (${p.direcao})</li>`
    ).join('');
}

function limpar() {
    rota.length = 0;
    render();
}

async function enviar(operacao) {
    if (rota.length === 0) { msg('Adicione passos antes de enviar.'); return; }
    try {
        const r = await fetch('/api/route', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ operacao, rota })
        });
        const d = await r.json();
        msg('Enviado: ' + (d.enfileirados || 0) + ' passos. Fila: ' + (d.fila || 0));
    } catch (e) {
        msg('Erro ao enviar: ' + e.message);
    }
}

async function emergencia() {
    try {
        await fetch('/api/emergency', { method: 'POST' });
        msg('EMERGÊNCIA enviada — fila limpa.');
    } catch (e) {
        msg('Erro: ' + e.message);
    }
}

function msg(t) {
    document.getElementById('msg').textContent = t;
}

async function status() {
    try {
        const r = await fetch('/api/status', { cache: 'no-store' });
        const d = await r.json();
        document.getElementById('status').textContent = 'Fila: ' + (d.fila ?? 0);
    } catch (e) {}
}

setInterval(status, 1500);
window.addEventListener('load', () => { render(); status(); });
