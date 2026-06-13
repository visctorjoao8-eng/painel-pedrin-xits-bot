const { EmbedBuilder } = require("discord.js");

module.exports = {
  name: "clientReady",
  once: true,
  async execute(client) {
    if (client.user.username !== "BotGelado") {
      await client.user.setUsername("BotGelado").catch(() => {});
    }

    client.user.setPresence({
      activities: [{ name: `Hot Applications` }],
      status: "dnd",
    });
  },
};
