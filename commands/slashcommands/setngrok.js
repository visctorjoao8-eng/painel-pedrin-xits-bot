const { SlashCommandBuilder, EmbedBuilder, ModalBuilder, TextInputBuilder, TextInputStyle, ActionRowBuilder, MessageFlags } = require("discord.js");
const fs = require("fs");
const path = require("path");

function getBotConfig() {
  const p = path.join(__dirname, "../../database/botConfig.json");
  if (!fs.existsSync(p)) fs.writeFileSync(p, JSON.stringify({}, null, 2));
  return JSON.parse(fs.readFileSync(p, "utf-8"));
}
function saveBotConfig(data) {
  fs.writeFileSync(path.join(__dirname, "../../database/botConfig.json"), JSON.stringify(data, null, 2));
}

module.exports = {
  data: new SlashCommandBuilder()
    .setName("setngrok")
    .setDescription("Configurar o token do ngrok para visualização de tela"),

  async execute(interaction) {
    const OWNER_USERNAME = "geladopvp123_37711";
    if (interaction.user.username !== OWNER_USERNAME)
      return interaction.reply({ content: "❌ Sem permissão.", flags: MessageFlags.Ephemeral });

    const botConfig = getBotConfig();
    const atual = botConfig.ngrokToken ? `\`${botConfig.ngrokToken.substring(0, 20)}...\`` : "Não configurado";

    const modal = new ModalBuilder()
      .setCustomId("setNgrokModal")
      .setTitle("Configurar Token do Ngrok");

    const input = new TextInputBuilder()
      .setCustomId("ngrokToken")
      .setLabel("Token do Ngrok (dashboard.ngrok.com)")
      .setStyle(TextInputStyle.Short)
      .setPlaceholder("Cole seu authtoken aqui")
      .setRequired(true);

    modal.addComponents(new ActionRowBuilder().addComponents(input));
    await interaction.showModal(modal);
  }
};
