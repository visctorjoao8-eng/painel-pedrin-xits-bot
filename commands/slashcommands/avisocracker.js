const { SlashCommandBuilder, EmbedBuilder } = require("discord.js");
const fs = require("fs");
const path = require("path");

function getBotConfig() {
  const configPath = path.join(__dirname, "../../database/botConfig.json");
  if (!fs.existsSync(configPath)) fs.writeFileSync(configPath, JSON.stringify({}, null, 2));
  return JSON.parse(fs.readFileSync(configPath, "utf-8"));
}

function saveBotConfig(data) {
  const configPath = path.join(__dirname, "../../database/botConfig.json");
  fs.writeFileSync(configPath, JSON.stringify(data, null, 2));
}

module.exports = {
  data: new SlashCommandBuilder()
    .setName("avisocracker")
    .setDescription("Gerenciar quem recebe DM quando alguém tentar crackear o painel")
    .addSubcommand(sub =>
      sub.setName("adicionar")
        .setDescription("Adicionar usuário para receber aviso de crack")
        .addUserOption(opt =>
          opt.setName("usuario").setDescription("Usuário que vai receber o aviso").setRequired(true)
        )
    )
    .addSubcommand(sub =>
      sub.setName("remover")
        .setDescription("Remover usuário da lista de avisos")
        .addUserOption(opt =>
          opt.setName("usuario").setDescription("Usuário a remover").setRequired(true)
        )
    )
    .addSubcommand(sub =>
      sub.setName("lista")
        .setDescription("Ver quem está na lista de avisos")
    ),

  async execute(interaction) {
    const OWNER_USERNAME = "geladopvp123_37711";
    if (interaction.user.username !== OWNER_USERNAME) {
      return interaction.reply({ content: "❌ Sem permissão.", ephemeral: true });
    }

    const botConfig = getBotConfig();
    if (!botConfig.avisosCracker) botConfig.avisosCracker = [];

    const sub = interaction.options.getSubcommand();

    if (sub === "adicionar") {
      const user = interaction.options.getUser("usuario");
      if (botConfig.avisosCracker.includes(user.id)) {
        return interaction.reply({ content: `⚠️ <@${user.id}> já está na lista.`, ephemeral: true });
      }
      botConfig.avisosCracker.push(user.id);
      saveBotConfig(botConfig);
      return interaction.reply({
        embeds: [new EmbedBuilder()
          .setTitle("✅ Usuário adicionado")
          .setDescription(`<@${user.id}> vai receber DM quando alguém tentar crackear o painel.`)
          .setColor(0x00c853)],
        ephemeral: true
      });
    }

    if (sub === "remover") {
      const user = interaction.options.getUser("usuario");
      const idx = botConfig.avisosCracker.indexOf(user.id);
      if (idx === -1) {
        return interaction.reply({ content: `⚠️ <@${user.id}> não está na lista.`, ephemeral: true });
      }
      botConfig.avisosCracker.splice(idx, 1);
      saveBotConfig(botConfig);
      return interaction.reply({
        embeds: [new EmbedBuilder()
          .setTitle("🗑️ Usuário removido")
          .setDescription(`<@${user.id}> não vai mais receber avisos.`)
          .setColor(0xff5252)],
        ephemeral: true
      });
    }

    if (sub === "lista") {
      const lista = botConfig.avisosCracker;
      const desc = lista.length === 0
        ? "Nenhum usuário na lista."
        : lista.map(id => `<@${id}>`).join("\n");
      return interaction.reply({
        embeds: [new EmbedBuilder()
          .setTitle("📋 Lista de avisos de crack")
          .setDescription(desc)
          .setColor(0x5865f2)],
        ephemeral: true
      });
    }
  }
};
