import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig({
  plugins: [vue()],
  server: {
    proxy: {
      '/api': 'http://127.0.0.1:8080',
      '/download': 'http://127.0.0.1:8080',
      '/qr': 'http://127.0.0.1:8080',
    },
  },
})
