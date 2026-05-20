const {
  SlashCommandBuilder,
  PermissionFlagsBits,
  ActionRowBuilder,
  ChannelSelectMenuBuilder,
  ChannelType,
  ModalBuilder,
  TextInputBuilder,
  TextInputStyle,
  EmbedBuilder,
  ButtonBuilder,
  ButtonStyle,
} = require(`discord.js`);

const wio = require("wio.db");

module.exports = {
  data: new SlashCommandBuilder()
    .setName(`botconfig`)
    .setDescription(`⚒ [Gerencie seu bot].`)
    .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),

  async execute(interaction, client) {
    const botConfig = new wio.JsonDatabase({
      databasePath: `database/botConfig.json`,
    });

    // Aceita: dono do bot, usersPerms, ou qualquer admin do servidor
    const usersPerms = botConfig.get("usersPerms") || [];
    const isAdmin = interaction.member.permissions.has(PermissionFlagsBits.Administrator);

    if (!usersPerms.includes(interaction.user.id) && !isAdmin) {
      return interaction.reply({
        content: `**❌ | Você não tem permissão.**`,
        ephemeral: true,
      });
    }

    const embed = new EmbedBuilder()
      .setTitle(`${client.user.username} | Gerenciar Ticket`)
      .setDescription("**_Selecione abaixo a opção que deseja configurar_**");

    interaction.reply({
      embeds: [embed],
      components: [
        new ActionRowBuilder().addComponents(
          new ButtonBuilder()
            .setCustomId("configLogs")
            .setEmoji("📰")
            .setLabel("Configurar LOGS")
            .setStyle(ButtonStyle.Secondary),
          new ButtonBuilder()
            .setCustomId("configAntiCrack")
            .setEmoji("🔒")
            .setLabel("Logs Anti-Crack")
            .setStyle(ButtonStyle.Danger),
          new ButtonBuilder()
            .setCustomId("configLogsPainel")
            .setEmoji("🖥️")
            .setLabel("Logs Painel Iniciado")
            .setStyle(ButtonStyle.Primary),
          new ButtonBuilder()
            .setCustomId("configAvaliacao")
            .setEmoji("⭐")
            .setLabel("Configurar AVALIAÇÃO")
            .setStyle(ButtonStyle.Secondary)
        ),
        new ActionRowBuilder().addComponents(
          new ButtonBuilder()
            .setCustomId("configMp")
            .setEmoji("💰")
            .setLabel("Configurar TOKEN MP")
            .setStyle(ButtonStyle.Secondary),
          new ButtonBuilder()
            .setCustomId("configCategorias")
            .setEmoji("📒")
            .setLabel("Configurar CATEGORIAS")
            .setStyle(ButtonStyle.Secondary)
        ),
      ],
      ephemeral: true,
    });
  },
};
