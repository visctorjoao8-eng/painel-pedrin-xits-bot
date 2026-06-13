# Deploy na VPS

1. Envie o `bot-vps.zip` para a VPS e extraia.
2. Rode `npm ci`.
3. Crie o arquivo `.env` com base no `.env.example`.
4. Preencha:
   - `DISCORD_TOKEN`
   - `CLIENT_ID`
   - `OWNER_ID`
   - `PORT`, se quiser usar outra porta
5. Inicie com `npm start`.

Para rodar em segundo plano com PM2:

```bash
npm install -g pm2
npm run pm2:start
pm2 save
```

Com `LOG_LEVEL=info`, o terminal mostra apenas a linha de bot online durante o funcionamento normal. Use `LOG_LEVEL=debug` somente para investigar problemas.
