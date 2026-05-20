const wio = require("wio.db");
const { PermissionFlagsBits } = require("discord.js");

const botConfig = new wio.JsonDatabase({
  databasePath: "database/botConfig.json",
});

/**
 * Retorna true se o usuário tem permissão para usar comandos do bot.
 * Aceita: usersPerms, dono do bot, ou Administrador no servidor.
 */
function hasPermission(interaction) {
  const usersPerms = botConfig.get("usersPerms") || [];
  const isAdmin = interaction.member?.permissions?.has(PermissionFlagsBits.Administrator);
  return usersPerms.includes(interaction.user.id) || isAdmin;
}

module.exports = { hasPermission };
