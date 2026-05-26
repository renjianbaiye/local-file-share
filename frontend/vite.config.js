import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

const uploadTimeoutPlugin = {
  name: 'local-file-share-upload-timeouts',
  configureServer(server) {
    if (!server.httpServer) return
    server.httpServer.requestTimeout = 0
    server.httpServer.headersTimeout = 0
    server.httpServer.keepAliveTimeout = 0
  },
}

const backendProxy = {
  target: 'http://127.0.0.1:8080',
  timeout: 0,
  proxyTimeout: 0,
}

export default defineConfig({
  plugins: [vue(), uploadTimeoutPlugin],
  server: {
    proxy: {
      '/api': backendProxy,
      '/download': backendProxy,
      '/qr': backendProxy,
    },
  },
})
