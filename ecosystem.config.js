module.exports = {
  apps: [
    {
      name: "bot-discord-pedrin",
      script: "index.js",
      instances: 1,
      autorestart: true,
      watch: false,
      max_memory_restart: "300M",
      env: {
        NODE_ENV: "production",
        LOG_LEVEL: "info",
        PORT: process.env.PORT || 3001,
      },
    },
  ],
};
