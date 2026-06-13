const fs = require("fs");
const path = require("path");

require("dotenv").config({ path: path.join(__dirname, ".env"), quiet: true });

const requiredEnv = ["DISCORD_TOKEN", "CLIENT_ID", "OWNER_ID"];
const hasAnyEnvConfig = requiredEnv.some((key) => process.env[key]);
const isProduction =
  process.env.NODE_ENV === "production" ||
  process.env.VPS === "true" ||
  Boolean(process.env.RENDER);

function loadFromEnv() {
  const missing = requiredEnv.filter((key) => !process.env[key]);
  if (missing.length > 0) {
    throw new Error(`Variaveis de ambiente ausentes: ${missing.join(", ")}`);
  }

  return {
    token: process.env.DISCORD_TOKEN,
    clientid: process.env.CLIENT_ID,
    ownerid: process.env.OWNER_ID,
  };
}

function loadFromFile() {
  const configPath = path.join(__dirname, "config.json");
  if (!fs.existsSync(configPath)) return null;
  return JSON.parse(fs.readFileSync(configPath, "utf-8"));
}

if (hasAnyEnvConfig) {
  module.exports = loadFromEnv();
} else if (!isProduction) {
  const localConfig = loadFromFile();
  if (localConfig) {
    module.exports = localConfig;
  } else {
    throw new Error("Defina DISCORD_TOKEN, CLIENT_ID e OWNER_ID ou crie config.json para uso local.");
  }
} else {
  throw new Error("Em VPS/producao, use DISCORD_TOKEN, CLIENT_ID e OWNER_ID nas variaveis de ambiente.");
}
