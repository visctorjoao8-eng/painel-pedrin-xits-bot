const { EmbedBuilder } = require("discord.js");

module.exports = {
  name: "ready",
  once: true,
  async execute(client) {
    console.log(`Successfully logged in to ${client.user.tag}`.green);

    await client.user.setUsername("BotGelado").catch(() => {});

    client.user.setPresence({
      activities: [{ name: `Hot Applications` }],
      status: "dnd",
    });

    setInterval(() => console.clear(), 10000);
  },
};
